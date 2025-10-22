#include "trans/priv/doca/rdma/endpoint.hxx"

#include "doca/helper.hxx"
#include "trans/priv/doca/rdma/connection.hxx"

namespace dpx::doca::rdma {

namespace {

static constexpr const uint32_t max_rw_task_number = 2048;
static constexpr const uint32_t max_buf_number = max_rw_task_number * 2;

}  // namespace

Endpoint::Endpoint(Device &dev_, doca::Buffers &send_buffers_, doca::Buffers &recv_buffers_, bool enable_data_path_)
    : dev(dev_), send_buffers(send_buffers_), recv_buffers(recv_buffers_), enable_data_path(enable_data_path_) {
  doca_check(doca_pe_create(&pe));
  doca_check(doca_rdma_create(dev.dev, &ctrl_path));
  if (enable_data_path) {
    doca_check(doca_rdma_create(dev.dev, &data_path));
    doca_check(doca_buf_inventory_create(max_buf_number, &inv));
    doca_check(doca_buf_inventory_start(inv));
  }
}

Endpoint::~Endpoint() {
  unregister_remote_memory();
  if (inv != nullptr) {
    doca_check(doca_buf_inventory_destroy(inv));
  }
  if (ctrl_path != nullptr) {
    doca_check(doca_rdma_destroy(ctrl_path));
  }
  if (data_path != nullptr) {
    doca_check(doca_rdma_destroy(data_path));
  }
  if (pe != nullptr) {
    doca_check(doca_pe_destroy(pe));
  }
}

bool Endpoint::progress() { return doca_pe_progress(pe); }

op_res_future_t Endpoint::post_recv(OpContext &ctx) { return post<TaskType::Recv>(ctx); }
op_res_future_t Endpoint::post_send(OpContext &ctx) { return post<TaskType::Send>(ctx); }
op_res_future_t Endpoint::post_write(BulkContext &ctx) {
  assert(enable_data_path);
  return post<TaskType::Write>(ctx);
}
op_res_future_t Endpoint::post_read(BulkContext &ctx) {
  assert(enable_data_path);
  return post<TaskType::Read>(ctx);
}

template <TaskType type>
op_res_future_t Endpoint::post(OpContext &ctx) {
  std::lock_guard l(cp_mu);
  if (!ctrl_path_running()) {
    ctx.op_res.set_value(0);
    return ctx.op_res.get_future();
  }
  auto &buf = static_cast<doca::BorrowedBuffer &>(ctx.l_buf);
  if constexpr (type == TaskType::Recv) {
    doca_rdma_task_receive *task;
    doca_check(doca_rdma_task_receive_allocate_init(ctrl_path, buf.buf, doca_data(&ctx), &task));
    submit_task(doca_rdma_task_receive_as_task(task));
  } else if constexpr (type == TaskType::Send) {
    doca_check(doca_buf_set_data_len(buf.buf, ctx.len));
    doca_rdma_task_send *task;
    doca_check(doca_rdma_task_send_allocate_init(ctrl_path, cp_conn, buf.buf, doca_data(&ctx), &task));
    submit_task(doca_rdma_task_send_as_task(task));
  } else {
    static_unreachable;
  }
  return ctx.op_res.get_future();
}

template <TaskType type>
op_res_future_t Endpoint::post(BulkContext &ctx) {
  std::lock_guard l(dp_mu);
  if (!data_path_running()) {
    ctx.op_res.set_value(0);
    return ctx.op_res.get_future();
  }
  auto &l_buf = static_cast<doca::BorrowedBuffer &>(ctx.l_buf);
  auto &r_buf = ctx.r_buf;
  assert(r_mmap != nullptr && from_mmap(r_mmap).contain(r_buf));  // remote buffer is registered
  assert(l_buf.size() >= ctx.len);
  auto l_mmap = l_buf.within;
  doca_buf *src_buf = nullptr;
  doca_buf *dst_buf = nullptr;
  if constexpr (type == TaskType::Write) {
    doca_rdma_task_write *task = nullptr;
    doca_check(doca_buf_inventory_buf_get_by_addr(inv, l_mmap, l_buf.data(), l_buf.size(), &src_buf));
    doca_check(doca_buf_inventory_buf_get_by_addr(inv, r_mmap, r_buf.data(), r_buf.size(), &dst_buf));
    TRACE("{} task local addr: {}, remote addr: {}, src_buf: {}, dst_buf:{}, len: {}", type, (void *)l_buf.data(),
          (void *)r_buf.data(), (void *)src_buf, (void *)dst_buf, ctx.len);
    doca_check(doca_buf_set_data_len(src_buf, ctx.len));
    doca_check(doca_rdma_task_write_allocate_init(data_path, dp_conn, src_buf, dst_buf, doca_data(&ctx), &task));
    submit_task(doca_rdma_task_write_as_task(task));
  } else if constexpr (type == TaskType::Read) {
    doca_rdma_task_read *task = nullptr;
    doca_check(doca_buf_inventory_buf_get_by_addr(inv, r_mmap, r_buf.data(), r_buf.size(), &src_buf));
    doca_check(doca_buf_inventory_buf_get_by_addr(inv, l_mmap, l_buf.data(), l_buf.size(), &dst_buf));
    TRACE("{} task local addr: {}, remote addr: {}, src_buf: {}, dst_buf:{}, len: {}", type, (void *)l_buf.data(),
          (void *)r_buf.data(), (void *)src_buf, (void *)dst_buf, ctx.len);
    doca_check(doca_buf_set_data_len(src_buf, ctx.len));
    doca_check(doca_rdma_task_read_allocate_init(data_path, dp_conn, src_buf, dst_buf, doca_data(&ctx), &task));
    submit_task(doca_rdma_task_read_as_task(task));
  } else {
    static_unreachable;
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
    doca_check(doca_mmap_create_from_export(nullptr, r_buf.desc.data(), r_buf.desc.size(), dev.dev, &r_mmap));
  }
}

void Endpoint::unregister_remote_memory() {
  if (r_mmap != nullptr) {
    doca_check(doca_mmap_stop(r_mmap));
    doca_check(doca_mmap_destroy(r_mmap));
    r_mmap = nullptr;
  }
}

void Endpoint::prepare(const ConnectionParam &param) {
  {  // ctrl path
    doca_check(doca_rdma_set_grh_enabled(ctrl_path, param.enable_grh));
    doca_check(doca_rdma_set_transport_type(ctrl_path, DOCA_RDMA_TRANSPORT_TYPE_RC));
    // ctrl path only use send/recv
    doca_check(doca_rdma_set_permissions(ctrl_path, DOCA_ACCESS_FLAG_LOCAL_READ_WRITE));
    doca_check(doca_rdma_set_send_queue_size(ctrl_path, send_buffers.n_elements()));
    doca_check(doca_rdma_task_send_set_conf(ctrl_path, task_cb<TaskType::Send, TaskResult::Success>,
                                            task_cb<TaskType::Send, TaskResult::Failure>, send_buffers.n_elements()));
    // doca_check(doca_rdma_task_send_imm_set_conf(ctrl_path, task_cb<TaskType::SendWithImm, TaskResult::Success>,
    //                                             task_cb<TaskType::SendWithImm, TaskResult::Failure>,
    //                                             send_buffers.n_elements()));
    doca_check(doca_rdma_task_receive_set_conf(ctrl_path, task_cb<TaskType::Recv, TaskResult::Success>,
                                               task_cb<TaskType::Recv, TaskResult::Failure>,
                                               recv_buffers.n_elements()));
    auto ctx = doca_rdma_as_ctx(ctrl_path);
    doca_check(doca_ctx_set_state_changed_cb(ctx, state_change_cb));
    doca_check(doca_pe_connect_ctx(pe, ctx));
    doca_check(doca_ctx_set_user_data(ctx, doca_data(this)));
    doca_check(doca_ctx_start(ctx));
  }
  if (enable_data_path) {  // data path
    doca_check(doca_rdma_set_grh_enabled(data_path, param.enable_grh));
    doca_check(doca_rdma_set_transport_type(data_path, DOCA_RDMA_TRANSPORT_TYPE_RC));
    doca_check(doca_rdma_set_permissions(
        data_path, DOCA_ACCESS_FLAG_LOCAL_READ_WRITE | DOCA_ACCESS_FLAG_RDMA_READ | DOCA_ACCESS_FLAG_RDMA_WRITE));
    // data path only use read/write
    doca_check(doca_rdma_set_send_queue_size(data_path, send_buffers.n_elements()));
    doca_check(doca_rdma_task_read_set_conf(data_path, task_cb<TaskType::Read, TaskResult::Success>,
                                            task_cb<TaskType::Read, TaskResult::Failure>, max_rw_task_number));
    doca_check(doca_rdma_task_write_set_conf(data_path, task_cb<TaskType::Write, TaskResult::Success>,
                                             task_cb<TaskType::Write, TaskResult::Failure>, max_rw_task_number));
    // doca_check(doca_rdma_task_write_imm_set_conf(data_path, task_cb<TaskType::WriteWithImm, TaskResult::Success>,
    //                                              task_cb<TaskType::WriteWithImm, TaskResult::Failure>, 512));
    // doca_check(doca_rdma_task_receive_set_conf(data_path, task_cb<TaskType::Recv, TaskResult::Success>,
    //                                            task_cb<TaskType::Recv, TaskResult::Failure>,
    //                                            recv_buffers.n_elements()));
    auto ctx = doca_rdma_as_ctx(data_path);
    doca_check(doca_ctx_set_state_changed_cb(ctx, state_change_cb));
    doca_check(doca_pe_connect_ctx(pe, ctx));
    doca_check(doca_ctx_set_user_data(ctx, doca_data(this)));
    doca_check(doca_ctx_start(ctx));
  }
  EndpointBase::prepare();
}

template <TaskType type, TaskResult r>
void Endpoint::task_cb(doca_rdma_task_t<type> *task, doca_data task_user_data, doca_data ctx_user_data) {
  [[maybe_unused]] auto e = static_cast<Endpoint *>(ctx_user_data.ptr);
  auto ctx = static_cast<OpContext *>(task_user_data.ptr);
  auto underlying_task = task_cast<type>(task);

  size_t data_len = 0;
  if constexpr (type == TaskType::Recv) {
    auto dst_buf = task_get_dst_buf<type>(task);
    doca_check(doca_buf_get_data_len(dst_buf, &data_len));
  } else if constexpr (type == TaskType::Send || type == TaskType::SendWithImm) {
    auto src_buf = task_get_src_buf<type>(task);
    doca_check(doca_buf_get_data_len(src_buf, &data_len));
  } else if constexpr (type == TaskType::Read || type == TaskType::Write || type == TaskType::WriteWithImm) {
    auto dst_buf = task_get_dst_buf<type>(task);
    auto src_buf = task_get_src_buf<type>(task);
    doca_check(doca_buf_get_data_len(src_buf, &data_len));
    doca_check(doca_buf_dec_refcount(dst_buf, nullptr));
    doca_check(doca_buf_dec_refcount(const_cast<doca_buf *>(src_buf), nullptr));
  } else {
    static_unreachable;
  }

  TRACE("{} {}", type, data_len);

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
      if constexpr (type == TaskType::Send || type == TaskType::SendWithImm || type == TaskType::Recv) {
        ctx->op_res.set_value(-status);
      } else if constexpr (type == TaskType::Read || type == TaskType::Write || type == TaskType::WriteWithImm) {
        if (ctx->tx_size == ctx->len) {
          ctx->op_res.set_value(-status);
        }
      } else {
        static_unreachable;
      }
    }
  } else if constexpr (r == TaskResult::Success) {
    if constexpr (type == TaskType::Send || type == TaskType::SendWithImm || type == TaskType::Recv) {
      ctx->op_res.set_value(ctx->tx_size);
    } else if constexpr (type == TaskType::Read || type == TaskType::Write || type == TaskType::WriteWithImm) {
      if (ctx->tx_size == ctx->len) {
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

void Endpoint::stop() {
  {
    std::lock_guard l(cp_mu);
    doca_check_ext(doca_ctx_stop(doca_rdma_as_ctx(ctrl_path)), DOCA_ERROR_IN_PROGRESS);
  }
  if (enable_data_path) {
    std::lock_guard l(dp_mu);
    doca_check_ext(doca_ctx_stop(doca_rdma_as_ctx(data_path)), DOCA_ERROR_IN_PROGRESS);
  }
  EndpointBase::stop();
}

void Endpoint::state_change_cb(const doca_data, doca_ctx *, doca_ctx_states prev_state, doca_ctx_states next_state) {
  INFO("DOCA RDMA state change: {} -> {}", prev_state, next_state);
  switch (next_state) {
    case DOCA_CTX_STATE_IDLE: {
    } break;
    case DOCA_CTX_STATE_STARTING: {
    } break;
    case DOCA_CTX_STATE_RUNNING: {
    } break;
    case DOCA_CTX_STATE_STOPPING: {
    } break;
  }
}

doca_ctx_states Endpoint::ctrl_path_state() { return get_ctx_state(doca_rdma_as_ctx(ctrl_path)); }
doca_ctx_states Endpoint::data_path_state() { return get_ctx_state(doca_rdma_as_ctx(data_path)); }
bool Endpoint::ctrl_path_running() { return ctrl_path_state() == DOCA_CTX_STATE_RUNNING; }
bool Endpoint::ctrl_path_stopped() { return ctrl_path_state() == DOCA_CTX_STATE_IDLE; }
bool Endpoint::data_path_running() { return enable_data_path && data_path_state() == DOCA_CTX_STATE_RUNNING; }
bool Endpoint::data_path_stopped() { return enable_data_path && data_path_state() == DOCA_CTX_STATE_IDLE; }

}  // namespace dpx::doca::rdma
