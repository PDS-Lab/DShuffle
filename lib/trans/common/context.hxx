#pragma once

#include "common/context_base.hxx"
#include "memory/local_buffer.hxx"
#include "trans/concept/rpc.hxx"

namespace dpx {

struct OpContext : public ContextBase {
  LocalBuffer &l_buf;
  size_t len = 0;
  size_t tx_size = 0;

  OpContext(Op op_, LocalBuffer &l_buf_) : ContextBase(op_), l_buf(l_buf_), len(l_buf_.size()) {}
  OpContext(Op op_, LocalBuffer &l_buf_, size_t len_) : ContextBase(op_), l_buf(l_buf_), len(len_) {}
};

struct BulkContext : public OpContext {
  MemoryRegion r_buf;

  BulkContext(Op op_, LocalBuffer &l_buf_, MemoryRegion r_buf_)
      : OpContext(op_, l_buf_, r_buf_.size()), r_buf(r_buf_) {}
};

template <Rpc Rpc>
using resp_promise_t = boost::fibers::promise<resp_t<Rpc>>;
template <Rpc Rpc>
using resp_future_t = boost::fibers::future<resp_t<Rpc>>;

struct RpcContextBase {};

template <Rpc Rpc>
struct RpcContext : RpcContextBase {
  resp_promise_t<Rpc> resp = {};
};

}  // namespace dpx
