#pragma once

#include <stdint.h>

#include "doca/kernel/mem.h"

#define prime 23
#define capacity 64
#define invalid_mark 0xFFFF
#define invalid_value_u32 0xFFFFFFFF
#define invalid_value_u64 0xFFFFFFFFFFFFFFFF

typedef struct {
  uint16_t n;

  uint16_t slots_k[prime];
  uint16_t next_k[capacity];

  struct {
    uint64_t k;
    uint64_t v;
  } s[capacity];
} map_t;

__forceinline uint16_t h(uint64_t x, uint16_t p) {
  // union {
  //   uint64_t x;
  //   struct {
  //     uint16_t w1;
  //     uint16_t w2;
  //     uint16_t w3;
  //     uint16_t w4;
  //   } w;
  // } q = {.x = x};
  // uint16_t hh = (q.w.w1 + q.w.w2 + q.w.w3 + q.w.w4) % p;
  return x % p;
}

__forceinline void init_map(map_t *m) {
  d_memset(m, -1, sizeof(map_t));
  m->n = 0;
}

__forceinline void map_add(map_t *m, uint64_t k, uint64_t v) {
  uint16_t n = m->n++;
  uint16_t nk = h(k, prime);
  m->next_k[n] = m->slots_k[nk];
  m->slots_k[nk] = n;
  m->s[n].k = k;
  m->s[n].v = v;
}

__forceinline uint64_t map_lookup(map_t *m, uint64_t k) {
  uint16_t p = m->slots_k[h(k, prime)];
  while (p != invalid_mark) {
    if (m->s[p].k == k) {
      return m->s[p].v;
    }
    p = m->next_k[p];
  }
  return invalid_value_u64;
}

__forceinline void *map_lookup_ref(map_t *m, uint64_t k) {
  uint16_t p = m->slots_k[h(k, prime)];
  while (p != invalid_mark) {
    if (m->s[p].k == k) {
      return &m->s[p].v;
    }
    p = m->next_k[p];
  }
  return 0;
}

__forceinline int map_insert(map_t *m, uint64_t k, uint64_t v) {
  uint16_t p = invalid_mark;
  uint16_t nxt = m->slots_k[h(k, prime)];
  while (1) {
    if (nxt == invalid_mark) {
      map_add(m, k, v);
      return 1;
    }
    p = nxt;
    if (m->s[p].k == k) {
      return 0;
    }
    nxt = m->next_k[p];
  }
}
