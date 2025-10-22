#pragma once

#include <doca_pe.h>
#include <doca_rdma.h>

#include "doca/buffer.hxx"
#include "doca/device.hxx"
#include "doca/helper.hxx"
#include "trans/common/context.hxx"
#include "trans/common/endpoint.hxx"

namespace dpx::doca::rdma {

struct ConnectionParam;

class Endpoint : public EndpointBase {
  friend class ConnectionHandle;

 public:
  Endpoint(Device &dev_, doca::Buffers &send_buffers_, doca::Buffers &recv_buffers_, bool enable_data_path_ = true);
  ~Endpoint();

  bool progress();

  op_res_future_t post_recv(OpContext &ctx);
  op_res_future_t post_send(OpContext &ctx);

  op_res_future_t post_write(BulkContext &ctx);
  op_res_future_t post_read(BulkContext &ctx);

  void register_remote_memory(RemoteBuffer r_buf);
  void unregister_remote_memory();

 protected:
  void prepare(const ConnectionParam &param);
  void stop();

 private:
  template <TaskType type>
  op_res_future_t post(OpContext &ctx);
  template <TaskType type>
  op_res_future_t post(BulkContext &ctx);

  template <TaskType type, TaskResult r>
  static void task_cb(doca_rdma_task_t<type> *task, doca_data task_user_data, doca_data ctx_user_data);

  static void state_change_cb(const doca_data user_data, doca_ctx *ctx, doca_ctx_states prev_state,
                              doca_ctx_states next_state);

  doca_ctx_states ctrl_path_state();
  doca_ctx_states data_path_state();
  bool ctrl_path_running();
  bool ctrl_path_stopped();
  bool data_path_running();
  bool data_path_stopped();

  Device &dev;
  doca::Buffers &send_buffers;
  doca::Buffers &recv_buffers;
  doca_pe *pe = nullptr;
  doca_rdma *ctrl_path;
  doca_rdma_connection *cp_conn;
  boost::fibers::mutex cp_mu;
  bool enable_data_path;
  doca_rdma *data_path;
  doca_rdma_connection *dp_conn;
  boost::fibers::mutex dp_mu;
  doca_buf_inventory *inv = nullptr;
  doca_mmap *r_mmap = nullptr;    // only support one active bulk transfer
  size_t max_bulk_task_size = 0;  // currently unused, 2M * 64 vs 128M * 1, have the same bandwidth
};

}  // namespace dpx::doca::rdma
