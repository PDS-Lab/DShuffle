#pragma once

#include <spdk/nvme.h>

#include <format>
#include <string>

#include "common/context_base.hxx"
#include "spdk_spill/io_buffer.hxx"
#include "util/noncopyable.hxx"
#include "util/nonmovable.hxx"

namespace dpx::spill {

struct IOContext : ContextBase {
  IOBuffer &buffer;

  size_t start_lba;
  size_t lba_count;

  IOContext(Op op_, IOBuffer &buffer_) : ContextBase(op_), buffer(buffer_) {}
};

struct NVMeDeviceDesc {
  bool via_pcie;
  uint32_t ns_id;
  std::string addr;
  std::string svc_id;
  std::string subnqn;

  std::string as_format() const {
    if (via_pcie) {
      return std::format("trtype:PCIe traddr:{}", addr);
    } else {
      return std::format("trtype:RDMA adrfam:IPv4 traddr:{} trsvcid:{} subnqn:{}", addr, svc_id,
                         (subnqn.empty() ? SPDK_NVMF_DISCOVERY_NQN : subnqn));
    }
  }
};

class NVMeDevice : Noncopyable, Nonmovable {
  friend class NVMeDeviceIOQueue;

 public:
  NVMeDevice(const NVMeDeviceDesc &desc);
  ~NVMeDevice();

  bool progress_admin_queue();

  bool need_keep_alive() const { return keep_alive_timeout_ms > 0; }

  size_t lba_size() const { return sector_size; }
  size_t keep_alive_in_ms() const { return keep_alive_timeout_ms; }

 private:
  spdk_nvme_transport_id *tr_id = nullptr;
  spdk_nvme_ctrlr *ctrlr = nullptr;
  spdk_nvme_ns *ns = nullptr;

  size_t n_sector = 0;
  size_t sector_size = 0;            // in bytes
  size_t max_xfer_size = 0;          // in bytes
  size_t keep_alive_timeout_ms = 0;  // 0 means disabled
};

class NVMeDeviceIOQueue : Noncopyable, Nonmovable {
 public:
  NVMeDeviceIOQueue(NVMeDevice &dev_, size_t max_io_depth_);
  ~NVMeDeviceIOQueue();

  bool progress();

  op_res_future_t submit(IOContext &ctx);

 private:
  NVMeDevice &dev;
  spdk_nvme_qpair *qpair = nullptr;
  size_t max_io_depth = 0;
};

}  // namespace dpx::spill
