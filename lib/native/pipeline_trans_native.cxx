#include "native/pipeline_trans_native.hxx"

#include "native/spill_agent.hxx"
#include "native/spill_worker.hxx"
#include "util/j_util.hxx"
#include "util/literal.hxx"

namespace {
inline static dpx::doca::Device *dev_mlx5_1 = nullptr;
inline static dpx::SpillAgent *sa = nullptr;
inline static dpx::BufferredHostSpillWorker *sw = nullptr;
inline static std::latch sp(3);
inline static std::atomic_bool running = true;
}  // namespace

/*
 * Class:     pdsl_dpx_PipelineTransEnv
 * Method:    Initialize
 * Signature: (Ljava/lang/String;Ljava/lang/String;)V
 */
JNIEXPORT void JNICALL Java_pdsl_dpx_PipelineTransEnv_Initialize(JNIEnv *j_env, jclass, jstring j_pci_addr,
                                                                 jstring j_spill_dir) {
  spdlog::set_level(spdlog::level::trace);

  auto pci_addr = dpx::get_j_string(j_env, j_pci_addr);
  auto spill_dir = dpx::get_j_string(j_env, j_spill_dir);

  dev_mlx5_1 = dpx::doca::Device::new_device_by_pci_addr(pci_addr);

  sa = new dpx::SpillAgent(*dev_mlx5_1, {.passive = false, .name = "spill"},
                           {.queue_depth = 32, .max_rpc_msg_size = 512}, 1, 64, 32_MB, 32, sp, running);

  std::this_thread::sleep_for(1s);

  sw = new dpx::BufferredHostSpillWorker(*dev_mlx5_1, {.passive = false, .name = "disk"},
                                         {.queue_depth = 32, .max_rpc_msg_size = 512}, spill_dir, 32, 32_MB, 32, sp,
                                         running, 0);

  std::this_thread::sleep_for(1s);

  sp.arrive_and_wait();

  std::this_thread::sleep_for(1s);

  INFO("PipelineTransEnv Initialized");
}

/*
 * Class:     pdsl_dpx_PipelineTransEnv
 * Method:    TriggerSpillStart
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_pdsl_dpx_PipelineTransEnv_TriggerSpillStart(JNIEnv *, jclass) {
  sw->create_partition_files(false);
}

/*
 * Class:     pdsl_dpx_PipelineTransEnv
 * Method:    Append
 * Signature: (I[B[BZ)V
 */
JNIEXPORT void JNICALL Java_pdsl_dpx_PipelineTransEnv_Append(JNIEnv *j_env, jclass, jint partition_id, jbyteArray j_key,
                                                             jbyteArray j_value, jboolean is_last) {
  auto key = dpx::get_raw_j_array<uint8_t>(j_env, j_key);
  auto value = dpx::get_raw_j_array<uint8_t>(j_env, j_value);
  // DEBUG("write partition {} with length {}", partition_id, raw_data_length);
  sa->append_or_spill(partition_id, key, value);
  if (is_last) {
    DEBUG("is last {}", is_last);
    sa->force_spill(partition_id);
  }
  dpx::release_raw_j_array(j_env, j_key, key);
  dpx::release_raw_j_array(j_env, j_value, value);
}

/*
 * Class:     pdsl_dpx_PipelineTransEnv
 * Method:    WaitForSpillDone
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_pdsl_dpx_PipelineTransEnv_WaitForSpillDone(JNIEnv *, jclass) {
  INFO("Check for spilling");
  while (sw->is_spilling()) {
    std::this_thread::sleep_for(1s);
  }
  sw->close_partition_files();
  INFO("Spilling is done");
}

/*
 * Class:     pdsl_dpx_PipelineTransEnv
 * Method:    Destroy
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_pdsl_dpx_PipelineTransEnv_Destroy(JNIEnv *, jclass) {
  running = false;
  if (sa != nullptr) {
    delete sa;
  }
  if (sw != nullptr) {
    delete sw;
  }
  if (dev_mlx5_1 != nullptr) {
    delete dev_mlx5_1;
  }

  INFO("PipelineTransEnv Destroyed");
}
