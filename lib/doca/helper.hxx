#pragma once

#include <doca_comch_consumer.h>
#include <doca_comch_producer.h>
#include <doca_ctx.h>
#include <doca_dev.h>
#include <doca_dma.h>
#include <doca_dpa.h>
#include <doca_rdma.h>

#include "util/enum_formatter.hxx"
#include "util/unreachable.hxx"

namespace dpx {

class MemoryRegion;

namespace doca {

doca_ctx_states get_ctx_state(doca_ctx *ctx);

void submit_task(doca_task *task);

doca_dev *open_dev_by_pci_addr(std::string_view pci_addr);

doca_dev *open_dev_by_ib_device_name(std::string_view ib_device_name);

doca_dev_rep *open_dev_rep(doca_dev *dev, std::string_view pci_addr, doca_devinfo_rep_filter filter);

MemoryRegion from_mmap(doca_mmap *mmap);

enum class TaskResult {
  Success,
  Failure,
};

namespace rdma {

enum class TaskType {
  Send,
  SendWithImm,
  Read,
  Recv,
  Write,
  WriteWithImm,
};

// clang-format off
template <TaskType t>
using doca_rdma_task_t = std::conditional_t<t == TaskType::Send,         doca_rdma_task_send,
                         std::conditional_t<t == TaskType::SendWithImm,  doca_rdma_task_send_imm,
                         std::conditional_t<t == TaskType::Read,         doca_rdma_task_read,
                         std::conditional_t<t == TaskType::Recv,         doca_rdma_task_receive,
                         std::conditional_t<t == TaskType::Write,        doca_rdma_task_write,
                         std::conditional_t<t == TaskType::WriteWithImm, doca_rdma_task_write_imm,
                                                                         void>>>>>>;
// clang-format on

#define task_constexpr_switch_return(task, what, ...)                        \
  if constexpr (type == TaskType::Send) {                                    \
    return doca_rdma_task_send_##what(task __VA_OPT__(, ) __VA_ARGS__);      \
  } else if constexpr (type == TaskType::SendWithImm) {                      \
    return doca_rdma_task_send_imm_##what(task __VA_OPT__(, ) __VA_ARGS__);  \
  } else if constexpr (type == TaskType::Read) {                             \
    return doca_rdma_task_read_##what(task __VA_OPT__(, ) __VA_ARGS__);      \
  } else if constexpr (type == TaskType::Recv) {                             \
    return doca_rdma_task_receive_##what(task __VA_OPT__(, ) __VA_ARGS__);   \
  } else if constexpr (type == TaskType::Write) {                            \
    return doca_rdma_task_write_##what(task __VA_OPT__(, ) __VA_ARGS__);     \
  } else if constexpr (type == TaskType::WriteWithImm) {                     \
    return doca_rdma_task_write_imm_##what(task __VA_OPT__(, ) __VA_ARGS__); \
  } else {                                                                   \
    static_unreachable;                                                      \
  }

template <TaskType type>
static doca_task *task_cast(doca_rdma_task_t<type> *task) {
  task_constexpr_switch_return(task, as_task);
}

template <TaskType type>
static const doca_buf *task_get_src_buf(doca_rdma_task_t<type> *task) {
  task_constexpr_switch_return(task, get_src_buf);
}

template <TaskType type>
static doca_buf *task_get_dst_buf(doca_rdma_task_t<type> *task) {
  task_constexpr_switch_return(task, get_dst_buf);
}

#undef task_constexpr_switch_return

}  // namespace rdma

namespace comch {

enum class TaskType {
  Send,
  Recv,
  Memcpy,
};

// clang-format off
template <TaskType t>
using doca_dma_task_t =  std::conditional_t<t == TaskType::Send,   doca_comch_producer_task_send,
                         std::conditional_t<t == TaskType::Recv,   doca_comch_consumer_task_post_recv,
                         std::conditional_t<t == TaskType::Memcpy, doca_dma_task_memcpy,
                                                                   void>>>;
// clang-format on

template <TaskType type>
static doca_task *task_cast(doca_dma_task_t<type> *task) {
  if constexpr (type == TaskType::Send) {
    return doca_comch_producer_task_send_as_task(task);
  } else if constexpr (type == TaskType::Recv) {
    return doca_comch_consumer_task_post_recv_as_task(task);
  } else if constexpr (type == TaskType::Memcpy) {
    return doca_dma_task_memcpy_as_task(task);
  } else {
    static_unreachable;
  };
}

template <TaskType type>
static const doca_buf *task_get_src_buf(doca_dma_task_t<type> *task) {
  if constexpr (type == TaskType::Send) {
    return doca_comch_producer_task_send_get_buf(task);
  } else if constexpr (type == TaskType::Recv) {
    return doca_comch_consumer_task_post_recv_get_buf(task);
  } else if constexpr (type == TaskType::Memcpy) {
    return doca_dma_task_memcpy_get_src(task);
  } else {
    static_unreachable;
  };
}

template <TaskType type>
static doca_buf *task_get_dst_buf(doca_dma_task_t<type> *task) {
  if constexpr (type == TaskType::Memcpy) {
    return doca_dma_task_memcpy_get_dst(task);
  } else {
    static_unreachable;
  };
}

#undef task_constexpr_switch_return

}  // namespace comch
}  // namespace doca
}  // namespace dpx

// clang-format off
EnumFormatter(doca_ctx_states,
    [DOCA_CTX_STATE_IDLE]     = "Idle",
    [DOCA_CTX_STATE_STARTING] = "Starting",
    [DOCA_CTX_STATE_RUNNING]  = "Running",
    [DOCA_CTX_STATE_STOPPING] = "Stopping",
);
EnumFormatter(dpx::doca::rdma::TaskType,
    [dpx::to_underlying(dpx::doca::rdma::TaskType::Send)]         = "Send",
    [dpx::to_underlying(dpx::doca::rdma::TaskType::SendWithImm)]  = "SendWithImm",
    [dpx::to_underlying(dpx::doca::rdma::TaskType::Read)]         = "Read",
    [dpx::to_underlying(dpx::doca::rdma::TaskType::Recv)]         = "Recv",
    [dpx::to_underlying(dpx::doca::rdma::TaskType::Write)]        = "Write",
    [dpx::to_underlying(dpx::doca::rdma::TaskType::WriteWithImm)] = "WriteWithImm",
);
EnumFormatter(dpx::doca::comch::TaskType,
    [dpx::to_underlying(dpx::doca::comch::TaskType::Send)]   = "Send",
    [dpx::to_underlying(dpx::doca::comch::TaskType::Recv)]   = "Recv",
    [dpx::to_underlying(dpx::doca::comch::TaskType::Memcpy)] = "Memcpy",
);
EnumFormatter(dpx::doca::TaskResult,
    [dpx::to_underlying(dpx::doca::TaskResult::Success)] = "Success",
    [dpx::to_underlying(dpx::doca::TaskResult::Failure)] = "Failure",
);
// clang-format on
