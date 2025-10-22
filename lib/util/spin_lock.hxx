#pragma once

#include <atomic>

namespace dpx {

class alignas(64) SpinLock {
 public:
  SpinLock() = default;
  ~SpinLock() = default;

  void lock();
  bool try_lock();
  void unlock();

 private:
  std::atomic_bool b = false;
};

}  // namespace dpx
