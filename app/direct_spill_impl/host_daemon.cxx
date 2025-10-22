// #include <sys/mount.h>
#include "../mount.hxx"
#include "rpc.hxx"
#include "trans/transport.hxx"
#include "util/literal.hxx"

int main() {
  std::string pci_addr = "0000:43:00.0";                  // device pci addr
  std::string output_device = "/dev/nvme2n1p1";           // absolute path
  std::string mount_point = "/home/lsc/dpx/.test_spill";  // absolute path
  // std::string fs_type = "ext4";
  auto dev = dpx::doca::Device::open_by_pci_addr(pci_addr);
  auto ch = dpx::ConnectionHolder<dpx::Backend::DOCA_Comch>(dev, dpx::ConnectionParam<dpx::Backend::DOCA_Comch>{
                                                                     .passive = false,
                                                                     .name = direct_spill_server_name,
                                                                 });
  auto t = dpx::Transport<dpx::Backend::DOCA_Comch, MountRpc, UmountRpc>(dev, ch,
                                                                         dpx::Config{
                                                                             .queue_depth = 16,
                                                                             .max_rpc_msg_size = 16_KB,
                                                                         });
  t.register_handler<MountRpc>([&](const MountRequest&) -> int {
    // int rc = mount(output_device.c_str(), mount_point.c_str(), fs_type.c_str(), 0, nullptr);
    int rc = mount(output_device, mount_point);
    INFO("trigger mount, rc: {}, errno: {}", rc, errno);

    // std::string output_file_prefix = "out";
    // auto n_file = 1024uz;
    // auto file_len = 128_MB;
    // auto buffer_len = 16_KB;
    // auto buffer = new (std::align_val_t(4_KB)) char[buffer_len];
    // auto n_buffer = file_len / buffer_len;
    // for (auto i = 0uz; i < n_file; i++) {
    //   auto file_name = std::format("{}/{}-{}", mount_point, output_file_prefix, i);
    //   auto fd = open(file_name.c_str(), O_RDONLY);
    //   if (fd == -1) {
    //     die("Fail to open {}, errno {}", file_name, errno);
    //   }
    //   INFO("Open {}", file_name);
    //   auto offset = 0uz;
    //   for (auto j = 0uz; j < n_buffer; j++) {
    //     memset(buffer, 0xCC, buffer_len);
    //     auto n_read = pread(fd, buffer, buffer_len, offset);
    //     if (n_read < 0 || (size_t)n_read != buffer_len) {
    //       die("Fail to pread {}, from {} to {}, errno: {}", file_name, offset, offset + buffer_len, errno);
    //     }
    //     char c = ('a' + (i + j) % 26);
    //     for (auto k = 0uz; k < buffer_len; k++) {
    //       if (buffer[k] != c) {
    //         die("?");
    //       }
    //     }
    //     offset += buffer_len;
    //   }
    //   close(fd);
    // }

    return rc;
  });

  t.register_handler<UmountRpc>([&](const UmountRequest&) -> int {
    // int rc = umount(mount_point.c_str());
    int rc = umount(mount_point);
    INFO("trigger umount, rc: {}, errno: {}", rc, errno);
    return rc;
  });

  ch.establish_connections();
  {
    dpx::TransportGuard g(t);
    t.serve();
  }
  ch.terminate_connections();

  return 0;
}