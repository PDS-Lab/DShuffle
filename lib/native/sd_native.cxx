#include "native/sd_native.hxx"

#include <glaze/glaze.hpp>

#include "sd/native/jenv_util.hxx"
#include "sd/native/sd.hxx"
#include "util/logger.hxx"

extern "C" doca_dpa_app *sd;

namespace {

static dpx::sd::Options g_options;
static dpx::sd::ClassResolver *g_r = nullptr;
static dpx::sd::Context *g_ctx = nullptr;
static dpx::doca::Device *dev_mlx5_1 = nullptr;
static dpx::sd::DPAContext *g_d_ctx = nullptr;
static dpx::doca::MappedRegion *jvm_heap = nullptr;

}  // namespace

/*
 * Class:     pdsl_dpx_SD
 * Method:    Start
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_pdsl_dpx_SD_Start(JNIEnv *, jclass) {
  if (g_options.use_dpa) {
    g_d_ctx->export_class_infos();
  }
}
/*
 * Class:     pdsl_dpx_SD
 * Method:    Stop
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_pdsl_dpx_SD_Stop(JNIEnv *, jclass) {}

/*
 * Class:     pdsl_dpx_SD
 * Method:    Destroy
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_pdsl_dpx_SD_Destroy(JNIEnv *, jclass) {
  if (g_ctx != nullptr) {
    delete g_ctx;
  }
  if (g_d_ctx != nullptr) {
    delete g_d_ctx;
  }
  if (g_r != nullptr) {
    delete g_r;
  }
  if (jvm_heap != nullptr) {
    delete jvm_heap;
  }
  if (dev_mlx5_1 != nullptr) {
    delete dev_mlx5_1;
  }
}

/*
 * Class:     pdsl_dpx_SD
 * Method:    ShowRegisteredClass
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_pdsl_dpx_SD_ShowRegisteredClass(JNIEnv *, jclass) { g_r->show_class_infos(); }

/*
 * Class:     pdsl_dpx_SD
 * Method:    Serialize
 * Signature: (Ljava/lang/Object;)[B
 */
JNIEXPORT jbyteArray JNICALL Java_pdsl_dpx_SD_Serialize(JNIEnv *j_env, jclass, jobject j_obj) {
  if (g_options.use_dpa) {
    return g_d_ctx->serialize(j_env, j_obj);
  }
  return g_ctx->serialize(j_env, j_obj);
}

/*
 * Class:     pdsl_dpx_SD
 * Method:    Deserialize
 * Signature: ([BLjava/lang/Class;)Ljava/lang/Object;
 */
JNIEXPORT jobject JNICALL Java_pdsl_dpx_SD_Deserialize(JNIEnv *j_env, jclass, jbyteArray j_input, jclass j_class) {
  return g_ctx->deserialize(j_env, j_input, j_class);
}

/*
 * Class:     pdsl_dpx_SD
 * Method:    Register
 * Signature: (Ljava/util/Set;)V
 */
JNIEXPORT void JNICALL Java_pdsl_dpx_SD_Register(JNIEnv *j_env, jclass, jobject j_set_classes) {
  g_r->register_classes(j_env, j_set_classes);
}

/*
 * Class:     pdsl_dpx_SD
 * Method:    Initialize
 * Signature: (Ljava/util/Map;Lpdsl/dpx/Options;)V
 */
JNIEXPORT void JNICALL Java_pdsl_dpx_SD_Initialize(JNIEnv *j_env, jclass, jobject j_jvm_options,
                                                   jobject j_dpx_options) {
  spdlog::set_level(spdlog::level::trace);
  auto jvm_options = dpx::sd::java_str2str_hashmap_cvt_stl_map(j_env, j_jvm_options).value();
  std::tie(g_options, dpx::sd::JVMArgs::jvm_args) = dpx::sd::Options::from_java(j_env, j_dpx_options).value();
  // INFO("jvm options: {}", glz::write<glz::opts{.prettify = true}>(jvm_options).value_or("Corrupted option"));
  INFO("dpx sd options: {}", glz::write<glz::opts{.prettify = true}>(g_options).value_or("Corrupted option"));
  INFO("jvm args options: {}",
       glz::write<glz::opts{.prettify = true}>(dpx::sd::JVMArgs::jvm_args).value_or("Corrupted option"));
  g_r = new dpx::sd::ClassResolver(g_options);
  g_ctx = new dpx::sd::Context(g_options, *g_r);
  if (g_options.use_dpa) {
    dev_mlx5_1 = new dpx::doca::Device("mlx5_1", dpx::doca::Device::FindByIBDevName);
    dev_mlx5_1->open_dpa(::sd, "sd");
    jvm_heap =
        new dpx::doca::MappedRegion(*dev_mlx5_1, dpx::sd::JVMArgs::heap_base_ptr(), dpx::sd::JVMArgs::heap_size());
    auto &args = dpx::sd::JVMArgs::jvm_args;
    dpx::doca::launch_rpc(*dev_mlx5_1, register_jvm_heap, args.h_heap_base, args.h_heap_size,
                          args.h_compressed_class_space_base, args.h_compressed_class_space_size,
                          jvm_heap->get_mmap_handle(), args.heap_compress_ptr_mode, args.metaspace_compress_ptr_mode,
                          args.heap_compress_ptr_shift, args.metaspace_compress_ptr_shift);
    g_d_ctx = new dpx::sd::DPAContext(*dev_mlx5_1, g_options, *g_r);
  }
}
