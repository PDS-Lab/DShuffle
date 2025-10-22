#include <doca_dev.h>

#include <format>
#include <iostream>
#include <source_location>

namespace std {
inline string to_string(const source_location &l) {
  return format("{}:{} `{}`: ", l.file_name(), l.line(), l.function_name());
}
}  // namespace std

[[noreturn]] inline void die(std::string why) { throw std::runtime_error(why); }
inline void footprint(std::string what) { std::cerr << what << std::endl; }

#define die(fmt, ...) \
  die(std::to_string(std::source_location::current()) + std::vformat(fmt, std::make_format_args(__VA_ARGS__)))

#define footprint(fmt, ...) \
  footprint(std::to_string(std::source_location::current()) + std::vformat(fmt, std::make_format_args(__VA_ARGS__)))

#define doca_check_ext(expr, ...)                                                                              \
  do {                                                                                                         \
    constexpr doca_error_t __expected_result__[] = {DOCA_SUCCESS __VA_OPT__(, ) __VA_ARGS__};                  \
    constexpr size_t __n_expected__ = sizeof(__expected_result__) / sizeof(doca_error_t);                      \
    doca_error_t __doca_check_result__ = expr;                                                                 \
    if (std::ranges::none_of(std::span<const doca_error_t>(__expected_result__, __n_expected__),               \
                             [&__doca_check_result__](doca_error_t e) { return e == __doca_check_result__; })) \
        [[unlikely]] {                                                                                         \
      die(#expr ": {}", doca_error_get_descr(__doca_check_result__));                                          \
    } else {                                                                                                   \
      footprint(#expr ": {}", doca_error_get_descr(__doca_check_result__));                                    \
    }                                                                                                          \
  } while (0);

#define doca_check(expr)                                                    \
  do {                                                                      \
    doca_error_t __doca_check_result__ = expr;                              \
    if (__doca_check_result__ != DOCA_SUCCESS) [[unlikely]] {               \
      die(#expr ": {}", doca_error_get_descr(__doca_check_result__));       \
    } else {                                                                \
      footprint(#expr ": {}", doca_error_get_descr(__doca_check_result__)); \
    }                                                                       \
  } while (0);

int main() try {
  doca_devinfo **dev_list = nullptr;
  uint32_t n_devs = 0;
  char dev_pci_addr[DOCA_DEVINFO_PCI_ADDR_SIZE] = {};
  char dev_name[DOCA_DEVINFO_IBDEV_NAME_SIZE] = {};
  doca_check(doca_devinfo_create_list(&dev_list, &n_devs));
  for (auto devinfo : std::span<doca_devinfo *>(dev_list, n_devs)) {
    doca_check(doca_devinfo_get_ibdev_name(devinfo, dev_name, DOCA_DEVINFO_IBDEV_NAME_SIZE));
    doca_check(doca_devinfo_get_pci_addr_str(devinfo, dev_pci_addr));
    std::cout << "Found device " << dev_name << " at " << dev_pci_addr << std::endl;
    uint8_t support_representor = false;
    if (doca_devinfo_rep_cap_is_filter_all_supported(devinfo, &support_representor) == DOCA_SUCCESS) {
      doca_dev *dev = nullptr;
      doca_check(doca_dev_open(devinfo, &dev));
      try {
        char dev_rep_pci_addr[DOCA_DEVINFO_REP_PCI_ADDR_SIZE] = {};
        uint32_t n_dev_reps = 0;
        doca_devinfo_rep **dev_rep_list = nullptr;
        doca_pci_func_type dev_rep_pci_func_type;
        doca_check(doca_devinfo_rep_create_list(dev, DOCA_DEVINFO_REP_FILTER_ALL, &dev_rep_list, &n_dev_reps));
        for (auto &devinfo_rep : std::span<doca_devinfo_rep *>(dev_rep_list, n_dev_reps)) {
          doca_check(doca_devinfo_rep_get_pci_addr_str(devinfo_rep, dev_rep_pci_addr));
          doca_check(doca_devinfo_rep_get_pci_func_type(devinfo_rep, &dev_rep_pci_func_type));
          std::cout << "Found representor at " << dev_rep_pci_addr << " with type "
                    << (dev_rep_pci_func_type == DOCA_PCI_FUNC_TYPE_PF   ? "PF"
                        : dev_rep_pci_func_type == DOCA_PCI_FUNC_TYPE_VF ? "VF"
                                                                         : "SF")
                    << std::endl;
        }
        doca_check(doca_devinfo_rep_destroy_list(dev_rep_list));
      } catch (const std::runtime_error &e) {
        std::cerr << "Skip, " << e.what() << std::endl;
      }
      doca_check(doca_dev_close(dev));
    } else {
      std::cout << "Run on DPU to show device representors" << std::endl;
    }
  }
  doca_check(doca_devinfo_destroy_list(dev_list));
  return 0;
} catch (const std::runtime_error &e) {
  std::cerr << "Exit, " << e.what() << std::endl;
  return -1;
}