#include "native/serde_native.hxx"

#include <glaze/glaze.hpp>
#include <set>

#include "sd/native/jenv_util.hxx"
#include "sd/native/sd.hxx"
#include "util/logger.hxx"

namespace {

static dpx::sd::Options g_options;
static dpx::sd::ClassResolver *g_r = nullptr;
static std::mutex mu;
static std::set<dpx::sd::Context *> g_ctx;

}  // namespace

/*
 * Class:     pdsl_dpx_Serde
 * Method:    Destroy
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_pdsl_dpx_Serde_Destroy(JNIEnv *, jclass) {
  if (g_r != nullptr) {
    delete g_r;
  }
  for (auto ctx : g_ctx) {
    delete ctx;
  }
}

/*
 * Class:     pdsl_dpx_Serde
 * Method:    ShowRegisteredClass
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_pdsl_dpx_Serde_ShowRegisteredClass(JNIEnv *, jclass) {
  if (g_r != nullptr) {
    g_r->show_class_infos();
  } else {
    WARN("Not initialized");
  }
}

/*
 * Class:     pdsl_dpx_Serde
 * Method:    Serialize
 * Signature: (JLjava/lang/Object;)[B
 */
JNIEXPORT jbyteArray JNICALL Java_pdsl_dpx_Serde_Serialize(JNIEnv *j_env, jclass, jlong j_handle, jobject j_obj) {
  return reinterpret_cast<dpx::sd::Context *>(j_handle)->serialize(j_env, j_obj);
}

/*
 * Class:     pdsl_dpx_Serde
 * Method:    Deserialize
 * Signature: (J[BLjava/lang/Class;)Ljava/lang/Object;
 */
JNIEXPORT jobject JNICALL Java_pdsl_dpx_Serde_Deserialize(JNIEnv *j_env, jclass, jlong j_handle, jbyteArray j_input,
                                                          jclass j_class) {
  return reinterpret_cast<dpx::sd::Context *>(j_handle)->deserialize(j_env, j_input, j_class);
}

/*
 * Class:     pdsl_dpx_Serde
 * Method:    Register
 * Signature: (Ljava/util/Set;)V
 */
JNIEXPORT void JNICALL Java_pdsl_dpx_Serde_Register(JNIEnv *j_env, jclass, jobject j_set_classes) {
  if (g_r != nullptr) {
    g_r->register_classes(j_env, j_set_classes);
  }
}

/*
 * Class:     pdsl_dpx_Serde
 * Method:    Initialize
 * Signature: (Ljava/util/Map;Lpdsl/dpx/Options;)V
 */
JNIEXPORT void JNICALL Java_pdsl_dpx_Serde_Initialize(JNIEnv *j_env, jclass, jobject j_jvm_options,
                                                      jobject j_dpx_options) {
  spdlog::set_level(spdlog::level::trace);
  auto jvm_options = dpx::sd::java_str2str_hashmap_cvt_stl_map(j_env, j_jvm_options).value();
  std::tie(g_options, dpx::sd::JVMArgs::jvm_args) = dpx::sd::Options::from_java(j_env, j_dpx_options).value();
  // INFO("jvm options: {}", glz::write<glz::opts{.prettify = true}>(jvm_options).value_or("Corrupted option"));
  INFO("dpx sd options: {}", glz::write<glz::opts{.prettify = true}>(g_options).value_or("Corrupted option"));
  INFO("jvm args options: {}",
       glz::write<glz::opts{.prettify = true}>(dpx::sd::JVMArgs::jvm_args).value_or("Corrupted option"));
  g_options.enable_utf16_to_utf8 = true;
  g_r = new dpx::sd::ClassResolver(g_options);
}

/*
 * Class:     pdsl_dpx_Serde
 * Method:    create
 * Signature: ()J
 */
JNIEXPORT jlong JNICALL Java_pdsl_dpx_Serde_create(JNIEnv *, jclass) {
  if (g_r == nullptr) {
    WARN("Not initialized");
    return 0;
  }
  auto ctx = new dpx::sd::Context(g_options, *g_r);
  {
    std::lock_guard l(mu);
    g_ctx.insert(ctx);
  }
  return reinterpret_cast<jlong>(ctx);
}

/*
 * Class:     pdsl_dpx_Serde
 * Method:    close
 * Signature: (J)V
 */
JNIEXPORT void JNICALL Java_pdsl_dpx_Serde_close(JNIEnv *, jclass, jlong j_handle) {
  if (j_handle != 0) {
    auto ctx = reinterpret_cast<dpx::sd::Context *>(j_handle);
    {
      std::lock_guard l(mu);
      g_ctx.erase(ctx);
    }
    delete ctx;
  }
}
