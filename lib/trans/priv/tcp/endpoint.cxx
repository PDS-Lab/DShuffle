#include "trans/priv/tcp/endpoint.hxx"

#include "util/fatal.hxx"
#include "util/socket.hxx"
#include "util/unreachable.hxx"

namespace dpx::tcp {

Endpoint::Endpoint(naive::Buffers &send_buffers_, naive::Buffers &recv_buffers_)
    : send_buffers(send_buffers_), recv_buffers(recv_buffers_) {
  uint32_t ring_size = send_buffers.n_elements() + recv_buffers.n_elements() + 1;
  if (auto ec = io_uring_queue_init(ring_size, &ring, 0); ec < 0) {
    die("Fail to init ring, errno: {}", -ec);
  }
  iovec v = {
      .iov_base = send_buffers.data(),
      .iov_len = send_buffers.size(),
  };
  if (auto ec = io_uring_register_buffers(&ring, &v, 1); ec < 0) {
    die("Fail to register sparse buffers, errno: {}", -ec);
  }
  EndpointBase::prepare();
}

Endpoint::~Endpoint() {
  if (auto ec = io_uring_unregister_buffers(&ring); ec < 0) {
    die("Fail to unregister buffers, errno: {}", -ec);
  }
  io_uring_queue_exit(&ring);
};

bool Endpoint::progress() {
  io_uring_cqe *cqe = nullptr;
  io_uring_peek_cqe(&ring, &cqe);
  if (cqe == nullptr) {
    return false;
  }
  OpContext *ctx = reinterpret_cast<OpContext *>(io_uring_cqe_get_data(cqe));
  if (cqe->res > 0) {
    ctx->tx_size += cqe->res;
  } else {
    ctx->op_res.set_value(cqe->res);  // something wrong
    io_uring_cqe_seen(&ring, cqe);
    return true;
  }
  DEBUG("tcp done post {} {} {} {}", ctx->op, (void *)ctx->l_buf.data(), ctx->l_buf.size(), ctx->tx_size);
  if (ctx->op == Op::Write && ctx->tx_size < ctx->l_buf.size()) {  // write partially
    io_uring_cqe_seen(&ring, cqe);
    post<Op::Write>(*ctx);
    return true;
  }
  ctx->op_res.set_value(ctx->tx_size);
  io_uring_cqe_seen(&ring, cqe);
  return true;
}

op_res_future_t Endpoint::post_recv(OpContext &ctx) {
  std::lock_guard l(rpc_mu);
  post<Op::Recv>(ctx);
  return ctx.op_res.get_future();
}

op_res_future_t Endpoint::post_send(OpContext &ctx) {
  std::lock_guard l(rpc_mu);
  post<Op::Send>(ctx);
  return ctx.op_res.get_future();
}

op_res_future_t Endpoint::post_write(BulkContext &ctx) {
  std::lock_guard l(bulk_mu);
  post<Op::Write>(ctx);
  return ctx.op_res.get_future();
}

op_res_future_t Endpoint::post_read(BulkContext &ctx) {
  std::lock_guard l(bulk_mu);
  post<Op::Read>(ctx);
  return ctx.op_res.get_future();
}

// NOTICE
// Because of the tcp stick package problem, we here send the whole buffer in one post.
template <Op op>
void Endpoint::post(OpContext &ctx) {
  auto &buf = ctx.l_buf;
  DEBUG("tcp post {} {} {} {}", ctx.op, (void *)buf.data(), buf.size(), ctx.tx_size);
  auto sqe = io_uring_get_sqe(&ring);
  if constexpr (op == Op::Send) {
    io_uring_prep_write_fixed(sqe, rpc_sock, buf.data(), buf.size(), 0, 0);
  } else if constexpr (op == Op::Recv) {
    io_uring_prep_recv(sqe, rpc_sock, buf.data(), buf.size(), MSG_WAITALL);
  } else if constexpr (op == Op::Read) {
    io_uring_prep_recv(sqe, bulk_sock, buf.data(), buf.size(), MSG_WAITALL);
  } else if constexpr (op == Op::Write) {
    io_uring_prep_write(sqe, bulk_sock, buf.data() + ctx.tx_size, buf.size() - ctx.tx_size, 0);
  } else {
    static_unreachable;
  }
  io_uring_sqe_set_data(sqe, &ctx);
  if (auto ec = io_uring_submit(&ring); ec < 0) {
    die("Fail to submit sqe, errno: {}", -ec);
  }
}

void Endpoint::stop() {
  {
    std::lock_guard l(rpc_mu);
    close_socket(rpc_sock);
  }
  {
    std::lock_guard l(bulk_mu);
    close_socket(bulk_sock);
  }
  EndpointBase::stop();
}

}  // namespace dpx::tcp
