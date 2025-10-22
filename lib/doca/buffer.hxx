#pragma once

#include <doca_buf.h>
#include <doca_buf_inventory.h>
#include <doca_buf_pool.h>
#include <doca_mmap.h>

#include "doca/check.hxx"
#include "doca/device.hxx"
#include "memory/local_buffer.hxx"
#include "memory/naive_buffer.hxx"
#include "util/logger.hxx"
#include "util/upper_align.hxx"

namespace dpx {
namespace doca {

namespace rdma {
class Endpoint;
}

namespace comch {
class Endpoint;
}

/*
 *
 * doca_buf layout:
 *
 * head   -->            +-------------------+
 *                       | head room         |
 *                       |                   |
 *                       |                   |
 *                       |                   |
 *                       |                   |
 * data   -->            +-------------------+
 *                       | data room         |
 *                       |                   |
 *                       |                   |
 *                       |                   |
 *                       |                   |
 *                       |                   |
 * data + data_len -->   +-------------------+
 *                       | tail room         |
 *                       |                   |
 *                       |                   |
 *                       |                   |
 * head + len      -->   +-------------------+
 *
 */

class BorrowedBuffer : public LocalBuffer {
  friend class rdma::Endpoint;
  friend class comch::Endpoint;
  friend class Buffers;

 public:
  BorrowedBuffer(doca_buf *buf_, doca_mmap *within_) : buf(buf_), within(within_) {
    doca_check(doca_buf_get_data(buf, reinterpret_cast<void **>(&base)));
    doca_check(doca_buf_get_len(buf, &len));
    DEBUG("Borrowed Local BufferBase at {} with length {}", (void *)base, len);
  }
  ~BorrowedBuffer() = default;

  void reset() {
    doca_check(doca_buf_reset_data_len(buf));
    LocalBuffer::reset();
  }

 private:
  doca_buf *buf = nullptr;
  doca_mmap *within = nullptr;
};

class MappedRegion : public MemoryRegion {
 public:
  MappedRegion(Device &dev_, void *ptr, size_t len, uint32_t extra_perm = 0)
      : MemoryRegion(reinterpret_cast<uint8_t *>(ptr), len), dev(dev_) {
    DEBUG("MappedRegion at {} with length {}", (void *)base, len);
    doca_check(doca_mmap_create(&mmap));
    doca_check(doca_mmap_set_memrange(mmap, data(), size()));
    doca_check(doca_mmap_set_permissions(mmap, DOCA_ACCESS_FLAG_LOCAL_READ_WRITE | extra_perm));
    doca_check(doca_mmap_add_dev(mmap, dev.dev));
    doca_check(doca_mmap_start(mmap));
  }

  ~MappedRegion() {
    if (mmap != nullptr) {
      doca_check(doca_mmap_stop(mmap));
      doca_check(doca_mmap_destroy(mmap));
    }
  }

  doca_dpa_dev_mmap_t get_mmap_handle() {
    doca_dpa_dev_mmap_t handle = 0;
    doca_check(doca_mmap_dev_get_dpa_handle(mmap, dev.dev, &handle));
    return handle;
  }

  std::string export_pci(Device &dev) {
    if (pci_desc.empty()) {
      const void *desc = nullptr;
      size_t desc_len = 0;
      doca_check(doca_mmap_export_pci(mmap, dev.dev, &desc, &desc_len));
      pci_desc = std::string(reinterpret_cast<const char *>(desc), desc_len);
    }
    return pci_desc;
  }

  std::string export_rdma(Device &dev) {
    if (rdma_desc.empty()) {
      const void *desc = nullptr;
      size_t desc_len = 0;
      doca_check(doca_mmap_export_rdma(mmap, dev.dev, &desc, &desc_len));
      rdma_desc = std::string(reinterpret_cast<const char *>(desc), desc_len);
    }
    return rdma_desc;
  }

 private:
  Device &dev;
  doca_mmap *mmap = nullptr;
  std::string pci_desc;
  std::string rdma_desc;
};

class OwnedBuffer : public LocalBuffer {
  friend class Buffers;
  friend class rdma::Endpoint;
  friend class comch::Endpoint;

 public:
  // default permission: DOCA_ACCESS_FLAG_LOCAL_READ_WRITE
  OwnedBuffer(Device &dev, size_t len_, uint32_t extra_perm = 0) : OwnedBuffer(DeviceRefs{dev}, len_, extra_perm) {}
  OwnedBuffer(const DeviceRefs &devs, size_t len_, uint32_t extra_perm = 0)
      : LocalBuffer(new(std::align_val_t(64)) uint8_t[upper_align(len_, 64)], len_), aligned_len(upper_align(len, 64)) {
    doca_check(doca_mmap_create(&mmap));
    doca_check(doca_mmap_set_memrange(mmap, data(), aligned_len));
    doca_check(doca_mmap_set_permissions(mmap, DOCA_ACCESS_FLAG_LOCAL_READ_WRITE | extra_perm));
    for (Device &dev : devs) {
      doca_check(doca_mmap_add_dev(mmap, dev.dev));
    }
    doca_check(doca_mmap_start(mmap));
    DEBUG("Owned Local BufferBase at {} with length {}", (void *)base, len);
  }

  ~OwnedBuffer() {
    if (mmap != nullptr) {
      uint32_t num_bufs = 0;
      doca_check(doca_mmap_get_num_bufs(mmap, &num_bufs));
      if (num_bufs != 0) {
        CRITICAL("Still has {} bufs point to this buffer {}", num_bufs, (void *)data());
      }
      doca_check(doca_mmap_stop(mmap));
      doca_check(doca_mmap_destroy(mmap));
    }
    if (!empty()) {
      delete[] data();
    }
  }

  std::string export_pci(Device &dev) {
    if (pci_desc.empty()) {
      const void *desc = nullptr;
      size_t desc_len = 0;
      doca_check(doca_mmap_export_pci(mmap, dev.dev, &desc, &desc_len));
      pci_desc = std::string(reinterpret_cast<const char *>(desc), desc_len);
    }
    return pci_desc;
  }

  std::string export_rdma(Device &dev) {
    if (rdma_desc.empty()) {
      const void *desc = nullptr;
      size_t desc_len = 0;
      doca_check(doca_mmap_export_rdma(mmap, dev.dev, &desc, &desc_len));
      rdma_desc = std::string(reinterpret_cast<const char *>(desc), desc_len);
    }
    return rdma_desc;
  }

  doca_dpa_dev_mmap_t get_mmap_handle(Device &dev) {
    doca_dpa_dev_mmap_t handle = 0;
    doca_check(doca_mmap_dev_get_dpa_handle(mmap, dev.dev, &handle));
    return handle;
  }

  naive::BorrowedBuffer borrow_as_naive() { return naive::BorrowedBuffer(data(), size()); }

 private:
  doca_mmap *mmap = nullptr;
  size_t aligned_len = 0;
  std::string pci_desc;
  std::string rdma_desc;
};

class Buffers : public OwnedBuffer {
  friend class comch::Endpoint;

 public:
  using BufferType = BorrowedBuffer;

  Buffers(Device &dev, size_t n, size_t piece_len_, uint32_t extra_perm = 0)
      : Buffers(DeviceRefs{dev}, n, piece_len_, extra_perm) {}
  Buffers(const DeviceRefs &devs, size_t n, size_t piece_len_, uint32_t extra_perm = 0)
      : OwnedBuffer(devs, n * piece_len_, extra_perm), piece_len(piece_len_) {
    DEBUG("Buffers have {} elements with piece length {}", n, piece_len);
    doca_check(doca_buf_pool_create(n, piece_len, mmap, &pool));
    doca_check(doca_buf_pool_start(pool));
    for (auto i = 0uz; i < n; ++i) {
      doca_buf *buf = nullptr;
      doca_check(doca_buf_pool_buf_alloc(pool, &buf));
      handles.emplace_back(buf, mmap);
    }
  }

  ~Buffers() {
    for (auto &h : handles) {
      doca_check(doca_buf_dec_refcount(h.buf, nullptr));
    }
    if (pool != nullptr) {
      doca_check(doca_buf_pool_stop(pool));
      doca_check(doca_buf_pool_destroy(pool));
    }
  }

  size_t n_elements() const { return handles.size(); }
  size_t piece_size() const { return piece_len; }
  BufferType &operator[](size_t index) { return handles[index]; }
  const BufferType &operator[](size_t index) const { return handles[index]; }

 protected:
  doca_buf_pool *pool = nullptr;
  size_t piece_len = -1;
  std::vector<BufferType> handles;
};

}  // namespace doca

}  // namespace dpx