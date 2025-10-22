#include "trans/priv/doca/rdma/connection.hxx"

#include <glaze/glaze.hpp>

#include "trans/priv/doca/caps.hxx"
#include "trans/priv/doca/rdma/endpoint.hxx"
#include "util/logger.hxx"
#include "util/socket.hxx"

namespace dpx::doca::rdma {

namespace {

doca_rdma_connection* exchange_desc(int sock, doca_rdma* rdma, bool passive) {
  const void* local_desc = nullptr;
  size_t local_desc_len = 0;
  doca_rdma_connection* conn = nullptr;
  doca_check(doca_rdma_export(rdma, &local_desc, &local_desc_len, &conn));
  std::string remote_desc;
  if (passive) {
    remote_desc = read_string_with_header(sock);
    write_string_with_header(sock, std::string_view(reinterpret_cast<const char*>(local_desc), local_desc_len));
  } else {
    write_string_with_header(sock, std::string_view(reinterpret_cast<const char*>(local_desc), local_desc_len));
    remote_desc = read_string_with_header(sock);
  }
  doca_check(doca_rdma_connect(rdma, remote_desc.data(), remote_desc.size(), conn));
  return conn;
}

}  // namespace

ConnectionHandle::ConnectionHandle(Device& dev_, const ConnectionParam& param_) : dev(dev_), param(param_) {
  RDMACapability caps = probe_rdma_caps(dev);
  INFO("RDMA capability:\n{}", glz::write<glz::opts{.prettify = true}>(caps).value_or("Unexpected!"));
}

ConnectionHandle::~ConnectionHandle() {
  if (conn_sock != -1) {
    close_socket(conn_sock);
  }
}

ConnectionHandle& ConnectionHandle::associate(Endpoint& e) {
  pending_endpoints.emplace_back(e);
  return *this;
}

ConnectionHandle& ConnectionHandle::associate(EndpointRefs&& es) {
  pending_endpoints.insert(pending_endpoints.end(), std::make_move_iterator(es.begin()),
                           std::make_move_iterator(es.end()));
  return *this;
}

void ConnectionHandle::listen_and_accept() {
  conn_sock = setup_and_bind_and_listen_and_accept(param.local_ip, param.local_port);
  INFO("Connection path: {}", get_socket_connection_info(conn_sock));
  std::ranges::for_each(pending_endpoints, [this](Endpoint& e) {
    e.prepare(param);
    e.cp_conn = exchange_desc(conn_sock, e.ctrl_path, true);
    INFO("Establish ctrl path");
    if (e.enable_data_path) {
      e.dp_conn = exchange_desc(conn_sock, e.data_path, true);
      INFO("Establish data path");
    }
    e.run();
  });
}

void ConnectionHandle::wait_for_disconnect() {
  read_string(conn_sock, 1);
  std::ranges::for_each(pending_endpoints, [](Endpoint& e) { e.stop(); });
}

void ConnectionHandle::connect() {
  conn_sock = setup_and_bind_and_connect(param.local_ip, param.local_port, param.remote_ip, param.remote_port);
  INFO("Connection path: {}", get_socket_connection_info(conn_sock));
  std::ranges::for_each(pending_endpoints, [this](Endpoint& e) {
    e.prepare(param);
    e.cp_conn = exchange_desc(conn_sock, e.ctrl_path, false);
    INFO("Establish ctrl path");
    if (e.enable_data_path) {
      e.dp_conn = exchange_desc(conn_sock, e.data_path, false);
      INFO("Establish data path");
    }
    e.run();
  });
}

void ConnectionHandle::disconnect() {
  write_string(conn_sock, "0");
  std::ranges::for_each(pending_endpoints, [](Endpoint& e) { e.stop(); });
}

}  // namespace dpx::doca::rdma
