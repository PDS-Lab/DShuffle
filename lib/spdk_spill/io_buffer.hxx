#pragma once

#include <spdk/env.h>

#include "memory/memory_region.hxx"
#include "util/logger.hxx"
#include "util/noncopyable.hxx"
#include "util/nonmovable.hxx"
#include "util/upper_align.hxx"

namespace dpx::spill {

class IOBuffer : public MemoryRegion, Noncopyable, Nonmovable {
 public:
  IOBuffer(size_t size, size_t align)
      : MemoryRegion(reinterpret_cast<uint8_t *>(spdk_dma_malloc(upper_align(size, align), align, nullptr)),
                     upper_align(size, align)) {
    DEBUG("IOBuffer base at {} with length {}", (void *)base, len);
  }
  ~IOBuffer() {
    if (!empty()) {
      spdk_dma_free(data());
    }
  }
};

}  // namespace dpx::spill