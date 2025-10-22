#include <jni.h>

#include <barrier>

#include "native/shuffle_worker.hxx"
#include "native/spill_worker.hxx"
#include "util/literal.hxx"
#include "util/timer.hxx"

uint32_t n_rmdas = 4;
uint32_t n_shuffle_worker = 10;

size_t java_string_hash(std::span<uint8_t> data) {
  size_t h = 0;
  for (auto v : data) {
    h = h * 31 + v;
  }
  return h;
}

std::vector<std::vector<uint8_t>> native_partition(std::span<uint8_t> data) {
  std::vector<std::vector<uint8_t>> results;
  results.resize(32);
  auto limit = data.size();
  auto offset = 0uz;
  auto get_object = [&]() -> std::span<uint8_t> {
    auto p = &data[offset];
    auto length = *reinterpret_cast<uint64_t*>(p);
    auto cur = data.subspan(offset, length);
    offset += length;
    return cur;
  };
  for (auto& result : results) {
    result.resize(sizeof(dpx::PartitionDataHeader), 0);
  }
  while (offset < limit) {
    auto key = get_object();
    auto value = get_object();
    // auto h = std::hash<std::string_view>{}(std::string_view(reinterpret_cast<char*>(key.data()), key.size_bytes()));
    auto h = java_string_hash(key);
    auto pid = h % 32;
    results[pid].insert(results[pid].end(), key.begin(), key.end());
    results[pid].insert(results[pid].end(), value.begin(), value.end());
  }
  for (uint32_t i = 0; i < 32; i++) {
    dpx::PartitionDataHeader* header = reinterpret_cast<dpx::PartitionDataHeader*>(results[i].data());
    header->partition_id = i;
    header->length = results[i].size();
  }
  return results;
}

namespace dpx {

std::atomic_bool running = true;

void dpu21_server_main() {
  bind_core(0);

  OffloadSpillTaskQueue lntq(64);
  OffloadSpillTaskQueue rntq(64);

  doca::Device ch_dev("mlx5_1", doca::Device::FindByIBDevName);
  ch_dev.open_representor("0000:43:00.1");
  doca::Device rdma_dev("mlx5_3", doca::Device::FindByIBDevName);

  std::vector<NaiveSpillWorker<Backend::DOCA_RDMA>*> rnsws(n_rmdas, nullptr);
  for (uint16_t i = 0; i < n_rmdas; i++) {
    rnsws[i] =
        new NaiveSpillWorker<Backend::DOCA_RDMA>(rdma_dev,
                                                 {
                                                     .passive = true,
                                                     .enable_grh = true,
                                                     .local_ip = "192.168.203.21",
                                                     .local_port = static_cast<uint16_t>(i + 10086),
                                                 },
                                                 {.queue_depth = 16, .max_rpc_msg_size = 512}, rntq, running, i + 11);
  }

  std::vector<TransportWrapper<Backend::DOCA_RDMA>*> rdmas(n_rmdas, nullptr);

  for (uint16_t i = 0; i < n_rmdas; i++) {
    rdmas[i] = new TransportWrapper<Backend::DOCA_RDMA>(rdma_dev,
                                                        {
                                                            .passive = false,
                                                            .enable_grh = true,
                                                            .remote_ip = "192.168.203.20",
                                                            .local_ip = "192.168.203.21",
                                                            .remote_port = static_cast<uint16_t>(i + 10086),
                                                            .local_port = static_cast<uint16_t>(i + 12306),
                                                        },
                                                        {.queue_depth = 16, .max_rpc_msg_size = 512});
  }
  for (uint32_t i = 0; i < n_rmdas; i++) {
    rdmas[i]->ch.establish_connections();
  }

  NaiveSpillWorker<Backend::DOCA_Comch> lnsw(ch_dev, rdma_dev, {.passive = true, .name = "spill"},
                                             {.queue_depth = 16, .max_rpc_msg_size = 512}, lntq, running, 1);

  TransportWrapper<Backend::DOCA_Comch> io(ch_dev, rdma_dev, {.passive = true, .name = "disk"},
                                           {.queue_depth = 16, .max_rpc_msg_size = 512});

  io.ch.establish_connections();

  NaiveShuffleWorkerPool nswp(n_shuffle_worker, io, rdmas, [](size_t id) { return id % 2 == 0; }, native_partition);

  while (true) {
    while (lntq.empty() && rntq.empty() && running) {
      std::this_thread::sleep_for(1ms);
    }
    if (!running) {
      break;
    }

    bool from_remote = false;
    OffloadSpillTask* t = nullptr;
    if (!lntq.empty()) {
      t = *lntq.front();
      lntq.pop();
      from_remote = false;
    } else if (!rntq.empty()) {
      t = *rntq.front();
      rntq.pop();
      from_remote = true;
    } else {
      unreachable();
    }
    if (from_remote) {
      DEBUG("got remote spill with length {} at {}", t->buffer->size(), (void*)t->buffer->data());
      auto pid = *(uint32_t*)t->buffer->data();
      // DEBUG("{} {}", l->length, l->partition_id);
      DEBUG("partition {}", pid);
    } else {
      DEBUG("got local spill with length {} at {}", t->buffer->size(), (void*)t->buffer->data());
      // auto l = (NaiveSpillDataLayout*)t->data();
      // DEBUG("{} {} {} {} {}", l->n_record, l->size, l->data_offset, l->hashcodes_offset, l->offsets_offset);
    }

    nswp.submit_task(t, from_remote);
  }

  for (uint32_t i = 0; i < n_rmdas; i++) {
    rdmas[i]->ch.terminate_connections();
    delete rdmas[i];
  }
  io.ch.terminate_connections();
  lnsw.join();
  for (uint32_t i = 0; i < n_rmdas; i++) {
    rnsws[i]->join();
    delete rnsws[i];
  }
}

void dpu20_server_main() {
  bind_core(0);

  OffloadSpillTaskQueue lntq(64);
  OffloadSpillTaskQueue rntq(64);

  doca::Device ch_dev("mlx5_1", doca::Device::FindByIBDevName);
  ch_dev.open_representor("0000:99:00.1");
  doca::Device rdma_dev("mlx5_3", doca::Device::FindByIBDevName);

  std::vector<NaiveSpillWorker<Backend::DOCA_RDMA>*> rnsws(n_rmdas, nullptr);
  for (uint16_t i = 0; i < n_rmdas; i++) {
    rnsws[i] =
        new NaiveSpillWorker<Backend::DOCA_RDMA>(rdma_dev,
                                                 {
                                                     .passive = true,
                                                     .enable_grh = true,
                                                     .local_ip = "192.168.203.20",
                                                     .local_port = static_cast<uint16_t>(i + 10086),
                                                 },
                                                 {.queue_depth = 16, .max_rpc_msg_size = 512}, rntq, running, i + 11);
  }

  std::vector<TransportWrapper<Backend::DOCA_RDMA>*> rdmas(n_rmdas, nullptr);

  for (uint16_t i = 0; i < n_rmdas; i++) {
    rdmas[i] = new TransportWrapper<Backend::DOCA_RDMA>(rdma_dev,
                                                        {
                                                            .passive = false,
                                                            .enable_grh = true,
                                                            .remote_ip = "192.168.203.21",
                                                            .local_ip = "192.168.203.20",
                                                            .remote_port = static_cast<uint16_t>(i + 10086),
                                                            .local_port = static_cast<uint16_t>(i + 12306),
                                                        },
                                                        {.queue_depth = 16, .max_rpc_msg_size = 512});
  }
  for (uint32_t i = 0; i < n_rmdas; i++) {
    rdmas[i]->ch.establish_connections();
  }

  NaiveSpillWorker<Backend::DOCA_Comch> lnsw(ch_dev, rdma_dev, {.passive = true, .name = "spill"},
                                             {.queue_depth = 16, .max_rpc_msg_size = 512}, lntq, running, 1);

  TransportWrapper<Backend::DOCA_Comch> io(ch_dev, rdma_dev, {.passive = true, .name = "disk"},
                                           {.queue_depth = 16, .max_rpc_msg_size = 512});

  io.ch.establish_connections();

  NaiveShuffleWorkerPool nswp(n_shuffle_worker, io, rdmas, [](size_t id) { return id % 2 == 1; }, native_partition);

  while (true) {
    while (lntq.empty() && rntq.empty() && running) {
      std::this_thread::sleep_for(1us);
    }
    if (!running) {
      break;
    }

    bool from_remote = false;
    OffloadSpillTask* t = nullptr;
    if (!lntq.empty()) {
      t = *lntq.front();
      lntq.pop();
      from_remote = false;
    } else if (!rntq.empty()) {
      t = *rntq.front();
      rntq.pop();
      from_remote = true;
    } else {
      unreachable();
    }

    if (from_remote) {
      DEBUG("got remote spill with length {} at {}", t->buffer->size(), (void*)t->buffer->data());
      auto pid = *(uint32_t*)t->buffer->data();
      // DEBUG("{} {}", l->length, l->partition_id);
      DEBUG("partition {}", pid);
    } else {
      DEBUG("got local spill with length {} at {}", t->buffer->size(), (void*)t->buffer->data());
      // auto l = (NaiveSpillDataLayout*)t->data();
      // DEBUG("{} {} {} {} {}", l->n_record, l->size, l->data_offset, l->hashcodes_offset, l->offsets_offset);
    }

    nswp.submit_task(t, from_remote);
  }

  for (uint32_t i = 0; i < n_rmdas; i++) {
    rdmas[i]->ch.terminate_connections();
    delete rdmas[i];
  }
  io.ch.terminate_connections();
  lnsw.join();
  for (uint32_t i = 0; i < n_rmdas; i++) {
    rnsws[i]->join();
    delete rnsws[i];
  }
}

}  // namespace dpx

int main(int argc, char* argv[]) {
  spdlog::set_level(spdlog::level::trace);
  if (argc != 2) {
    die("Usage: %s [dpu20/dpu21]\n", argv[0]);
  }

  // signal(SIGINT, [](int) {
  //   INFO("trigger stop");
  //   dpx::running = false;
  // });
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
