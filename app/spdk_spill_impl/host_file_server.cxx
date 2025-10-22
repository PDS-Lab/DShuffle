#include <unordered_map>

#include "file_rpc.hxx"
#include "trans/transport.hxx"

int main() {
  std::string working_dir = "/home/lsc/dpx/.test_spill";  // absolute path
  std::string pci_addr = "0000:43:00.0";                  // device pci addr

  std::unordered_map<int, size_t> opened_files;

  auto dev = dpx::doca::Device::open_by_pci_addr(pci_addr);
  auto ch = dpx::ConnectionHolder<dpx::Backend::DOCA_Comch>(dev, dpx::ConnectionParam<dpx::Backend::DOCA_Comch>{
                                                                     .passive = false,
                                                                     .name = file_server_name,
                                                                 });
  auto t = dpx::Transport<dpx::Backend::DOCA_Comch, CreateRpc, OpenRpc, CloseRpc, AllocateRpc, FilefragRpc>(
      dev, ch,
      dpx::Config{
          .queue_depth = 16,
          .max_rpc_msg_size = 16_KB,
      });

  ch.establish_connections();

  t.register_handler<CreateRpc>([&](const CreateRequest& req) -> int {
    auto s = std::format("{}/{}", working_dir, req.file_name);
    INFO("create {}", s);
    int fd = ::creat(s.c_str(), 0644);
    if (fd != -1) {
      opened_files.emplace(fd, 0);
      INFO("OK, fd: {}", fd);
      return fd;
    } else {
      INFO("Fail, errno: {}", errno);
      return -errno;
    }
  });

  t.register_handler<OpenRpc>([&](const OpenRequest& req) -> int {
    auto s = std::format("{}/{}", working_dir, req.file_name);
    INFO("open {}", s);
    int fd = ::open(s.c_str(), O_RDWR);
    if (fd != -1) {
      struct stat64 s = {};
      auto rc = ::fstat64(fd, &s);
      assert(rc = 0);
      opened_files.emplace(fd, s.st_size);
      INFO("OK, fd: {}, size: {}", fd, s.st_size);
      return fd;
    } else {
      INFO("Fail, errno: {}", errno);
      return -errno;
    }
  });

  t.register_handler<AllocateRpc>([&](const AllocateRequest& req) -> int {
    auto iter = opened_files.find(req.fd);
    if (iter == opened_files.end()) {
      return -NOT_OPEN;
    }
    if (req.size % default_lba_size) {
      return -INVALID_TARGET_SIZE;
    }
    int rc = ::fallocate64(req.fd, 0, iter->second, req.size);
    INFO("allocate {} from {} to {}", req.fd, iter->second, iter->second + req.size);
    if (rc != -1) {
      iter->second += req.size;
      INFO("OK");
      return OK;
    } else {
      INFO("Fail, errno: {}", errno);
      return -errno;
    }
  });

  t.register_handler<CloseRpc>([&](const CloseRequest& req) -> int {
    auto iter = opened_files.find(req.fd);
    if (iter == opened_files.end()) {
      return -NOT_OPEN;
    }
    INFO("close {}", req.fd);
    int rc = ::close(req.fd);
    if (rc != -1) {
      opened_files.erase(iter);
      INFO("OK");
      return OK;
    } else {
      INFO("Fail, errno: {}", errno);
      return -errno;
    }
  });

  t.register_handler<FilefragRpc>([&](const FilefragRequest& req) -> dpx::spill::FileFragments {
    if (opened_files.find(req.fd) == opened_files.end()) {
      return {};
    }
    INFO("get frag of {} with lba: {}", req.fd, default_lba_size);
    auto frags = dpx::spill::get_file_fragments(req.fd, default_lba_size);
    for (auto& frag : frags) {
      INFO("{} {} {} {} {}", frag.logic_offset, frag.physical_offset, frag.length, frag.start_lba, frag.lba_count);
    }
    return frags;
  });

  {
    dpx::TransportGuard g(t);
    t.serve();
  }

  ch.terminate_connections();

  for (auto [fd, _] : opened_files) {
    ::close(fd);
  }

  return 0;
}