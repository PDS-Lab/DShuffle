#pragma once

#include <cassert>

#include "memory/memory_region.hxx"
#include "util/logger.hxx"
#include "util/upper_align.hxx"

namespace dpx {

class SimpleAllocator {
  inline constexpr const static uint32_t alignment = sizeof(uint64_t);

 public:
  SimpleAllocator(MemoryRegion &mr_) : mr(mr_), pos(0), lim(mr.size()) {}
  // NOTICE: aligned sizeof(uint64_t)
  uintptr_t allocate(uint64_t size) {
    assert(size > 0);
    assert(pos < lim);
    auto p = pos;
    pos = upper_align(p + size, alignment);
    assert(pos % alignment == 0);
    assert(p < pos);
    assert(p + size <= pos);
    assert(p + size <= lim);
    auto res = mr.handle() + p;
    TRACE("allocate {:X}", res);
    return res;
  }

  uint64_t allocated() {
    assert(pos % alignment == 0);
    return pos;
  }
  uintptr_t handle() { return mr.handle(); }

  ~SimpleAllocator() = default;

 private:
  MemoryRegion &mr;
  uint64_t pos;
  uint64_t lim;
};

}  // namespace dpx
