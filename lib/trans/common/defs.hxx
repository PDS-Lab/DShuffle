#pragma once

#include <cstdint>

#include "util/enum_formatter.hxx"

namespace dpx {

enum class Status : uint32_t {
  Idle,
  Ready,
  Running,
  Stopping,
  Exited,
};

enum class Side : uint32_t {
  ClientSide,
  ServerSide,
};

enum class Where : uint32_t {
  Host,
  DPU,
};

enum class Backend : uint32_t {
  TCP,
  // Verbs,
  DOCA_Comch,
  DOCA_RDMA,
};

template <Backend b>
struct BackendTag {};

}  // namespace dpx

// clang-format off
EnumFormatter(dpx::Status,
    [dpx::to_underlying(dpx::Status::Idle)]     = "Idle",
    [dpx::to_underlying(dpx::Status::Ready)]    = "Ready",
    [dpx::to_underlying(dpx::Status::Running)]  = "Running",
    [dpx::to_underlying(dpx::Status::Stopping)] = "Stopping",
    [dpx::to_underlying(dpx::Status::Exited)]   = "Exited",
);
EnumFormatter(dpx::Side,
    [dpx::to_underlying(dpx::Side::ServerSide)] = "Server",
    [dpx::to_underlying(dpx::Side::ClientSide)] = "Client",
);
EnumFormatter(dpx::Where,
    [dpx::to_underlying(dpx::Where::Host)] = "Host",
    [dpx::to_underlying(dpx::Where::DPU)] = "DPU",
);
EnumFormatter(dpx::Backend,
    [dpx::to_underlying(dpx::Backend::TCP)] = "TCP",
    // [dpx::to_underlying(dpx::Backend::Verbs)] = "Verbs",
    [dpx::to_underlying(dpx::Backend::DOCA_Comch)] = "DOCA DMA",
    [dpx::to_underlying(dpx::Backend::DOCA_RDMA)] = "DOCA RDMA",
);
// clang-format on
