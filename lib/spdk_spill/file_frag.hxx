#pragma once

#include <assert.h>
#include <fcntl.h>
#include <linux/fiemap.h>
#include <linux/fs.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <cstring>
#include <string>
#include <vector>

#include "util/fatal.hxx"
#include "util/literal.hxx"
#include "util/logger.hxx"

using namespace dpx::literal;

namespace dpx::spill {

struct frag_t {
  size_t length;  // in bytes
  size_t logic_offset;
  size_t physical_offset;

  size_t start_lba;
  size_t lba_count;
};

using FileFragments = std::vector<frag_t>;

inline FileFragments get_file_fragments(int fd, size_t lba_size) {
  FileFragments frags;

  char buf[8_KB];
  fiemap* p = reinterpret_cast<fiemap*>(buf);
  fiemap_extent* exts_p = &p->fm_extents[0];
  uint32_t count = (sizeof(buf) - sizeof(*p)) / sizeof(fiemap_extent);
  memset(p, 0, sizeof(fiemap));

  while (true) {
    p->fm_length = FIEMAP_MAX_OFFSET;
    p->fm_flags = FIEMAP_FLAG_SYNC;
    p->fm_extent_count = count;

    auto rc = ioctl(fd, FS_IOC_FIEMAP, reinterpret_cast<intptr_t>(p));
    if (rc < 0) {
      die("ioctl failure, errno: {}", errno);
    }

    if (p->fm_mapped_extents == 0) {
      break;
    }

    bool eof = false;
    auto exts = std::span<fiemap_extent>(exts_p, p->fm_mapped_extents);
    for (auto& e : exts) {
      if (e.fe_flags & FIEMAP_EXTENT_NOT_ALIGNED) {
        WARN("File extent is not aligned to {}", lba_size);
      }
      assert(e.fe_length % lba_size == 0);
      assert(e.fe_physical % lba_size == 0);
      frags.emplace_back(frag_t{
          .length = e.fe_length,
          .logic_offset = e.fe_logical,
          .physical_offset = e.fe_physical,
          .start_lba = e.fe_physical / lba_size,
          .lba_count = e.fe_length / lba_size,
      });
      if (e.fe_flags & FIEMAP_EXTENT_LAST) {
        eof = true;
      }
    }
    p->fm_start = (exts.back().fe_logical + exts.back().fe_length);
    if (eof) {
      break;
    }
  }
  return frags;
}

inline FileFragments get_file_fragments(std::string path, size_t lba_size) {
  int fd = -1;
  if (fd = open(path.c_str(), O_RDONLY); fd < 0) {
    die("Fail to open {}, errno: {}", path, errno);
  }
  FileFragments frags = get_file_fragments(fd, lba_size);
  close(fd);
  return frags;
}

}  // namespace dpx::spill

// int main() {
//   auto frags = dpx::spill::get_file_fragments("/home/lsc/hadoop-bak/spark-core_2.11-2.4.3.jar", 512);

//   for (auto& f : frags) {
//     printf("%lu, %lu, %lu, %lu, %lu\n", f.length, f.logic_offset, f.physical_offset, f.start_lba, f.lba_count);
//   }
//   return 0;
// }