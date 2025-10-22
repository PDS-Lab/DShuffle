#include "doca/device.hxx"

#include "doca/check.hxx"
#include "util/logger.hxx"

namespace dpx::doca {

void Device::open_representor(std::string_view dev_rep_pci_addr, doca_devinfo_rep_filter filter) {
  if (rep != nullptr) {
    WARN("Representor is already opened");
    return;
  }
  rep = open_dev_rep(dev, dev_rep_pci_addr, filter);
}

void Device::open_dpa(doca_dpa_app* app, std::string log_file_name) {
  if (dpa != nullptr) {
    WARN("DPA is already opened");
    return;
  }
  doca_check(doca_dpa_create(dev, &dpa));
  doca_check(doca_dpa_set_app(dpa, app));
#ifdef NDEBUG
  doca_check(doca_dpa_set_log_level(dpa, DOCA_DPA_DEV_LOG_LEVEL_INFO));
#else
  doca_check(doca_dpa_set_log_level(dpa, DOCA_DPA_DEV_LOG_LEVEL_DEBUG));
#endif
  std::string dir_path = "./.log";
  if (access(dir_path.data(), F_OK) != 0) {
    if (int ec = mkdir(dir_path.data(), 0777); ec != 0) {
      die("Fail to create dpa log file, errno: {}", errno);
    }
  }
  std::string log_file_path = std::format("{}/{}.log", dir_path, log_file_name);
  std::string trace_file_path = std::format("{}/{}.trace", dir_path, log_file_name);
  doca_check(doca_dpa_log_file_set_path(dpa, log_file_path.data()));
  doca_check(doca_dpa_trace_file_set_path(dpa, trace_file_path.data()));
  doca_check(doca_dpa_start(dpa));
}

Device::~Device() {
  if (rep != nullptr) {
    doca_check(doca_dev_rep_close(rep));
  }
  if (dpa != nullptr) {
    doca_check(doca_dpa_destroy(dpa));
  }
  if (dev != nullptr) {
    doca_check(doca_dev_close(dev));
  }
}

}  // namespace dpx::doca
