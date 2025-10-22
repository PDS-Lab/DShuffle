#pragma once

#include <doca_comch.h>
#include <doca_ctx.h>
#include <doca_pe.h>

#include <string>
#include <vector>

#include "doca/device.hxx"

namespace dpx::doca::comch {

struct ConnectionParam {
  bool passive;
  std::string name;
};

class Endpoint;

class ConnectionHandle {
 public:
  using Endpoint = Endpoint;
  using EndpointRef = std::reference_wrapper<Endpoint>;
  using EndpointRefs = std::vector<EndpointRef>;

  ConnectionHandle(Device& ch_dev_, const ConnectionParam& param_);
  ~ConnectionHandle();

  ConnectionHandle& associate(Endpoint& e);
  ConnectionHandle& associate(EndpointRefs&& es);

  void listen_and_accept();
  void wait_for_disconnect();
  void connect();
  void disconnect();

 private:
  template <typename Fn>
  void for_each_endpoint(Fn&& fn) {
    std::ranges::for_each(pending_endpoints, fn);
  }

  template <typename Fn>
  bool all_of_endpoint(Fn&& fn) {
    return std::ranges::all_of(pending_endpoints, fn);
  }

  void progress_all_until(std::function<bool(void)>&& predictor);

  bool all_running();
  bool all_exited();

  bool progress();
  void progress_until(std::function<bool(void)>&& predictor);

  doca_ctx_states client_state();
  doca_ctx_states server_state();

  bool established();
  bool terminated();

  static void server_state_change_cb(const doca_data, doca_ctx*, doca_ctx_states, doca_ctx_states);
  static void client_state_change_cb(const doca_data, doca_ctx*, doca_ctx_states, doca_ctx_states);
  static void connect_event_cb(doca_comch_event_connection_status_changed*, doca_comch_connection*, uint8_t);
  static void disconnect_event_cb(doca_comch_event_connection_status_changed*, doca_comch_connection*, uint8_t);
  static void server_new_consumer_event_cb(doca_comch_event_consumer*, doca_comch_connection*, uint32_t);
  static void server_expired_consumer_event_cb(doca_comch_event_consumer*, doca_comch_connection*, uint32_t);
  static void client_new_consumer_event_cb(doca_comch_event_consumer*, doca_comch_connection*, uint32_t);
  static void client_expired_consumer_event_cb(doca_comch_event_consumer*, doca_comch_connection*, uint32_t);

  // unused
  static void task_completion_cb(doca_comch_task_send*, doca_data, doca_data);
  static void task_error_cb(doca_comch_task_send*, doca_data, doca_data);
  static void recv_event_cb(doca_comch_event_msg_recv*, uint8_t*, uint32_t, doca_comch_connection*);

  Device& ch_dev;
  const ConnectionParam& param;
  doca_pe* pe;
  union {
    doca_comch_server* s = nullptr;
    doca_comch_client* c;
  };
  doca_comch_connection* conn = nullptr;
  EndpointRefs pending_endpoints;
};

}  // namespace dpx::doca::comch
