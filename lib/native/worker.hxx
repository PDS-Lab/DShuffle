#pragma once

#include "trans/transport.hxx"
#include "util/bind_core.hxx"

namespace dpx {

template <Backend b, Rpc... rpcs>
struct TransportWrapper : Noncopyable, Nonmovable {
  TransportWrapper(doca::Device& dev, const ConnectionParam<b>& param, const Config& trans_conf)
      : bulk_dev(dev), ch(dev, param), t(dev, ch, trans_conf) {}

  TransportWrapper(doca::Device& ch_dev, doca::Device& dma_dev, const ConnectionParam<b>& param,
                   const Config& trans_conf)
    requires(b == Backend::DOCA_Comch)
      : bulk_dev(dma_dev), ch(ch_dev, param), t(ch_dev, dma_dev, ch, trans_conf) {}

  doca::Device& bulk_dev;
  ConnectionHolder<b> ch;
  Transport<b, rpcs...> t;
};

template <Backend b, Rpc... rpcs>
class Worker : public TransportWrapper<b, rpcs...> {
  using Base = TransportWrapper<b, rpcs...>;

 public:
  Worker(doca::Device& dev, const ConnectionParam<b>& param, const Config& trans_conf, std::atomic_bool& running_)
      : Base(dev, param, trans_conf), running(running_) {}
  Worker(doca::Device& ch_dev, doca::Device& dma_dev, const ConnectionParam<b>& param, const Config& trans_conf,
         std::atomic_bool& running_)
    requires(b == Backend::DOCA_Comch)
      : Base(ch_dev, dma_dev, param, trans_conf), running(running_) {}

  ~Worker() {}

  template <typename Fn>
  void run(Fn&& fn, size_t core_idx) {
    th = std::thread([this, core_idx, fn = std::move(fn)]() {
      bind_core(core_idx);
      Base::ch.establish_connections();
      TransportGuard g(Base::t);
      INFO("start");
      fn();
      INFO("stop");
      Base::ch.terminate_connections();
    });
  }

  template <typename Fn>
  void run(Fn&& fn) {
    th = std::thread([this, fn = std::move(fn)]() {
      Base::ch.establish_connections();
      TransportGuard g(Base::t);
      fn();
      Base::ch.terminate_connections();
    });
  }

  void join() {
    if (th.joinable()) {
      INFO("wait");
      th.join();
    }
  }

 public:
  std::atomic_bool& running;

 private:
  std::thread th;
};

}  // namespace dpx
