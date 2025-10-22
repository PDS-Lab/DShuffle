// clang-format off
#include "doca/kernel/helper.h"
// clang-format on
#include <doca_dpa_dev_comch_msgq.h>

__dpa_rpc__ static uint64_t wakeup(doca_dpa_dev_comch_consumer_t cons_h) {
  LOG_INFO("wake up");
  doca_dpa_dev_comch_consumer_ack(cons_h, 1);
  return 0;
}
