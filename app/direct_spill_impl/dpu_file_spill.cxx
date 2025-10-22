#include "../mount.hxx"
#include "rpc.hxx"
#include "trans/transport.hxx"
#include "util/literal.hxx"

using namespace std::chrono_literals;

int main() {
  std::string pci_addr = "0000:03:00.0";                  // device pci addr
  std::string pci_rep_addr = "0000:43:00.0";              // representor pci addr
  std::string mount_point = "/home/lsc/dpx/.test_spill";  // absolute path
  std::string output_device = "/dev/nvme1n1p1";           // absolute path

  auto dev = dpx::doca::Device::open_by_pci_addr(pci_addr);
  dev.open_representor(pci_rep_addr);
  auto ch = dpx::ConnectionHolder<dpx::Backend::DOCA_Comch>(dev, dpx::ConnectionParam<dpx::Backend::DOCA_Comch>{
                                                                     .passive = true,
                                                                     .name = direct_spill_server_name,
                                                                 });

  auto t = dpx::Transport<dpx::Backend::DOCA_Comch, MountRpc, UmountRpc>(dev, ch,
                                                                         dpx::Config{
                                                                             .queue_depth = 16,
                                                                             .max_rpc_msg_size = 16_KB,
                                                                         });
  ch.establish_connections();
  {
    dpx::TransportGuard g(t);
    // {
    //   auto rc = mount(output_device, mount_point);
    //   if (rc != 0) {
    //     die("Fail to mount {} at {}", output_device, mount_point);
    //   }
    // }

    // std::string output_file_prefix = "out";
    // auto n_file = 1024uz;
    // auto file_len = 128_MB;
    // auto buffer_len = 16_KB;
    // auto buffer = new (std::align_val_t(4_KB)) char[buffer_len];
    // auto n_buffer = file_len / buffer_len;
    // for (auto i = 0uz; i < n_file; i++) {
    //   auto file_name = std::format("{}/{}-{}", mount_point, output_file_prefix, i);
    //   auto fd = open(file_name.c_str(), O_CREAT | O_TRUNC | O_RDWR, 0644);
    //   if (fd == -1) {
    //     die("Fail to open {}, errno {}", file_name, errno);
    //   }
    //   INFO("Open {}", file_name);
    //   auto rc = fallocate64(fd, 0, 0, file_len);
    //   if (rc != 0) {
    //     die("Fail to fallocate {}, errno {}", file_name, errno);
    //   }
    //   auto offset = 0uz;
    //   for (auto j = 0uz; j < n_buffer; j++) {
    //     memset(buffer, ('a' + (i + j) % 26), buffer_len);
    //     auto n_write = pwrite(fd, buffer, buffer_len, offset);
    //     if (n_write < 0 || (size_t)n_write != buffer_len) {
    //       die("Fail to pwrite {}, from {} to {}, errno: {}", file_name, offset, offset + buffer_len, errno);
    //     }
    //     offset += buffer_len;
    //   }
    //   rc = fsync(fd);
    //   if (rc != 0) {
    //     die("Fail to sync {}, errno", file_name, errno);
    //   }
    //   close(fd);
    // }

    // {
    //   auto rc = umount(mount_point);
    //   if (rc != 0) {
    //     die("Fail to umount {}", mount_point);
    //   }
    // }

    {
      auto rc = t.call<MountRpc>({.dummy = 0}).get();
      INFO("mount rc: {}", rc);
    }
  }
  ch.terminate_connections();

  return 0;
}