#pragma once

#include <thread>

#include "trans/common/defs.hxx"
#include "trans/concept/connection_handle.hxx"
#include "trans/priv/doca/comch/connection.hxx"
#include "trans/priv/doca/rdma/connection.hxx"
#include "trans/priv/tcp/connection.hxx"
#include "util/noncopyable.hxx"
#include "util/nonmovable.hxx"

namespace dpx {

// clang-format off
template <Backend b>
using ConnectionParam =
  std::conditional_t<b == Backend::TCP,        tcp::ConnectionParam,
  std::conditional_t<b == Backend::DOCA_Comch, doca::comch::ConnectionParam,
  std::conditional_t<b == Backend::DOCA_RDMA,  doca::rdma::ConnectionParam,
                                               void>>>;
// clang-format on

template <Backend b>
class ConnectionHolder : Noncopyable, Nonmovable {
  // clang-format off
  using ConnectionHandle =
    std::conditional_t<b == Backend::TCP,        tcp::ConnectionHandle,
    std::conditional_t<b == Backend::DOCA_Comch, doca::comch::ConnectionHandle,
    std::conditional_t<b == Backend::DOCA_RDMA,  doca::rdma::ConnectionHandle,
                                                 void>>>;
  // clang-format on
  using Endpoint = ConnectionHandle::Endpoint;

  static_assert(CanBeUsedAsConnectionHandle<ConnectionHandle>, "Invalid connection handle!");

 public:
  explicit ConnectionHolder(ConnectionParam<b> param_)
    requires(b == Backend::TCP)
      : param(param_), h(param) {}
  ConnectionHolder(doca::Device& dev, ConnectionParam<b> param_)
    requires(b == Backend::DOCA_Comch || b == Backend::DOCA_RDMA)
      : param(param_), h(dev, param) {}
  ~ConnectionHolder() {}

  void associate(Endpoint& e) { h.associate(e); }

  void terminate_connections() {
    if (param.passive) {
      if (daemon.joinable()) {
        daemon.join();
      }
    } else {
      h.disconnect();
    }
  }

  void establish_connections() {
    if (param.passive) {
      h.listen_and_accept();
      daemon = std::thread([this]() { h.wait_for_disconnect(); });
    } else {
      h.connect();
    }
  }

  bool is_passive() const { return param.passive; }

 private:
  ConnectionParam<b> param;
  ConnectionHandle h;
  std::thread daemon;
};

}  // namespace dpx
