#pragma once

#include <spdk/env.h>

#include "util/fatal.hxx"
#include "util/logger.hxx"
#include "util/noncopyable.hxx"
#include "util/nonmovable.hxx"

namespace dpx::spill {

class SpdkEnv : Noncopyable, Nonmovable {
 public:
  SpdkEnv(std::string hugedir, size_t reserved_mem_size_mb) {
    spdk_env_opts opts;
    spdk_env_opts_init(&opts);
    opts.name = "spdk_env";
    opts.mem_size = reserved_mem_size_mb;
    opts.opts_size = sizeof(spdk_env_opts);
    opts.hugedir = hugedir.c_str();
    INFO("Preallocated memory size: {} MB", opts.mem_size);
    if (auto rc = spdk_env_init(&opts); rc != 0) {
      die("Fail to init spdk env");
    }
    INFO("SPDK env up");
  }
  ~SpdkEnv() {
    spdk_env_fini();
    INFO("SPDK env down");
  }
};

}  // namespace dpx::spill