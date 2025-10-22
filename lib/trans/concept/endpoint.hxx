#pragma once

#include <concepts>

namespace dpx {

class EndpointBase;
struct OpContext;
struct BulkContext;
class RemoteBuffer;

template <typename T>
concept CanBeUsedAsEndpoint =
    std::derived_from<T, EndpointBase> && requires(T e, OpContext &c1, BulkContext &c2, RemoteBuffer r_buf) {
      { e.progress() };
      { e.post_recv(c1) };
      { e.post_send(c1) };
      { e.post_write(c2) };
      { e.post_read(c2) };
      { e.register_remote_memory(r_buf) };
      { e.unregister_remote_memory() };
    };

}  // namespace dpx
