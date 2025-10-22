#include "util/spin_lock.hxx"

#if defined(__x86_64__)
#include <emmintrin.h>
#endif

namespace dpx {

namespace {

#if defined(__x86_64__)

inline auto cpu_relax() -> void { _mm_pause(); }
#elif defined(__aarch64__)
inline auto cpu_relax() -> void { asm volatile("yield" ::: "memory"); }
#else
inline auto cpu_relax() -> void {}
#endif

}  // namespace

void SpinLock::lock() {
  while (true) {
    if (!b.exchange(true, std::memory_order_acquire)) {
      return;
    }
    while (b.load(std::memory_order_relaxed)) {
      cpu_relax();
    }
  }
}

bool SpinLock::try_lock() { return !b.load(std::memory_order_relaxed) && !b.exchange(true, std::memory_order_acquire); }

void SpinLock::unlock() { b.store(false, std::memory_order_release); }

}  // namespace dpx