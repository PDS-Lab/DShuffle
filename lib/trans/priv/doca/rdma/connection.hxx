#pragma once

#include "doca/device.hxx"

namespace dpx::doca::rdma {

struct ConnectionParam {
  bool passive;
  bool enable_grh = false;
  std::string remote_ip = "";
  std::string local_ip = "";
  uint16_t remote_port = 0;
  uint16_t local_port = 0;
};

class Endpoint;

class ConnectionHandle {
 public:
  using Endpoint = Endpoint;
  using EndpointRef = std::reference_wrapper<Endpoint>;
  using EndpointRefs = std::vector<EndpointRef>;

  ConnectionHandle(Device& dev_, const ConnectionParam& param_);
  ~ConnectionHandle();

  ConnectionHandle& associate(Endpoint& e);
  ConnectionHandle& associate(EndpointRefs&& es);

  void listen_and_accept();
  void wait_for_disconnect();
  void connect();
  void disconnect();

 private:
  Device& dev;
  const ConnectionParam& param;
  EndpointRefs pending_endpoints;
  int conn_sock;
};

}  // namespace doca::rdma
