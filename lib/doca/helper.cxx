#include "doca/helper.hxx"

#include <doca_mmap.h>
#include <doca_pe.h>

#include <boost/fiber/operations.hpp>

#include "doca/check.hxx"
#include "memory/memory_region.hxx"
#include "util/logger.hxx"

namespace dpx::doca {

doca_ctx_states get_ctx_state(doca_ctx *ctx) {
  doca_ctx_states state;
  doca_check(doca_ctx_get_state(ctx, &state));
  return state;
}

void submit_task(doca_task *task) {
  while (true) {
    auto status = doca_task_submit(task);
    if (status == DOCA_ERROR_AGAIN) {
      TRACE("Submit task later");
      boost::this_fiber::yield();
      continue;
    } else if (status == DOCA_SUCCESS) {
      TRACE("Succeed to submit task");
      break;
    } else {
      die("Fail to submit doca task, because: {}", doca_error_get_descr(status));
    }
  }
}

doca_dev *open_dev_by_pci_addr(std::string_view pci_addr) {
  doca_devinfo **dev_list;
  uint32_t n_devs = 0;
  doca_check(doca_devinfo_create_list(&dev_list, &n_devs));
  for (auto devinfo : std::span<doca_devinfo *>(dev_list, n_devs)) {
    uint8_t is_equal = 0;
    doca_check(doca_devinfo_is_equal_pci_addr(devinfo, pci_addr.data(), &is_equal));
    if (is_equal) {
      doca_dev *dev = nullptr;
      doca_check(doca_dev_open(devinfo, &dev));
      doca_check(doca_devinfo_destroy_list(dev_list));
      return dev;
    }
  }
  die("Device {} not found", pci_addr);
}

doca_dev *open_dev_by_ib_device_name(std::string_view ib_device_name) {
  doca_devinfo **dev_list;
  uint32_t n_devs = 0;
  doca_check(doca_devinfo_create_list(&dev_list, &n_devs));
  char ibdev_name[DOCA_DEVINFO_IBDEV_NAME_SIZE];
  for (auto devinfo : std::span<doca_devinfo *>(dev_list, n_devs)) {
    doca_check(doca_devinfo_get_ibdev_name(devinfo, ibdev_name, sizeof(ibdev_name)));
    if (ib_device_name == ibdev_name) {
      doca_dev *dev = nullptr;
      doca_check(doca_dev_open(devinfo, &dev));
      doca_check(doca_devinfo_destroy_list(dev_list));
      return dev;
    }
  }
  die("Device {} not found", ib_device_name);
}

doca_dev_rep *open_dev_rep(doca_dev *dev, std::string_view pci_addr, doca_devinfo_rep_filter filter) {
  uint32_t n_dev_reps = 0;
  doca_devinfo_rep **dev_rep_list = nullptr;
  doca_check(doca_devinfo_rep_create_list(dev, filter, &dev_rep_list, &n_dev_reps));
  for (auto &devinfo_rep : std::span<doca_devinfo_rep *>(dev_rep_list, n_dev_reps)) {
    uint8_t is_equal = 0;
    doca_check(doca_devinfo_rep_is_equal_pci_addr(devinfo_rep, pci_addr.data(), &is_equal));
    if (is_equal) {
      doca_dev_rep *dev_rep = nullptr;
      doca_check(doca_dev_rep_open(devinfo_rep, &dev_rep));
      doca_check(doca_devinfo_rep_destroy_list(dev_rep_list));
      return dev_rep;
    }
  }
  die("Device representor {} not found", pci_addr);
}

MemoryRegion from_mmap(doca_mmap *mmap) {
  uint8_t *addr = nullptr;
  size_t len = 0;
  doca_check(doca_mmap_get_memrange(mmap, reinterpret_cast<void **>(&addr), &len));
  return MemoryRegion(addr, len);
}

}  // namespace dpx::doca
