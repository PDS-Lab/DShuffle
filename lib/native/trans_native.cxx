#include "native/trans_native.hxx"

#include "native/disk_agent.hxx"
#include "native/spill_agent.hxx"
#include "util/j_util.hxx"
#include "util/literal.hxx"

using namespace std::chrono_literals;

namespace {

inline static dpx::doca::Device *dev_mlx5_1 = nullptr;
inline static dpx::SpillAgent *sa = nullptr;
inline static dpx::DiskAgent *da = nullptr;
inline static std::latch sp(3);
inline static std::atomic_bool running = true;

}  // namespace

/*
 * Class:     pdsl_dpx_TransEnv
 * Method:    Initialize
 * Signature: (Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V
 */
JNIEXPORT void JNICALL Java_pdsl_dpx_TransEnv_Initialize(JNIEnv *j_env, jclass, jstring j_pci_addr,
                                                         jstring j_mount_point, jstring j_output_device) {
  spdlog::set_level(spdlog::level::trace);

  auto pci_addr = dpx::get_j_string(j_env, j_pci_addr);
  auto mount_point = dpx::get_j_string(j_env, j_mount_point);
  auto output_device = dpx::get_j_string(j_env, j_output_device);

  dev_mlx5_1 = dpx::doca::Device::new_device_by_pci_addr(pci_addr);

  sa = new dpx::SpillAgent(*dev_mlx5_1, {.passive = false, .name = "spill"},
                           {.queue_depth = 32, .max_rpc_msg_size = 512}, 2, 256, 8_MB, 192, sp, running);

  da = new dpx::DiskAgent(*dev_mlx5_1, {.passive = false, .name = "disk"}, {.queue_depth = 4, .max_rpc_msg_size = 512},
                          mount_point, output_device, 192);

  sp.arrive_and_wait();

  INFO("TransEnv Initialized");
}

/*
 * Class:     pdsl_dpx_TransEnv
 * Method:    TriggerSpillStart
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_pdsl_dpx_TransEnv_TriggerSpillStart(JNIEnv *, jclass) { da->mount_on_dpu(); }
/*
 * Class:     pdsl_dpx_TransEnv
 * Method:    Append
 * Signature: (I[B[BZ)V
 */
JNIEXPORT void JNICALL Java_pdsl_dpx_TransEnv_Append(JNIEnv *j_env, jclass, jint partition_id, jbyteArray j_key,
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
 * Class:     pdsl_dpx_TransEnv
 * Method:    WaitForSpillDone
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_pdsl_dpx_TransEnv_WaitForSpillDone(JNIEnv *, jclass) { da->umount_on_dpu(); }

/*
 * Class:     pdsl_dpx_TransEnv
 * Method:    Destroy
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_pdsl_dpx_TransEnv_Destroy(JNIEnv *, jclass) {
  running = false;
  if (sa != nullptr) {
    delete sa;
  }
  if (da != nullptr) {
    delete da;
  }
  if (dev_mlx5_1 != nullptr) {
    delete dev_mlx5_1;
  }
  INFO("TransEnv Destroyed");
}