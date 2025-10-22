#pragma once

#include <doca_dev.h>
#include <doca_dpa.h>

#include <glaze/core/common.hpp>
#include <glaze/core/meta.hpp>
#include <string_view>

#include "doca/check.hxx"
#include "doca/helper.hxx"
#include "util/noncopyable.hxx"
#include "util/nonmovable.hxx"
#include "util/unreachable.hxx"

namespace dpx::doca {

struct ComchCapability;
struct DMACapability;
struct RDMACapability;

namespace comch {
class Endpoint;
class ConnectionHandle;
}  // namespace comch

namespace rdma {
class Endpoint;
class ConnectionHandle;
}  // namespace rdma

class Device;

template <typename... Args>
void launch_fn(Device &dev, doca_sync_event *wait_event, uint64_t n_wait, doca_sync_event *comp_event, uint64_t n_comp,
               uint32_t n_threads, doca_dpa_func_t *fn, Args &&...args);

template <typename... Args>
uint64_t launch_rpc(Device &dev, doca_dpa_func_t *fn, Args &&...args);

class Device : Noncopyable, Nonmovable {
  friend class comch::Endpoint;
  friend class comch::ConnectionHandle;
  friend class rdma::Endpoint;
  friend class rdma::ConnectionHandle;
  friend class MappedRegion;
  friend class OwnedBuffer;
  friend class Buffers;
  friend class DPABuffer;
  friend class DPAThread;
  friend class DPAThreadGroup;

  friend ComchCapability probe_comch_caps(Device &);
  friend RDMACapability probe_rdma_caps(Device &);
  friend DMACapability probe_dma_caps(Device &, doca_dma *);

  template <typename... Args>
  friend void launch_fn(Device &dev, doca_sync_event *wait_event, uint64_t n_wait, doca_sync_event *comp_event,
                        uint64_t n_comp, uint32_t n_threads, doca_dpa_func_t *fn, Args &&...args);
  template <typename... Args>
  friend uint64_t launch_rpc(Device &dev, doca_dpa_func_t *fn, Args &&...args);

  enum class DiscoveryMethod {
    ByPCIAddress,
    ByIBDevName,
  };

  template <DiscoveryMethod m>
  struct DiscoveryTag {};

 public:
  inline static constexpr const DiscoveryTag<DiscoveryMethod::ByIBDevName> FindByIBDevName = {};
  inline static constexpr const DiscoveryTag<DiscoveryMethod::ByPCIAddress> FindByPCIAddress = {};

  template <DiscoveryMethod m>
  Device(std::string_view identity, DiscoveryTag<m>) {
    if constexpr (m == Device::DiscoveryMethod::ByIBDevName) {
      dev = open_dev_by_ib_device_name(identity);
    } else if constexpr (m == Device::DiscoveryMethod::ByPCIAddress) {
      dev = open_dev_by_pci_addr(identity);
    } else {
      static_unreachable;
    }
  }

  inline static Device open_by_pci_addr(std::string_view pci_addr) { return Device(pci_addr, FindByPCIAddress); }

  inline static Device open_by_ibdev_name(std::string_view ibdev_name) { return Device(ibdev_name, FindByIBDevName); }

  inline static Device *new_device_by_pci_addr(std::string_view pci_addr) {
    return new Device(pci_addr, FindByPCIAddress);
  }

  inline static Device *new_device_by_ibdev_name(std::string_view ibdev_name) {
    return new Device(ibdev_name, FindByIBDevName);
  }

  void open_representor(std::string_view dev_rep_pci_addr,
                        doca_devinfo_rep_filter filter = DOCA_DEVINFO_REP_FILTER_NET);
  void open_dpa(doca_dpa_app *app, std::string log_file);

  ~Device();

 private:
  doca_dev *dev = nullptr;
  doca_dev_rep *rep = nullptr;
  doca_dpa *dpa = nullptr;
};

template <typename... Args>
void launch_fn(Device &dev, doca_sync_event *wait_event, uint64_t n_wait, doca_sync_event *comp_event, uint64_t n_comp,
               uint32_t n_threads, doca_dpa_func_t *fn, Args &&...args) {
  doca_check(doca_dpa_kernel_launch_update_set(dev.dpa, wait_event, n_wait, comp_event, n_comp, n_threads, fn,
                                               std::forward<Args>(args)...));
}

template <typename... Args>
void launch_fn(Device &dev, uint32_t n_threads, doca_dpa_func_t *fn, Args &&...args) {
  launch_fn(dev, nullptr, 0, nullptr, 0, n_threads, fn, std::forward<Args>(args)...);
}

template <typename... Args>
void launch_fn_one(Device &dev, doca_dpa_func_t *fn, Args &&...args) {
  launch_fn(dev, 1, fn, std::forward<Args>(args)...);
}

template <typename... Args>
uint64_t launch_rpc(Device &dev, doca_dpa_func_t *fn, Args &&...args) {
  uint64_t r = 0;
  doca_check(doca_dpa_rpc(dev.dpa, fn, &r, args...));
  return r;
}

using DeviceRefs = std::vector<std::reference_wrapper<Device>>;

}  // namespace dpx::doca
