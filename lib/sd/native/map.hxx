#pragma once

#include <cassert>
#include <cstdint>
#include <utility>

#include "memory/naive_buffer.hxx"

namespace dpx::sd {

inline uint16_t h(uint64_t x, uint16_t p) {
  union {
    uint64_t x;
    struct {
      uint16_t w1;
      uint16_t w2;
      uint16_t w3;
      uint16_t w4;
    } w;
  } q = {.x = x};
  uint16_t hh = (q.w.w1 + q.w.w2 + q.w.w3 + q.w.w4) % p;
  return hh;
}

enum class MapMode {
  Normal,
  Bidirect,
};

template <MapMode mode, typename K, typename V, uint16_t capacity, uint16_t prime>
struct MapImpl {
  static_assert(sizeof(K) <= sizeof(uint64_t), "?");
  static_assert(sizeof(V) <= sizeof(uint64_t), "?");

  constexpr const static uint16_t invalid_mark = -1;
  constexpr const static uint64_t invalid_value = -1;

  struct bidirect_map_t {
    uint16_t n;

    uint16_t slots_k[prime];
    uint16_t next_k[capacity];

    uint16_t slots_v[prime];
    uint16_t next_v[capacity];

    struct {
      uint64_t k;
      uint64_t v;
    } s[capacity];
  };

  struct normal_map_t {
    uint16_t n;

    uint16_t slots_k[prime];
    uint16_t next_k[capacity];

    struct {
      uint64_t k;
      uint64_t v;
    } s[capacity];
  };

  using map_t = std::conditional_t<mode == MapMode::Normal, normal_map_t, bidirect_map_t>;

  naive::BorrowedBuffer b;
  map_t *m = nullptr;
  bool hold = false;

 public:
  uint16_t size() const { return m->n; }
  uint32_t underlying_size() const { return sizeof(map_t); }
  const char *underlying() const { return (const char *)m; }
  explicit MapImpl(naive::BorrowedBuffer &b_) : b(b_), m((map_t *)b.data()), hold(false) {
    assert(b.size() >= sizeof(map_t));
    init();
  }
  MapImpl() : m(new map_t), hold(true) { init(); }
  ~MapImpl() {
    if (hold) {
      delete m;
    }
  }

 private:
  void init() {
    memset(m, -1, underlying_size());
    m->n = 0;
  }
  void add(uint64_t k, uint64_t v) {
    uint16_t n = m->n++;
    assert(n < capacity);
    uint16_t nk = h(k, prime);
    m->next_k[n] = std::exchange(m->slots_k[nk], n);
    if constexpr (mode == MapMode::Bidirect) {
      uint16_t nv = h(v, prime);
      m->next_v[n] = std::exchange(m->slots_v[nv], n);
    }
    m->s[n].k = k;
    m->s[n].v = v;
  }
  uint64_t _lookup_v(uint64_t k) const {
    uint16_t p = m->slots_k[h(k, prime)];
    while (p != invalid_mark) {
      if (m->s[p].k == k) {
        return m->s[p].v;
      }
      p = m->next_k[p];
    }
    return invalid_value;
  }
  uint64_t _lookup_k(uint64_t v) const
    requires(mode == MapMode::Bidirect)
  {
    uint16_t p = m->slots_v[h(v, prime)];
    while (p != invalid_mark) {
      if (m->s[p].v == v) {
        return m->s[p].k;
      }
      p = m->next_v[p];
    }
    return invalid_value;
  }
  bool _insert_kv(uint64_t k, uint64_t v) {
    uint16_t p = invalid_mark;
    uint16_t nxt = m->slots_k[h(k, prime)];
    while (true) {
      if (nxt == invalid_mark) {
        add(k, v);
        return true;
      }
      p = nxt;
      if (m->s[p].k == k) {
        return false;
      }
      nxt = m->next_k[p];
    }
  }
  bool _insert_vk(uint64_t v, uint64_t k)
    requires(mode == MapMode::Bidirect)
  {
    uint16_t p = invalid_mark;
    uint16_t nxt = m->slots_v[h(v, prime)];
    while (true) {
      if (nxt == invalid_mark) {
        add(k, v);
        return true;
      }
      p = nxt;
      if (m->s[p].v == v) {
        return false;
      }
      nxt = m->next_v[p];
    }
  }
  uint64_t *_lookup_v_ref(uint64_t k)
    requires(mode == MapMode::Normal)
  {
    uint16_t p = m->slots_k[h(k, prime)];
    while (p != invalid_mark) {
      if (m->s[p].k == k) {
        return &m->s[p].v;
      }
      p = m->next_k[p];
    }
    return nullptr;
  }

 public:
  bool insert_kv(K k, V v) { return _insert_kv((uint64_t)k, (uint64_t)v); }
  bool insert_vk(K v, V k)
    requires(mode == MapMode::Bidirect)
  {
    return _insert_vk((uint64_t)v, (uint64_t)k);
  }
  std::pair<V, bool> lookup_v(K k) const {
    uint64_t v = _lookup_v((uint64_t)k);
    return {(V)v, v != invalid_value};
  }

  std::pair<K, bool> lookup_k(V v) const
    requires(mode == MapMode::Bidirect)
  {
    uint64_t k = _lookup_k((uint64_t)v);
    return {(K)k, k != invalid_value};
  }

  std::pair<V *, bool> lookup_v_ref(K k)
    requires(mode == MapMode::Normal)
  {
    uint64_t *p = _lookup_v_ref((uint64_t)k);
    return {(V *)p, p != nullptr};
  }

  template <typename Fn>
  void for_each(Fn &&fn) const {
    for (uint16_t i = 0; i < m->n; i++) {
      fn((K)m->s[i].k, (V)m->s[i].v);
    }
  }
};

template <typename K, typename V, uint16_t capacity, uint16_t prime>
using BiMap = MapImpl<MapMode::Bidirect, K, V, capacity, prime>;

template <typename K, typename V, uint16_t capacity, uint16_t prime>
using Map = MapImpl<MapMode::Normal, K, V, capacity, prime>;

}  // namespace dpx::sd
