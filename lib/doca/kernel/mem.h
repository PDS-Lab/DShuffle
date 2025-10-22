#pragma once
// clang-format off
#include "doca/kernel/helper.h"
// clang-format on

#include <doca_dpa_dev.h>
#include <stdint.h>

__forceinline void *d_memcpy_u64(ptr_t d, ptr_t s, uint64_t bytes) {
  uint64_t u64_len = bytes / sizeof(uint64_t);
  uint64_t n = u64_len / 8;

  LOG_DBG("u64_len %ld, n %ld", u64_len, n);

  switch (u64_len % 8) {
    case 0:
      break;
    case 7:
      *d.u64++ = *s.u64++;
    case 6:
      *d.u64++ = *s.u64++;
    case 5:
      *d.u64++ = *s.u64++;
    case 4:
      *d.u64++ = *s.u64++;
    case 3:
      *d.u64++ = *s.u64++;
    case 2:
      *d.u64++ = *s.u64++;
    case 1:
      *d.u64++ = *s.u64++;
  }

  while (n-- > 0) {
    *d.u64++ = *s.u64++;
    *d.u64++ = *s.u64++;
    *d.u64++ = *s.u64++;
    *d.u64++ = *s.u64++;
    *d.u64++ = *s.u64++;
    *d.u64++ = *s.u64++;
    *d.u64++ = *s.u64++;
    *d.u64++ = *s.u64++;
  }

  switch (bytes % sizeof(uint64_t)) {
    case 0:
      break;
    case 7:
      *d.u8++ = *s.u8++;
    case 6:
      *d.u8++ = *s.u8++;
    case 5:
      *d.u8++ = *s.u8++;
    case 4:
      *d.u8++ = *s.u8++;
    case 3:
      *d.u8++ = *s.u8++;
    case 2:
      *d.u8++ = *s.u8++;
    case 1:
      *d.u8++ = *s.u8++;
  }

  return s.raw;
}

__forceinline void *d_memcpy_u32(ptr_t d, ptr_t s, uint64_t bytes) {
  uint64_t u32_len = bytes / sizeof(uint32_t);
  uint64_t n = u32_len / 8;

  LOG_DBG("u32_len %ld, n %ld", u32_len, n);

  switch (u32_len % 8) {
    case 0:
      break;
    case 7:
      *d.u32++ = *s.u32++;
    case 6:
      *d.u32++ = *s.u32++;
    case 5:
      *d.u32++ = *s.u32++;
    case 4:
      *d.u32++ = *s.u32++;
    case 3:
      *d.u32++ = *s.u32++;
    case 2:
      *d.u32++ = *s.u32++;
    case 1:
      *d.u32++ = *s.u32++;
  }

  while (n-- > 0) {
    *d.u32++ = *s.u32++;
    *d.u32++ = *s.u32++;
    *d.u32++ = *s.u32++;
    *d.u32++ = *s.u32++;
    *d.u32++ = *s.u32++;
    *d.u32++ = *s.u32++;
    *d.u32++ = *s.u32++;
    *d.u32++ = *s.u32++;
  }

  switch (bytes % sizeof(uint32_t)) {
    case 0:
      break;
    case 3:
      *d.u8++ = *s.u8++;
    case 2:
      *d.u8++ = *s.u8++;
    case 1:
      *d.u8++ = *s.u8++;
  }

  return s.raw;
}

__forceinline void *d_memset_u32(ptr_t p, uint8_t v, uint64_t len) {
  uint64_t u32_len = len / sizeof(uint32_t);
  uint64_t n = u32_len / 8;
  uint32_t u = v;
  u = u | (u << 8);
  u = u | (u << 16);

  LOG_DBG("u32_len %ld, n %ld", u32_len, n);

  switch (u32_len % 8) {
    case 0:
      break;
    case 7:
      *p.u32++ = u;
    case 6:
      *p.u32++ = u;
    case 5:
      *p.u32++ = u;
    case 4:
      *p.u32++ = u;
    case 3:
      *p.u32++ = u;
    case 2:
      *p.u32++ = u;
    case 1:
      *p.u32++ = u;
  }

  while (n-- > 0) {
    *p.u32++ = u;
    *p.u32++ = u;
    *p.u32++ = u;
    *p.u32++ = u;
    *p.u32++ = u;
    *p.u32++ = u;
    *p.u32++ = u;
    *p.u32++ = u;
  }

  switch (len % sizeof(uint32_t)) {
    case 0:
      break;
    case 3:
      *p.u8++ = v;
    case 2:
      *p.u8++ = v;
    case 1:
      *p.u8++ = v;
  }
  return p.raw;
}

__forceinline void *d_memset_u64(ptr_t p, uint8_t v, uint64_t len) {
  uint64_t u64_len = len / sizeof(uint64_t);
  uint64_t n = u64_len / 8;
  uint64_t u = v;
  u = u | (u << 8);
  u = u | (u << 16);
  u = u | (u << 32);

  LOG_DBG("u64_len %ld, n %ld", u64_len, n);

  switch (u64_len % 8) {
    case 0:
      break;
    case 7:
      *p.u64++ = u;
    case 6:
      *p.u64++ = u;
    case 5:
      *p.u64++ = u;
    case 4:
      *p.u64++ = u;
    case 3:
      *p.u64++ = u;
    case 2:
      *p.u64++ = u;
    case 1:
      *p.u64++ = u;
  }

  while (n-- > 0) {
    *p.u64++ = u;
    *p.u64++ = u;
    *p.u64++ = u;
    *p.u64++ = u;
    *p.u64++ = u;
    *p.u64++ = u;
    *p.u64++ = u;
    *p.u64++ = u;
  }

  switch (len % sizeof(uint64_t)) {
    case 0:
      break;
    case 7:
      *p.u8++ = v;
    case 6:
      *p.u8++ = v;
    case 5:
      *p.u8++ = v;
    case 4:
      *p.u8++ = v;
    case 3:
      *p.u8++ = v;
    case 2:
      *p.u8++ = v;
    case 1:
      *p.u8++ = v;
  }

  return p.raw;
}

// WARN: dest shall align to 8
__forceinline void *d_memset(void *d_dest, uint8_t val, uint64_t len) {
  ptr_t p;
  p.raw = d_dest;

  LOG_DBG("d_memset start %lx %lu", p.p, len);

  ptr_t e;
  switch (p.p % sizeof(uint64_t)) {
    case 0:
      e.raw = d_memset_u64(p, val, len);
      break;
    case 4:
      e.raw = d_memset_u32(p, val, len);
      break;
    default:
      UNREACHABLE_CRIT;
  }

  LOG_DBG("d_memset done %lx %lu", e.p, e.p - p.p);

  return e.raw;
}

// WARN: dest src shall align to 8
// WARN: dest src cannot overlap
__forceinline void *d_memcpy(void *d_dest, const void *d_src, uint64_t len) {
  ptr_t d;
  ptr_t s;

  d.raw = d_dest;
  s.raw = (void *)d_src;

  LOG_DBG("d_memcpy start %lx %lx %lu", d.p, s.p, len);
  if (len == 0) {
    return (void *)d_src;
  }
  uint32_t align_d = d.p % sizeof(uint64_t);
  uint32_t align_s = s.p % sizeof(uint64_t);
  // LOG_DBG("d_memcpy start %d %d", align_d, align_s);

  ptr_t e;

  switch (align_d | align_s) {
    case 0:
      e.raw = d_memcpy_u64(d, s, len);
      break;
    case 4:
      e.raw = d_memcpy_u32(d, s, len);
      break;
    default:
      UNREACHABLE_CRIT;
  }

  LOG_DBG("d_memcpy done %lx %lu", e.p, e.p - s.p);

  return e.raw;
}
