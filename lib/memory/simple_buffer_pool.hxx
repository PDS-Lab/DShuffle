#pragma once

#include <cassert>
#include <functional>
#include <list>
#include <optional>

#include "util/noncopyable.hxx"
#include "util/nonmovable.hxx"
#include "util/spin_lock.hxx"

namespace dpx {

template <typename T>
concept BuffersType = requires(T bs) {
  typename T::BufferType;
  { bs.n_elements() };
  { bs.piece_size() };
  { bs.data() };
  { bs.size() };
  { bs[0] };
};

template <BuffersType Buffers>
class BufferPool : Noncopyable, Nonmovable {
 public:
  using BufferType = typename Buffers::BufferType;
  using BufferTypeRef = std::reference_wrapper<BufferType>;
  template <typename... Args>
  BufferPool(Args &&...args) : bs(args...) {
    for (auto i = 0uz; i < bs.n_elements(); ++i) {
      bs[i].reset();
      q.emplace_back(std::ref(bs[i]));
    }
  }
  ~BufferPool() = default;

  void enable_mt() { mt = true; }

  std::optional<BufferTypeRef> acquire_one() {
    if (mt) {
      std::lock_guard g(l);
      return _acquire_one();
    } else {
      return _acquire_one();
    }
  }
  void release_one(BufferType &buf) {
    if (mt) {
      std::lock_guard g(l);
      return _release_one(buf);
    } else {
      return _release_one(buf);
    }
  }

  Buffers &buffers() { return bs; }
  const Buffers &buffers() const { return bs; }

 private:
  std::optional<BufferTypeRef> _acquire_one() {
    if (q.empty()) {
      return {};
    }
    auto buf = q.front();
    q.pop_front();
    return std::make_optional(buf);
  }
  void _release_one(BufferType &buffer) {
    assert((buffer.size() == bs.piece_size() && bs.data() <= buffer.data() &&
            buffer.data() + buffer.size() <= bs.data() + bs.size()));
    buffer.reset();
    q.emplace_back(buffer);
  }

  using BufferQ = std::list<BufferTypeRef>;

  SpinLock l;
  bool mt = false;
  BufferQ q;
  Buffers bs;
};

}  // namespace dpx
