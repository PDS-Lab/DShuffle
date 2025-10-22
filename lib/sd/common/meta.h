#pragma once

#include "sd/common/class_info.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  class_info_t **class_infos_by_id;
  uint64_t class_infos_by_id_len;
  class_info_t **class_infos_by_klass;
  uint64_t class_infos_by_klass_len;
  void *class_infos_base;
  uint64_t class_infos_lim;
  char __reserved__[16];
} meta_idx_t;

#ifdef __cplusplus
}
#endif
