#pragma once

#include <doca_buf_array.h>
#include <doca_mmap.h>

#include "doca/check.hxx"
#include "doca/device.hxx"
#include "memory/memory_region.hxx"
#include "util/logger.hxx"

namespace dpx::doca {

class DPABuffer : public MemoryRegion {
 public:
  DPABuffer(Device &dev_, size_t size_) : MemoryRegion(alloc_dpa_mem(dev_.dpa, size_), size_), dev(dev_) {
    DEBUG("DPA Heap Buffer at {} with length {}", (void *)base, len);
    device_memset('?');
  }

  ~DPABuffer() {
    if (buf_arr != nullptr) {
      doca_check(doca_buf_arr_stop(buf_arr));
      doca_check(doca_buf_arr_destroy(buf_arr));
    }
    if (mmap != nullptr) {
      doca_check(doca_mmap_stop(mmap));
      doca_check(doca_mmap_destroy(mmap));
    }
    doca_check(doca_dpa_mem_free(dev.dpa, base));
  }

  void device_memset(char c) { doca_check(doca_dpa_memset(dev.dpa, base, c, len)); }

  void copy_to_device(const void *src, size_t length, size_t offset) {
    assert(offset + length <= len);
    TRACE("copy to device: dst: {:X} length: {} src: {}", base + offset, length, src);
    doca_check(doca_dpa_h2d_memcpy(dev.dpa, base + offset, const_cast<void *>(src), length));
  }

  void copy_from_device(void *dst, size_t length, size_t offset) {
    assert(offset + length <= len);
    TRACE("copy from device: src: {:X} length: {} dst: {}", base + offset, length, dst);
    doca_check(doca_dpa_d2h_memcpy(dev.dpa, dst, base + offset, length));
  }

  doca_dpa_dev_mmap_t get_dpa_handle() {
    lazy_init_mmap();
    doca_dpa_dev_mmap_t handle;
    doca_check(doca_mmap_dev_get_dpa_handle(mmap, dev.dev, &handle));
    return handle;
  }

  std::string get_rdma_desc() {
    lazy_init_mmap();
    if (rdma_desc.empty()) {
      const void *desc = nullptr;
      size_t desc_len = 0;
      doca_check(doca_mmap_export_rdma(mmap, dev.dev, &desc, &desc_len));
      rdma_desc = std::string(reinterpret_cast<const char *>(desc), desc_len);
    }
    return rdma_desc;
  }

  doca_dpa_dev_buf_arr_t as_buf_arr(size_t num_elem) {
    assert(len % num_elem == 0);
    lazy_init_mmap();
    auto elem_size = len / num_elem;
    doca_check(doca_buf_arr_create(num_elem, &buf_arr));
    doca_check(doca_buf_arr_set_params(buf_arr, mmap, elem_size, 0));
    doca_check(doca_buf_arr_set_target_dpa(buf_arr, dev.dpa));
    doca_check(doca_buf_arr_start(buf_arr));
    doca_dpa_dev_buf_arr_t handle;
    doca_check(doca_buf_arr_get_dpa_handle(buf_arr, &handle));
    return handle;
  }

 private:
  void lazy_init_mmap() {
    if (mmap == nullptr) {
      doca_check(doca_mmap_create(&mmap));
      doca_check(doca_mmap_set_permissions(mmap, DOCA_ACCESS_FLAG_LOCAL_READ_WRITE | DOCA_ACCESS_FLAG_RDMA_READ |
                                                     DOCA_ACCESS_FLAG_RDMA_WRITE | DOCA_ACCESS_FLAG_RDMA_ATOMIC));
      doca_check(doca_mmap_set_dpa_memrange(mmap, dev.dpa, base, len));
      doca_check(doca_mmap_start(mmap));
    }
  }

  static uintptr_t alloc_dpa_mem(doca_dpa *dpa, uint32_t size) {
    doca_dpa_dev_uintptr_t addr = 0;
    doca_check(doca_dpa_mem_alloc(dpa, size, &addr));
    TRACE("allocate dpa memory at {:X} with length {}", addr, size);
    return addr;
  }

  Device &dev;
  doca_mmap *mmap = nullptr;
  doca_buf_arr *buf_arr = nullptr;
  std::string rdma_desc;
};

}  // namespace dpx::doca
