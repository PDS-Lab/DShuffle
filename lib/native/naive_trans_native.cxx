#include "native/naive_trans_native.hxx"

#include "native/spill_agent.hxx"
#include "native/spill_worker.hxx"
#include "util/j_util.hxx"
#include "util/literal.hxx"

namespace {

inline static dpx::doca::Device *dev_mlx5_1 = nullptr;
inline static dpx::NaiveSpillAgent *sa = nullptr;
inline static dpx::HostSpillWorker *sw = nullptr;
inline static std::mutex mu;
inline static dpx::NaiveSpillTaskQueue q(16);
inline static std::latch sp(3);

inline static std::atomic_bool running = true;

}  // namespace

/*
 * Class:     pdsl_dpx_NaiveTransEnv
 * Method:    Initialize
 * Signature: (Ljava/lang/String;Ljava/lang/String;)V
 */
JNIEXPORT void JNICALL Java_pdsl_dpx_NaiveTransEnv_Initialize(JNIEnv *j_env, jclass, jstring j_pci_addr,
                                                              jstring j_spill_dir) {
  spdlog::set_level(spdlog::level::trace);

  auto pci_addr = dpx::get_j_string(j_env, j_pci_addr);
  auto spill_dir = dpx::get_j_string(j_env, j_spill_dir);

  dev_mlx5_1 = dpx::doca::Device::new_device_by_pci_addr(pci_addr);

  sa = new dpx::NaiveSpillAgent(*dev_mlx5_1, {.passive = false, .name = "spill"},
                                {.queue_depth = 16, .max_rpc_msg_size = 512}, q, sp, running, 16);

  std::this_thread::sleep_for(1s);  // split two connections

  sw = new dpx::HostSpillWorker(*dev_mlx5_1, {.passive = false, .name = "disk"},
                                {.queue_depth = 16, .max_rpc_msg_size = 512}, spill_dir, 192, sp, running, 17);

  sp.arrive_and_wait();

  INFO("NativeTransEnv Initialized");
}

/*
 * Class:     pdsl_dpx_NaiveTransEnv
 * Method:    TriggerSpillStart
 * Signature: (Z)V
 */
JNIEXPORT void JNICALL Java_pdsl_dpx_NaiveTransEnv_TriggerSpillStart(JNIEnv *, jclass, jboolean need_header) {
  sw->create_partition_files(need_header);
}

/*
 * Class:     pdsl_dpx_NaiveTransEnv
 * Method:    Spill
 * Signature: ([B)V
 */
JNIEXPORT void JNICALL Java_pdsl_dpx_NaiveTransEnv_Spill(JNIEnv *j_env, jclass, jbyteArray j_data) {
  auto data = dpx::get_raw_j_array<uint8_t>(j_env, j_data);
  // auto offsets = dpx::get_raw_j_array<uint32_t>(j_env, j_offsets);
  // auto hashcodes = dpx::get_raw_j_array<uint32_t>(j_env, j_hashcodes);
  INFO("do spill");
  // sa->spill(data, offsets, hashcodes);
  // sa->spill(data);
  dpx::NaiveSpillTask t{.res = {}, .data = data};
  {
    std::lock_guard l(mu);
    q.push(&t);
  }
  INFO("wait spill");
  size_t n_spill = t.res.get_future().get();
  if (n_spill != data.size_bytes()) {
    die("Fail to spill");
  }
  INFO("done spill");
  dpx::release_raw_j_array(j_env, j_data, data);
  // dpx::release_raw_j_array(j_env, j_offsets, offsets);
  // dpx::release_raw_j_array(j_env, j_hashcodes, hashcodes);
}

/*
 * Class:     pdsl_dpx_NaiveTransEnv
 * Method:    WaitForSpillDone
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_pdsl_dpx_NaiveTransEnv_WaitForSpillDone(JNIEnv *, jclass) {
  INFO("Check for spilling");
  while (sw->is_spilling()) {
    std::this_thread::sleep_for(1s);
  }
  sw->close_partition_files();
  INFO("Spilling is done");
}
/*
 * Class:     pdsl_dpx_NaiveTransEnv
 * Method:    Destroy
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_pdsl_dpx_NaiveTransEnv_Destroy(JNIEnv *, jclass) {
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

  INFO("NaiveTransEnv Destroyed");
}
