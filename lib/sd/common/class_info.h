#pragma once

#include "sd/common/basic_type.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  uint64_t object;
  uint64_t handle;
} obj_with_handle_t;

typedef struct field_info_t {
  class_id_t id;
  uint16_t offset;    // offset after header
  basic_type_t type;  // basic type
  uint16_t flag;      // reserved for flag or annotation
  uint8_t reserved;
  uintptr_t j_field_id;
} field_info_t;

// TODO add global reference for enum class
// TODO split the refrence type fileds from primitive type fileds

typedef struct class_info_t {
  void *klass;
  class_id_t id;
  uint16_t enum_ref_arr_off;
  uint32_t klass_cptr;
  uint16_t obj_size;    // actual size for object, including header
  uint8_t header_size;  // there will be padding, so we calculate by ourselves
  uint8_t n_non_static_field;
  uint8_t n_static_field;
  uint8_t dim;  // for array
  // uint8_t n_ref_field;
  // uint8_t n_pri_field;
  uint16_t sig_off;
  field_info_t fields[];
} class_info_t;

#define MAX_SIGNATURE_LENGTH 80

/* ... meta header ... */
/* ... object body ... */
/* ... meta footer ... */

typedef struct meta_header_t {
  uint64_t total_length;
  // uint32_t object_end_offset;
} meta_header_t;

// typedef struct meta_footer_t {
//   uint32_t n_type;
//   uint32_t n_instance;
//   char item[];
// } meta_footer_t;

// item: object item | array item
// object item: id cnt
// array item: id cnt len

#define OBJECT_DATA_OFFSET sizeof(meta_header_t)

// typedef struct allocator_idx_t {
//   uint32_t length;
//   class_id_t id;
//   uint16_t offset; // offset from base
// } alloc_idx_t;

// clang-format off
// | allocator 1 | allocator 2 | ... | allocator n | Index Header | idx 1 | idx 2 | ... | idx n |
// | <-                start_off                -> | <-             end_off                  -> |
// clang-format on
// typedef struct index_header_t {
//   uint32_t start_off; // start off
//   uint32_t end_off;   // end off
//   uint32_t len;       // total length
//   uint32_t n;         // n of idx
//   alloc_idx_t idx[];
// } index_header_t;

// typedef struct object_allocator_t {
//   uint32_t n;
//   uint32_t nxt;
//   obj_with_handle_t objs[];
// } object_allocator_t;

#ifdef __cplusplus
}
#endif
