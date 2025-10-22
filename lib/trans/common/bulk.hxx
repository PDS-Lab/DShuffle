#pragma once

#include "doca/device.hxx"
#include "memory/memory_region.hxx"
#include "trans/common/defs.hxx"
#include "util/unreachable.hxx"

namespace dpx {

template <Backend b, typename BufferType>
  requires(b == Backend::DOCA_Comch || b == Backend::DOCA_RDMA)
RemoteBuffer export_remote_buffer(BufferType& buf, doca::Device& dev) {
  std::string desc;
  if constexpr (b == Backend::DOCA_Comch) {
    desc = buf.export_pci(dev);
  } else if constexpr (b == Backend::DOCA_RDMA) {
    desc = buf.export_rdma(dev);
  } else {
    static_unreachable;
  }
  return RemoteBuffer(buf, desc);
}

};  // namespace dpx
