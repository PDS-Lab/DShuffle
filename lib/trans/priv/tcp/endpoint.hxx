#pragma once

#include <liburing.h>

#include "memory/naive_buffer.hxx"
#include "trans/common/context.hxx"
#include "trans/common/endpoint.hxx"

namespace dpx::tcp {

class Endpoint : public EndpointBase {
  friend class ConnectionHandle;

 public:
  Endpoint(naive::Buffers &send_buffers_, naive::Buffers &recv_buffers_);

  ~Endpoint();

  bool progress();

  op_res_future_t post_recv(OpContext &ctx);
  op_res_future_t post_send(OpContext &ctx);
  op_res_future_t post_write(BulkContext &ctx);
  op_res_future_t post_read(BulkContext &ctx);

  void register_remote_memory(RemoteBuffer) { /* do nothing */ }
  void unregister_remote_memory() { /* do nothing */ }

  void stop();

 private:
  template <Op op>
  void post(OpContext &ctx);

  boost::fibers::mutex rpc_mu;
  int rpc_sock = -1;  // do not own, just borrowed
  boost::fibers::mutex bulk_mu;
  int bulk_sock = -1;  // do not own just borrowed
  io_uring ring;

  // currently unused
  naive::Buffers &send_buffers;
  naive::Buffers &recv_buffers;
};

}  // namespace dpx::tcp
