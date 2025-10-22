#define _GNU_SOURCE

#include <bits/types.h>
#include <dlfcn.h>
#include <stdio.h>

#define _SYS_MMAN_H
#include <bits/mman-linux.h>
#include <bits/mman-map-flags-generic.h>
#undef _SYS_MMAN_H

static void parse_flags(int flags) {
#define CHECK_FLAG(flag, F)  \
  if (flags & F) {           \
    fprintf(stderr, #F " "); \
  }
  CHECK_FLAG(flags, MAP_SHARED);
  CHECK_FLAG(flags, MAP_PRIVATE);
  CHECK_FLAG(flags, MAP_FIXED);
  CHECK_FLAG(flags, MAP_ANONYMOUS);
  CHECK_FLAG(flags, MAP_HUGETLB);
  CHECK_FLAG(flags, MAP_NORESERVE);
  fprintf(stderr, "\n");
}

void *mmap(void *addr, size_t, int, int, int, __off_t) __attribute__((weak, alias("hook_mmap")));

typedef void *(*mmap_ptr_t)(void *, size_t, int, int, int, __off_t);

void *hook_mmap(void *addr, size_t length, int prot, int flags, int fd, __off_t offset) {
  mmap_ptr_t original_mmap = (mmap_ptr_t)dlsym(RTLD_NEXT, "mmap");
  void *allocated_addr = original_mmap(addr, length, prot, flags, fd, offset);
  fprintf(stderr,
          "addr %p length %lu prot %d flags %x fd %d offset %lu allocated "
          "addr %p\n",
          addr, length, prot, flags, fd, offset, allocated_addr);
  parse_flags(flags);
  return allocated_addr;
}
