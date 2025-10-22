#pragma once

#include <inttypes.h>

#include <cassert>
#include <cmath>

#include "sd/common/args.h"
#include "util/unreachable.hxx"

namespace dpx::sd {

// NOTICE: we donot accept options from jvm, we just check the options here
// because we need the jvm to be started by these options
struct JVMArgs {
  inline static jvm_args_t jvm_args = {};

  inline static uint64_t heap_base_addr() { return jvm_args.h_heap_base; }
  inline static void *heap_base_ptr() { return (void *)heap_base_addr(); }
  inline static uint64_t heap_size() { return jvm_args.h_heap_size; }
  inline static uint64_t compressed_class_space_base_addr() { return jvm_args.h_compressed_class_space_base; }
  inline static uint64_t compressed_class_space_size() { return jvm_args.h_compressed_class_space_size; }
  inline static void *compressed_class_space_base_ptr() { return (void *)compressed_class_space_base_addr(); }

  inline static void *parse_cptr(uint32_t cptr, uint64_t base, uint32_t mode, uint32_t shift) {
    uint64_t ptr = cptr;
    switch (mode) {
      case JVM_COMPRESS_PTR_MODE_RAW32:
        return (void *)ptr;
      case JVM_COMPRESS_PTR_MODE_ZERO_BASED:
        return (void *)(ptr << shift);
      case JVM_COMPRESS_PTR_MODE_NON_ZERO_BASED:
        return (void *)(base + (ptr << shift));
      case JVM_COMPRESS_PTR_MODE_RAW64:
      default:
        unreachable();
    }
  }

  inline static uint32_t compress_ptr(uint64_t ptr, uint64_t base, uint32_t mode, uint32_t shift) {
    switch (mode) {
      case JVM_COMPRESS_PTR_MODE_RAW32:
        return ptr;
      case JVM_COMPRESS_PTR_MODE_ZERO_BASED:
        return ptr >> shift;
      case JVM_COMPRESS_PTR_MODE_NON_ZERO_BASED:
        return (ptr >> shift) - base;
      case JVM_COMPRESS_PTR_MODE_RAW64:
      default:
        unreachable();
    }
  }

  inline static void *parse_heap_cptr(uint64_t cptr) {
    return parse_cptr(cptr, jvm_args.h_heap_base, jvm_args.heap_compress_ptr_mode, jvm_args.heap_compress_ptr_shift);
  }

  inline static void *parse_metaspace_cptr(uint64_t cptr) {
    return parse_cptr(cptr, jvm_args.h_compressed_class_space_base, jvm_args.metaspace_compress_ptr_mode,
                      jvm_args.metaspace_compress_ptr_shift);
  }

  template <typename T>
  inline static uint32_t compress_heap_ptr(T ptr)
    requires std::is_pointer_v<T> || std::is_same_v<T, uint64_t>
  {
    return compress_ptr((uint64_t)ptr, jvm_args.h_heap_base, jvm_args.heap_compress_ptr_mode,
                        jvm_args.heap_compress_ptr_shift);
  }

  template <typename T>
  inline static uint32_t compress_metaspace_ptr(T ptr)
    requires std::is_pointer_v<T> || std::is_same_v<T, uint64_t>
  {
    return compress_ptr((uint64_t)ptr, jvm_args.h_compressed_class_space_base, jvm_args.metaspace_compress_ptr_mode,
                        jvm_args.metaspace_compress_ptr_shift);
  }
};

}  // namespace dpx::sd
