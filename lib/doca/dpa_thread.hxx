#pragma once

#include <doca_comch.h>
#include <doca_comch_consumer.h>
#include <doca_comch_msgq.h>
#include <doca_comch_producer.h>
#include <doca_dpa.h>
#include <doca_pe.h>

#include <boost/fiber/future.hpp>

#include "doca/device.hxx"
#include "doca/dpa_buffer.hxx"
#include "doca/kernel/thread_args.h"
#include "util/noncopyable.hxx"
#include "util/nonmovable.hxx"

extern "C" doca_dpa_func_t wakeup;

namespace dpx::doca {

struct TaskContext {
  boost::fibers::promise<void> done;
  payload_t input;
  payload_t output;

  void set_input_id(uint64_t id) { input.id = id; }
  uint64_t get_input_id() { return input.id; }
  uint64_t get_output_id() { return output.id; }

  template <typename Input>
  void set_input(const Input &in) {
    memcpy(input.payload, &in, sizeof(in));
  }

  template <typename Output>
  std::type_identity_t<Output> get_output() {
    return *reinterpret_cast<Output *>(output.payload);
  }
};

class DPAThread;

class DPAThreadGroup : Noncopyable, Nonmovable {
  friend class DPAThread;

 public:
  DPAThreadGroup(Device &dev_, uint32_t n_thread, doca_dpa_func_t fn_);

  ~DPAThreadGroup();

  template <typename T>
    requires(std::is_trivially_copyable_v<T>)
  void add(const T &user_args);

  bool progress();

  boost::fibers::future<void> trigger(TaskContext &ctx);

 private:
  DPAThread *get_one_inactive_thread();

  Device &dev;
  doca_dpa_func_t *fn;
  doca_pe *pe;
  std::vector<DPAThread *> threads;
  std::vector<bool> active_flags;
  std::unordered_map<uint32_t, TaskContext *> outstanding_tasks;
};

class DPAThread : Noncopyable, Nonmovable {
  friend class DPAThreadGroup;

 public:
  DPAThread(DPAThreadGroup &g_, uint32_t thread_idx_, doca_dpa_func_t func, uint32_t tls_size);

  ~DPAThread();

 private:
  template <typename T>
    requires(std::is_trivially_copyable_v<T>)
  void run(const T &user_args);

  template <comch::TaskType type, TaskResult r>
  static void task_cb(comch::doca_dma_task_t<type> *task, doca_data task_user_data, doca_data ctx_user_data);

  static void state_change_cb(const doca_data, doca_ctx *, doca_ctx_states prev_state, doca_ctx_states next_state);

  DPAThreadGroup &g;
  uint32_t thread_idx;
  DPABuffer tls;
  doca_dpa_thread *t;
  /// For local peer wake up
  doca_dpa_notification_completion *notify_comp;
  /// For async operations
  // doca_dpa_completion *async_ops_comp;
  // doca_dpa_async_ops *async_ops;
  /// For communication between dpu and dpa
  doca_comch_msgq *in_q;   // cpu -> dpa
  doca_comch_msgq *out_q;  // dpa -> cpu
  doca_comch_producer *cpu_prod;
  doca_comch_consumer *cpu_cons;
  doca_comch_consumer_completion *cons_comp;
  doca_dpa_completion *prod_comp;
  doca_comch_producer *dpa_prod;
  doca_comch_consumer *dpa_cons;
  dpa_thread_args_t args;
};

template <typename T>
  requires(std::is_trivially_copyable_v<T>)
void DPAThreadGroup::add(const T &user_args) {
  TRACE("add one dpa thread");
  auto t = new DPAThread(*this, threads.size(), fn, sizeof(T) + sizeof(dpa_thread_args_t));
  t->run(user_args);
  threads.emplace_back(t);
}

template <typename T>
  requires(std::is_trivially_copyable_v<T>)
void DPAThread::run(const T &user_args) {
  /// set arguments
  memset(&args, 0, sizeof(args));
  args.tls_size = tls.size();
  doca_check(doca_dpa_notification_completion_get_dpa_handle(notify_comp, &args.notify_comp_h));
  // doca_check(doca_dpa_completion_get_dpa_handle(async_ops_comp, &args.async_ops_comp_h));
  // doca_check(doca_dpa_async_ops_get_dpa_handle(async_ops, &args.async_ops_h));
  doca_check(doca_comch_consumer_completion_get_dpa_handle(cons_comp, &args.cons_comp_h));
  doca_check(doca_dpa_completion_get_dpa_handle(prod_comp, &args.prod_comp_h));
  doca_check(doca_comch_consumer_get_dpa_handle(dpa_cons, &args.cons_h));
  doca_check(doca_comch_producer_get_dpa_handle(dpa_prod, &args.prod_h));
  doca_check(doca_comch_consumer_get_id(dpa_cons, &args.dpa_cons_id));
  doca_check(doca_comch_producer_get_id(dpa_prod, &args.dpa_prod_id));
  doca_check(doca_comch_consumer_get_id(cpu_cons, &args.cpu_cons_id));
  doca_check(doca_comch_producer_get_id(cpu_prod, &args.cpu_prod_id));
  args.d_ctx = tls.handle() + sizeof(args);
  /// export
  tls.copy_to_device(&args, sizeof(args), 0);
  tls.copy_to_device(&user_args, sizeof(T), sizeof(args));
  /// run
  doca_check(doca_dpa_thread_run(t));
  /// wake up
  dpx::doca::launch_rpc(g.dev, wakeup, args.cons_h);
}

}  // namespace dpx::doca
