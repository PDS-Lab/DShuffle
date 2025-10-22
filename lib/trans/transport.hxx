#pragma once

#include "memory/simple_buffer_pool.hxx"
#include "trans/common/bulk.hxx"
#include "trans/common/connection.hxx"
#include "trans/common/context.hxx"
#include "trans/common/defs.hxx"
#include "trans/common/rpc.hxx"
#include "trans/common/serializer.hxx"
#include "trans/concept/endpoint.hxx"
#include "trans/priv/doca/comch/endpoint.hxx"
#include "trans/priv/doca/rdma/endpoint.hxx"
#include "trans/priv/tcp/endpoint.hxx"
#include "util/fatal.hxx"
#include "util/logger.hxx"

using namespace std::chrono_literals;

namespace dpx {

struct Config {
  size_t queue_depth;
  size_t max_rpc_msg_size;
};

template <Backend b>
class ConnectionHolder;

template <Backend b, Rpc... rpcs>
class TransportGuard;

template <Backend b, Rpc... rpcs>
class Transport {
  // clang-format off
  using Endpoint =
    std::conditional_t<b == Backend::TCP,        tcp::Endpoint,
    std::conditional_t<b == Backend::DOCA_Comch, doca::comch::Endpoint,
    std::conditional_t<b == Backend::DOCA_RDMA,  doca::rdma::Endpoint,
                                                 void>>>;
  using RpcBufferPool =
    std::conditional_t<b == Backend::DOCA_Comch || b == Backend::DOCA_RDMA, BufferPool<doca::Buffers>,
                                                                            BufferPool<naive::Buffers>>;
  // clang-format on
  using RpcBuffer = RpcBufferPool::BufferType;

  // private rpcs
  struct Bulk : RpcBase<"Bulk", MemoryRegion, size_t> {};
  struct RegisterMemory : RpcBase<"RegMem", RemoteBuffer, void> {};
  struct UnregisterMemory : RpcBase<"UnregMem", MemoryRegion, void> {};

  using NormalRpcs = NormalRpcFilter<Bulk, rpcs...>::type;
  using OnewayRpcs = OnewayRpcFilter<RegisterMemory, UnregisterMemory, rpcs...>::type;
  using NormalRpcHandler = general_handler_t<NormalRpcs>;
  using OnewayRpcHandler = general_handler_t<OnewayRpcs>;

  static_assert(CanBeUsedAsEndpoint<Endpoint>, "Invalid endpoint backend!");
  // static_assert(std::tuple_size_v<NormalRpcs> > 1, "No registered normal rpc!");

  friend class TransportGuard<b, rpcs...>;

 public:
  Transport(ConnectionHolder<b> &holder_, Config config_)
    requires(b == Backend::TCP)
      : config(config_),
        holder(holder_),
        send_bufs(config.queue_depth, config.max_rpc_msg_size),
        recv_bufs(config.queue_depth, config.max_rpc_msg_size),
        e(send_bufs.buffers(), recv_bufs.buffers()) {
    (register_handler<rpcs>(), ...);  // register default handlers
    holder.associate(e);
  }

  Transport(doca::Device &ch_dev, doca::Device &dma_dev, ConnectionHolder<b> &holder_, Config config_)
    requires(b == Backend::DOCA_Comch)
      : config(config_),
        holder(holder_),
        send_bufs(ch_dev, config.queue_depth, config.max_rpc_msg_size, DOCA_ACCESS_FLAG_PCI_READ_WRITE),
        recv_bufs(ch_dev, config.queue_depth, config.max_rpc_msg_size, DOCA_ACCESS_FLAG_PCI_READ_WRITE),
        e(ch_dev, dma_dev, send_bufs.buffers(), recv_bufs.buffers()) {
    (register_handler<rpcs>(), ...);  // register default handlers
    holder.associate(e);
  }

  Transport(doca::Device &dev, ConnectionHolder<b> &holder_, Config config_)
    requires(b == Backend::DOCA_Comch)
      : config(config_),
        holder(holder_),
        send_bufs(dev, config.queue_depth, config.max_rpc_msg_size, DOCA_ACCESS_FLAG_PCI_READ_WRITE),
        recv_bufs(dev, config.queue_depth, config.max_rpc_msg_size, DOCA_ACCESS_FLAG_PCI_READ_WRITE),
        e(dev, dev, send_bufs.buffers(), recv_bufs.buffers()) {
    (register_handler<rpcs>(), ...);  // register default handlers
    holder.associate(e);
  }

  Transport(doca::Device &dev, ConnectionHolder<b> &holder_, Config config_)
    requires(b == Backend::DOCA_RDMA)
      : config(config_),
        holder(holder_),
        send_bufs(dev, config.queue_depth, config.max_rpc_msg_size),
        recv_bufs(dev, config.queue_depth, config.max_rpc_msg_size),
        e(dev, send_bufs.buffers(), recv_bufs.buffers()) {
    (register_handler<rpcs>(), ...);  // register default handlers
    holder.associate(e);
  }

  ~Transport() {}

  void register_bulk_handler(const handler_t<Bulk> &h) {
    register_handler<Bulk>(h);
    reigster_related_bulk_handlers();
  }

  void register_bulk_handler(handler_t<Bulk> &&h) {
    register_handler<Bulk>(std::move(h));
    reigster_related_bulk_handlers();
  }

  template <Rpc Rpc>
  void register_handler() {
    if constexpr (is_oneway_v<Rpc>) {
      oneway_rpc_handler[Rpc::id] = Rpc();
    } else {
      normal_rpc_handler[Rpc::id] = Rpc();
    }
  }

  template <Rpc Rpc>
  void register_handler(handler_t<Rpc> &&h) {
    if constexpr (is_oneway_v<Rpc>) {
      oneway_rpc_handler[Rpc::id] = OnewayRpcHandler(std::move(h));
    } else {
      normal_rpc_handler[Rpc::id] = NormalRpcHandler(std::move(h));
    }
  }

  template <Rpc Rpc>
  void register_handler(const handler_t<Rpc> &h) {
    if constexpr (is_oneway_v<Rpc>) {
      oneway_rpc_handler[Rpc::id] = OnewayRpcHandler(h);
    } else {
      normal_rpc_handler[Rpc::id] = NormalRpcHandler(h);
    }
  }

  template <Rpc Rpc>
  void oneway(const req_t<Rpc> &req)
    requires(is_oneway_v<Rpc>)
  {
    OpContext send_ctx(Op::Send, acquire_send_buffer());
    auto serializer = Serializer(send_ctx.l_buf);
    auto h = PayloadHeader().as_oneway_req().with_seq(current_seq++).with_id(Rpc::id);
    serializer(h, req).or_throw();
    DEBUG("caller post send {}", serializer.position());
    send_ctx.len = serializer.position();
    auto n_send = e.post_send(send_ctx).get();
    DEBUG("send oneway with seq: {} id: {}", h.seq, h.id);
    if (n_send <= 0) {
      die("Fail to send payload, errno: {}", -n_send);
    }
    DEBUG("caller send {}", n_send);
    release_send_buffer(send_ctx.l_buf);
  }

  template <Rpc Rpc>
  resp_future_t<Rpc> call(const req_t<Rpc> &req)
    requires(!is_oneway_v<Rpc>)
  {
    post_recv_resp<Rpc>();
    OpContext send_ctx(Op::Send, acquire_send_buffer());
    auto serializer = Serializer(send_ctx.l_buf);
    auto h = PayloadHeader().with_seq(current_seq++).with_id(Rpc::id);
    serializer(h, req).or_throw();
    DEBUG("caller post send {}", serializer.position());
    send_ctx.len = serializer.position();
    auto n_send_f = e.post_send(send_ctx);
    DEBUG("send normal with seq: {} id: {}", h.seq, h.id);
    auto rpc_ctx = new RpcContext<Rpc>;
    resp_future_t<Rpc> f = rpc_ctx->resp.get_future();
    [[maybe_unused]] auto [_, ok] = outstanding_rpcs.emplace(h.seq, rpc_ctx);
    assert(ok);
    auto n_send = n_send_f.get();
    if (n_send <= 0) {
      die("Fail to send payload, errno: {}", -n_send);
    }
    DEBUG("caller send {}", n_send);
    release_send_buffer(send_ctx.l_buf);
    return f;
  }

  template <typename BufferType>
    requires(b == Backend::DOCA_Comch || b == Backend::DOCA_RDMA)
  void register_memory(BufferType &buffer, doca::Device &dev) {
    INFO("register {} length {}", (void *)buffer.data(), buffer.size());
    oneway<RegisterMemory>(export_remote_buffer<b>(buffer, dev));
  }

  template <typename BufferType>
    requires(b == Backend::DOCA_Comch || b == Backend::DOCA_RDMA)
  void unregister_memory(BufferType &buffer) {
    INFO("unregister {} length {}", (void *)buffer.data(), buffer.size());
    oneway<UnregisterMemory>(buffer);
  }

  template <typename BufferType>
  resp_future_t<Bulk> bulk(BufferType &buffer) {
    resp_future_t<Bulk> result_f = call<Bulk>(buffer);
    if constexpr (b == Backend::TCP) {
      // TCP use send to simulate remote memory access
      bulk_write(buffer, MemoryRegion());
    }
    return result_f;
  }

  template <typename BufferType>
  resp_future_t<Bulk> bulk(BufferType &buffer, size_t length) {
    resp_future_t<Bulk> result_f = call<Bulk>(MemoryRegion(buffer.data(), length));
    if constexpr (b == Backend::TCP) {
      // TCP use send to simulate remote memory access
      bulk_write(buffer, MemoryRegion());
    }
    return result_f;
  }

  resp_future_t<Bulk> bulk(uint8_t *data, size_t length) {
    resp_future_t<Bulk> result_f = call<Bulk>(MemoryRegion(data, length));
    if constexpr (b == Backend::TCP) {
      // TCP use send to simulate remote memory access
      // bulk_write(buffer, MemoryRegion());
      die("?");
    }
    return result_f;
  }

  size_t bulk_write(LocalBuffer &lbuf, MemoryRegion rbuf) {
    BulkContext ctx(Op::Write, lbuf, rbuf);
    auto n_write = e.post_write(ctx).get();
    if (n_write <= 0) {
      die("Fail to bulk write, errno: {}", -n_write);
    }
    DEBUG("bulk write {}", n_write);
    return n_write;
  }

  size_t bulk_read(LocalBuffer &lbuf, MemoryRegion rbuf) {
    BulkContext ctx(Op::Read, lbuf, rbuf);
    auto n_read = e.post_read(ctx).get();
    if (n_read <= 0) {
      die("Fail to bulk read, errno: {}", -n_read);
    }
    DEBUG("bulk read {}", n_read);
    return n_read;
  }

  template <typename Fn>
  void serve_until(Fn &&fn, std::function<void(void)> &&cb) {
    serve_until(fn, cb);
  }

  template <typename Fn>
  void serve_until(Fn &&fn) {
    serve_until(fn, nullptr);
  }

  template <typename Fn>
  void serve_until(Fn &fn, std::function<void(void)> &cb) {
    TRACE("Start serving");
    std::vector<boost::fibers::fiber> workers;
    workers.reserve(config.queue_depth);
    for (auto i = 0uz; i < workers.capacity(); ++i) {
      workers.emplace_back([this, i, &fn]() {
        TRACE("worker {} is active", i);
        active_workers++;
        while (e.running() && !fn()) {
          if (!serve_once(i, fn)) {
            break;  // something goes wrong, or normally exit
          }
        }
        active_workers--;
        TRACE("worker {} is inactive", i);
      });
    }
    while (active_workers != workers.capacity()) {
      boost::this_fiber::yield();
    }
    TRACE("All worker start");
    if (cb != nullptr) {
      cb();
    }
    for (auto &worker : workers) {
      worker.join();
    }
    TRACE("Stop serving");
  }

  void serve() {
    return serve_until([]() { return false; }, nullptr);
  }

  template <Rpc Rpc>
  void show_rpc_info() {
    INFO(
        "\n\tRPC name:   {}"
        "\n\tRPC id:     {}"
        "\n\tRequest:    {}"
        "\n\tResponse:   {}"
        "\n\tRegistered: {}",
        Rpc::name, Rpc::id, boost::core::demangle(typeid(req_t<Rpc>).name()),
        boost::core::demangle(typeid(resp_t<Rpc>).name()),
        normal_rpc_handler.contains(Rpc::id) || oneway_rpc_handler.contains(Rpc::id));
  };

  void show_rpc_infos() {
    return (show_rpc_info<Bulk>(), show_rpc_info<RegisterMemory>(), show_rpc_info<UnregisterMemory>(),
            (show_rpc_info<rpcs>(), ...));
  }

 private:
  void reigster_related_bulk_handlers() {
    register_handler<RegisterMemory>([this](const req_t<RegisterMemory> &rb) -> void {
      TRACE("register {:X} {}", rb.handle(), rb.size());
      e.register_remote_memory(rb);
    });
    register_handler<UnregisterMemory>([this](const req_t<UnregisterMemory> &rb) -> void {
      TRACE("unregister {:X} {}", rb.handle(), rb.size());
      e.unregister_remote_memory();
    });
  }

  template <Rpc Rpc>
  void post_recv_resp()
    requires(!is_oneway_v<Rpc>)
  {
    DEBUG("caller post recv");
    auto recv_ctx = new OpContext(Op::Recv, acquire_recv_buffer());
    auto n_recv_f = e.post_recv(*recv_ctx);
    boost::fibers::fiber([this, n_recv_f = std::move(n_recv_f), recv_ctx]() mutable {
      auto n_recv = n_recv_f.get();
      if (n_recv <= 0) {
        die("Fail to recv payload, errno: {}", -n_recv);
      }
      DEBUG("caller recv {}", n_recv);
      auto deserializer = Deserializer(recv_ctx->l_buf);
      PayloadHeader h = {};
      deserializer(h).or_throw();
      DEBUG("recv resp with seq: {} id: {}", h.seq, h.id);
      if (!h.is_resp()) {
        die("Payload is not response");
      }
      if (!dispatch_resp<NormalRpcs>(h.seq, h.id, deserializer)) {
        die("Mismatch rpc id, got {}", h.id);
      }
      release_recv_buffer(recv_ctx->l_buf);
      delete recv_ctx;
    }).detach();
  }

  template <Rpc Rpc>
  bool dispatch_normal_resp(int64_t seq, rpc_id_t id, Deserializer &deserializer)
    requires(!is_oneway_v<Rpc>)
  {
    if (Rpc::id == id) {
      resp_t<Rpc> resp = {};
      deserializer(resp).or_throw();
      auto iter = outstanding_rpcs.find(seq);
      assert(iter != outstanding_rpcs.end());
      auto ctx = iter->second;
      static_cast<RpcContext<Rpc> *>(ctx)->resp.set_value(resp);
      outstanding_rpcs.erase(iter);
      delete ctx;
      return true;
    }
    return false;
  }

  template <typename Rpcs, size_t... Is>
  bool dispatch_resp_impl(int64_t seq, rpc_id_t id, Deserializer &deserializer, std::index_sequence<Is...>) {
    return (dispatch_normal_resp<std::tuple_element_t<Is, Rpcs>>(seq, id, deserializer) || ...);
  }

  template <typename Rpcs>
  bool dispatch_resp(int64_t seq, rpc_id_t id, Deserializer &deserializer) {
    return dispatch_resp_impl<Rpcs>(seq, id, deserializer, std::make_index_sequence<std::tuple_size_v<Rpcs>>());
  }

  template <Rpc Rpc>
  bool dispatch_oneway_req(rpc_id_t id, Deserializer &deserializer)
    requires(is_oneway_v<Rpc>)
  {
    if (Rpc::id == id) {
      req_t<Rpc> req = {};
      deserializer(req).or_throw();
      std::get<handler_t<Rpc>>(oneway_rpc_handler[Rpc::id])(req);
      return true;
    }
    return false;
  }

  template <Rpc Rpc>
  bool dispatch_normal_req(int64_t seq, rpc_id_t id, Deserializer &deserializer, Serializer &serializer)
    requires(!is_oneway_v<Rpc>)
  {
    if (Rpc::id == id) {
      req_t<Rpc> req = {};
      deserializer(req).or_throw();
      resp_t<Rpc> resp = std::get<handler_t<Rpc>>(normal_rpc_handler[Rpc::id])(req);
      auto h = PayloadHeader().as_resp().with_seq(seq).with_id(id);
      DEBUG("send seq: {}, id: {}", h.seq, h.id);
      serializer(h, resp).or_throw();
      return true;
    }
    return false;
  }

  template <typename Rpcs, size_t... Is>
  bool dispatch_reqs_impl(int64_t seq, rpc_id_t id, Deserializer &deserializer, Serializer &serializer,
                          std::index_sequence<Is...>) {
    return (dispatch_normal_req<std::tuple_element_t<Is, Rpcs>>(seq, id, deserializer, serializer) || ...);
  }

  template <typename Rpcs>
  bool dispatch_reqs(int64_t seq, rpc_id_t id, Deserializer &deserializer, Serializer &serializer) {
    return dispatch_reqs_impl<Rpcs>(seq, id, deserializer, serializer,
                                    std::make_index_sequence<std::tuple_size_v<Rpcs>>());
  }

  template <typename Rpcs, size_t... Is>
  bool dispatch_reqs_impl(rpc_id_t id, Deserializer &deserializer, std::index_sequence<Is...>) {
    return (dispatch_oneway_req<std::tuple_element_t<Is, Rpcs>>(id, deserializer) || ...);
  }

  template <typename Rpcs>
  bool dispatch_reqs(rpc_id_t id, Deserializer &deserializer) {
    return dispatch_reqs_impl<Rpcs>(id, deserializer, std::make_index_sequence<std::tuple_size_v<Rpcs>>());
  }

  template <typename Fn>
  bool serve_once([[maybe_unused]] size_t idx, Fn &fn) {
    DEBUG("worker {} post recv", idx);
    OpContext recv_ctx(Op::Recv, acquire_recv_buffer());
    auto recv_f = e.post_recv(recv_ctx);
    while (true) {
      if (recv_f.wait_for(1s) != boost::fibers::future_status::timeout || fn()) {
        break;
      }
    }
    if (fn()) {
      WARN("Not serving, exit");
      return false;
    }
    auto n_recv = recv_f.get();
    if (n_recv <= 0) {
      WARN("Fail to read payload, release buffer and exit, errno: {}", -n_recv);
      release_recv_buffer(recv_ctx.l_buf);
      return false;
    }
    DEBUG("worker {} recv: {}", idx, n_recv);
    auto deserializer = Deserializer(recv_ctx.l_buf);
    PayloadHeader h = {};
    deserializer(h).or_throw();
    if (h.is_resp()) {
      die("Payload is not request, seq: {}, id: {}", h.seq, h.id);
    }

    if constexpr (!std::is_void_v<OnewayRpcs>) {
      if (h.is_oneway_req()) {
        DEBUG("recv oneway seq: {} id: {}", h.seq, h.id);
        if (!dispatch_reqs<OnewayRpcs>(h.id, deserializer)) {
          die("Mismatch rpc id, got {}", h.id);
        }
        release_recv_buffer(recv_ctx.l_buf);
        return true;
      }
    } else {
      assert(!h.is_oneway_req());
    }

    DEBUG("recv normal seq: {} id: {}", h.seq, h.id);
    OpContext send_ctx(Op::Send, acquire_send_buffer());
    auto serializer = Serializer(send_ctx.l_buf);
    if (!dispatch_reqs<NormalRpcs>(h.seq, h.id, deserializer, serializer)) {
      die("Mismatch rpc id, got {}", h.id);
    }
    release_recv_buffer(recv_ctx.l_buf);

    DEBUG("worker {} post send {}", idx, serializer.position());
    send_ctx.len = serializer.position();
    auto send_f = e.post_send(send_ctx);
    while (true) {
      if (send_f.wait_for(1s) != boost::fibers::future_status::timeout || fn()) {
        break;
      }
    }
    if (fn()) {
      WARN("Not serving, exit");
      return false;
    }
    auto n_send = send_f.get();
    if (n_send <= 0) {
      WARN("Fail to send payload, release buffer and exit, errno: {}", -n_send);
      release_send_buffer(send_ctx.l_buf);
      return false;
    }
    DEBUG("worker {} send: {}", idx, n_send);

    release_send_buffer(send_ctx.l_buf);
    return true;
  }

  LocalBuffer &acquire_recv_buffer() {
    while (true) {
      if (auto buf = recv_bufs.acquire_one(); buf.has_value()) {
        return buf.value();
      } else {
        boost::this_fiber::yield();
      }
    }
  }

  void release_recv_buffer(LocalBuffer &buf) { recv_bufs.release_one(static_cast<RpcBuffer &>(buf)); }

  LocalBuffer &acquire_send_buffer() {
    while (true) {
      if (auto buf = send_bufs.acquire_one(); buf.has_value()) {
        return buf.value();
      } else {
        boost::this_fiber::yield();
      }
    }
  }

  void release_send_buffer(LocalBuffer &buf) { send_bufs.release_one(static_cast<RpcBuffer &>(buf)); }

  void progress_until(std::function<bool()> &&predictor) {
    while (!(outstanding_rpcs.empty() && active_workers == 0 && predictor())) {
      if (!e.progress()) {
        boost::this_fiber::yield();
      }
    }
  }

  std::mutex mu;  // for only one thread usage
  const Config config;
  ConnectionHolder<b> &holder;
  std::unordered_map<rpc_id_t, NormalRpcHandler> normal_rpc_handler;
  std::unordered_map<rpc_id_t, OnewayRpcHandler> oneway_rpc_handler;
  RpcBufferPool send_bufs;
  RpcBufferPool recv_bufs;
  Endpoint e;
  uint32_t active_workers = 0;
  rpc_seq_t current_seq = 1;
  std::unordered_map<rpc_seq_t, RpcContextBase *> outstanding_rpcs;
};

template <Backend b, Rpc... rpcs>
class TransportGuard : Noncopyable, Nonmovable {
 public:
  TransportGuard(Transport<b, rpcs...> &t_) : t(t_), l(t.mu) {
    poller = boost::fibers::fiber([this]() { t.progress_until([this] { return exit; }); });
    DEBUG("Transport attach to current thread");
  }
  ~TransportGuard() {
    exit = true;
    poller.join();
    DEBUG("Transport dettach with current thread");
  }

 private:
  bool exit = false;
  Transport<b, rpcs...> &t;
  boost::fibers::fiber poller;
  std::lock_guard<std::mutex> l;
};

}  // namespace dpx
