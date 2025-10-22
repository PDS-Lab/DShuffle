#pragma once

#include <latch>

#include "native/offload.hxx"
#include "native/worker.hxx"

namespace dpx {

template <Backend b, Rpc... rpcs>
class SpillWorker : public Worker<b, rpcs...> {
  using Base = Worker<b, rpcs...>;

 public:
  SpillWorker(doca::Device& ch_dev, doca::Device& dma_dev, const ConnectionParam<b>& param, const Config& trans_conf,
              TaskQueue& task_q_, std::atomic_bool& running_)
      : Base(ch_dev, dma_dev, param, trans_conf, running_), task_q(task_q_) {}
  SpillWorker(doca::Device& dev, const ConnectionParam<b>& param, const Config& trans_conf, TaskQueue& task_q_,
              std::atomic_bool& running_)
      : Base(dev, param, trans_conf, running_), task_q(task_q_) {}
  ~SpillWorker() {}

  void run_as_producer(SpillBufferPool& spill_buffer_pool, std::latch& start_point, size_t core_idx) {
    Base::run(
        [&]() {
          INFO("Producer spill worker start");
          Base::t.register_bulk_handler([&](auto&& req) { return handle_spill_request(spill_buffer_pool, req); });
          Base::t.serve_until([this]() -> bool { return !Base::running; }, [&]() { start_point.arrive_and_wait(); });
          INFO("Producer spill worker stop");
        },
        core_idx);
  }

  void run_as_consumer(SpillBufferPool& spill_buffer_pool, size_t n_fiber, std::latch& start_point, size_t core_idx) {
    Base::run(
        [&, n_fiber]() {
          INFO("Consumer spill worker start");
          start_point.arrive_and_wait();
          Base::t.register_memory(spill_buffer_pool.buffers(), Base::bulk_dev);
          std::vector<boost::fibers::fiber> fs;
          for (auto i = 0uz; i < n_fiber; ++i) {
            INFO("start fiber {}", i);
            fs.emplace_back(boost::fibers::fiber([this]() { do_transfer(); }));
          }
          for (auto i = 0uz; i < n_fiber; ++i) {
            fs[i].join();
            INFO("stop fiber {}", i);
          }
          Base::t.unregister_memory(spill_buffer_pool.buffers());
          INFO("Consumer spill worker stop");
        },
        core_idx);
  }

 private:
  void do_transfer() {
    while (true) {
      while (Base::running && task_q.empty()) {
        boost::this_fiber::yield();
      }
      if (!Base::running) {
        break;
      }
      auto t = *task_q.front();
      task_q.pop();
      auto n_bulk = Base::t.bulk(t->buffer.underlying(), t->buffer.total_size()).get();
      DEBUG("transfer partition {} size {}", t->buffer.partition_id(), t->buffer.total_size());
      t->res.set_value(n_bulk);
    }
  }

  size_t handle_spill_request(SpillBufferPool& spill_buffer_pool, const MemoryRegion& rbuf) {
    auto buf_o = spill_buffer_pool.acquire_one();
    if (!buf_o.has_value()) {
      die("Too fast! No buffer to use");
    }
    DEBUG("read {} with length {}", (void*)rbuf.data(), rbuf.size());
    auto& buf = buf_o->get();
    auto n_read = Base::t.bulk_read(buf, rbuf);
    auto task = SpillTask(buf);
    if (task.buffer.total_size() != n_read) {
      die("Fail to read partition buffer, expected: {}, got: {}", task.buffer.total_size(), n_read);
    }
    DEBUG("Fetch {} of partition {}", n_read, task.buffer.partition_id());
    task_q.push(&task);
    size_t n_spill = task.res.get_future().get();
    if (n_spill != task.buffer.total_size()) {
      die("Fail to spill partition buffer, expected: {}, got: {}", task.buffer.total_size(), n_read);
    }
    spill_buffer_pool.release_one(buf);
    return n_read;
  }

  TaskQueue& task_q;
};

using LocalSpillWorker = SpillWorker<Backend::DOCA_Comch>;
using RemoteSpillWorker = SpillWorker<Backend::DOCA_RDMA>;

class DirectSpillWorker : public Worker<Backend::DOCA_Comch, MountRpc, UmountRpc> {
  using Base = Worker<Backend::DOCA_Comch, MountRpc, UmountRpc>;

 public:
  DirectSpillWorker(doca::Device& dev, const ConnectionParam<Backend::DOCA_Comch>& param, const Config& trans_conf,
                    TaskQueue& spill_q_, std::string mount_point_, std::string output_device_, size_t io_depth,
                    SpillBufferPool& bp, std::atomic_bool& running_)
      : Base(dev, param, trans_conf, running_),
        spill_q(spill_q_),
        mount_point(mount_point_),
        output_device(output_device_) {
    if (auto ec = io_uring_queue_init(io_depth, &ring, 0); ec < 0) {
      die("Fail to init ring, errno: {}", -ec);
    }
    auto& bs = bp.buffers();
    for (auto i = 0uz; i < bs.n_elements(); i++) {
      auto& b = bs[i];
      iovecs.push_back(b);
    }
    base = bs.data();
    piece_size = bs.piece_size();
    if (auto ec = io_uring_register_buffers(&ring, iovecs.data(), bs.n_elements()); ec < 0) {
      die("Fail to register buffers, errno: {}", -ec);
    }
    t.register_handler<MountRpc>([this](const MountRequest& req) -> int { return do_mount(req.max_n_partition); });
    t.register_handler<UmountRpc>([this](const UmountRequest& req) -> int { return do_umount(req.max_n_partition); });
  }
  ~DirectSpillWorker() { io_uring_queue_exit(&ring); }

  void run(std::latch& start_point, size_t core_idx) {
    Base::run(
        [&]() {
          // one fiber for file io
          auto io_poller = boost::fibers::fiber([this]() {
            INFO("IO poller start");
            progress_io();
            INFO("IO poller stop");
          });
          // one fiber for pull task from spill_q;
          auto spiller = boost::fibers::fiber([this]() {
            INFO("spiller start");
            submit_direct_spill();
            INFO("spiller stop");
          });
          Base::t.serve_until([&]() -> bool { return !running; }, [&]() -> void { start_point.arrive_and_wait(); });
          io_poller.join();
          spiller.join();
        },
        core_idx);
  }

 private:
  void submit_direct_spill() {
    while (true) {
      while (running && spill_q.empty()) {
        boost::this_fiber::yield();
      }
      if (!running) {
        break;
      }

      auto task = *spill_q.front();
      spill_q.pop();

      DEBUG("spill {} of partition {} at {}", task->buffer.actual_size(), task->buffer.partition_id(),
            (void*)task->buffer.underlying().data());

      auto sqe = io_uring_get_sqe(&ring);

      {
        std::unique_lock l(mu);
        c.wait(l, [&]() { return !running || files.size() == max_n_partition; });
        if (!running) {
          break;
        }
        auto& f = files[task->buffer.partition_id()];
        // io_uring_prep_write(sqe, f.fd, task->buffer.actual_data(), task->buffer.actual_size(), f.offset);
        auto idx = (task->buffer.underlying().data() - base) / piece_size;

        io_uring_prep_write_fixed(sqe, f.fd, task->buffer.actual_data(), task->buffer.actual_size(), f.offset, idx);

        f.offset += task->buffer.actual_size();
      }

      io_uring_sqe_set_data(sqe, task);
      if (auto ec = io_uring_submit(&ring); ec < 0) {
        die("Fail to submit sqe, errno: {}", -ec);
      }
      issued_io++;
    }
  }

  int do_mount(size_t max_n_partition_) {
    if (mounted) {
      die("Already mounted.");
    }
    max_n_partition = max_n_partition_;
    int rc = mount(output_device, mount_point);
    INFO("trigger mount, rc: {}, errno: {}", rc, errno);
    mounted = true;
    {
      std::lock_guard l(mu);
      files.reserve(max_n_partition);
      for (auto i = 0uz; i < max_n_partition; ++i) {
        PartitionFile f;
        auto fname = std::format("{}/p{}", mount_point, i);
        f.fd = open(fname.c_str(), O_CREAT | O_TRUNC | O_RDWR, 0644);
        if (f.fd == -1) {
          die("Fail to create partition file {}", fname);
        }
        INFO("open {}", fname);
        f.partition_id = i;
        f.offset = 0;
        files.emplace_back(std::move(f));
      }
    }
    c.notify_one();
    return rc;
  }

  int do_umount(size_t max_n_partition [[maybe_unused]]) {
    if (!mounted) {
      die("Not mounted.");
    }
    // WARN: still not safe
    while (!spill_q.empty() || issued_io != 0) {
      boost::this_fiber::sleep_for(1ms);
    }
    {
      std::lock_guard l(mu);
      for (auto& f : files) {
        INFO("close p{}", f.partition_id);
        close(f.fd);
      }
    }
    int rc = umount(mount_point);
    INFO("trigger umount, rc: {}, errno: {}", rc, errno);
    mounted = false;
    return rc;
  }

  void progress_io() {
    io_uring_cqe* cqe = nullptr;
    while (true) {
      while (running) {
        io_uring_peek_cqe(&ring, &cqe);
        if (cqe == nullptr) {
          boost::this_fiber::yield();
        } else {
          break;
        }
      }
      if (!running) {
        break;
      }

      auto task = reinterpret_cast<SpillTask*>(io_uring_cqe_get_data(cqe));

      DEBUG("spill {} of partition {}, res {}", task->buffer.actual_size(), task->buffer.partition_id(), cqe->res);

      if (cqe->res > 0) {
        task->res.set_value(task->buffer.total_size());
      } else {
        task->res.set_value(cqe->res);
      }
      io_uring_cqe_seen(&ring, cqe);
      issued_io--;
    }
  }

  struct PartitionFile {
    int fd = -1;
    size_t partition_id = -1;
    size_t offset = 0;
  };

  bool mounted = false;

  TaskQueue& spill_q;
  std::string mount_point;    // absolute path
  std::string output_device;  // absolute path

  boost::fibers::mutex mu;
  boost::fibers::condition_variable c;
  io_uring ring;
  size_t max_n_partition;
  size_t issued_io = 0;

  std::vector<iovec> iovecs;
  uint8_t* base = nullptr;
  size_t piece_size = 0;
  std::vector<PartitionFile> files;
};

template <Backend b, Rpc... rpcs>
class NaiveSpillWorker : public Worker<b, rpcs...> {
  using Base = Worker<b, rpcs...>;

 public:
  NaiveSpillWorker(doca::Device& ch_dev, doca::Device& dma_dev, const ConnectionParam<b>& param,
                   const Config& trans_conf, OffloadSpillTaskQueue& task_q_, std::atomic_bool& running_,
                   size_t core_idx)
      : Base(ch_dev, dma_dev, param, trans_conf, running_), task_q(task_q_) {
    Base::t.register_bulk_handler([this](auto&& req) { return handle_spill_request(req); });
    Base::run([this]() { Base::t.serve_until([this]() -> bool { return !Base::running; }); }, core_idx);
    INFO("Naive Spill Worker start");
  }
  NaiveSpillWorker(doca::Device& dev, const ConnectionParam<b>& param, const Config& trans_conf,
                   OffloadSpillTaskQueue& task_q_, std::atomic_bool& running_, size_t core_idx)
      : Base(dev, param, trans_conf, running_), task_q(task_q_) {
    Base::t.register_bulk_handler([this](auto&& req) { return handle_spill_request(req); });
    Base::run([this]() { Base::t.serve_until([this]() -> bool { return !Base::running; }); }, core_idx);
    INFO("Naive Spill Worker start");
  }
  ~NaiveSpillWorker() { INFO("Spill Worker destory"); }

 private:
  size_t handle_spill_request(const MemoryRegion& rbuf) {
    TRACE("read {} with length {}", (void*)rbuf.data(), rbuf.size());
    doca::Buffers buf(Base::bulk_dev, 1, rbuf.size(), DOCA_ACCESS_FLAG_PCI_READ_WRITE);
    auto& lbuf = buf[0];
    auto n_read = Base::t.bulk_read(lbuf, rbuf);
    TRACE("Fetch {} at remote {}", n_read, (void*)rbuf.data());
    OffloadSpillTask t{.res = {}, .buffer = &buf};
    task_q.push(&t);
    TRACE("push to task queue");
    size_t res = t.res.get_future().get();
    if (res != buf.size()) {
      die("Fail to spill");
    }
    return n_read;
  }

  OffloadSpillTaskQueue& task_q;
};

class HostSpillWorker : public Worker<Backend::DOCA_Comch> {
  using Base = Worker<Backend::DOCA_Comch>;

 public:
  HostSpillWorker(doca::Device& dev, const ConnectionParam<Backend::DOCA_Comch>& param, const Config& trans_conf,
                  std::string spill_dir_, size_t max_n_partition_, std::latch& start_point_, std::atomic_bool& running_,
                  size_t core_idx)
      : Base(dev, param, trans_conf, running_), io_q(64), spill_dir(spill_dir_), max_n_partition(max_n_partition_) {
    if (auto ec = io_uring_queue_init(64, &ring, 0); ec < 0) {
      die("Fail to init ring, errno: {}", -ec);
    }
    Base::t.register_bulk_handler([this](auto&& req) { return handle_spill_request(req); });
    Base::run(
        [&]() {
          // one fiber for file io
          pthread_setname_np(pthread_self(), "host_spill");
          auto io_poller = boost::fibers::fiber([this]() {
            INFO("IO poller start");
            progress_io();
            INFO("IO poller stop");
          });
          // one fiber for pull task from spill_q;
          auto spiller = boost::fibers::fiber([this]() {
            INFO("spiller start");
            submit_direct_spill();
            INFO("spiller stop");
          });
          Base::t.serve_until([this] { return !running; }, [&]() { start_point_.arrive_and_wait(); });
          io_poller.join();
          spiller.join();
        },
        core_idx);
  }
  ~HostSpillWorker() {
    Base::join();
    io_uring_queue_exit(&ring);
  }

  void create_partition_files(bool need_header) {
    {
      std::lock_guard l(mu);
      if (files.size() == max_n_partition) {
        return;
      }
      for (auto i = 0uz; i < max_n_partition; ++i) {
        PartitionFile f;
        auto fname = std::format("{}/p{}", spill_dir, i);
        f.fd = open(fname.c_str(), O_CREAT | O_TRUNC | O_RDWR, 0644);
        if (f.fd == -1) {
          die("Fail to create partition file {}", fname);
        }
        INFO("open {}", fname);
        f.partition_id = i;
        if (need_header) {
          uint32_t magic_number_and_version = 0x0500edac;
          write(f.fd, &magic_number_and_version, sizeof(uint32_t));
          f.offset = sizeof(uint32_t);
        } else {
          f.offset = 0;
        }
        files.emplace_back(std::move(f));
      }
      c.notify_all();
    }
  }

  void close_partition_files() {
    {
      std::lock_guard l(mu);
      if (files.size() == 0) {
        return;
      }
      for (auto& f : files) {
        INFO("close p{}", f.partition_id);
        close(f.fd);
      }
      files.resize(0);
    }
  }

  bool is_spilling() { return spilling; }

 private:
  void submit_direct_spill() {
    while (true) {
      while (running && io_q.empty()) {
        boost::this_fiber::yield();
      }
      if (!running) {
        break;
      }

      auto task = *io_q.front();
      io_q.pop();

      auto header = reinterpret_cast<PartitionDataHeader*>(task->buffer->data());

      DEBUG("spill {} of partition {} at {}", task->buffer->size(), header->partition_id, (void*)task->buffer->data());

      auto sqe = io_uring_get_sqe(&ring);
      {
        std::unique_lock l(mu);
        c.wait(l, [&]() { return !running || files.size() == max_n_partition; });
        if (!running) {
          break;
        }
        auto& f = files[header->partition_id];
        io_uring_prep_write(sqe, f.fd, task->buffer->data() + sizeof(PartitionDataHeader),
                            task->buffer->size() - sizeof(PartitionDataHeader), f.offset);
        f.offset += task->buffer->size() - sizeof(PartitionDataHeader);
      }

      io_uring_sqe_set_data(sqe, task);
      if (auto ec = io_uring_submit(&ring); ec < 0) {
        die("Fail to submit sqe, errno: {}", -ec);
      }
    }
  }

  void progress_io() {
    io_uring_cqe* cqe = nullptr;
    while (true) {
      int count = 0;
      while (running) {
        io_uring_peek_cqe(&ring, &cqe);
        if (cqe == nullptr) {
          if (count == 10000) {
            spilling = false;
          }
          boost::this_fiber::sleep_for(1ms);
        } else {
          break;
        }
        count++;
      }
      if (!running) {
        break;
      }

      spilling = true;

      auto task = reinterpret_cast<FileIOTask*>(io_uring_cqe_get_data(cqe));

      auto header = reinterpret_cast<PartitionDataHeader*>(task->buffer->data());

      DEBUG("spill {} of partition {}, res {}", task->buffer->size(), header->partition_id, cqe->res);

      if (cqe->res > 0) {
        task->res.set_value(task->buffer->size());
      } else {
        task->res.set_value(cqe->res);
      }
      io_uring_cqe_seen(&ring, cqe);
    }
  }

  size_t handle_spill_request(const MemoryRegion& rbuf) {
    doca::Buffers b(bulk_dev, 1, rbuf.size());
    auto& buf = b[0];
    DEBUG("read {} with length {}", (void*)rbuf.data(), rbuf.size());
    auto n_read = Base::t.bulk_read(buf, rbuf);
    // auto header = reinterpret_cast<PartitionDataHeader*>(buf.data());
    // if (header->length != n_read) {
    //   die("Fail to read partition buffer, expected: {}, got: {}", header->length, n_read);
    // }
    auto pid = *reinterpret_cast<uint32_t*>(buf.data());
    DEBUG("Fetch {} of partition {}", n_read, pid);
    // auto task = SpillTask(buf);
    auto task = FileIOTask{.res = {}, .buffer = &b};
    io_q.push(&task);
    size_t n_spill = task.res.get_future().get();
    if (n_spill != task.buffer->size()) {
      die("Fail to spill partition buffer, expected: {}, got: {}", task.buffer->size(), n_spill);
    }
    return n_read;
  }

 private:
  struct FileIOTask {
    op_res_promise_t res;
    doca::Buffers* buffer;
  };
  struct PartitionFile {
    int fd = -1;
    size_t partition_id = -1;
    size_t offset = 0;
  };
  rigtorp::SPSCQueue<FileIOTask*> io_q;
  std::string spill_dir;  // absolute path
  io_uring ring;
  std::mutex mu;
  std::condition_variable c;
  size_t max_n_partition;
  std::vector<PartitionFile> files;
  std::atomic_bool spilling;
};

class BufferredHostSpillWorker : public Worker<Backend::DOCA_Comch> {
  using Base = Worker<Backend::DOCA_Comch>;

 public:
  BufferredHostSpillWorker(doca::Device& dev, const ConnectionParam<Backend::DOCA_Comch>& param,
                           const Config& trans_conf, std::string spill_dir_, size_t n_buffer, size_t buffer_size,
                           size_t max_n_partition_, std::latch& start_point_, std::atomic_bool& running_,
                           size_t core_idx)
      : Base(dev, param, trans_conf, running_),
        bp(dev, n_buffer, buffer_size, DOCA_ACCESS_FLAG_PCI_READ_WRITE),
        io_q(64),
        spill_dir(spill_dir_),
        max_n_partition(max_n_partition_) {
    if (auto ec = io_uring_queue_init(64, &ring, 0); ec < 0) {
      die("Fail to init ring, errno: {}", -ec);
    }
    Base::t.register_bulk_handler([this](auto&& req) { return handle_spill_request(req); });
    Base::run(
        [&]() {
          // one fiber for file io
          auto io_poller = boost::fibers::fiber([this]() {
            INFO("IO poller start");
            progress_io();
            INFO("IO poller stop");
          });
          // one fiber for pull task from spill_q;
          auto spiller = boost::fibers::fiber([this]() {
            INFO("spiller start");
            submit_direct_spill();
            INFO("spiller stop");
          });
          Base::t.serve_until([this] { return !running; }, [&]() { start_point_.arrive_and_wait(); });
          io_poller.join();
          spiller.join();
        },
        core_idx);
  }
  ~BufferredHostSpillWorker() {
    Base::join();
    io_uring_queue_exit(&ring);
  }

  void create_partition_files(bool need_header) {
    {
      std::lock_guard l(mu);
      if (files.size() == max_n_partition) {
        return;
      }
      for (auto i = 0uz; i < max_n_partition; ++i) {
        PartitionFile f;
        auto fname = std::format("{}/p{}", spill_dir, i);
        f.fd = open(fname.c_str(), O_CREAT | O_TRUNC | O_RDWR, 0644);
        if (f.fd == -1) {
          die("Fail to create partition file {}", fname);
        }
        INFO("open {}", fname);
        f.partition_id = i;
        if (need_header) {
          uint32_t magic_number_and_version = 0x0500edac;
          write(f.fd, &magic_number_and_version, sizeof(uint32_t));
          f.offset = sizeof(uint32_t);
        } else {
          f.offset = 0;
        }
        files.emplace_back(std::move(f));
      }
      c.notify_all();
    }
  }

  void close_partition_files() {
    {
      std::lock_guard l(mu);
      if (files.size() == 0) {
        return;
      }
      for (auto& f : files) {
        INFO("close p{}", f.partition_id);
        close(f.fd);
      }
      files.resize(0);
    }
  }

  bool is_spilling() { return spilling; }

 private:
  void submit_direct_spill() {
    while (true) {
      while (running && io_q.empty()) {
        boost::this_fiber::yield();
      }
      if (!running) {
        break;
      }

      auto task = *io_q.front();
      io_q.pop();

      auto header = reinterpret_cast<PartitionDataHeader*>(task->buffer.data());

      INFO("append {} of partition {} at {} to file", header->length, header->partition_id, (void*)task->buffer.data());

      auto sqe = io_uring_get_sqe(&ring);
      {
        std::unique_lock l(mu);
        c.wait(l, [&]() { return !running || files.size() == max_n_partition; });
        if (!running) {
          break;
        }
        auto& f = files[header->partition_id];
        io_uring_prep_write(sqe, f.fd, task->buffer.data() + sizeof(PartitionDataHeader),
                            header->length - sizeof(PartitionDataHeader), f.offset);
        f.offset += header->length - sizeof(PartitionDataHeader);
      }

      io_uring_sqe_set_data(sqe, task);
      if (auto ec = io_uring_submit(&ring); ec < 0) {
        die("Fail to submit sqe, errno: {}", -ec);
      }
    }
  }

  void progress_io() {
    io_uring_cqe* cqe = nullptr;
    while (true) {
      int count = 0;
      while (running) {
        io_uring_peek_cqe(&ring, &cqe);
        if (cqe == nullptr) {
          if (count == 5000) {
            spilling = false;
          }
          boost::this_fiber::sleep_for(1ms);
        } else {
          break;
        }
        count++;
      }
      if (!running) {
        break;
      }

      spilling = true;

      auto task = reinterpret_cast<FileIOTask*>(io_uring_cqe_get_data(cqe));

      auto header = reinterpret_cast<PartitionDataHeader*>(task->buffer.data());

      INFO("spill {} of partition {}, res {}", header->length, header->partition_id, cqe->res);

      if (cqe->res > 0) {
        task->res.set_value(header->length);
      } else {
        task->res.set_value(cqe->res);
      }
      io_uring_cqe_seen(&ring, cqe);
    }
  }

  size_t handle_spill_request(const MemoryRegion& rbuf) {
    auto buf_o = bp.acquire_one();
    if (!buf_o.has_value()) {
      die("Too fast! No buffer to use!");
    }
    doca::BorrowedBuffer& buf = buf_o.value();
    DEBUG("read {} with length {}", (void*)rbuf.data(), rbuf.size());
    auto n_read = Base::t.bulk_read(buf, rbuf);
    auto header = reinterpret_cast<PartitionDataHeader*>(buf.data());
    if (header->length != n_read) {
      die("Fail to read partition buffer, expected: {}, got: {}", header->length, n_read);
    }
    // auto pid = *reinterpret_cast<uint32_t*>(buf.data());
    // DEBUG("Fetch {} of partition {}", n_read, pid);
    // auto task = SpillTask(buf);
    auto task = FileIOTask{.res = {}, .buffer = buf};
    io_q.push(&task);
    size_t n_spill = task.res.get_future().get();
    if (n_spill != header->length) {
      die("Fail to spill partition buffer, expected: {}, got: {}", header->length, n_spill);
    }
    bp.release_one(buf);
    return n_read;
  }

 private:
  struct PartitionFile {
    int fd = -1;
    size_t partition_id = -1;
    size_t offset = 0;
  };
  struct FileIOTask {
    op_res_promise_t res;
    doca::BorrowedBuffer& buffer;
  };
  BufferPool<doca::Buffers> bp;
  rigtorp::SPSCQueue<FileIOTask*> io_q;
  std::string spill_dir;  // absolute path
  io_uring ring;
  std::mutex mu;
  std::condition_variable c;
  size_t max_n_partition;
  std::vector<PartitionFile> files;
  std::atomic_bool spilling;
};

}  // namespace dpx
