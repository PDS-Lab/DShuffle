#include "trans/priv/doca/comch/connection.hxx"

#include <sys/epoll.h>

#include <glaze/glaze.hpp>

#include "doca/check.hxx"
#include "doca/device.hxx"
#include "doca/helper.hxx"
#include "priv/doca/caps.hxx"
#include "trans/priv/doca/comch/endpoint.hxx"
#include "util/logger.hxx"

using namespace std::chrono_literals;

namespace dpx::doca::comch {

ConnectionHandle::ConnectionHandle(Device& ch_dev_, const ConnectionParam& param_) : ch_dev(ch_dev_), param(param_) {
  auto caps = probe_comch_caps(ch_dev);

  INFO("Comch capability:\n{}", glz::write<glz::opts{.prettify = true}>(caps).value_or("Unexpected!"));

  doca_check(doca_pe_create(&pe));
  if (param.passive) {
    doca_check(doca_comch_server_create(ch_dev.dev, ch_dev.rep, param.name.data(), &s));
  } else {
    doca_check(doca_comch_client_create(ch_dev.dev, param.name.data(), &c));
  }

  if (param.passive) {
    auto ctx = doca_comch_server_as_ctx(s);
    doca_check(
        doca_comch_server_task_send_set_conf(s, task_completion_cb, task_error_cb, caps.ctrl_path.max_recv_queue_size));
    doca_check(doca_comch_server_event_msg_recv_register(s, recv_event_cb));
    doca_check(doca_comch_server_event_connection_status_changed_register(s, connect_event_cb, disconnect_event_cb));
    doca_check(
        doca_comch_server_event_consumer_register(s, server_new_consumer_event_cb, server_expired_consumer_event_cb));
    doca_check(doca_comch_server_set_max_msg_size(s, caps.ctrl_path.max_msg_size));
    doca_check(doca_comch_server_set_recv_queue_size(s, caps.ctrl_path.max_recv_queue_size));
    doca_check(doca_pe_connect_ctx(pe, ctx));
    doca_check(doca_ctx_set_state_changed_cb(ctx, server_state_change_cb));
    doca_check(doca_ctx_set_user_data(ctx, doca_data(this)));
    doca_check(doca_ctx_start(ctx));
  } else {
    auto ctx = doca_comch_client_as_ctx(c);
    doca_check(
        doca_comch_client_task_send_set_conf(c, task_completion_cb, task_error_cb, caps.ctrl_path.max_recv_queue_size));
    doca_check(doca_comch_client_event_msg_recv_register(c, recv_event_cb));
    doca_check(
        doca_comch_client_event_consumer_register(c, client_new_consumer_event_cb, client_expired_consumer_event_cb));
    doca_check(doca_comch_client_set_max_msg_size(c, caps.ctrl_path.max_msg_size));
    doca_check(doca_comch_client_set_recv_queue_size(c, caps.ctrl_path.max_recv_queue_size));
    doca_check(doca_pe_connect_ctx(pe, ctx));
    doca_check(doca_ctx_set_state_changed_cb(ctx, client_state_change_cb));
    doca_check(doca_ctx_set_user_data(ctx, doca_data(this)));
    doca_check_ext(doca_ctx_start(ctx), DOCA_ERROR_IN_PROGRESS);
  }

  if (param.passive) {
    progress_all_until([this]() { return established(); });
  } else {
    progress_until([this]() { return established(); });
    doca_check(doca_comch_client_get_connection(c, &conn));
  }
}

ConnectionHandle::~ConnectionHandle() {
  if (param.passive) {
    doca_check_ext(doca_ctx_stop(doca_comch_server_as_ctx(s)), DOCA_ERROR_IN_PROGRESS);
  } else {
    doca_check_ext(doca_ctx_stop(doca_comch_client_as_ctx(c)), DOCA_ERROR_IN_PROGRESS);
  }

  progress_until([this]() { return terminated(); });

  if (param.passive) {
    if (s != nullptr) {
      doca_check(doca_comch_server_destroy(s));
    }
  } else {
    if (c != nullptr) {
      doca_check(doca_comch_client_destroy(c));
    }
  }
  if (pe != nullptr) {
    doca_check(doca_pe_destroy(pe));
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

bool ConnectionHandle::all_running() {
  return all_of_endpoint([](Endpoint& e) { return e.running(); });
}

bool ConnectionHandle::all_exited() {
  return all_of_endpoint([](Endpoint& e) { return e.exited(); });
}

void ConnectionHandle::listen_and_accept() {
  for_each_endpoint([this](Endpoint& e) { e.prepare(conn); });
  progress_all_until([this]() { return all_running(); });
}

void ConnectionHandle::wait_for_disconnect() {
  auto epfd = epoll_create(1);
  int pefd = 0;
  doca_check(doca_pe_get_notification_handle(pe, &pefd));
  doca_check(doca_pe_request_notification(pe));

  auto ev = epoll_event{
      .events = EPOLLIN | EPOLLOUT,
      .data.fd = pefd,
  };
  if (auto ec = epoll_ctl(epfd, EPOLL_CTL_ADD, pefd, &ev); ec != 0) {
    die("Fail to add pe fd into epoll, errno: {}", errno);
  }

  while (true) {
    epoll_event ev;
    auto n = epoll_wait(epfd, &ev, 1, 1000);
    if (n < 0) {
      if (errno == EINTR) {
        continue;
      }
      die("Fail to call epoll_wait, errno: {}", errno);
    } else if (n == 0) {
      // do nothing
    } else {
      assert(n == 1);
      assert(ev.data.fd == pefd);
      doca_check(doca_pe_clear_notification(pe, pefd));
      while (progress()) {
        continue;
      }
      doca_check(doca_pe_request_notification(pe));
    }
    if (all_exited()) {
      break;
    }
  }

  close(epfd);
}

void ConnectionHandle::connect() {
  for_each_endpoint([this](Endpoint& e) { e.prepare(conn); });
  progress_all_until([this]() { return all_running(); });
}

void ConnectionHandle::disconnect() {
  for_each_endpoint([](Endpoint& e) { e.stop(); });
  progress_all_until([this]() { return all_exited(); });
}

void ConnectionHandle::progress_all_until(std::function<bool(void)>&& predictor) {
  while (!predictor()) {
    progress();
    for_each_endpoint([](Endpoint& e) { e.progress(); });
    std::this_thread::sleep_for(1ms);
  }
}

bool ConnectionHandle::progress() { return doca_pe_progress(pe); }

void ConnectionHandle::progress_until(std::function<bool(void)>&& predictor) {
  while (!predictor()) {
    if (!progress()) {
      std::this_thread::sleep_for(1s);
    }
  }
}

doca_ctx_states ConnectionHandle::client_state() { return get_ctx_state(doca_comch_client_as_ctx(c)); }

doca_ctx_states ConnectionHandle::server_state() { return get_ctx_state(doca_comch_server_as_ctx(s)); }

bool ConnectionHandle::established() {
  if (param.passive) {
    return server_state() == DOCA_CTX_STATE_RUNNING && conn != nullptr;
  } else {
    return client_state() == DOCA_CTX_STATE_RUNNING;
  }
}

bool ConnectionHandle::terminated() {
  if (param.passive) {
    return server_state() == DOCA_CTX_STATE_IDLE;
  } else {
    return client_state() == DOCA_CTX_STATE_IDLE;
  }
}

void ConnectionHandle::server_state_change_cb(const doca_data ctx_user_data, doca_ctx*,
                                              [[maybe_unused]] doca_ctx_states prev_state,
                                              [[maybe_unused]] doca_ctx_states next_state) {
  auto e = reinterpret_cast<ConnectionHandle*>(ctx_user_data.ptr);
  INFO("DOCA Comch Server {} state change: {} -> {}", e->param.name, prev_state, next_state);
}
void ConnectionHandle::client_state_change_cb(const doca_data ctx_user_data, doca_ctx*,
                                              [[maybe_unused]] doca_ctx_states prev_state,
                                              [[maybe_unused]] doca_ctx_states next_state) {
  auto e = reinterpret_cast<ConnectionHandle*>(ctx_user_data.ptr);
  INFO("DOCA Comch Client {} state change: {} -> {}", e->param.name, prev_state, next_state);
}

void ConnectionHandle::connect_event_cb(doca_comch_event_connection_status_changed*, doca_comch_connection* conn,
                                        uint8_t success) {
  if (!success) {
    ERROR("Unsucceed connection");
  }
  doca_data user_data(nullptr);
  doca_check(doca_ctx_get_user_data(doca_comch_server_as_ctx(doca_comch_server_get_server_ctx(conn)), &user_data));
  auto e = reinterpret_cast<ConnectionHandle*>(user_data.ptr);
  if (e->conn == nullptr) {
    e->conn = conn;
    INFO("Establish ctrl path of {}", e->param.name);
  } else {
    WARN("Only support one connection, ignored");
  }
}

void ConnectionHandle::disconnect_event_cb(doca_comch_event_connection_status_changed*, doca_comch_connection* conn,
                                           uint8_t success) {
  if (!success) {
    ERROR("Unsucceed disconnection");
  }
  doca_data user_data(nullptr);
  doca_check(doca_ctx_get_user_data(doca_comch_server_as_ctx(doca_comch_server_get_server_ctx(conn)), &user_data));
  auto e = reinterpret_cast<ConnectionHandle*>(user_data.ptr);
  if (e->conn == conn) {
    e->conn = nullptr;
    INFO("Disconnect ctrl path of {}", e->param.name);
  } else {
    WARN("Only support one connection, ignore");
  }
}
// TODO make template
void ConnectionHandle::server_new_consumer_event_cb(doca_comch_event_consumer*, doca_comch_connection* conn,
                                                    uint32_t remote_consumer_id) {
  doca_data user_data(nullptr);
  doca_check(doca_ctx_get_user_data(doca_comch_server_as_ctx(doca_comch_server_get_server_ctx(conn)), &user_data));
  auto ch = reinterpret_cast<ConnectionHandle*>(user_data.ptr);

  if (conn != ch->conn) {
    WARN("Only support one connection, ignore");
  } else {
    for (Endpoint& endpoint : ch->pending_endpoints) {
      if (endpoint.consumer_id == remote_consumer_id) {
        endpoint.run(remote_consumer_id);
        INFO("Establish data path of {}:{}", ch->param.name, remote_consumer_id);
      }
    }
  }
}

void ConnectionHandle::server_expired_consumer_event_cb(doca_comch_event_consumer*, doca_comch_connection* conn,
                                                        uint32_t remote_consumer_id) {
  doca_data user_data(nullptr);
  doca_check(doca_ctx_get_user_data(doca_comch_server_as_ctx(doca_comch_server_get_server_ctx(conn)), &user_data));
  auto ch = reinterpret_cast<ConnectionHandle*>(user_data.ptr);
  if (conn != ch->conn) {
    WARN("Only support one connection, ignore");
  } else {
    for (Endpoint& endpoint : ch->pending_endpoints) {
      if (endpoint.consumer_id == remote_consumer_id) {
        endpoint.stop();
        INFO("Disconnect data path of {}:{}", ch->param.name, remote_consumer_id);
      }
    }
  }
}

void ConnectionHandle::client_new_consumer_event_cb(doca_comch_event_consumer*, doca_comch_connection* conn,
                                                    uint32_t remote_consumer_id) {
  doca_data user_data(nullptr);
  doca_check(doca_ctx_get_user_data(doca_comch_client_as_ctx(doca_comch_client_get_client_ctx(conn)), &user_data));
  auto ch = reinterpret_cast<ConnectionHandle*>(user_data.ptr);
  if (conn != ch->conn) {
    WARN("Only support one connection, ignore");
  } else {
    for (Endpoint& endpoint : ch->pending_endpoints) {
      if (endpoint.consumer_id == remote_consumer_id) {
        endpoint.run(remote_consumer_id);
        INFO("Establish data path of {}:{}", ch->param.name, remote_consumer_id);
      }
    }
  }
}

void ConnectionHandle::client_expired_consumer_event_cb(doca_comch_event_consumer*, doca_comch_connection* conn,
                                                        uint32_t remote_consumer_id) {
  doca_data user_data(nullptr);
  doca_check(doca_ctx_get_user_data(doca_comch_client_as_ctx(doca_comch_client_get_client_ctx(conn)), &user_data));
  auto ch = reinterpret_cast<ConnectionHandle*>(user_data.ptr);
  if (conn != ch->conn) {
    WARN("Only support one connection, ignore");
  } else {
    for (Endpoint& endpoint : ch->pending_endpoints) {
      if (endpoint.consumer_id == remote_consumer_id) {
        endpoint.stop();
        INFO("Disconnect data path of {}:{}", ch->param.name, remote_consumer_id);
      }
    }
  }
}

void ConnectionHandle::task_completion_cb(doca_comch_task_send*, doca_data, doca_data) { unreachable(); }

void ConnectionHandle::task_error_cb(doca_comch_task_send*, doca_data, doca_data) { unreachable(); }

void ConnectionHandle::recv_event_cb(doca_comch_event_msg_recv*, uint8_t*, uint32_t, doca_comch_connection*) {
  unreachable();
}

}  // namespace dpx::doca::comch