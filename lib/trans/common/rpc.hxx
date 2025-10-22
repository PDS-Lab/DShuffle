#pragma once

#include "trans/concept/rpc.hxx"

namespace dpx {

struct [[gnu::packed]] PayloadHeader {
  uint32_t flags = 0;
  rpc_seq_t seq = 0;
  rpc_id_t id = 0;

  PayloadHeader &with_seq(rpc_seq_t seq_) {
    seq = seq_;
    return *this;
  }

  PayloadHeader &with_id(rpc_id_t id_) {
    id = id_;
    return *this;
  }

  PayloadHeader &as_resp() {
    flags |= resp_bit;
    return *this;
  }

  PayloadHeader &as_oneway_req() {
    flags |= oneway_req_bit;
    return *this;
  }

  inline static constexpr const uint32_t resp_bit = 0x8000'0000;

  inline static constexpr const uint32_t oneway_req_bit = 0x4000'0000;

  bool is_resp() const { return flags & resp_bit; }

  bool is_oneway_req() const { return flags & oneway_req_bit; }
};

static_assert(sizeof(PayloadHeader) == sizeof(uint64_t) * 2, "PayloadHeader size should be 16 bytes");

}  // namespace dpx
