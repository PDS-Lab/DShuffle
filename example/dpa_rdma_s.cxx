#include "trans/transport.hxx"

int main(int argc, char* argv[]) {
  auto dev = dpx::doca::Device::open_by_ibdev_name("mlx5_0");
  dpx::ConnectionHolder<dpx::Backend::DOCA_RDMA> c(dev, {
                                                            .passive = true,
                                                            .enable_grh = true,
                                                            .local_ip = "192.168.200.20",
                                                            .local_port = 10086,
                                                        });
  dpx::doca::Buffers sbs(dev, 32, 1024);
  dpx::doca::Buffers rbs(dev, 32, 1024);
  dpx::doca::rdma::Endpoint e(dev, sbs, rbs, false);
  c.associate(e);
  c.establish_connections();

  std::atomic_bool stop = false;
  boost::fibers::fiber f([&]() {
    while (!stop) {
      if (!e.progress()) {
        boost::this_fiber::yield();
      }
    }
  });
  boost::fibers::fiber f2([&]() {
    INFO("init");
    std::vector<dpx::OpContext> ops;
    for (uint32_t i = 0; i < 32; ++i) {
      ops.emplace_back(dpx::Op::Recv, rbs[i]);
    }
    INFO("post");
    std::vector<dpx::op_res_future_t> res_fs;
    for (auto& op : ops) {
      res_fs.emplace_back(e.post_recv(op));
    }
    INFO("wait");
    for (auto& res_f : res_fs) {
      INFO("{}", res_f.get());
    }
    INFO("done");
  });
  f2.join();
  stop = true;
  f.join();
  c.terminate_connections();
  return 0;
}