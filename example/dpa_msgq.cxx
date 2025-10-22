#include <spdlog/spdlog.h>

#include "doca/device.hxx"
#include "doca/dpa_thread.hxx"
#include "util/hex_dump.hxx"
#include "util/timer.hxx"

extern "C" doca_dpa_app* dpa_msgq;
extern "C" doca_dpa_func_t dpa_msgq_func;
extern "C" doca_dpa_func_t dpa_se_func;
extern "C" doca_dpa_func_t dpa_rdma_func;
extern "C" doca_dpa_func_t trigger;
extern "C" doca_dpa_func_t notify;

using namespace std::chrono_literals;

int main(int argc, char* argv[]) {
  spdlog::set_level(spdlog::level::trace);
  auto dev = dpx::doca::Device::open_by_ibdev_name("mlx5_0");
  // bool passive = argc == 1;
  dev.open_dpa(dpa_msgq, "dpa_msgq");
  // int conn_sock = 0;
  // if (passive) {
  // conn_sock = dpx::setup_and_bind_and_listen_and_accept("192.168.200.20", 10086);
  // } else {
  // }
  dpx::doca::DPAThread t(dev, dpa_rdma_func);
  int conn_sock = dpx::setup_and_bind_and_connect("192.168.200.20", 10087, "192.168.200.20", 10086);
  t.connect(false, conn_sock);
  dpx::doca::launch_rpc(dev, trigger, t.args.cons_h);
  t.post_recv();
  t.trigger();
  uint32_t n = 0;
  while (true) {
    if (t.progress()) {
      n++;
    }
    if (n == 2) {
      break;
    }
  }
  t.disconnect(conn_sock);
  // char raw[4096];
  // t.rdma_trans.copy_from_device(raw, 4096, 0);
  // INFO("{}", dpx::Hexdump(raw, 4096));

  // dpx::doca::DPAThread t(dev, dpa_se_func);
  // dpx::doca::launch_rpc(dev, notify, t.args.notify_comp_h);
  // dpx::Timer timer;
  // doca_check(doca_sync_event_update_set(t.wake_se, 1));
  // doca_check(doca_sync_event_wait_eq(t.exit_se, 1, -1));
  // INFO("{}us", timer.elapsed_us());

  // dev.open_dpa(dpa_msgq, "dpa.log");
  // dpx::doca::DPAThread t(dev, dpa_msgq_func);
  // dpx::doca::launch_rpc(dev, trigger, t.args.cons_h);
  // dpx::Timer timer;
  // for (uint32_t i = 0; i < 32; ++i) {
  //   t.post_recv();
  //   t.trigger();
  //   uint32_t n = 0;
  //   while (true) {
  //     if (t.progress()) {
  //       n++;
  //     } else {
  //       // std::this_thread::sleep_for(10us);
  //     }
  //     if (n == 2) {
  //       break;
  //     }
  //   }
  // }
  // INFO("{}us", timer.elapsed_us());
  return 0;
}
