#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// device args will copy to the header of tls
typedef struct {
  uint64_t tls_size;

  uint64_t notify_comp_h;
  // uint64_t async_ops_comp_h;
  // uint64_t async_ops_h;

  uint64_t cons_comp_h;
  uint64_t prod_comp_h;
  uint64_t cons_h;
  uint64_t prod_h;
  uint32_t dpa_cons_id;
  uint32_t dpa_prod_id;
  uint32_t cpu_cons_id;
  uint32_t cpu_prod_id;

  uint64_t d_ctx;

  // uint64_t rdma_h;
  // uint64_t rdma_comp_h;
  // uint32_t rdma_conn_id;

  // uint64_t passive;
  // uint32_t mmap_h;
  // uint32_t buf_arr_h;
  // uint64_t addr;
  // uint32_t length;

  // uint32_t ok;

  // uint64_t wake_se_h;
  // uint64_t exit_se_h;
} dpa_thread_args_t;

#define MSGQ_PAYLOAD_LENGTH 24

typedef struct {
  uint64_t id;
  char payload[MSGQ_PAYLOAD_LENGTH];
} payload_t;

#ifdef __cplusplus
}
#endif
