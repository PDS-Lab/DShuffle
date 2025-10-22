#pragma once

#include <queue>

#include "native/offload.hxx"
#include "native/worker.hxx"

namespace dpx {

using DispatchFn = std::function<bool(size_t)>;
using PartitionFn = std::function<std::vector<std::vector<uint8_t>>(std::span<uint8_t>)>;

class ShuffleWorker : public Worker<Backend::DOCA_RDMA> {
 public:
  ShuffleWorker(doca::Device& dev_, const ConnectionParam<Backend::DOCA_RDMA>& param, const Config& trans_conf,
                SpillBufferPool& lbp_, SpillBufferPool& rbp_, size_t n_fiber, TaskQueue& local_task_q_,
                TaskQueue& remote_task_q_, TaskQueue& spill_q_, DispatchFn fn_, std::atomic_bool& running_,
                size_t core_idx)
      : Worker<Backend::DOCA_RDMA>(dev_, param, trans_conf, running_),
        dev(dev_),
        local_bp(lbp_),
        remote_bp(rbp_),
        local_task_q(local_task_q_),
        remote_task_q(remote_task_q_),
        spill_q(spill_q_),
        fn(fn_) {
    run(
        // th = std::thread(
        [this, n_fiber]() {
          std::vector<boost::fibers::fiber> l_fs(n_fiber);
          std::vector<boost::fibers::fiber> r_fs(n_fiber);
          t.register_memory(local_bp.buffers(), dev);
          for (size_t i = 0; i < n_fiber; i++) {
            l_fs.emplace_back(boost::fibers::fiber([this, i]() {
              INFO("local shuffle worker {} start", i);
              handle_spill_task(local_task_q);
              INFO("local shuffle worker {} stop", i);
            }));
            r_fs.emplace_back(boost::fibers::fiber([this, i]() {
              INFO("remote shuffle worker {} start", i);
              handle_spill_task(remote_task_q);
              INFO("remote shuffle worker {} stop", i);
            }));
          }
          for (auto& f : l_fs) {
            f.join();
          }
          for (auto& f : r_fs) {
            f.join();
          }
          t.unregister_memory(local_bp.buffers());
        }
        //);
        ,
        core_idx);

    INFO("shuffle worker serve");
  }

  ~ShuffleWorker() { INFO("shuffle worker destroy"); }

 private:
  void handle_spill_task(TaskQueue& q) {
    while (true) {
      while (running && q.empty()) {
        boost::this_fiber::yield();
      }
      if (!running) {
        break;
      }
      auto task = *q.front();
      q.pop();
      // TODO run offload logic, currently just redirect to spill
      if (fn(task->buffer.partition_id())) {
        TRACE("transfer to remote");
        auto length = task->buffer.actual_size();
        auto n_bulk = t.bulk(task->buffer.underlying(), length).get();
        if (n_bulk != length) {
          die("Fail to transfer buffer of partition {} with length {}", task->buffer.partition_id(), length);
        }
        TRACE("transfer done");
        task->res.set_value(length);
      } else {
        TRACE("transfer to local");
        spill_q.push(std::move(task));
        TRACE("transfer done");
      }
    }
  }

  doca::Device& dev;
  SpillBufferPool& local_bp;
  SpillBufferPool& remote_bp;
  TaskQueue& local_task_q;
  TaskQueue& remote_task_q;
  TaskQueue& spill_q;
  DispatchFn fn;
  // std::thread th;
};

class NaiveShuffleWorkerPool {
 public:
  NaiveShuffleWorkerPool(size_t n_worker, TransportWrapper<Backend::DOCA_Comch>& io_,
                         std::vector<TransportWrapper<Backend::DOCA_RDMA>*>& rdmas_, DispatchFn fn_, PartitionFn pfn_)
      : io(io_), rdma_mus(rdmas_.size()), rdmas(rdmas_), fn(fn_), pfn(pfn_), running(true) {
    if (n_worker < 4) {
      die("n worker >= 4");
    }
    for (auto i = 0uz; i < n_worker; i++) {
      workers.emplace_back(std::thread([this, i, n_worker]() {
        bind_core(2 + i);
        if (i < n_worker / 2) {
          do_shuffle(false, lmu, lc, ldq, i);
        } else {
          do_shuffle(true, rmu, rc, rdq, i);
        }
      }));
    }
  }
  ~NaiveShuffleWorkerPool() {
    {
      std::scoped_lock l(lmu, rmu);
      running = false;
    }
    lc.notify_all();
    rc.notify_all();
    for (auto& worker : workers) {
      worker.join();
    }
  }

  void submit_task(OffloadSpillTask* t, bool from_remote) {
    TRACE("dispatch one");
    if (from_remote) {
      {
        std::unique_lock l(rmu);
        rdq.push(t);
      }
      rc.notify_one();
    } else {
      {
        std::unique_lock l(lmu);
        ldq.push(t);
      }
      lc.notify_one();
    }
  }

  void do_shuffle(bool serve_remote, std::mutex& mu, std::condition_variable& c, std::queue<OffloadSpillTask*>& q,
                  size_t idx) {
    INFO("naive shuffle worker {} start", idx);
    while (true) {
      OffloadSpillTask* t = nullptr;
      {
        std::unique_lock l(mu);
        c.wait(l, [&] { return !running || !q.empty(); });
        if (!running && ldq.empty() && rdq.empty()) {
          return;
        }
        TRACE("got one task");
        t = q.front();
        q.pop();
      }
      if (t == nullptr) {
        continue;
      }
      doca::Buffers* b = t->buffer;
      if (serve_remote) {  // spill
        auto header = (PartitionDataHeader*)b->data();
        // auto pid = *reinterpret_cast<uint32_t*>(b->data());
        TRACE("receive data from remote of partition {} length {}", b->size(), header->partition_id);
        {
          std::lock_guard l(io_mu);
          TransportGuard g(io.t);
          // doca::MappedRegion mr(io.bulk_dev, b->data(), b->size(), DOCA_ACCESS_FLAG_PCI_READ_WRITE);
          io.t.register_memory(*b, io.bulk_dev);
          auto n_bulk = io.t.bulk(*b).get();
          if (n_bulk != b->size()) {
            die("Fail to trans data with length {} of partition {}", b->size(), header->partition_id);
          }
          io.t.unregister_memory(*b);
        }
        TRACE("Trans local data with length {} of partition {}", b->size(), header->partition_id);
      } else {
        // do actual shuffle
        // size_t n_partition = 192;
        // auto base = b->data();
        // auto l = (NaiveSpillDataLayout*)base;
        // auto data = l->data(base);
        // auto offsets = l->offsets(base);
        // auto hashcodes = l->hashcodes(base);
        // assert(hashcodes.size() + 1 == offsets.size());
        // std::vector<std::vector<uint8_t>> partitions(n_partition);
        // for (auto i = 0uz; i < n_partition; i++) {
        //   auto& p = partitions[i];
        //   p.resize(sizeof(PartitionDataHeader), 0);
        //   auto header = (PartitionDataHeader*)p.data();
        //   header->partition_id = i;
        //   header->length += sizeof(PartitionDataHeader);
        // }
        // for (auto i = 0uz; i < l->n_record; i++) {
        //   auto length = offsets[i + 1] - offsets[i];
        //   auto offset = offsets[i];
        //   auto pid = hashcodes[i];
        //   // TRACE("offset {} length {} pid {}", offset, length, pid);
        //   auto record = data.subspan(offset, length);
        //   auto& p = partitions[pid];
        //   p.insert(p.end(), record.begin(), record.end());
        //   auto header = (PartitionDataHeader*)p.data();
        //   header->length += record.size_bytes();
        // }
        std::vector<uint8_t> scatter;
        std::vector<size_t> partition_length;
        std::vector<size_t> partition_offsets;
        {
          auto partitions = pfn(std::span<uint8_t>(*b));
          for (auto& p : partitions) {
            partition_offsets.push_back(scatter.size());
            scatter.insert(scatter.end(), p.begin(), p.end());
            partition_length.push_back(p.size());
          }
        }
        // for (auto& p : partitions) {
        //   auto header = (PartitionDataHeader*)p.data();
        //   if (fn(header->partition_id)) {
        std::vector<size_t> remote_ps;
        std::vector<size_t> local_ps;
        for (auto pid = 0uz; pid < partition_length.size(); pid++) {
          if (fn(pid)) {
            remote_ps.emplace_back(pid);
          } else {
            local_ps.emplace_back(pid);
          }
        }
        doca::MappedRegion mr(
            io.bulk_dev, scatter.data(), scatter.size(),
            DOCA_ACCESS_FLAG_RDMA_READ | DOCA_ACCESS_FLAG_RDMA_WRITE | DOCA_ACCESS_FLAG_PCI_READ_WRITE);
        {
          std::lock_guard l(io_mu);
          TransportGuard g(io.t);
          io.t.register_memory(mr, io.bulk_dev);
          std::vector<boost::fibers::future<size_t>> fs;
          for (auto pid : local_ps) {
            auto offset = partition_offsets[pid];
            auto length = partition_length[pid];
            TRACE("partition {} offset {} size {}", pid, offset, length);
            auto n_bulk = io.t.bulk(scatter.data() + offset, length);
            fs.emplace_back(std::move(n_bulk));
            TRACE("Trans local data with length {} of partition {}", length, pid);
          }
          for (auto& f : fs) {
            size_t n_bulk = f.get();
            if (n_bulk != mr.size()) {
              die("Fail to trans data");
            }
          }
          io.t.unregister_memory(mr);
        }

        {
          auto idx = rdmas.size() > 1 ? active_rdma_idx.fetch_add(1) % rdmas.size() : 0;
          auto& rdma = *rdmas[idx];
          auto& rdma_mu = rdma_mus[idx];
          std::lock_guard l(rdma_mu);
          TransportGuard g(rdma.t);
          rdma.t.register_memory(mr, rdma.bulk_dev);
          std::vector<boost::fibers::future<size_t>> fs;
          for (auto pid : local_ps) {
            auto offset = partition_offsets[pid];
            auto length = partition_length[pid];
            TRACE("partition {} offset {} size {}", pid, offset, length);
            auto n_bulk = rdma.t.bulk(scatter.data() + offset, length);
            fs.emplace_back(std::move(n_bulk));
            TRACE("Trans Remote data with length {} of partition {}", length, pid);
          }
          for (auto& f : fs) {
            size_t n_bulk = f.get();
            if (n_bulk != mr.size()) {
              die("Fail to trans data");
            }
          }
          rdma.t.unregister_memory(mr);
        }

        // for (auto pid = 0uz; pid < partitions.size(); pid++) {
        //   auto& p = partitions[pid];
        //   auto header = reinterpret_cast<PartitionDataHeader*>(p.data());
        //   TRACE("header partition id: {}", header->partition_id);
        //   if (fn(pid)) {
        //     auto idx = rdmas.size() > 1 ? active_rdma_idx.fetch_add(1) % rdmas.size() : 0;
        //     auto& rdma = *rdmas[idx];
        //     auto& rdma_mu = rdma_mus[idx];
        //     doca::MappedRegion mr(rdma.bulk_dev, p.data(), p.size(),
        //                           DOCA_ACCESS_FLAG_RDMA_READ | DOCA_ACCESS_FLAG_RDMA_WRITE);
        //     TRACE("partition {} size {}", pid, p.size());
        //     {
        //       std::lock_guard l(rdma_mu);
        //       TransportGuard g(rdma.t);
        //       rdma.t.register_memory(mr, rdma.bulk_dev);
        //       auto n_bulk = rdma.t.bulk(mr).get();
        //       if (n_bulk != mr.size()) {
        //         die("Fail to trans data with length {} of partition {}", p.size(), pid);
        //       }
        //       rdma.t.unregister_memory(mr);
        //     }
        //     TRACE("Trans remote data with length {} of partition {}", p.size(), pid);
        //   } else {
        //     doca::MappedRegion mr(io.bulk_dev, p.data(), p.size(), DOCA_ACCESS_FLAG_PCI_READ_WRITE);
        //     TRACE("partition {} size {}", pid, p.size());
        //     {
        //       std::lock_guard l(io_mu);
        //       TransportGuard g(io.t);
        //       io.t.register_memory(mr, io.bulk_dev);
        //       auto n_bulk = io.t.bulk(mr).get();
        //       if (n_bulk != mr.size()) {
        //         die("Fail to trans data with length {} of partition {}", p.size(), pid);
        //       }
        //       io.t.unregister_memory(mr);
        //     }
        //     TRACE("Trans local data with length {} of partition {}", p.size(), pid);
        //   }
        // }
      }
      t->res.set_value(b->size());
    }
    INFO("naive shuffle worker {} stop", idx);
  }

 private:
  std::mutex io_mu;
  TransportWrapper<Backend::DOCA_Comch>& io;
  std::atomic_uint64_t active_rdma_idx = 0;
  std::vector<std::mutex> rdma_mus;
  std::vector<TransportWrapper<Backend::DOCA_RDMA>*>& rdmas;

  std::vector<std::thread> workers;
  std::mutex lmu;
  std::condition_variable lc;
  std::queue<OffloadSpillTask*> ldq;
  std::mutex rmu;
  std::condition_variable rc;
  std::queue<OffloadSpillTask*> rdq;

  DispatchFn fn;
  PartitionFn pfn;
  bool running;
};

using TaskQueues = std::vector<TaskQueue*>;

class PipelineShuffleWorkerPool {
 public:
  PipelineShuffleWorkerPool(TaskQueues lsqs_, TaskQueues rsqs_, TaskQueues trsqs_, TaskQueue& dsq_, DispatchFn fn_,
                            std::atomic_bool& running_)
      : lsqs(lsqs_), rsqs(rsqs_), trsqs(trsqs_), dsq(dsq_), fn(fn_), running(running_) {}
  ~PipelineShuffleWorkerPool() {}

  void run_dispatch(size_t n_fiber, size_t core_idx) {
    ld = std::thread([this, core_idx, n_fiber]() {
      bind_core(core_idx);
      std::vector<boost::fibers::fiber> fs;
      for (auto i = 0uz; i < n_fiber; i++) {
        auto f = boost::fibers::fiber([this, i]() { do_dispatch(i); });
        fs.push_back(std::move(f));
      }
      for (auto& f : fs) {
        f.join();
      }
    });
  }

  void join() { ld.join(); }

 private:
  void do_dispatch(int i) {
    INFO("Start dispatcher {}", i);
    while (true) {
      while (running && all_queue_empty()) {
        boost::this_fiber::yield();
      }
      if (!running) {
        break;
      }
      for (TaskQueue* q : lsqs) {
        if (q->empty()) {
          continue;
        }
        auto t = *q->front();
        q->pop();
        if (fn(t->buffer.partition_id())) {
          choose_one_trsq()->push(t);
        } else {
          dsq.push(t);
        }
      }
      for (TaskQueue* q : rsqs) {
        if (q->empty()) {
          continue;
        }
        auto t = *q->front();
        q->pop();
        dsq.push(t);
      }
    }
    INFO("Stop dispatcher {}", i);
  }

  bool all_queue_empty() {
    auto p = [](TaskQueue* q) { return q->empty(); };
    return std::ranges::all_of(lsqs, p) && std::ranges::all_of(rsqs, p);
  }

  TaskQueue* choose_one_trsq() { return trsqs[(++trsqs_i) % rsqs.size()]; }

  TaskQueues lsqs;
  TaskQueues rsqs;
  uint32_t trsqs_i = 0;
  TaskQueues trsqs;
  TaskQueue& dsq;
  std::thread ld;
  DispatchFn fn;
  std::atomic_bool& running;
};

}  // namespace dpx
