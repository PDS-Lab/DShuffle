#pragma once

#include <MPMCQueue.h>
#include <SPSCQueue.h>

#include <cstdlib>
#include <format>

#include "common/context_base.hxx"
#include "doca/buffer.hxx"
#include "memory/simple_buffer_pool.hxx"
#include "trans/concept/rpc.hxx"

namespace dpx {

struct PartitionDataHeader {
  size_t partition_id;
  size_t length;
};

class PartitionBuffer {
 public:
  explicit PartitionBuffer(doca::BorrowedBuffer& buffer_)
      : buffer(buffer_), header(reinterpret_cast<PartitionDataHeader*>(buffer.data())) {}
  PartitionBuffer(doca::BorrowedBuffer& buffer_, size_t partition_id)
      : buffer(buffer_), header(reinterpret_cast<PartitionDataHeader*>(buffer.data())) {
    header->partition_id = partition_id;
    header->length = sizeof(PartitionDataHeader);
  }

  bool need_spill(size_t expected_size) { return header->length + expected_size > buffer.size(); }
  void append(std::span<uint8_t> data) { append(data.data(), data.size_bytes()); }
  void append(void* data, size_t length) {
    memcpy(buffer.data() + header->length, data, length);
    header->length += length;
  }
  bool empty() { return header->length == sizeof(PartitionDataHeader); }
  size_t actual_size() { return header->length - sizeof(PartitionDataHeader); }
  size_t total_size() { return header->length; }
  size_t partition_id() { return header->partition_id; }
  void* actual_data() { return buffer.data() + sizeof(PartitionDataHeader); }
  doca::BorrowedBuffer& underlying() { return buffer; }

 private:
  doca::BorrowedBuffer& buffer;
  PartitionDataHeader* header;
};

struct SpillTask {
  op_res_promise_t res;
  PartitionBuffer buffer;

  // currently unused
  // std::string ip;
  // uint16_t port;

  explicit SpillTask(doca::BorrowedBuffer& buffer_) : buffer(buffer_) {}
};

struct OffloadSpillTask {
  op_res_promise_t res;
  doca::Buffers* buffer;
};

struct NaiveSpillTask {
  op_res_promise_t res;
  std::span<uint8_t> data;
};

using TaskQueue = rigtorp::SPSCQueue<SpillTask*>;
using DispatchTaskQueue = rigtorp::MPMCQueue<SpillTask*>;

using NaiveTaskQueue = rigtorp::SPSCQueue<doca::Buffers*>;
using OffloadSpillTaskQueue = rigtorp::SPSCQueue<OffloadSpillTask*>;
using NaiveSpillTaskQueue = rigtorp::SPSCQueue<NaiveSpillTask*>;

struct MountRequest {
  size_t max_n_partition;
};
struct MountRpc : dpx::RpcBase<"Mount", MountRequest, int> {};

struct UmountRequest {
  size_t max_n_partition;
};
struct UmountRpc : dpx::RpcBase<"Umount", UmountRequest, int> {};

inline int mount(std::string dev, std::string mount_point) {
  auto cmd = std::format("sudo mount {} {}", dev, mount_point);
  INFO("do {}", cmd);
  return system(cmd.c_str());
  // return 0;
}

inline int umount(std::string mount_point) {
  auto cmd = std::format("sudo umount {}", mount_point);
  INFO("do {}", cmd);
  return system(cmd.c_str());
  // return 0;
}

struct SetStatusRpc {
  int dummy;
};

struct GetStatusRpc {
  int dummy;
};

// struct NaiveSpillDataLayout {
//   size_t n_record;
//   size_t partition_id;
//   size_t size;
//   size_t offsets_offset;
//   size_t hashcodes_offset;
//   size_t data_offset;

//   std::span<uint8_t> data(uint8_t* base) { return {base + data_offset, size - data_offset}; }
//   std::span<uint32_t> offsets(uint8_t* base) {
//     return {reinterpret_cast<uint32_t*>(base + offsets_offset), (hashcodes_offset - offsets_offset) /
//     sizeof(uint32_t)};
//   };
//   std::span<uint32_t> hashcodes(uint8_t* base) {
//     return {reinterpret_cast<uint32_t*>(base + hashcodes_offset), (data_offset - hashcodes_offset) /
//     sizeof(uint32_t)};
//   }
// };

using SpillBufferPool = BufferPool<doca::Buffers>;

}  // namespace dpx

/*
  Host SpillAgent     DiskAgent
      |                   |
      |                   V
      |           DirectSpillWorker
      |                   ^
      V                   |
    LocalSpillWorker -> ShuffleWorker <- RemoteSpillWorker <- Remote ShuffleWorker
*/
