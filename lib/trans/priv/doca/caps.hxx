#pragma once

#include <doca_comch.h>
#include <doca_comch_consumer.h>
#include <doca_comch_producer.h>
#include <doca_dma.h>
#include <doca_rdma.h>

#include "doca/check.hxx"
#include "doca/device.hxx"

namespace dpx::doca {

struct ComchCapability {
  struct {
    bool client_is_supported = false;
    bool server_is_supported = false;
    uint32_t max_clients_per_server = -1;
    uint32_t max_name_len = -1;
    uint32_t max_msg_size = -1;
    uint32_t max_send_tasks = -1;
    uint32_t max_recv_queue_size = -1;
  } ctrl_path;
  struct {
    struct {
      bool is_supported = false;
      uint32_t max_number = -1;
      uint32_t max_num_tasks = -1;
      uint32_t max_buf_size = -1;
      uint32_t max_buf_list_len = -1;
    } producer;
    struct {
      bool is_supported = false;
      uint32_t max_number = -1;
      uint32_t max_num_tasks = -1;
      uint32_t max_buf_size = -1;
      uint32_t max_buf_list_len = -1;
      uint32_t max_imm_data_len = -1;
    } consumer;
  } data_path;
};

struct DMACapability {
  bool is_supported = false;
  uint32_t max_num_tasks = -1;
  uint64_t max_buf_size = -1;
  uint32_t max_buf_list_len = -1;
};

struct RDMACapability {
  bool is_supported = false;
  bool task_send_imm_is_supported = false;
  bool task_send_is_supported = false;
  bool task_read_is_supported = false;
  bool task_write_is_supported = false;
  bool task_write_imm_is_supported = false;
  bool task_receive_is_supported = false;
  uint32_t task_receive_max_dst_buf_list_len = -1;
  uint32_t max_message_size = -1;
  uint32_t max_send_buf_list_len = -1;
  uint32_t max_send_queue_size = -1;
  uint32_t max_recv_queue_size = -1;
  uint32_t gid_table_size = -1;
};

inline ComchCapability probe_comch_caps(Device& dev) {
  ComchCapability caps = {};
  auto dev_info = doca_dev_as_devinfo(dev.dev);
  caps.ctrl_path.client_is_supported = doca_comch_cap_client_is_supported(dev_info) == DOCA_SUCCESS;
  caps.ctrl_path.server_is_supported = doca_comch_cap_server_is_supported(dev_info) == DOCA_SUCCESS;
  if (caps.ctrl_path.client_is_supported || caps.ctrl_path.server_is_supported) {
    doca_check(doca_comch_cap_get_max_clients(dev_info, &caps.ctrl_path.max_clients_per_server));
    doca_check(doca_comch_cap_get_max_name_len(dev_info, &caps.ctrl_path.max_name_len));
    doca_check(doca_comch_cap_get_max_msg_size(dev_info, &caps.ctrl_path.max_msg_size));
    doca_check(doca_comch_cap_get_max_send_tasks(dev_info, &caps.ctrl_path.max_send_tasks));
    doca_check(doca_comch_cap_get_max_recv_queue_size(dev_info, &caps.ctrl_path.max_recv_queue_size));
  }
  caps.data_path.producer.is_supported = doca_comch_producer_cap_is_supported(dev_info) == DOCA_SUCCESS;
  if (caps.data_path.producer.is_supported) {
    doca_check(doca_comch_producer_cap_get_max_producers(dev_info, &caps.data_path.producer.max_number));
    doca_check(doca_comch_producer_cap_get_max_num_tasks(dev_info, &caps.data_path.producer.max_num_tasks));
    doca_check(doca_comch_producer_cap_get_max_buf_size(dev_info, &caps.data_path.producer.max_buf_size));
    doca_check(doca_comch_producer_cap_get_max_buf_list_len(dev_info, &caps.data_path.producer.max_buf_list_len));
  }
  caps.data_path.consumer.is_supported = doca_comch_consumer_cap_is_supported(dev_info) == DOCA_SUCCESS;
  if (caps.data_path.consumer.is_supported) {
    doca_check(doca_comch_consumer_cap_get_max_consumers(dev_info, &caps.data_path.consumer.max_number));
    doca_check(doca_comch_consumer_cap_get_max_imm_data_len(dev_info, &caps.data_path.consumer.max_imm_data_len));
    doca_check(doca_comch_consumer_cap_get_max_buf_list_len(dev_info, &caps.data_path.consumer.max_buf_list_len));
    doca_check(doca_comch_consumer_cap_get_max_buf_size(dev_info, &caps.data_path.consumer.max_buf_size));
    doca_check(doca_comch_consumer_cap_get_max_num_tasks(dev_info, &caps.data_path.consumer.max_num_tasks));
  }
  return caps;
}

inline DMACapability probe_dma_caps(Device& dev, doca_dma* dma) {
  DMACapability caps = {};
  auto dev_info = doca_dev_as_devinfo(dev.dev);
  caps.is_supported = doca_dma_cap_task_memcpy_is_supported(dev_info) == DOCA_SUCCESS;
  if (caps.is_supported) {
    doca_check(doca_dma_cap_get_max_num_tasks(dma, &caps.max_num_tasks));
    doca_check(doca_dma_cap_task_memcpy_get_max_buf_size(dev_info, &caps.max_buf_size));
    doca_check(doca_dma_cap_task_memcpy_get_max_buf_list_len(dev_info, &caps.max_buf_list_len));
  }
  return caps;
}

inline RDMACapability probe_rdma_caps(Device& dev) {
  RDMACapability caps = {};
  auto dev_info = doca_dev_as_devinfo(dev.dev);
  caps.is_supported = doca_rdma_cap_transport_type_is_supported(dev_info, DOCA_RDMA_TRANSPORT_TYPE_RC) == DOCA_SUCCESS;
  if (caps.is_supported) {
    caps.task_send_imm_is_supported = doca_rdma_cap_task_send_imm_is_supported(dev_info) == DOCA_SUCCESS;
    caps.task_send_is_supported = doca_rdma_cap_task_send_is_supported(dev_info) == DOCA_SUCCESS;
    caps.task_read_is_supported = doca_rdma_cap_task_read_is_supported(dev_info) == DOCA_SUCCESS;
    caps.task_write_is_supported = doca_rdma_cap_task_write_is_supported(dev_info) == DOCA_SUCCESS;
    caps.task_write_imm_is_supported = doca_rdma_cap_task_write_imm_is_supported(dev_info) == DOCA_SUCCESS;
    caps.task_receive_is_supported = doca_rdma_cap_task_receive_is_supported(dev_info) == DOCA_SUCCESS;
    doca_check(doca_rdma_cap_get_max_message_size(dev_info, &caps.max_message_size));
    doca_check(doca_rdma_cap_get_max_send_buf_list_len(dev_info, &caps.max_send_buf_list_len));
    doca_check(doca_rdma_cap_get_max_send_queue_size(dev_info, &caps.max_send_queue_size));
    doca_check(doca_rdma_cap_get_max_recv_queue_size(dev_info, &caps.max_recv_queue_size));
    doca_check(doca_rdma_cap_task_receive_get_max_dst_buf_list_len(dev_info, DOCA_RDMA_TRANSPORT_TYPE_RC,
                                                                   &caps.task_receive_max_dst_buf_list_len));
    doca_check(doca_rdma_cap_get_gid_table_size(dev_info, &caps.gid_table_size));
    // doca_check(doca_rdma_cap_get_gid(dev_info, 0, caps.gid_table_size, caps.gid_table));
  }
  return caps;
}

}  // namespace dpx::doca
