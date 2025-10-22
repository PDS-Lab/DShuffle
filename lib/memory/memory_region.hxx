#pragma once

#include <bits/types/struct_iovec.h>

#include <cstdint>
#include <string>

#include "util/hex_dump.hxx"

namespace dpx {

inline static bool is_overlap(uintptr_t a, size_t a_len, uintptr_t b, size_t b_len) {
  return std::max(a, b) <= std::min(a + a_len, b + b_len);
}

inline static bool is_contain(uintptr_t a, size_t a_len, uintptr_t b, size_t b_len) {
  return (a <= b) && ((b + b_len) <= (a + a_len));
}

class MemoryRegion {
 public:
  MemoryRegion() = default;
  MemoryRegion(uintptr_t base_, size_t len_) : base(base_), len(len_) {}
  MemoryRegion(uint8_t *base_, size_t len_) : base(reinterpret_cast<uintptr_t>(base_)), len(len_) {}
  ~MemoryRegion() = default;

  uintptr_t handle() const { return base; }
  uint8_t *data() { return reinterpret_cast<uint8_t *>(base); }
  const uint8_t *data() const { return reinterpret_cast<uint8_t *>(base); }
  size_t size() const { return len; }

  bool empty() const { return base == 0 && len == 0; }

  bool within(const MemoryRegion &other) const { return !empty() && !other.empty() && other.contain(*this); }

  bool contain(const MemoryRegion &other) const {
    return !empty() && !other.empty() && is_contain(base, len, other.base, other.len);
  }

  bool overlap(const MemoryRegion &other) const {
    return !empty() && !other.empty() && is_overlap(base, len, other.base, other.len);
  }

  bool operator==(const MemoryRegion &other) const { return base == other.base && len == other.len; }

  operator Hexdump() const { return Hexdump(data(), size()); }
  operator std::string_view() const { return std::string_view(reinterpret_cast<const char *>(data()), size()); }
  operator std::span<uint8_t>() { return std::span<uint8_t>(data(), size()); }
  operator std::span<const uint8_t>() const { return std::span<const uint8_t>(data(), size()); }
  operator iovec() { return iovec{.iov_base = reinterpret_cast<void *>(data()), .iov_len = size()}; }

  constexpr static auto serialize(auto &archive, auto &self) { return archive(self.base, self.len); }

 protected:
  uintptr_t base = 0;
  size_t len = 0;
};

class RemoteBuffer : public MemoryRegion {
 public:
  RemoteBuffer() = default;
  RemoteBuffer(MemoryRegion &buf, std::string desc_) : MemoryRegion(buf.data(), buf.size()), desc(desc_) {}
  ~RemoteBuffer() = default;

  constexpr static auto serialize(auto &archive, auto &self) { return archive(self.base, self.len, self.desc); }

  std::string desc;
};

}  // namespace dpx
