#pragma once

#include <latch>
#include <queue>

#include "native/offload.hxx"
#include "native/worker.hxx"
#include "util/literal.hxx"
#include "util/spin_lock.hxx"

namespace dpx {

class SpillAgent {
 public:
  SpillAgent(doca::Device& dev, const ConnectionParam<Backend::DOCA_Comch>& param, const Config& trans_conf,
             size_t n_worker, size_t n_buffer, size_t buffer_size, size_t max_n_partition, std::latch& start_point_,
             std::atomic_bool& running_)
      : running(running_),
        locks(max_n_partition),
        active_buffers(max_n_partition, nullptr),
        dma_buffer_pool(dev, n_buffer, buffer_size, DOCA_ACCESS_FLAG_PCI_READ_WRITE) {
    // locks = new SpinLock[max_n_partition];
    dma_buffer_pool.enable_mt();
    dma_worker.reserve(n_worker);
    for (uint32_t i = 0; i < n_worker; i++) {
      auto cur_param = param;
      cur_param.name += std::to_string(i);
      auto w = new Worker<Backend::DOCA_Comch>(dev, cur_param, trans_conf, running_);
      w->run([&, i]() {
        pthread_setname_np(pthread_self(), ("spill_agent" + std::to_string(i)).c_str());
        start_point_.arrive_and_wait();
        issue_spill(i);
      });
      dma_worker.emplace_back(w);
    }
  }

  ~SpillAgent() {
    {
      std::unique_lock g(q_mu);
      running = false;
    }
    q_c.notify_all();
    for (auto w : dma_worker) {
      w->join();
      delete w;
    }
    // if (locks != nullptr) {
    //   delete[] locks;
    // }
  }

  void append_or_spill(size_t partition_id, std::span<uint8_t> key, std::span<uint8_t> value) {
    std::lock_guard g(locks[partition_id]);
    auto b = active_buffers[partition_id];
    if (b == nullptr) {
      b = acquire_one(partition_id);
      b->append(key);
      b->append(value);
      active_buffers[partition_id] = b;
    } else if (b->need_spill(key.size_bytes() + value.size_bytes())) {
      INFO("buffer {} need spill, length: {}({})", partition_id, b->actual_size(), b->total_size());
      submit_spill_buffer(b);
      b = acquire_one(partition_id);
      b->append(key);
      b->append(value);
      active_buffers[partition_id] = b;
    } else {
      b->append(key);
      b->append(value);
    }
  }

  void force_spill(size_t partition_id) {
    std::lock_guard g(locks[partition_id]);
    if (active_buffers[partition_id] == nullptr) {
      return;
    }
    submit_spill_buffer(active_buffers[partition_id]);
    active_buffers[partition_id] = acquire_one(partition_id);
  }

 private:
  void issue_spill(int i) {
    INFO("spiller {} start", i);
    auto w = dma_worker[i];
    INFO("register {} with length {}", (void*)dma_buffer_pool.buffers().data(), dma_buffer_pool.buffers().size());
    w->t.register_memory(dma_buffer_pool.buffers(), w->bulk_dev);
    while (true) {
      std::vector<PartitionBuffer*> buffers;
      {
        std::unique_lock g(q_mu);
        q_c.wait(g, [&] { return !running || !spill_q.empty(); });
        if (!running && spill_q.empty()) {
          break;
        }
        buffers.swap(spill_q);
      }
      std::vector<boost::fibers::future<size_t>> size_fs;
      for (auto buffer : buffers) {
        INFO("trans partition {} with length {}({}) at {}", buffer->partition_id(), buffer->actual_size(),
             buffer->total_size(), (void*)buffer->underlying().data());
        size_fs.emplace_back(w->t.bulk(buffer->underlying(), buffer->total_size()));
      }
      for (auto i = 0uz; i < buffers.size(); i++) {
        auto size = size_fs[i].get();
        if (size != buffers[i]->total_size()) {
          die("Fail to trans buffer of partition {}", buffers[i]->partition_id());
        }
        release_one(buffers[i]);
      }
    }
    w->t.unregister_memory(dma_buffer_pool.buffers());
    INFO("spiller stop");
  }

  void submit_spill_buffer(PartitionBuffer* b) {
    {
      std::unique_lock g(q_mu);
      spill_q.push_back(b);
    }
    q_c.notify_one();
  }

  PartitionBuffer* acquire_one(size_t partition_id) {
    while (true) {
      auto buf = dma_buffer_pool.acquire_one();
      if (!buf.has_value()) {
        std::this_thread::sleep_for(1ms);
        continue;
      }
      INFO("acquire one buf");
      return new PartitionBuffer(buf->get(), partition_id);
    }
  }

  void release_one(PartitionBuffer* buf) {
    INFO("release one buf");
    dma_buffer_pool.release_one(buf->underlying());
    delete buf;
  }

  std::atomic_bool& running;
  std::vector<Worker<Backend::DOCA_Comch>*> dma_worker;

  std::vector<PartitionBuffer*> spill_q;
  std::mutex q_mu;
  std::condition_variable q_c;

  // SpinLock* locks;
  std::vector<std::mutex> locks;
  std::vector<PartitionBuffer*> active_buffers;
  BufferPool<doca::Buffers> dma_buffer_pool;
};

class NaiveSpillAgent : Worker<Backend::DOCA_Comch> {
  using Base = Worker<Backend::DOCA_Comch>;

 public:
  NaiveSpillAgent(doca::Device& dev_, const ConnectionParam<Backend::DOCA_Comch>& param, const Config& trans_conf,
                  NaiveSpillTaskQueue& q_, std::latch& start_point_, std::atomic_bool& running_, uint32_t core_idx_)
      : Base(dev_, param, trans_conf, running_),
        dev(dev_),
        q(q_),
        bp(dev_, 16, 512_MB, DOCA_ACCESS_FLAG_PCI_READ_WRITE) {
    Base::run(
        [&]() {
          pthread_setname_np(pthread_self(), "spill_agent");
          start_point_.arrive_and_wait();
          t.register_memory(bp.buffers(), dev);
          std::vector<boost::fibers::fiber> fs;
          for (uint32_t i = 0; i < 16; i++) {
            fs.push_back(boost::fibers::fiber([this, i]() { do_spill(i); }));
          }
          for (auto& f : fs) {
            f.join();
          }
          t.unregister_memory(bp.buffers());
        },
        core_idx_);
  }

  ~NaiveSpillAgent() { Base::join(); }

  void do_spill(size_t idx) {
    INFO("Spiller {} start", idx);
    while (true) {
      while (running && q.empty()) {
        boost::this_fiber::yield();
      }
      if (!running) {
        break;
      }

      auto task = *q.front();
      INFO("spill one at {} with length {}", (void*)task->data.data(), task->data.size_bytes());
      q.pop();
      auto b = bp.acquire_one();
      while (!b.has_value()) {
        boost::this_fiber::yield();
        b = bp.acquire_one();
      }
      auto& buffer = b->get();
      memcpy(buffer.data(), task->data.data(), task->data.size_bytes());
      auto trans_size = t.bulk(buffer, task->data.size_bytes()).get();
      if (trans_size != task->data.size_bytes()) {
        die("Fail to trans buffer with length {} at {}", task->data.size_bytes(), (void*)buffer.data());
      }
      bp.release_one(buffer);
      task->res.set_value(task->data.size_bytes());
    }
    INFO("Spiller {} stop", idx);
  }

  // void spill(std::span<uint8_t> data, std::span<uint32_t> offsets, std::span<uint32_t> hashcodes) {
  //   std::lock_guard lg(mu);
  //   TransportGuard g(t);
  //   auto& mr = b[0];

  //   auto layout = (NaiveSpillDataLayout*)mr.data();
  //   layout->n_record = hashcodes.size();
  //   auto offset = sizeof(NaiveSpillDataLayout);

  //   layout->offsets_offset = offset;
  //   memcpy(mr.data() + offset, offsets.data(), offsets.size_bytes());
  //   offset += offsets.size_bytes();

  //   layout->hashcodes_offset = offset;
  //   memcpy(mr.data() + offset, hashcodes.data(), hashcodes.size_bytes());
  //   offset += hashcodes.size_bytes();

  //   layout->data_offset = offset;
  //   memcpy(mr.data() + offset, data.data(), data.size_bytes());
  //   offset += data.size_bytes();

  //   layout->size = offset;
  //   DEBUG("trans buffer with length {} at {}", offset, (void*)mr.data());
  //   auto trans_size = t.bulk(mr, offset).get();
  //   if (trans_size != offset) {
  //     die("Fail to trans buffer with length {} at {}", offset, (void*)mr.data());
  //   }
  // }

 private:
  doca::Device& dev;
  NaiveSpillTaskQueue& q;
  BufferPool<doca::Buffers> bp;
};

}  // namespace dpx