#pragma once

#include <infiniband/verbs.h>
#include <rdma/rdma_cma.h>

#include "memory/simple_buffer.hxx"
#include "priv/context.hxx"
#include "priv/endpoint.hxx"
#include "priv/verbs/connection.hxx"

namespace verbs {

class Endpoint : public EndpointBase {
  struct MRHandle {
    uint64_t address = -1;
    size_t length = -1;
    uint32_t rkey = -1;
  };

  friend class ConnectionHandle;

 public:
  Endpoint(naive::Buffers& send_buffers_, naive::Buffers& recv_buffers_);
  ~Endpoint();

  op_res_future_t post_recv(OpContext& ctx);

  op_res_future_t post_send(OpContext& ctx);

  bool progress();

 protected:
  void prepare(rdma_cm_id* id_);

 private:
  void setup_remote_param(const rdma_conn_param& remote_);

  naive::Buffers& send_buffers;
  naive::Buffers& recv_buffers;
  rdma_cm_id* id = nullptr;  // not own, just borrowed
  ibv_pd* pd = nullptr;
  ibv_cq* cq = nullptr;
  ibv_qp* qp = nullptr;
  ibv_mr* send_mr = nullptr;
  ibv_mr* recv_mr = nullptr;
  MRHandle local_mr_h[2];  // used for connection
  rdma_conn_param local = {};
  rdma_conn_param remote = {};
  MRHandle remote_mr_h;
};

}  // namespace verbs
