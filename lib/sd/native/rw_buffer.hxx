#pragma once

#include <cstdint>
#include <cstring>

#include "memory/naive_buffer.hxx"
#include "util/upper_align.hxx"

namespace dpx {

struct RWBuffer {
  explicit RWBuffer(naive::BorrowedBuffer b_) : b(b_), off(0) {}

  ~RWBuffer() = default;

  template <typename T>
  void put_at(T value, size_t offset) {
    memcpy(b.data() + offset, &value, sizeof(T));
  }

  void put_at(const char *src, size_t length, size_t offset) { memcpy(b.data() + offset, src, length); }

  template <typename T>
  void put(T value) {
    memcpy(b.data() + off, &value, sizeof(T));
    off += sizeof(T);
  }

  void put(const void *src, size_t length) {
    memcpy(b.data() + off, src, length);
    off += length;
  }

  template <typename T>
  T get_at(size_t offset) const {
    T value = *(T *)(b.data() + offset);
    return value;
  }

  void get_at(char *dest, size_t length, size_t offset) const { memcpy(dest, b.data() + offset, length); }

  template <typename T>
  T peek(size_t offset) const {
    T value = *(T *)(b.data() + off + offset);
    return value;
  }

  template <typename T>
  T get() const {
    T value = *(T *)(b.data() + off);
    off += sizeof(T);
    return value;
  }

  void get(char *dest, size_t length) const {
    memcpy(dest, b.data() + off, length);
    off += length;
  }

  size_t offset() const { return off; }
  size_t limit() const { return b.size(); }

  void skip(size_t n) const { off += n; }
  void fill_next_align_8() {
    size_t len = upper_align(off, 8) - off;
    if (len == 0) {
      return;
    }
    memset(b.data() + off, 0, len);
    off += len;
  }
  void skip_next_align_8() const {
    size_t nxt_off = upper_align(off, 8);
    off = nxt_off;
  }

  uint8_t *raw() { return b.data() + off; }
  uint8_t *raw_at(size_t offset) { return b.data() + offset; }
  const uint8_t *raw() const { return b.data() + off; }
  const uint8_t *raw_at(size_t offset) const { return b.data() + offset; }

 private:
  naive::BorrowedBuffer b;
  mutable size_t off;
};

}  // namespace dpx
