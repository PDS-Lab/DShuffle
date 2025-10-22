#pragma once

#include <jni.h>

#include <cstddef>
#include <optional>
#include <util/literal.hxx>

#include "sd/common/args.h"
#include "util/fatal.hxx"
#include "util/j_util.hxx"
#include "util/logger.hxx"

using namespace dpx::literal;

namespace dpx::sd {

struct Options {
  bool use_dpa = false;
  bool enable_utf16_to_utf8 = false;
  size_t max_class_info_size = 16_KB;
  size_t max_task_ctx_buffer_size = 128_KB;
  size_t max_task_out_buffer_size = 128_KB;
  size_t max_device_threads = 4;
  size_t max_heap_size;
  size_t min_heap_size;
  size_t heap_base_min_address;
  size_t metaspace_base_min_address;
  size_t metaspace_size;
  size_t max_metaspace_size;
  size_t compressed_class_space_size;
  bool use_compressed_oops;
  bool use_compressed_class_pointers;
  std::string compressed_oops_mode;
  size_t oop_shift_amount;
  size_t narrow_klass_base;
  size_t narrow_klass_shift;
  size_t compressed_class_space_base;

  static std::optional<std::pair<Options, jvm_args_t>> from_java(JNIEnv* j_env, jobject j_options) {
    jclass j_class = j_env->GetObjectClass(j_options);
    if (j_class == nullptr) {
      return std::nullopt;
    }
    Options o;
    jvm_args_t args;

    auto get_bool = [&](const char* field) -> bool {
      return j_env->GetBooleanField(j_options, j_env->GetFieldID(j_class, field, "Z"));
    };
    auto get_long = [&](const char* field) -> int64_t {
      return j_env->GetLongField(j_options, j_env->GetFieldID(j_class, field, "J"));
    };
    auto get_string = [&](const char* field) -> std::string {
      auto jfield = j_env->GetFieldID(j_class, field, "Ljava/lang/String;");
      assert(jfield != nullptr);
      auto jstr = (jstring)j_env->GetObjectField(j_options, jfield);
      assert(jstr != nullptr);
      return get_j_string(j_env, jstr);
    };

    o.use_dpa = get_bool("useDpa");
    o.enable_utf16_to_utf8 = get_bool("enableUtf16ToUtf8");
    o.max_class_info_size = get_long("maxClassInfoSize");
    o.max_task_ctx_buffer_size = get_long("maxTaskCtxBufferSize");
    o.max_task_out_buffer_size = get_long("maxTaskOutBufferSize");
    o.max_device_threads = get_long("maxDeviceThreads");
    args.h_heap_size = o.max_heap_size = get_long("maxHeapSize");
    o.min_heap_size = get_long("minHeapSize");
    args.h_heap_base = o.heap_base_min_address = get_long("heapBaseMinAddress");
    o.metaspace_base_min_address = get_long("metaspaceBaseMinAddress");
    o.metaspace_size = get_long("metaspaceSize");
    o.max_metaspace_size = get_long("maxMetaspaceSize");
    args.h_compressed_class_space_size = o.compressed_class_space_size = get_long("compressedClassSpaceSize");
    o.use_compressed_oops = get_bool("useCompressedOops");
    o.use_compressed_class_pointers = get_bool("useCompressedClassPointers");
    o.compressed_oops_mode = get_string("compressedOopsMode");
    args.heap_compress_ptr_shift = o.oop_shift_amount = get_long("oopShiftAmount");
    o.narrow_klass_base = get_long("narrowKlassBase");
    args.metaspace_compress_ptr_shift = o.narrow_klass_shift = get_long("narrowKlassShift");
    args.h_compressed_class_space_base = o.compressed_class_space_base = get_long("compressedClassSpaceBase");

    if (o.compressed_oops_mode == "Zero based") {
      args.heap_compress_ptr_mode = args.metaspace_compress_ptr_mode = JVM_COMPRESS_PTR_MODE_ZERO_BASED;
    } else {
      die("check jvm args");
    }

    if (o.use_dpa && o.enable_utf16_to_utf8) {
      WARN("dpa does not support utf16 to utf8, set to false");
      o.enable_utf16_to_utf8 = false;
    }

    return {std::make_pair(o, args)};
  }
};

}  // namespace dpx::sd
