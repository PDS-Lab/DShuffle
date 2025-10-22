#pragma once

#include <arpa/inet.h>

#include <string_view>

namespace dpx {

int setup_and_bind(std::string_view local_ip, uint16_t local_port);

int setup_and_bind_and_listen_and_accept(std::string_view local_ip, uint16_t local_port);

int setup_and_bind_and_connect(std::string_view local_ip, uint16_t local_port, sockaddr_in &remote_addr_in);

int setup_and_bind_and_connect(std::string_view local_ip, uint16_t local_port, std::string_view remote_ip,
                               uint16_t remote_port);

std::string get_socket_connection_info(int sock);

std::string read_string(int sock, size_t len);

void write_string(int sock, std::string_view sv);

std::string read_string_with_header(int sock);

void write_string_with_header(int sock, std::string_view sv);

void close_socket(int s);

}  // namespace dpx
