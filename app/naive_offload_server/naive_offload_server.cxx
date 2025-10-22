#include <jni.h>

#include <barrier>

#include "native/shuffle_worker.hxx"
#include "native/spill_worker.hxx"
#include "util/literal.hxx"
#include "util/timer.hxx"

JavaVM* jvm = nullptr;

uint32_t n_shuffle_worker = 12;
uint32_t n_partition = 0;

struct JEnv {
  JEnv() {
    INFO("Attach to JVM");
    jvm->AttachCurrentThread((void**)&env, nullptr);
  }
  ~JEnv() {
    INFO("Detach with JVM");
    jvm->DetachCurrentThread();
  }

  JNIEnv* operator->() { return env; }

  JNIEnv* env;
};

std::vector<std::vector<uint8_t>> jvm_partition(std::span<uint8_t> data) {
  dpx::Timer t;
  static thread_local JEnv jenv;
  uint32_t np = n_partition;
  std::vector<std::vector<uint8_t>> results(np);
  auto partitioner_cls = jenv->FindClass("Partitioner");
  if (partitioner_cls == nullptr) {
    die("Fail to get partitioner clazz");
  }
  auto partitioner_doPartition_method = jenv->GetStaticMethodID(partitioner_cls, "doPartition", "([BI)[[B");
  if (partitioner_doPartition_method == nullptr) {
    die("Fail to get partitioner doPartition method id");
  }

  auto j_input = jenv->NewByteArray(data.size_bytes());
  if (j_input == nullptr) {
    jenv->ExceptionDescribe();
    die("Fail to create j_input");
  }
  auto input = (jbyte*)(data.data());
  jenv->SetByteArrayRegion(j_input, 0, data.size_bytes(), input);
  dpx::Timer t2;
  auto j_outputs =
      (jobjectArray)jenv->CallStaticObjectMethod(partitioner_cls, partitioner_doPartition_method, j_input, np);
  auto jvm_call_elapsed_us = t2.elapsed_us();
  if (j_outputs == nullptr) {
    jenv->ExceptionDescribe();
    die("Fail to call partitioner");
  }
  jboolean is_copy = false;
  for (uint32_t i = 0; i < np; i++) {
    auto j_output = (jbyteArray)jenv->GetObjectArrayElement(j_outputs, i);
    auto j_raw = (const uint8_t*)jenv->GetPrimitiveArrayCritical(j_output, &is_copy);
    auto j_raw_length = jenv->GetArrayLength(j_output);
    auto raw = std::span<const uint8_t>(j_raw, j_raw_length);
    dpx::PartitionDataHeader h{.partition_id = i,
                               .length = j_raw_length + sizeof(dpx::PartitionDataHeader) - sizeof(uint32_t)};
    auto p = reinterpret_cast<uint8_t*>(&h);
    results[i].insert(results[i].end(), p, p + sizeof(dpx::PartitionDataHeader));
    results[i].insert(results[i].end(), raw.begin() + sizeof(uint32_t), raw.end());
    jenv->ReleasePrimitiveArrayCritical(j_output, (jbyte*)j_raw, 0);
    jenv->DeleteLocalRef(j_output);
  }
  jenv->DeleteLocalRef(j_outputs);
  // jenv->ReleaseByteArrayElements(j_input, input, 0);
  jenv->DeleteLocalRef(j_input);
  jenv->DeleteLocalRef(partitioner_cls);
  WARN("partition size: {} call {} do partition elapsed: {}", data.size(), jvm_call_elapsed_us, t.elapsed_us());
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

  std::vector<NaiveSpillWorker<Backend::DOCA_RDMA>*> rnsws(1, nullptr);
  for (uint16_t i = 0; i < 1; i++) {
    rnsws[i] =
        new NaiveSpillWorker<Backend::DOCA_RDMA>(rdma_dev,
                                                 {
                                                     .passive = true,
                                                     .enable_grh = true,
                                                     .local_ip = "192.168.203.21",
                                                     .local_port = static_cast<uint16_t>(i + 10086),
                                                 },
                                                 {.queue_depth = 16, .max_rpc_msg_size = 512}, rntq, running, i + 1);
  }

  std::vector<TransportWrapper<Backend::DOCA_RDMA>*> rdmas(1, nullptr);

  for (uint16_t i = 0; i < 1; i++) {
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
  for (uint32_t i = 0; i < 1; i++) {
    rdmas[i]->ch.establish_connections();
  }

  NaiveSpillWorker<Backend::DOCA_Comch> lnsw(ch_dev, rdma_dev, {.passive = true, .name = "spill"},
                                             {.queue_depth = 16, .max_rpc_msg_size = 512}, lntq, running, 2);

  TransportWrapper<Backend::DOCA_Comch> io(ch_dev, rdma_dev, {.passive = true, .name = "disk"},
                                           {.queue_depth = 16, .max_rpc_msg_size = 512});

  io.ch.establish_connections();

  NaiveShuffleWorkerPool nswp(n_shuffle_worker, io, rdmas, [](size_t id) { return id % 2 == 0; }, jvm_partition);

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

  for (uint32_t i = 0; i < 1; i++) {
    rdmas[i]->ch.terminate_connections();
    delete rdmas[i];
  }
  io.ch.terminate_connections();
  lnsw.join();
  for (uint32_t i = 0; i < 1; i++) {
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

  std::vector<NaiveSpillWorker<Backend::DOCA_RDMA>*> rnsws(1, nullptr);
  for (uint16_t i = 0; i < 1; i++) {
    rnsws[i] =
        new NaiveSpillWorker<Backend::DOCA_RDMA>(rdma_dev,
                                                 {
                                                     .passive = true,
                                                     .enable_grh = true,
                                                     .local_ip = "192.168.203.20",
                                                     .local_port = static_cast<uint16_t>(i + 10086),
                                                 },
                                                 {.queue_depth = 16, .max_rpc_msg_size = 512}, rntq, running, i + 1);
  }

  std::vector<TransportWrapper<Backend::DOCA_RDMA>*> rdmas(1, nullptr);

  for (uint16_t i = 0; i < 1; i++) {
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
  for (uint32_t i = 0; i < 1; i++) {
    rdmas[i]->ch.establish_connections();
  }

  NaiveSpillWorker<Backend::DOCA_Comch> lnsw(ch_dev, rdma_dev, {.passive = true, .name = "spill"},
                                             {.queue_depth = 16, .max_rpc_msg_size = 512}, lntq, running, 2);

  TransportWrapper<Backend::DOCA_Comch> io(ch_dev, rdma_dev, {.passive = true, .name = "disk"},
                                           {.queue_depth = 16, .max_rpc_msg_size = 512});

  io.ch.establish_connections();

  NaiveShuffleWorkerPool nswp(n_shuffle_worker, io, rdmas, [](size_t id) { return id % 2 == 1; }, jvm_partition);

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

  for (uint32_t i = 0; i < 1; i++) {
    rdmas[i]->ch.terminate_connections();
    delete rdmas[i];
  }
  io.ch.terminate_connections();
  lnsw.join();
  for (uint32_t i = 0; i < 1; i++) {
    rnsws[i]->join();
    delete rnsws[i];
  }
}

}  // namespace dpx

int main(int argc, char* argv[]) {
  spdlog::set_level(spdlog::level::trace);
  if (argc != 3) {
    die("Usage: %s [dpu20/dpu21] [partition number]\n", argv[0]);
  }

  JavaVMOption options[3];
  JavaVMInitArgs vm_args;
  JNIEnv* env = nullptr;
  options[0].optionString = "-Djava.class.path=/home/lsc/dpx/build/app/naive_offload_server/Partitioner.jar";
  options[1].optionString = "-Xmx8G";
  options[2].optionString = "-Xms8G";
  vm_args.version = JNI_VERSION_1_8;
  vm_args.nOptions = 3;
  vm_args.options = options;
  auto status = JNI_CreateJavaVM(&jvm, (void**)&env, &vm_args);
  if (status == JNI_ERR) {
    die("Fail to create JVM");
  }

  // signal(SIGINT, [](int) {
  //   INFO("trigger stop");
  //   dpx::running = false;
  // });
  auto which = std::string(argv[1]);
  n_partition = std::atoi(argv[2]);
  if (which == "dpu21") {
    dpx::dpu21_server_main();
  } else if (which == "dpu20") {
    dpx::dpu20_server_main();
  } else {
    die("Usage: %s [dpu20/dpu21]\n", argv[0]);
  }

  // auto f = std::string("/home/lsc/dpx/.test_spill/t0");
  // int fd = open(f.c_str(), O_RDONLY);
  // struct stat s = {};
  // stat(f.c_str(), &s);
  // INFO("{} size: {}", f, s.st_size);
  // auto buf = new uint8_t[s.st_size];
  // read(fd, buf, s.st_size);
  // close(fd);

  // std::thread([&]() {
  //   auto result = jvm_partition(std::span<uint8_t>(buf, s.st_size));
  //   for (uint32_t i = 0; i < 32; i++) {
  //     INFO("result {}: {}", i, result[i].size());
  //   }
  // }).join();

  // delete[] buf;
  jvm->DestroyJavaVM();  // wait for all
  return 0;
}
