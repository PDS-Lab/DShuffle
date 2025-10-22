#pragma once

#include "native/offload.hxx"
#include "native/worker.hxx"

namespace dpx {

class DiskAgent : TransportWrapper<Backend::DOCA_Comch, MountRpc, UmountRpc> {
  using Base = TransportWrapper<Backend::DOCA_Comch, MountRpc, UmountRpc>;

 public:
  DiskAgent(doca::Device& dev, const ConnectionParam<Backend::DOCA_Comch>& param, const Config& trans_conf,
            std::string mount_point_, std::string output_device_, size_t max_n_partition_)
      : Base(dev, param, trans_conf),
        mount_point(mount_point_),
        output_device(output_device_),
        max_n_partition(max_n_partition_) {
    ch.establish_connections();
  }
  ~DiskAgent() { ch.terminate_connections(); }

  void mount_on_dpu() {
    INFO("trigger spill start");
    std::lock_guard l(mu);
    if (!mounted) {
      INFO("Already mounted on dpu");
      return;
    }
    int rc = umount(mount_point);
    INFO("trigger umount on host, rc: {}, errno: {}", rc, errno);
    if (rc != 0) {
      die("Fail to umount on host");
    }
    mounted = false;
    dpx::TransportGuard g(Base::t);
    rc = t.call<MountRpc>({.max_n_partition = max_n_partition}).get();
    if (rc != 0) {
      die("Fail to mount disk on DPU");
    }
    INFO("trigger mount on dpu, rc: {}", rc);
  }

  void umount_on_dpu() {
    std::lock_guard l(mu);
    if (mounted) {
      INFO("Already mounted on host");
      return;
    }
    dpx::TransportGuard g(Base::t);
    auto rc = t.call<UmountRpc>({.max_n_partition = max_n_partition}).get();
    if (rc != 0) {
      die("Fail to umount disk on DPU");
    }
    INFO("trigger umount on dpu, rc: {}", rc);
    rc = mount(output_device, mount_point);
    if (rc != 0) {
      die("Fail to mount on host");
    }
    INFO("trigger mount on host, rc: {}, errno: {}", rc, errno);
    mounted = true;
  }

 private:
  std::mutex mu;
  std::string mount_point;
  std::string output_device;
  size_t max_n_partition;
  bool mounted = true;
};

}  // namespace dpx
