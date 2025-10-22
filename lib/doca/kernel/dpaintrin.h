#pragma once

#include <stdint.h>

#define __dpa_global__ __attribute__((unused))
#define __dpa_rpc__ __attribute__((unused))

#define __dpa_thread_fence(MEMORY_SPACE, PRED_OP, SUCC_OP)
#define __dpa_thread_memory_fence(OP1, OP2) __dpa_thread_fence(__DPA_MEMORY, OP1, OP2)
#define __dpa_thread_outbox_fence(OP1, OP2) __dpa_thread_fence(__DPA_MMIO, OP1, OP2)
#define __dpa_thread_window_fence(OP1, OP2) __dpa_thread_fence(__DPA_MMIO, OP1, OP2)
#define __dpa_thread_system_fence() __dpa_thread_fence(__DPA_SYSTEM, __DPA_RW, __DPA_RW)
#define __dpa_thread_window_read_inv() __dpa_thread_fence(__DPA_MMIO, __DPA_R, __DPA_R)
#define __dpa_thread_window_writeback() __dpa_thread_fence(__DPA_MMIO, __DPA_W, __DPA_W)
#define __dpa_thread_memory_writeback() __dpa_thread_fence(__DPA_MEMORY, __DPA_W, __DPA_W)

#define __dpa_fxp_rcp(N) 0
#define __dpa_fxp_pow2(N) 0
#define __dpa_fxp_log2(N) 0
#define __dpa_data_ignore(ADDR) 0

#define __dpa_thread_cycles() 0
#define __dpa_thread_inst_ret() 0
#define __dpa_thread_time() 0
