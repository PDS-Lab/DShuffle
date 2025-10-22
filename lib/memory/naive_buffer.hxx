#pragma once

#include "memory/local_buffer.hxx"
#include "util/logger.hxx"
#include "util/noncopyable.hxx"
#include "util/nonmovable.hxx"
#include "util/upper_align.hxx"

namespace dpx::naive {

class BorrowedBuffer : public LocalBuffer {
 public:
  BorrowedBuffer(uint8_t *base, size_t len) : LocalBuffer(base, len) {
    TRACE("Borrowed Local BufferBase at {} with length {}", (void *)base, len);
  }
  ~BorrowedBuffer() = default;
};

class OwnedBuffer : public LocalBuffer, Noncopyable, Nonmovable {
 public:
  OwnedBuffer(size_t size, size_t align)
      : LocalBuffer(new(std::align_val_t(align)) uint8_t[upper_align(size, align)], upper_align(size, align)) {
    TRACE("Owned Local BufferBase at {} with length {}", (void *)base, len);
  }
  explicit OwnedBuffer(size_t size) : LocalBuffer(new uint8_t[size], size) {
    TRACE("Owned Local BufferBase at {} with length {}", (void *)base, len);
  }
  ~OwnedBuffer() {
    if (!empty()) {
      delete[] data();
    }
  }

  BorrowedBuffer borrow() { return BorrowedBuffer(data(), size()); }
};

class Buffers : public OwnedBuffer {
 public:
  using BufferType = BorrowedBuffer;

  Buffers(size_t n, size_t piece_len_) : OwnedBuffer(piece_len_ * n), piece_len(piece_len_) {
    TRACE("Buffers have {} elements with piece length {}", n, piece_len);
    for (auto p = data(); p < data() + size(); p += piece_len) {
      handles.emplace_back(p, piece_len);
    }
  }

  ~Buffers() = default;

  size_t n_elements() const { return handles.size(); }
  size_t piece_size() const { return piece_len; }
  BufferType &operator[](size_t index) { return handles[index]; }
  const BufferType &operator[](size_t index) const { return handles[index]; }

 protected:
  size_t piece_len = -1;
  std::vector<BufferType> handles;
};

}  // namespace dpx::naive
