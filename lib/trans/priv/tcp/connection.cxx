#include "trans/priv/tcp/connection.hxx"

#include "trans/priv/tcp/endpoint.hxx"
#include "util/fatal.hxx"
#include "util/socket.hxx"

namespace dpx::tcp {

ConnectionHandle::ConnectionHandle(const ConnectionParam &param_) : param(param_) {}

ConnectionHandle::~ConnectionHandle() { close_socket(conn_sock); };

ConnectionHandle &ConnectionHandle::associate(Endpoint &e) {
  pending_endpoints.emplace_back(e);
  return *this;
}

ConnectionHandle &ConnectionHandle::associate(EndpointRefs &&es) {
  pending_endpoints.insert(pending_endpoints.end(), std::make_move_iterator(es.begin()),
                           std::make_move_iterator(es.end()));
  return *this;
}

// WARN: this connection procedure is quite naive, but feasible under this circumstance.
void ConnectionHandle::listen_and_accept() {
  auto listen_sock = setup_and_bind(param.local_ip, param.local_port);
  if (auto ec = ::listen(listen_sock, pending_endpoints.size() + 1); ec < 0) {
    die("Fail to listen, errno: {}", errno);
  }
  conn_sock = ::accept(listen_sock, nullptr, nullptr);
  if (conn_sock < 0) {
    die("Fail to accept connection manage socket, errno: {}", errno);
  }
  std::ranges::for_each(pending_endpoints, [listen_sock](Endpoint &e) {
    e.rpc_sock = ::accept(listen_sock, nullptr, nullptr);
    if (e.rpc_sock < 0) {
      die("Fail to accept connection from peer, errno: {}", errno);
    }
    INFO("Establish ctrl path: {}", get_socket_connection_info(e.rpc_sock));
    e.bulk_sock = ::accept(listen_sock, nullptr, nullptr);
    if (e.bulk_sock < 0) {
      die("Fail to accept connection from peer, errno: {}", errno);
    }
    INFO("Establish data path: {}", get_socket_connection_info(e.bulk_sock));
    e.run();
  });
  close_socket(listen_sock);
}

void ConnectionHandle::wait_for_disconnect() {
  read_string(conn_sock, 1);
  // we don't care the return value, any case will indicate the connection is going to close.
  std::ranges::for_each(pending_endpoints, [](Endpoint &e) {
    e.stop();
    e.shutdown();
  });
  write_string(conn_sock, "x");
}

void ConnectionHandle::connect() {
  auto remote_addr_in = sockaddr_in{
      .sin_family = AF_INET,
      .sin_port = htons(param.remote_port),
      .sin_addr = {.s_addr = inet_addr(param.remote_ip.data())},
      .sin_zero = {},
  };
  auto conn_port = (param.local_port != 0 ? param.local_port + 2 * pending_endpoints.size() : 0);
  conn_sock = setup_and_bind_and_connect(param.local_ip, conn_port, remote_addr_in);
  std::ranges::for_each(pending_endpoints, [this, &remote_addr_in, port = param.local_port](Endpoint &e) mutable {
    auto rpc_port = (port == 0 ? 0 : port);
    e.rpc_sock = setup_and_bind_and_connect(param.local_ip, rpc_port, remote_addr_in);
    INFO("Establish ctrl path: {}", get_socket_connection_info(e.rpc_sock));

    auto bulk_port = (port == 0 ? 0 : port + 1);
    e.bulk_sock = setup_and_bind_and_connect(param.local_ip, bulk_port, remote_addr_in);
    INFO("Establish data path: {}", get_socket_connection_info(e.bulk_sock));

    e.run();
    port += 2;
  });
}

void ConnectionHandle::disconnect() {
  write_string(conn_sock, "x");
  read_string(conn_sock, 1);
  std::ranges::for_each(pending_endpoints, [](Endpoint &e) {
    e.stop();
    e.shutdown();
  });
}

}  // namespace dpx::tcp
