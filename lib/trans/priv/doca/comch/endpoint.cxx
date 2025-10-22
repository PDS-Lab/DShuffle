#include "trans/priv/doca/comch/endpoint.hxx"

#include <glaze/glaze.hpp>

#include "doca/helper.hxx"
#include "trans/common/context.hxx"
#include "trans/priv/doca/caps.hxx"
#include "util/unreachable.hxx"

namespace dpx::doca::comch {

namespace {

static constexpr const uint32_t max_dma_task_number = 2048;
static constexpr const uint32_t max_buf_number = max_dma_task_number * 2;

}  // namespace

Endpoint::Endpoint(Device &ch_dev_, Device &dma_dev_, doca::Buffers &send_buffers_, doca::Buffers &recv_buffers_)
    : ch_dev(ch_dev_), dma_dev(dma_dev_), send_buffers(send_buffers_), recv_buffers(recv_buffers_) {
  doca_check(doca_pe_create(&pe));
  doca_check(doca_buf_inventory_create(max_buf_number, &inv));
  doca_check(doca_buf_inventory_start(inv));
}

Endpoint::~Endpoint() {
  unregister_remote_memory();
  if (inv != nullptr) {
    doca_check(doca_buf_inventory_destroy(inv));
  }
  if (dma != nullptr) {
    doca_check(doca_dma_destroy(dma));
  }
  if (pro != nullptr) {
    doca_check(doca_comch_producer_destroy(pro));
  }
  if (con != nullptr) {
    doca_check(doca_comch_consumer_destroy(con));
  }
  if (pe != nullptr) {
    doca_check(doca_pe_destroy(pe));
  }
}

bool Endpoint::progress() { return doca_pe_progress(pe); }

op_res_future_t Endpoint::post_recv(OpContext &ctx) {
  std::lock_guard l(con_mu);
  if (!consumer_running()) {
    ctx.op_res.set_value(0);
    return ctx.op_res.get_future();
  }
  doca_comch_consumer_task_post_recv *task = nullptr;
  auto &buf = static_cast<doca::BorrowedBuffer &>(ctx.l_buf);
  doca_check(doca_comch_consumer_task_post_recv_alloc_init(con, buf.buf, &task));
  doca_task_set_user_data(doca_comch_consumer_task_post_recv_as_task(task), doca_data(&ctx));
  submit_task(doca_comch_consumer_task_post_recv_as_task(task));
  return ctx.op_res.get_future();
}

op_res_future_t Endpoint::post_send(OpContext &ctx) {
  std::lock_guard l(pro_mu);
  if (!producer_running()) {
    ctx.op_res.set_value(0);
    return ctx.op_res.get_future();
  }
  doca_comch_producer_task_send *task = nullptr;
  auto &buf = static_cast<doca::BorrowedBuffer &>(ctx.l_buf);
  doca_check(doca_buf_set_data_len(buf.buf, ctx.len));
  doca_check(doca_comch_producer_task_send_alloc_init(pro, buf.buf, nullptr, 0, consumer_id, &task));
  doca_task_set_user_data(doca_comch_producer_task_send_as_task(task), doca_data(&ctx));
  submit_task(doca_comch_producer_task_send_as_task(task));
  return ctx.op_res.get_future();
}

op_res_future_t Endpoint::post_write(BulkContext &ctx) { return post<Op::Write>(ctx); }
op_res_future_t Endpoint::post_read(BulkContext &ctx) { return post<Op::Read>(ctx); }

template <Op op>
op_res_future_t Endpoint::post(BulkContext &ctx) {
  std::lock_guard l(dma_mu);
  if (!dma_running()) {
    ctx.op_res.set_value(0);
    return ctx.op_res.get_future();
  }
  auto &l_buf = static_cast<doca::BorrowedBuffer &>(ctx.l_buf);
  auto &r_buf = ctx.r_buf;
  assert(r_mmap != nullptr && from_mmap(r_mmap).contain(r_buf));  // remote buffer is registered
  assert(l_buf.size() >= ctx.len);
  auto l_mmap = l_buf.within;
  for (auto off = 0uz; off < ctx.len; off += max_bulk_task_size) {
    doca_dma_task_memcpy *task = nullptr;
    doca_buf *local_buf = nullptr;
    doca_buf *remote_buf = nullptr;
    uint8_t *l_addr = l_buf.data() + off;
    uint8_t *r_addr = r_buf.data() + off;
    size_t len = std::min(max_bulk_task_size, ctx.len - off);
    doca_check(doca_buf_inventory_buf_get_by_addr(inv, l_mmap, l_addr, len, &local_buf));
    doca_check(doca_buf_inventory_buf_get_by_addr(inv, r_mmap, r_addr, len, &remote_buf));
    TRACE("{} task local addr: {}, remote addr: {}, local_buf: {}, remote_buf:{}, off: {}, len: {}", op, (void *)l_addr,
          (void *)r_addr, (void *)local_buf, (void *)remote_buf, off, len);
    if constexpr (op == Op::Write) {
      doca_check(doca_buf_set_data_len(local_buf, len));
      doca_check(doca_dma_task_memcpy_alloc_init(dma, local_buf, remote_buf, doca_data(&ctx), &task));
    } else if constexpr (op == Op::Read) {
      doca_check(doca_buf_set_data_len(remote_buf, len));
      doca_check(doca_dma_task_memcpy_alloc_init(dma, remote_buf, local_buf, doca_data(&ctx), &task));
    } else {
      static_unreachable;
    }
    submit_task(doca_dma_task_memcpy_as_task(task));
  }
  return ctx.op_res.get_future();
}

void Endpoint::register_remote_memory(RemoteBuffer r_buf) {
  if (r_mmap != nullptr) [[unlikely]] {
    if (r_buf == from_mmap(r_mmap)) {
      return;
    } else {
      die("Duplicated registration");
    }
  } else {
    TRACE("register remote memory region, addr: {}, size: {}, desc_len: {}", reinterpret_cast<void *>(r_buf.data()),
          r_buf.size(), r_buf.desc.size());
    doca_check(doca_mmap_create_from_export(nullptr, r_buf.desc.data(), r_buf.desc.size(), dma_dev.dev, &r_mmap));
  }
}

void Endpoint::unregister_remote_memory() {
  if (r_mmap != nullptr) {
    doca_check(doca_mmap_stop(r_mmap));
    doca_check(doca_mmap_destroy(r_mmap));
    r_mmap = nullptr;
  }
}

void Endpoint::prepare(doca_comch_connection *conn) {
  {
    doca_check(doca_comch_consumer_create(conn, recv_buffers.mmap, &con));
    auto ctx = doca_comch_consumer_as_ctx(con);
    doca_check(doca_pe_connect_ctx(pe, ctx));
    doca_check(doca_ctx_set_state_changed_cb(ctx, consumer_state_change_cb));
    doca_check(doca_comch_consumer_task_post_recv_set_conf(con, task_cb<TaskType::Recv, TaskResult::Success>,
                                                           task_cb<TaskType::Recv, TaskResult::Failure>,
                                                           recv_buffers.n_elements()));
    doca_check(doca_ctx_set_user_data(ctx, doca_data(this)));
    doca_check(doca_comch_consumer_get_id(con, &consumer_id));
    doca_check_ext(doca_ctx_start(ctx), DOCA_ERROR_IN_PROGRESS);
  }
  {
    doca_check(doca_comch_producer_create(conn, &pro));
    auto ctx = doca_comch_producer_as_ctx(pro);
    doca_check(doca_pe_connect_ctx(pe, ctx));
    doca_check(doca_ctx_set_state_changed_cb(ctx, producer_state_change_cb));
    doca_check(doca_comch_producer_task_send_set_conf(pro, task_cb<TaskType::Send, TaskResult::Success>,
                                                      task_cb<TaskType::Send, TaskResult::Failure>,
                                                      send_buffers.n_elements()));
    doca_check(doca_ctx_set_user_data(ctx, doca_data(this)));
    doca_check(doca_ctx_start(ctx));
  }
  {
    doca_check(doca_dma_create(dma_dev.dev, &dma));
    auto ctx = doca_dma_as_ctx(dma);
    doca_check(doca_pe_connect_ctx(pe, ctx));
    doca_check(doca_ctx_set_state_changed_cb(ctx, dma_state_change_cb));
    doca_check(doca_dma_task_memcpy_set_conf(dma, task_cb<TaskType::Memcpy, TaskResult::Success>,
                                             task_cb<TaskType::Memcpy, TaskResult::Failure>, max_dma_task_number));
    doca_check(doca_ctx_set_user_data(ctx, doca_data(this)));
    doca_check(doca_ctx_start(ctx));
  }
  DMACapability caps = probe_dma_caps(dma_dev, dma);
  INFO("DMA capability:\n{}", glz::write<glz::opts{.prettify = true}>(caps).value_or("Unexpected!"));
  max_bulk_task_size = caps.max_buf_size;
  EndpointBase::prepare();
}

void Endpoint::run(uint32_t remote_consumer_id) {
  assert(consumer_id == remote_consumer_id);
  EndpointBase::run();
}

void Endpoint::stop() {
  EndpointBase::stop();
  {
    std::lock_guard l(pro_mu);
    doca_check_ext(doca_ctx_stop(doca_comch_producer_as_ctx(pro)), DOCA_ERROR_IN_PROGRESS);
  }
  {
    std::lock_guard l(con_mu);
    consumer_id = 0;
    // pre post tasks will cause DOCA_ERROR_IN_PROGRESS
    doca_check_ext(doca_ctx_stop(doca_comch_consumer_as_ctx(con)), DOCA_ERROR_IN_PROGRESS);
  }
  {
    std::lock_guard l(dma_mu);
    doca_check_ext(doca_ctx_stop(doca_dma_as_ctx(dma)), DOCA_ERROR_IN_PROGRESS);
  }
}

template <TaskType type, TaskResult r>
void Endpoint::task_cb(doca_dma_task_t<type> *task, doca_data task_user_data, doca_data ctx_user_data) {
  [[maybe_unused]] auto e = static_cast<Endpoint *>(ctx_user_data.ptr);

  auto underlying_task = task_cast<type>(task);

  auto ctx = static_cast<OpContext *>(task_user_data.ptr);

  size_t data_len = 0;
  if constexpr (type == TaskType::Send || type == TaskType::Recv) {
    auto src_buf = task_get_src_buf<type>(task);
    doca_check(doca_buf_get_data_len(src_buf, &data_len));
  } else if constexpr (type == TaskType::Memcpy) {
    auto dst_buf = task_get_dst_buf<type>(task);
    auto src_buf = task_get_src_buf<type>(task);
    auto bctx = static_cast<BulkContext *>(ctx);
    doca_check(doca_buf_get_data_len(src_buf, &data_len));
    TRACE("{} task local addr: {}, remote addr: {}, local_buf: {}, remote_buf:{}, total len: {}, len: {}", bctx->op,
          (void *)bctx->l_buf.data(), (void *)bctx->r_buf.data(), (void *)src_buf, (void *)dst_buf, ctx->tx_size,
          data_len);
    doca_check(doca_buf_dec_refcount(dst_buf, nullptr));
    doca_check(doca_buf_dec_refcount(const_cast<doca_buf *>(src_buf), nullptr));
  } else {
    static_unreachable;
  }

  if (!e->running()) {
    WARN("Endpoint is not running");
    doca_task_free(underlying_task);
    return;
  }

  ctx->tx_size += data_len;

  if constexpr (r == TaskResult::Failure) {
    auto status = doca_task_get_status(underlying_task);
    if (status == DOCA_ERROR_BAD_STATE) {
      WARN("Endpoint may be stopped...");
      ctx->op_res.set_value(0);
    } else {
      ERROR("{} task failure: {}", type, doca_error_get_descr(status));
      if constexpr (type == TaskType::Send || type == TaskType::Recv) {
        ctx->op_res.set_value(-status);
      } else if constexpr (type == TaskType::Memcpy) {
        if (ctx->tx_size == ctx->len) {
          TRACE("trigger dma task done, local addr: {}", (void *)ctx->l_buf.data());
          ctx->op_res.set_value(-status);
        }
      } else {
        static_unreachable;
      }
    }
  } else if constexpr (r == TaskResult::Success) {
    if constexpr (type == TaskType::Send || type == TaskType::Recv) {
      ctx->op_res.set_value(ctx->tx_size);
    } else if constexpr (type == TaskType::Memcpy) {
      if (ctx->tx_size == ctx->len) {
        TRACE("trigger dma task done, local addr: {}", (void *)ctx->l_buf.data());
        ctx->op_res.set_value(ctx->tx_size);
      }
    } else {
      static_unreachable;
    }
  } else {
    static_unreachable;
  }

  doca_task_free(underlying_task);
}

void Endpoint::producer_state_change_cb(const doca_data ctx_user_data, doca_ctx *,
                                        [[maybe_unused]] doca_ctx_states prev_state,
                                        [[maybe_unused]] doca_ctx_states next_state) {
  [[maybe_unused]] auto e = reinterpret_cast<Endpoint *>(ctx_user_data.ptr);
  INFO("DOCA Comch producer state change: {} -> {}", prev_state, next_state);
}

void Endpoint::consumer_state_change_cb(const doca_data ctx_user_data, doca_ctx *,
                                        [[maybe_unused]] doca_ctx_states prev_state,
                                        [[maybe_unused]] doca_ctx_states next_state) {
  auto e = reinterpret_cast<Endpoint *>(ctx_user_data.ptr);
  INFO("DOCA Comch consumer state change: {} -> {}", prev_state, next_state);
  switch (next_state) {
    case DOCA_CTX_STATE_IDLE: {
      e->shutdown();
    } break;
    case DOCA_CTX_STATE_STARTING: {
    } break;
    case DOCA_CTX_STATE_RUNNING: {
    } break;
    case DOCA_CTX_STATE_STOPPING: {
    } break;
  }
}

void Endpoint::dma_state_change_cb(const doca_data ctx_user_data, doca_ctx *,
                                   [[maybe_unused]] doca_ctx_states prev_state,
                                   [[maybe_unused]] doca_ctx_states next_state) {
  [[maybe_unused]] auto e = reinterpret_cast<Endpoint *>(ctx_user_data.ptr);
  INFO("DOCA DMA state change: {} -> {}", prev_state, next_state);
}

doca_ctx_states Endpoint::consumer_state() { return get_ctx_state(doca_comch_consumer_as_ctx(con)); }
doca_ctx_states Endpoint::producer_state() { return get_ctx_state(doca_comch_producer_as_ctx(pro)); }
doca_ctx_states Endpoint::dma_state() { return get_ctx_state(doca_dma_as_ctx(dma)); }
bool Endpoint::consumer_running() { return consumer_state() == DOCA_CTX_STATE_RUNNING; }
bool Endpoint::consumer_stopped() { return consumer_state() == DOCA_CTX_STATE_IDLE; }
bool Endpoint::producer_running() { return producer_state() == DOCA_CTX_STATE_RUNNING; }
bool Endpoint::producer_stopped() { return producer_state() == DOCA_CTX_STATE_IDLE; }
bool Endpoint::dma_running() { return dma_state() == DOCA_CTX_STATE_RUNNING; }
bool Endpoint::dma_stopped() { return dma_state() == DOCA_CTX_STATE_IDLE; }

}  // namespace dpx::doca::comch

template <>
struct std::formatter<dpx::OpContext> : std::formatter<std::string> {
  template <typename Context>
  Context::iterator format(const dpx::OpContext &ctx, Context out) const {
    return std::formatter<std::string>::format(
        std::format("{} op, buf: {}, buf_len: {}, data_len: {}", ctx.op,
                    reinterpret_cast<const void *>(ctx.l_buf.data()), ctx.l_buf.size(), ctx.len),
        out);
  }
};
