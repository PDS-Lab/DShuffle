#include "trans/concept/rpc.hxx"

struct MountRequest {
  int dummy;
};
struct MountRpc : dpx::RpcBase<"Mount", MountRequest, int> {};

struct UmountRequest {
  int dummy;
};
struct UmountRpc : dpx::RpcBase<"Umount", UmountRequest, int> {};

inline static constexpr const char* direct_spill_server_name = "direct_spill";
