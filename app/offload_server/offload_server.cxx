#include "native/offload.hxx"
#include "native/shuffle_worker.hxx"
#include "native/spill_worker.hxx"
#include "util/literal.hxx"

namespace dpx {

inline static uint32_t n_r_worker = 2;
inline static uint32_t n_l_worker = 2;
inline static std::atomic_bool running = true;
inline static std::latch l(2 + n_l_worker + n_r_worker * 2);

void server_main(std::string dev_pci_addr, std::string local_ip, std::string remote_ip, std::string output_device,
                 DispatchFn fn) {
  doca::Device ch_dev("mlx5_1", doca::Device::FindByIBDevName);
  ch_dev.open_representor(dev_pci_addr);
  doca::Device rdma_dev("mlx5_3", doca::Device::FindByIBDevName);

  SpillBufferPool spill_bp(rdma_dev, 1024, 8_MB,
                           DOCA_ACCESS_FLAG_PCI_READ_WRITE | DOCA_ACCESS_FLAG_RDMA_READ | DOCA_ACCESS_FLAG_RDMA_WRITE);
  spill_bp.enable_mt();
  // SpillBufferPool remote_spill_bp(
  //     rdma_dev, 32, 32_MB, DOCA_ACCESS_FLAG_PCI_READ_WRITE | DOCA_ACCESS_FLAG_RDMA_READ |
  //     DOCA_ACCESS_FLAG_RDMA_WRITE);

  TaskQueue dsq(1024);
  std::vector<TaskQueue*> lsqs(n_l_worker, nullptr);
  std::vector<TaskQueue*> rsqs(n_r_worker, nullptr);
  std::vector<TaskQueue*> trsqs(n_r_worker, nullptr);

  std::vector<RemoteSpillWorker*> rsws(n_r_worker, nullptr);
  std::vector<RemoteSpillWorker*> rws(n_r_worker, nullptr);
  std::vector<LocalSpillWorker*> lsws(n_l_worker, nullptr);

  for (auto i = 0uz; i < n_r_worker; i++) {
    rsqs[i] = new TaskQueue(1024);
    trsqs[i] = new TaskQueue(1024);
    rsws[i] = new RemoteSpillWorker(rdma_dev, {true, true, "", local_ip, 0, static_cast<uint16_t>(10086 + i)},
                                    {.queue_depth = 32, .max_rpc_msg_size = 512}, *rsqs[i], running);
    rws[i] = new RemoteSpillWorker(
        rdma_dev,
        {false, true, remote_ip, local_ip, static_cast<uint16_t>(10086 + i), static_cast<uint16_t>(12306 + i)},
        {.queue_depth = 32, .max_rpc_msg_size = 512}, *trsqs[i], running);
  }
  for (auto i = 0uz; i < n_l_worker; i++) {
    lsqs[i] = new TaskQueue(1024);
    lsws[i] = new LocalSpillWorker(ch_dev, rdma_dev, {.passive = true, .name = "spill" + std::to_string(i)},
                                   {.queue_depth = 32, .max_rpc_msg_size = 512}, *lsqs[i], running);
  }

  DirectSpillWorker dsw(ch_dev, {true, "disk"}, {.queue_depth = 4, .max_rpc_msg_size = 512}, dsq,
                        "/home/lsc/dpx/.test_spill", output_device, 1024, spill_bp, running);

  PipelineShuffleWorkerPool pswp(lsqs, rsqs, trsqs, dsq, fn, running);

  uint32_t core_idx = 0;
  dsw.run(l, core_idx++);
  pswp.run_dispatch(1, core_idx++);
  for (auto i = 0uz; i < n_l_worker; i++) {
    lsws[i]->run_as_producer(spill_bp, l, core_idx++);
  }
  for (auto i = 0uz; i < n_r_worker; i++) {
    rsws[i]->run_as_producer(spill_bp, l, core_idx++);
    rws[i]->run_as_consumer(spill_bp, 16, l, core_idx++);
  }

  l.arrive_and_wait();
  INFO("All Worker Started");

  for (auto i = 0uz; i < n_l_worker; i++) {
    lsws[i]->join();
    delete lsws[i];
  }
  running = false;
  pswp.join();
  dsw.join();
  for (auto rsw : rsws) {
    rsw->join();
    delete rsw;
  }
  for (auto rw : rws) {
    rw->join();
    delete rw;
  }
  for (auto lsq : lsqs) {
    delete lsq;
  }
  for (auto rsq : rsqs) {
    delete rsq;
  }
  for (auto trsq : trsqs) {
    delete trsq;
  }
}

void dpu21_server_main() {
  server_main("0000:43:00.1", "192.168.203.21", "192.168.203.20", "/dev/nvme1n1p1",
              [](size_t pid) { return pid % 2 == 0; });
}
void dpu20_server_main() {
  server_main("0000:99:00.1", "192.168.203.20", "192.168.203.21", "/dev/nvme1n1p1",
              [](size_t pid) { return pid % 2 == 1; });
}

}  // namespace dpx

int main(int argc, char* argv[]) {
  spdlog::set_level(spdlog::level::trace);
  if (argc != 2) {
    die("Usage: %s [dpu20/dpu21]\n", argv[0]);
  }
  auto which = std::string(argv[1]);
  if (which == "dpu21") {
    dpx::dpu21_server_main();
  } else if (which == "dpu20") {
    dpx::dpu20_server_main();
  } else {
    die("Usage: %s [dpu20/dpu21]\n", argv[0]);
  }
  return 0;
}
