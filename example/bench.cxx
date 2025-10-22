#include <pthread.h>
#include <sched.h>
#include <spdlog/spdlog.h>

#include <args.hxx>
#include <array>
#include <boost/fiber/fiber.hpp>
#include <future>
#include <glaze/glaze.hpp>
#include <thread>
#include <vector>

#include "trans/transport.hxx"
#include "util/fatal.hxx"
#include "util/literal.hxx"
#include "util/timer.hxx"

using namespace dpx::literal;

template <typename... T>
constexpr inline auto make_array(T&&... values)
    -> std::array<typename std::decay<typename std::common_type<T...>::type>::type, sizeof...(T)> {
  return {std::forward<T>(values)...};
}

std::string human_readable_bytes(double bytes) {
  constexpr static auto units = make_array("B", "KB", "MB", "GB", "TB", "PB");
  for (auto i = 0uz; i < units.size(); ++i) {
    bytes /= 1024.;
    if (bytes < 1024.) {
      return std::format("{:0.2}{}", bytes, units[i]);
    }
  }
  return std::format("{:0.2}{}", bytes, units.back());
}

void bind_to_core(size_t core_idx) {
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(core_idx, &cpuset);
  if (auto ec = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset); ec != 0) {
    die("Fail to set affinity for thread, errno: {}", errno);
  }
}

void set_max_priority() {
  sched_param param = {
      .sched_priority = sched_get_priority_max(SCHED_FIFO),
  };
  if (auto ec = pthread_setschedparam(pthread_self(), SCHED_FIFO, &param); ec != 0) {
    die("Fail to set sched priority for thread, errno: {}", errno);
  }
}

template <typename Fn>
std::packaged_task<uint64_t(void)> wrap(Fn& fn, [[maybe_unused]] size_t core_idx) {
  return std::packaged_task<uint64_t(void)>([&]() {
    // bind_to_core(core_idx);
    // set_max_priority();
    dpx::Timer t;
    fn();
    return t.elapsed_us();
  });
}

template <typename Fn>
std::vector<uint64_t> run_threads(Fn& fn, const std::vector<size_t>& core_set) {
  std::vector<std::future<uint64_t>> result_fs;
  result_fs.reserve(core_set.size());
  for (auto core_idx : core_set) {
    auto task = wrap(fn, core_idx);
    result_fs.emplace_back(task.get_future());
    std::thread(std::move(task)).detach();
  }
  std::vector<uint64_t> results;
  results.reserve(core_set.size());
  for (auto& f : result_fs) {
    results.emplace_back(f.get());
  }
  return results;
}

template <typename Fn>
std::vector<uint64_t> run_threads(size_t thread_num, Fn& fn) {
  std::vector<size_t> core_set(thread_num);
  std::iota(core_set.begin(), core_set.end(), 0);
  return run_threads(fn, core_set);
}

template <typename Fn>
void run_fibers(size_t fiber_num, Fn& fn) {
  std::vector<boost::fibers::fiber> fs;
  fs.reserve(fiber_num);
  for (auto i = 0uz; i < fiber_num; ++i) {
    fs.emplace_back(fn);
  }
  for (auto& f : fs) {
    f.join();
  }
}

args::ArgumentParser p("DPX Transport Benchmark");
args::HelpFlag help(p, "help", "display this help menu", {'h', "help"});
args::MapFlag<std::string, dpx::Backend> backend(p, "backend", "TCP, Comch or RDMA", {"backend"},
                                                 std::unordered_map<std::string, dpx::Backend>{
                                                     {"TCP", dpx::Backend::TCP},
                                                     {"Comch", dpx::Backend::DOCA_Comch},
                                                     {"RDMA", dpx::Backend::DOCA_RDMA},
                                                 },
                                                 args::Options::Required);
args::MapFlag<std::string, dpx::Side> side(p, "side", "Client or Server", {"side"},
                                           std::unordered_map<std::string, dpx::Side>{
                                               {"Client", dpx::Side::ClientSide},
                                               {"Server", dpx::Side::ServerSide},
                                           },
                                           args::Options::Required);
args::MapFlag<std::string, dpx::Where> where(p, "where", "Host or DPU", {"where"},
                                             std::unordered_map<std::string, dpx::Where>{
                                                 {"Host", dpx::Where::Host},
                                                 {"DPU", dpx::Where::DPU},
                                             },
                                             args::Options::Required);

args::ValueFlag<std::string> local_ip(p, "local ip", "local ip", {"local_ip"}, "");
args::ValueFlag<uint16_t> local_port(p, "local port", "local port", {"local_port"}, 0);
args::ValueFlag<std::string> remote_ip(p, "remote ip", "remote ip", {"remote_ip"}, "");
args::ValueFlag<uint16_t> remote_port(p, "remote port", "remote port", {"remote_port"}, 0);
args::ValueFlag<std::string> comch_name(p, "comch name", "comch name", {"comch_name"}, "");
args::ValueFlag<std::string> dev_identity(p, "device identity", "device pci address or ib device name", {"dev_id"}, "");
args::ValueFlag<std::string> rep_pci_address(p, "representor pci address", "representor pci address", {"rep_pci_addr"},
                                             "");

args::Flag rpc(p, "rpc", "run rpc bench", {"rpc"}, false);
args::Flag bulk(p, "bulk", "run bulk bench", {"bulk"}, false);
args::ValueFlag<uint32_t> n_thread(p, "n thread", "n thread", {"n_thread"}, 1);
args::ValueFlag<uint32_t> n_fiber(p, "n fiber", "n fiber", {"n_fiber"}, 1);
args::ValueFlag<uint32_t> n_req(p, "n request", "n request", {"n_req"}, 10);
args::ValueFlag<uint32_t> req_size(p, "rpc request size", "rpc request size, in bytes", {"req_size"}, 4096);
args::ValueFlag<uint32_t> bulk_size(p, "bulk request size", "bulk request size, in MB", {"bulk_size"}, 4);

struct Params {
  dpx::Backend b;
  dpx::Where w;
  dpx::Side s;
  dpx::Config config;
  dpx::ConnectionParam<dpx::Backend::TCP> tcp;
  dpx::ConnectionParam<dpx::Backend::DOCA_Comch> comch;
  dpx::ConnectionParam<dpx::Backend::DOCA_RDMA> rdma;
} params;

void parse_args(int argc, char* argv[]) {
  try {
    p.ParseCLI(argc, argv);
  } catch (args::Help) {
    std::cout << p;
    exit(0);
  } catch (args::ParseError e) {
    std::cerr << e.what() << std::endl << std::endl << p;
    exit(-1);
  } catch (args::ValidationError e) {
    std::cerr << e.what() << std::endl << std::endl << p;
    exit(-1);
  }

  params = {
      .b = args::get(backend),
      .w = args::get(where),
      .s = args::get(side),
      .config =
          {
              .queue_depth = args::get(n_fiber),
              .max_rpc_msg_size = args::get(req_size),
          },
      .tcp =
          {
              .passive = args::get(side) == dpx::Side::ServerSide,
              .remote_ip = args::get(remote_ip),
              .local_ip = args::get(local_ip),
              .remote_port = args::get(remote_port),
              .local_port = args::get(local_port),
          },
      .comch =
          {
              .passive = args::get(where) == dpx::Where::DPU,
              .name = args::get(comch_name),
          },
      .rdma =
          {
              .passive = args::get(side) == dpx::Side::ServerSide,
              .enable_grh = true,
              .remote_ip = args::get(remote_ip),
              .local_ip = args::get(local_ip),
              .remote_port = args::get(remote_port),
              .local_port = args::get(local_port),
          },
  };
}

struct EchoRpc : dpx::RpcBase<"Echo", std::string, std::string> {};

std::string generate_string(size_t length) {
  static constexpr const char char_set[] =
      "0123456789"
      "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
      "abcdefghijklmnopqrstuvwxyz";
  std::string result(length, 0);
  std::sample(char_set, char_set, result.begin(), length, std::mt19937{});
  return result;
}

std::string generate_payload() {
  size_t size = args::get(req_size);
  size_t meta_size = sizeof(uint32_t) + sizeof(dpx::PayloadHeader);
  assert(size > meta_size);
  std::string payload = generate_string(size - meta_size);
  {
    dpx::PayloadHeader h = {};
    dpx::naive::OwnedBuffer b(size);
    auto s = dpx::Serializer(b);
    s(h, payload).or_throw();
    DEBUG("{} {} {}", s.position(), size, meta_size);
  }
  return payload;
}

template <typename Transport, typename BorrowedBuffer>
void run(Transport& t, BorrowedBuffer& l_buf) {
  if (params.s == dpx::Side::ServerSide) {
    dpx::TransportGuard g(t);
    t.template register_handler<EchoRpc>([](const dpx::req_t<EchoRpc>& req) { return req; });
    t.register_bulk_handler([&t, &l_buf](dpx::MemoryRegion& r_buf) {
#if SPDLOG_ACTIVE_LEVEL <= SPDLOG_LEVEL_DEBUG
      memset(l_buf.data(), 'B', l_buf.size());
#endif
      DEBUG("bulk write {}", r_buf.size());
      auto n_read = t.bulk_read(l_buf, r_buf);
      DEBUG("bulk read {}", n_read);
      DEBUG("begin 100:\n{}\nend 100:\n{}", dpx::Hexdump(l_buf.data(), 100),
            dpx::Hexdump(l_buf.data() + l_buf.size() - 100, 100));
      return n_read;
    });
    t.show_rpc_infos();
    t.serve();
  } else {
    if (args::get(rpc)) {
      auto payload = generate_payload();
      auto rpc_bench = [&]() {
        dpx::TransportGuard g(t);
        auto fn = [&]() {
          auto n_request = args::get(n_req);
          for (auto i = 0uz; i < n_request; ++i) {
            [[maybe_unused]] auto result = t.template call<EchoRpc>(payload).get();
          }
        };
        run_fibers(args::get(n_fiber), fn);
      };
      auto elapsed_us = run_threads(1, rpc_bench);
      INFO("{}us", elapsed_us[0]);
    }
    if (args::get(bulk)) {
      auto bulk_bench = [&]() {
        dpx::TransportGuard g(t);
        auto fn = [&]() {
          auto n_request = args::get(n_req);
          for (auto i = 0uz; i < n_request; ++i) {
#if SPDLOG_ACTIVE_LEVEL <= SPDLOG_LEVEL_DEBUG
            memset(l_buf.data(), 'A', l_buf.size());
#endif
            [[maybe_unused]] auto n = t.bulk(l_buf).get();
          }
        };
        run_fibers(1, fn);
      };
      auto elapsed_us = run_threads(1, bulk_bench);
      INFO("{}us", elapsed_us[0]);
    }
  }
}

void dispatch_and_run() {
  auto l_buf_size = args::get(bulk_size) * 1_MB;
  if (params.b == dpx::Backend::TCP) {
    dpx::ConnectionHolder<dpx::Backend::TCP> c(params.tcp);
    dpx::Transport<dpx::Backend::TCP, EchoRpc> t(c, params.config);
    dpx::naive::Buffers l_buf(1, l_buf_size);
    c.establish_connections();
    run(t, l_buf[0]);
    c.terminate_connections();
  } else {
    if (params.w == dpx::Where::DPU && params.b == dpx::Backend::DOCA_Comch) {
      auto dev = dpx::doca::Device::open_by_pci_addr(args::get(dev_identity));
      dev.open_representor(args::get(rep_pci_address));
      dpx::ConnectionHolder<dpx::Backend::DOCA_Comch> c(dev, params.comch);
      dpx::Transport<dpx::Backend::DOCA_Comch, EchoRpc> t(dev, c, params.config);
      dpx::doca::Buffers l_buf(dev, 1, l_buf_size, DOCA_ACCESS_FLAG_PCI_READ_WRITE);
      c.establish_connections();
      if (params.s == dpx::Side::ClientSide) {
        dpx::TransportGuard g(t);
        t.register_memory(l_buf, dev);
      }
      run(t, l_buf[0]);
      c.terminate_connections();
    } else {
      if (params.b == dpx::Backend::DOCA_Comch) {
        auto dev = dpx::doca::Device::open_by_pci_addr(args::get(dev_identity));
        dpx::ConnectionHolder<dpx::Backend::DOCA_Comch> c(dev, params.comch);
        dpx::Transport<dpx::Backend::DOCA_Comch, EchoRpc> t(dev, c, params.config);
        dpx::doca::Buffers l_buf(dev, 1, l_buf_size, DOCA_ACCESS_FLAG_PCI_READ_WRITE);
        c.establish_connections();
        if (params.s == dpx::Side::ClientSide) {
          dpx::TransportGuard g(t);
          t.register_memory(l_buf, dev);
        }
        run(t, l_buf[0]);
        c.terminate_connections();
      } else {
        auto dev = dpx::doca::Device::open_by_ibdev_name(args::get(dev_identity));
        dpx::ConnectionHolder<dpx::Backend::DOCA_RDMA> c(dev, params.rdma);
        dpx::Transport<dpx::Backend::DOCA_RDMA, EchoRpc> t(dev, c, params.config);
        dpx::doca::Buffers l_buf(dev, 1, l_buf_size, DOCA_ACCESS_FLAG_RDMA_READ | DOCA_ACCESS_FLAG_RDMA_WRITE);
        c.establish_connections();
        if (params.s == dpx::Side::ClientSide) {
          dpx::TransportGuard g(t);
          t.register_memory(l_buf, dev);
        }
        run(t, l_buf[0]);
        c.terminate_connections();
      }
    }
  }
}

int main(int argc, char* argv[]) {
  spdlog::set_level(spdlog::level::trace);
  parse_args(argc, argv);
  dispatch_and_run();
  return 0;
}