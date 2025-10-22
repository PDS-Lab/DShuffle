#include "util/socket.hxx"

#include <unistd.h>

#include <cerrno>

#include "util/fatal.hxx"
#include "util/logger.hxx"

using namespace std::chrono_literals;

namespace dpx {

int setup_and_bind(std::string_view ip, uint16_t port) {
  // create socket
  int sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0) {
    die("Fail to create server side socket, errno: {}", errno);
  }
  // set socket option, reusable
  bool enable = true;
  if (auto ec = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &enable, sizeof(int)); ec < 0) {
    die("Fail to set socket options, errno: {}", errno);
  }
  // bind
  auto ip_in = (ip.empty() ? INADDR_ANY : inet_addr(ip.data()));
  if (ip_in == INADDR_NONE) {
    die("Wrong format: {}", ip);
  }
  auto addr_in = sockaddr_in{
      .sin_family = AF_INET,
      .sin_port = htons(port),
      .sin_addr = {.s_addr = ip_in},
      .sin_zero = {},
  };
  if (auto ec = bind(sock, reinterpret_cast<const sockaddr *>(&addr_in), sizeof(addr_in)); ec < 0) {
    die("Fail to bind {}:{}, errno: {}", ip, port, errno);
  }
  return sock;
}

int setup_and_bind_and_listen_and_accept(std::string_view ip, uint16_t port) {
  auto listen_sock = setup_and_bind(ip, port);
  if (auto ec = ::listen(listen_sock, 0); ec < 0) {
    die("Fail to listen, errno: {}", errno);
  }
  auto client_sock = ::accept(listen_sock, nullptr, nullptr);
  if (client_sock < 0) {
    die("Fail to accept client connection, errno: {}", errno);
  }
  close_socket(listen_sock);
  return client_sock;
}

int setup_and_bind_and_connect(std::string_view local_ip, uint16_t local_port, std::string_view remote_ip,
                               uint16_t remote_port) {
  auto remote_addr_in = sockaddr_in{
      .sin_family = AF_INET,
      .sin_port = htons(remote_port),
      .sin_addr = {.s_addr = inet_addr(remote_ip.data())},
      .sin_zero = {},
  };
  return setup_and_bind_and_connect(local_ip, local_port, remote_addr_in);
}

int setup_and_bind_and_connect(std::string_view ip, uint16_t port, sockaddr_in &remote_addr_in) {
  auto sock = setup_and_bind(ip, port);
  int count = 0;
  while (true) {
    count++;
    if (auto ec = ::connect(sock, reinterpret_cast<sockaddr *>(&remote_addr_in), sizeof(remote_addr_in)); ec < 0) {
      WARN("Fail to connect with peer {}:{}, errno: {}, count: {}", inet_ntoa(remote_addr_in.sin_addr),
           ntohs(remote_addr_in.sin_port), errno, count);
    } else {
      break;
    }
    std::this_thread::sleep_for(1s * count);
    if (count == 10) {
      die("Stop trying to connect");
    }
  }
  return sock;
}

std::string get_socket_connection_info(int sock) {
  sockaddr_in addr_in = {};
  socklen_t addr_in_len = sizeof(addr_in);
  if (auto ec = ::getsockname(sock, reinterpret_cast<sockaddr *>(&addr_in), &addr_in_len); ec < 0) {
    die("Fail to get local addr, errno: {}", errno);
  }
  auto local_addr = std::format("{}:{}", inet_ntoa(addr_in.sin_addr), ntohs(addr_in.sin_port));
  if (auto ec = ::getpeername(sock, reinterpret_cast<sockaddr *>(&addr_in), &addr_in_len); ec < 0) {
    die("Fail to get remote addr, errno: {}", errno);
  }
  auto remote_addr = std::format("{}:{}", inet_ntoa(addr_in.sin_addr), ntohs(addr_in.sin_port));
  return std::format("connection {} <-> {}", local_addr, remote_addr);
}

std::string read_string(int sock, size_t len) {
  std::string buf(len, 0);
  size_t offset = 0;
  size_t remain = len;
  while (remain > 0) {
    auto n = ::read(sock, buf.data() + offset, remain);
    if (n < 0) {
      die("Fail to read from socket {}, errno: {}", sock, errno);
    }
    remain -= n;
    offset += n;
  }
  return buf;
}

std::string read_string_with_header(int sock) {
  auto header = read_string(sock, sizeof(size_t));
  return read_string(sock, *reinterpret_cast<size_t *>(header.data()));
}

void write_string(int sock, std::string_view sv) {
  if (auto n = ::write(sock, sv.data(), sv.size()); n < 0) {
    die("Fail to write to socket {}, errno: {}", sock, errno);
  }
}

void write_string_with_header(int sock, std::string_view sv) {
  size_t length = sv.size();
  write_string(sock, std::string_view(reinterpret_cast<const char *>(&length), sizeof(size_t)));
  write_string(sock, sv);
}

void close_socket(int s) {
  if (s > 0) {
    if (auto ec = ::close(s); ec < 0) {
      die("Fail to close socket {}, errno: {}", s, errno);
    }
  }
}

}  // namespace dpx
