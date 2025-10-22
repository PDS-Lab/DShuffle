#pragma once

#include <boost/fiber/future.hpp>

#include "util/enum_formatter.hxx"

namespace dpx {

enum class Op : uint32_t {
  Send,
  Recv,
  Read,
  Write,
};

using op_res_promise_t = boost::fibers::promise<ssize_t>;
using op_res_future_t = boost::fibers::future<ssize_t>;

struct ContextBase {
  Op op;
  op_res_promise_t op_res = {};

  explicit ContextBase(Op op_) : op(op_) {}
};

}  // namespace dpx

// clang-format off
EnumFormatter(dpx::Op,
    [dpx::to_underlying(dpx::Op::Send)] = "Send",
    [dpx::to_underlying(dpx::Op::Recv)] = "Recv",
    [dpx::to_underlying(dpx::Op::Read)] = "Read",
    [dpx::to_underlying(dpx::Op::Write)] = "Write",
);
// clang-format on