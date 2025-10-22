
#include "file_rpc.hxx"
#include "spdk_spill/spdk_env.hxx"
#include "spdk_spill/spdk_nvme.hxx"
#include "trans/transport.hxx"

using namespace std::chrono_literals;

int main() {
  std::string pci_addr = "0000:03:00.0";      // device pci addr
  std::string pci_rep_addr = "0000:43:00.0";  // representor pci addr
  dpx::spill::NVMeDeviceDesc desc = {
      .via_pcie = false,
      .ns_id = 1,
      .addr = "192.168.200.21",
      .svc_id = "4420",
      .subnqn = "nqn.2016-06.io.spdk:cnode1",
  };
  std::string huge_page_dir = "/home/lsc/dpx/.hugepage-2MB";
  size_t io_mem_reserved_in_mb = 1_KB;
  size_t max_io_depth = 1024;

  bool stop = false;

  dpx::spill::SpdkEnv env(huge_page_dir, io_mem_reserved_in_mb);

  size_t buf_len = 4_KB;
  dpx::spill::IOBuffer w_buf(buf_len, 8);
  memset(w_buf.data(), 'x', buf_len);

  auto dev = dpx::doca::Device::open_by_pci_addr(pci_addr);
  dev.open_representor(pci_rep_addr);
  auto ch = dpx::ConnectionHolder<dpx::Backend::DOCA_Comch>(dev, dpx::ConnectionParam<dpx::Backend::DOCA_Comch>{
                                                                     .passive = true,
                                                                     .name = file_server_name,
                                                                 });
  auto t = dpx::Transport<dpx::Backend::DOCA_Comch, CreateRpc, OpenRpc, CloseRpc, AllocateRpc, FilefragRpc>(
      dev, ch,
      dpx::Config{
          .queue_depth = 16,
          .max_rpc_msg_size = 16_KB,
      });
  ch.establish_connections();

  auto disk = dpx::spill::NVMeDevice(desc);
  auto io_queue = dpx::spill::NVMeDeviceIOQueue(disk, max_io_depth);
  auto io_poller = boost::fibers::fiber([&]() {
    while (!stop) {
      if (!io_queue.progress()) {
        boost::this_fiber::yield();
      }
    }
  });
  auto admin_poller = boost::fibers::fiber([&]() {
    if (!disk.need_keep_alive()) {
      return;
    }
    while (!stop) {
      disk.progress_admin_queue();
      boost::this_fiber::sleep_for(disk.keep_alive_in_ms() * 1ms);
    }
  });

  {
    dpx::TransportGuard g(t);
    std::string file_name = "hello";
    auto fd = t.call<CreateRpc>(CreateRequest{.file_name = file_name}).get();
    if (fd <= 0) {
      die("Fail to create file {}, errno: {}", file_name, -fd);
    }
    size_t size = 128_KB;
    auto res = t.call<AllocateRpc>(AllocateRequest{.fd = fd, .size = size}).get();
    if (res != OK) {
      die("Fail to allocate file {} with extra length {}, errno: {}", file_name, size, -res);
    }
    auto frags = t.call<FilefragRpc>(FilefragRequest{.fd = fd}).get();
    for (auto &frag : frags) {
      INFO("{} {} {} {} {}", frag.logic_offset, frag.physical_offset, frag.length, frag.start_lba, frag.lba_count);
    }
    dpx::spill::IOContext ctx(dpx::Op::Write, w_buf);
    ctx.start_lba = frags[0].start_lba;
    ctx.lba_count = w_buf.size() / disk.lba_size();
    auto n_write = io_queue.submit(ctx).get();
    INFO("write from lba {} with length {}, {} lba", ctx.start_lba, n_write, ctx.lba_count);
    auto rc = t.call<CloseRpc>(CloseRequest{.fd = fd}).get();
    if (rc != 0) {
      die("Fail to close file {}", file_name);
    }
    INFO("Close");
  }
  ch.terminate_connections();

  admin_poller.join();
  io_poller.join();

  return 0;
}