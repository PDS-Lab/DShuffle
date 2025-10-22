#pragma once

#include <cassert>
#include <cstring>

#include "memory/memory_region.hxx"

namespace dpx {

class LocalBuffer : public MemoryRegion {
 public:
  LocalBuffer() = default;
  LocalBuffer(uint8_t *base, size_t len) : MemoryRegion(base, len) {}
  ~LocalBuffer() = default;

  // for zpp_bits inner traits
  using value_type = uint8_t;

  uint8_t &operator[](size_t i) {
    assert(i >= 0 && i < size());
    return data()[i];
  }
  uint8_t operator[](size_t i) const {
    assert(i >= 0 && i < size());
    return data()[i];
  }

  void reset() {
    if (empty()) {
      return;
    }
    memset(data(), 0, size());
  }
};

}  // namespace dpx
