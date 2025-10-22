#include "spdk_spill/file_frag.hxx"
#include "trans/concept/rpc.hxx"

enum ErrorCode {
  OK = 0,
  NOT_OPEN = 10086,
  INVALID_TARGET_SIZE,
};

struct CreateRequest {
  std::string file_name;
};
struct CreateRpc : dpx::RpcBase<"Create", CreateRequest, int> {};

struct OpenRequest {
  std::string file_name;
};
struct OpenRpc : dpx::RpcBase<"Open", OpenRequest, int> {};

struct CloseRequest {
  int fd;
};
struct CloseRpc : dpx::RpcBase<"Close", CloseRequest, int> {};

struct AllocateRequest {
  int fd;
  size_t size;
};
struct AllocateRpc : dpx::RpcBase<"Allocate", AllocateRequest, int> {};

struct FilefragRequest {
  int fd;
};
struct FilefragRpc : dpx::RpcBase<"Filefrag", FilefragRequest, dpx::spill::FileFragments> {};

inline static constexpr const char* file_server_name = "direct_spill";

inline static constexpr const size_t default_lba_size = 512;