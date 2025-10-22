#pragma once

#include <pthread.h>

#include "util/fatal.hxx"

namespace dpx {

inline void bind_core(size_t core_idx) {
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(core_idx, &cpuset);
  if (auto ec = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset); ec != 0) {
    die("Fail to set affinity for thread, errno: {}", errno);
  }
}

inline void set_max_priority() {
  sched_param param = {
      .sched_priority = sched_get_priority_max(SCHED_FIFO),
  };
  if (auto ec = pthread_setschedparam(pthread_self(), SCHED_FIFO, &param); ec != 0) {
    die("Fail to set sched priority for thread, errno: {}", errno);
  }
}

}  // namespace dpx
