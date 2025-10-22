#pragma once

#include <doca_comch.h>
#include <doca_comch_consumer.h>
#include <doca_comch_producer.h>
#include <doca_ctx.h>
#include <doca_pe.h>

#include "doca/buffer.hxx"
#include "doca/helper.hxx"
#include "trans/common/context.hxx"
#include "trans/common/endpoint.hxx"

namespace dpx::doca::comch {

class Endpoint : public EndpointBase {
  friend class ConnectionHandle;

 public:
  Endpoint(Device &ch_dev_, Device &dma_dev_, doca::Buffers &send_buffers_, doca::Buffers &recv_buffers_);
  ~Endpoint();

  bool progress();

  op_res_future_t post_recv(OpContext &ctx);
  op_res_future_t post_send(OpContext &ctx);

  op_res_future_t post_write(BulkContext &ctx);
  op_res_future_t post_read(BulkContext &ctx);

  void register_remote_memory(RemoteBuffer r_buf);
  void unregister_remote_memory();

 protected:
  void prepare(doca_comch_connection *conn);
  void run(uint32_t remote_consumer_id);
  void stop();

 private:
  template <Op op>
  op_res_future_t post(BulkContext &ctx);

  template <TaskType type, TaskResult r>
  static void task_cb(doca_dma_task_t<type> *task, doca_data task_user_data, doca_data ctx_user_data);

  static void producer_state_change_cb(const doca_data, doca_ctx *, doca_ctx_states, doca_ctx_states);
  static void consumer_state_change_cb(const doca_data, doca_ctx *, doca_ctx_states, doca_ctx_states);
  static void dma_state_change_cb(const doca_data, doca_ctx *, doca_ctx_states, doca_ctx_states);

  doca_ctx_states consumer_state();
  doca_ctx_states producer_state();
  doca_ctx_states dma_state();
  bool consumer_running();
  bool consumer_stopped();
  bool producer_running();
  bool producer_stopped();
  bool dma_running();
  bool dma_stopped();

  Device &ch_dev;
  Device &dma_dev;
  doca::Buffers &send_buffers;
  doca::Buffers &recv_buffers;
  doca_pe *pe = nullptr;
  boost::fibers::mutex con_mu;
  boost::fibers::mutex pro_mu;
  doca_comch_consumer *con = nullptr;
  doca_comch_producer *pro = nullptr;
  uint32_t consumer_id = 0;
  boost::fibers::mutex dma_mu;
  doca_dma *dma = nullptr;
  doca_buf_inventory *inv = nullptr;
  doca_mmap *r_mmap = nullptr;  // only support one active bulk transfer
  size_t max_bulk_task_size = 0;
};

}  // namespace dpx::doca::comch
