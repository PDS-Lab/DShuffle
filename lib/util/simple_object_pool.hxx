#pragma once

#include <deque>
#include <mutex>

#ifndef NDEBUG
#include <unordered_set>
#endif

#include "util/noncopyable.hxx"
#include "util/nonmovable.hxx"

namespace dpx {

template <typename T>
class SimpleObjectPool : Noncopyable, Nonmovable {
 public:
  explicit SimpleObjectPool(size_t max_size_) : max_size(max_size_) {}
  ~SimpleObjectPool() {
    while (!q.empty()) {
      delete q.front();
      q.pop_front();
    }
  }

  size_t size() const { return q.size(); }
  size_t capacity() const { return max_size; }

  // not safe, only for init
  void add(T* obj) { q.push_back(obj); }

  T* acquire() {
    std::lock_guard l(m);
    if (q.empty()) {
      return nullptr;
    } else {
      auto o = q.front();
#ifndef NDEBUG
      allocated.insert(o);
#endif
      q.pop_front();
      return o;
    }
  }

  void release(T* obj) {
    std::lock_guard l(m);
#ifndef NDEBUG
    auto iter = allocated.find(obj);
    assert(iter != allocated.end());
    allocated.erase(iter);
#endif
    q.push_back(obj);
  }

 private:
  std::mutex m;
  std::deque<T*> q;
#ifndef NDEBUG
  std::unordered_set<T*> allocated;
#endif
  size_t max_size;
};

}  // namespace dpx