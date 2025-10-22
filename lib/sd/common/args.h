#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define JVM_COMPRESS_PTR_MODE_RAW32 1
#define JVM_COMPRESS_PTR_MODE_ZERO_BASED 2
#define JVM_COMPRESS_PTR_MODE_NON_ZERO_BASED 3
#define JVM_COMPRESS_PTR_MODE_RAW64 4

typedef struct {
  uintptr_t d_ctx;
  uint64_t d_ctx_len;
  uintptr_t d_output;
  uint64_t d_output_len;
  uintptr_t h_output;
  uint32_t h_output_h;
} sd_thread_args_t __attribute__((aligned(8)));

typedef struct {
  uint64_t h_heap_base;
  uint64_t h_heap_size;
  uint64_t h_compressed_class_space_base;
  uint64_t h_compressed_class_space_size;
  uint32_t heap_mmap_h;
  uint32_t heap_compress_ptr_mode;
  uint32_t metaspace_compress_ptr_mode;
  uint32_t heap_compress_ptr_shift;
  uint32_t metaspace_compress_ptr_shift;
} jvm_args_t;

typedef struct {
  uintptr_t h_object;
} se_input_t;

typedef struct {
  uint64_t length;
  uintptr_t h_output;
} se_output_t;

#ifdef __cplusplus
}
#endif
