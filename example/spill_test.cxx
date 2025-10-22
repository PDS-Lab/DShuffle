#include <spdlog/spdlog.h>

#include "spdk_spill/spdk_env.hxx"
#include "spdk_spill/spdk_nvme.hxx"
// #include "util/hex_dump.hxx"
#include "util/literal.hxx"

using namespace dpx::literal;

int main() {
  spdlog::set_level(spdlog::level::trace);

  dpx::spill::SpdkEnv env("./hugepage-2MB", 1_KB);

  // dpx::spill::NVMeDeviceDesc desc = {
  //     .via_pcie = true,
  //     .ns_id = 1,
  //     .addr = "0000:71:00.0",
  //     .svc_id = "",
  //     .subnqn = "",
  // };
  dpx::spill::NVMeDeviceDesc desc = {
      .via_pcie = false,
      .ns_id = 1,
      .addr = "192.168.200.21",
      .svc_id = "4420",
      .subnqn = "nqn.2016-06.io.spdk:cnode1",
  };

  dpx::spill::NVMeDevice dev(desc);
  dpx::spill::NVMeDeviceIOQueue q1(dev, 1024);
  // dpx::spill::NVMeDeviceIOQueue q2(dev, 1024);
  bool stop = false;
  auto poller = boost::fibers::fiber([&]() {
    INFO("poller start");
    while (!stop) {
      if (!q1.progress()) {
        boost::this_fiber::yield();
      }
    }
    INFO("poller stop");
  });

  size_t len = dev.lba_size() * 8;
  INFO("len {}", len);
  dpx::spill::IOBuffer w_buf(len, 4_KB);
  memset(w_buf.data(), 'C', len);
  dpx::spill::IOBuffer r_buf(len, 4_KB);
  memset(r_buf.data(), '0', len);
  dpx::spill::IOContext w_ctx(dpx::Op::Write, w_buf);
  dpx::spill::IOContext r_ctx(dpx::Op::Read, r_buf);

  r_ctx.start_lba = w_ctx.start_lba = 1000;
  r_ctx.lba_count = w_ctx.lba_count = 8;

  auto n_write = q1.submit(w_ctx).get();
  auto n_read = q1.submit(r_ctx).get();
  INFO("n_write: {} n_read: {}", n_write, n_read);

  auto not_equal = strncmp((const char*)w_buf.data(), (const char*)r_buf.data(), len);
  if (not_equal) {
    ERROR("Not equal!");
  }

  // INFO("{}", dpx::Hexdump(w_buf));
  // INFO("{}", dpx::Hexdump(r_buf));

  stop = true;

  poller.join();

  return 0;
}