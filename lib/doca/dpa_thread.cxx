#include "doca/dpa_thread.hxx"

#include "doca/check.hxx"
#include "util/logger.hxx"

namespace dpx::doca {

namespace {
inline static constexpr const uint32_t max_n_thread = 16;  // 1 eu
inline static constexpr const uint32_t comp_queue_size = 8;
// inline static constexpr const uint32_t async_queue_size = 32;
}  // namespace

DPAThreadGroup::DPAThreadGroup(Device &dev_, uint32_t n_thread, doca_dpa_func_t fn_)
    : dev(dev_), fn(fn_), active_flags(n_thread, false) {
  // if (n_thread > max_n_thread) {
  //   die("One group can only add {} threads for one EU", max_n_thread);
  // }
  threads.reserve(n_thread);
  //// Create
  /// msgq
  doca_check(doca_pe_create(&pe));
}

DPAThreadGroup::~DPAThreadGroup() {
  for (auto t : threads) {
    delete t;
  }
  //// Destroy
  /// pe
  doca_check(doca_pe_destroy(pe));
}

bool DPAThreadGroup::progress() { return doca_pe_progress(pe); }

DPAThread *DPAThreadGroup::get_one_inactive_thread() {
  while (true) {
    for (uint32_t i = 0; i < threads.size(); ++i) {
      if (active_flags[i]) {
        continue;
      }
      TRACE("get inactive thread {}", i);
      active_flags[i] = true;
      return threads[i];
    }
    boost::this_fiber::yield();
  }
  unreachable();
}

boost::fibers::future<void> DPAThreadGroup::trigger(TaskContext &ctx) {
  DPAThread *inactive_thread = get_one_inactive_thread();
  ctx.set_input_id(inactive_thread->thread_idx);

  {
    doca_comch_consumer_task_post_recv *task;
    doca_check(doca_comch_consumer_task_post_recv_alloc_init(inactive_thread->cpu_cons, nullptr, &task));
    auto t = doca_comch_consumer_task_post_recv_as_task(task);
    // doca_task_set_user_data(t, doca_data(&ctx));
    submit_task(t);
  }

  doca_comch_producer_task_send *task;
  doca_check(doca_comch_producer_task_send_alloc_init(inactive_thread->cpu_prod, nullptr,
                                                      reinterpret_cast<uint8_t *>(&ctx.input), sizeof(ctx.input),
                                                      inactive_thread->args.dpa_cons_id, &task));
  auto t = doca_comch_producer_task_send_as_task(task);
  doca_task_set_user_data(t, doca_data(&ctx));
  outstanding_tasks.emplace(ctx.get_input_id(), &ctx);
  submit_task(t);
  TRACE("task id: {}", ctx.get_input_id());
  return ctx.done.get_future();
}

template <comch::TaskType type, TaskResult r>
void DPAThread::task_cb(comch::doca_dma_task_t<type> *task, doca_data task_user_data, doca_data ctx_user_data) {
  auto t = static_cast<DPAThread *>(ctx_user_data.ptr);
  auto &g = t->g;
  auto underlying_task = comch::task_cast<type>(task);
  if constexpr (r == TaskResult::Failure) {
    auto status = doca_task_get_status(underlying_task);
    ERROR("{} task failure, {}", type, doca_error_get_descr(status));
  } else if constexpr (r == TaskResult::Success) {
    if constexpr (type == comch::TaskType::Recv) {
      // auto ctx = static_cast<RecvContext *>(task_user_data.ptr);
      auto p = reinterpret_cast<const payload_t *>(doca_comch_consumer_task_post_recv_get_imm_data(task));
      auto iter = g.outstanding_tasks.find(p->id);
      if (iter == g.outstanding_tasks.end()) {
        ERROR("drop one ?, size: {}, id: {}", g.outstanding_tasks.size(), p->id);
      } else {
        auto ctx = iter->second;
        ctx->output = *p;
        TRACE("Task {} done", ctx->get_input_id());
        g.outstanding_tasks.erase(iter);
        g.active_flags[p->id] = false;
        ctx->done.set_value();
      }
    } else if constexpr (type == comch::TaskType::Send) {
      auto ctx = static_cast<TaskContext *>(task_user_data.ptr);
      TRACE("Trigger task {} done", ctx->get_input_id());
    } else {
      static_unreachable;
    }
  } else {
    static_unreachable;
  }
  doca_task_free(underlying_task);
}

void DPAThread::state_change_cb(const doca_data, doca_ctx *, doca_ctx_states prev_state, doca_ctx_states next_state) {
  INFO("DOCA MsgQ state change: {} -> {}", prev_state, next_state);
  switch (next_state) {
    case DOCA_CTX_STATE_IDLE: {
    } break;
    case DOCA_CTX_STATE_STARTING: {
    } break;
    case DOCA_CTX_STATE_RUNNING: {
    } break;
    case DOCA_CTX_STATE_STOPPING: {
    } break;
  }
}

DPAThread::DPAThread(DPAThreadGroup &g_, uint32_t thread_idx_, doca_dpa_func_t func, uint32_t tls_size)
    : g(g_), thread_idx(thread_idx_), tls(g.dev, tls_size) {
  if (tls_size < sizeof(dpa_thread_args_t)) {
    die("tls size must bigger than {}", sizeof(dpa_thread_args_t));
  }
  //// Create
  /// thread
  doca_check(doca_dpa_thread_create(g.dev.dpa, &t));
  doca_check(doca_dpa_thread_set_func_arg(t, func, 0));
  doca_check(doca_dpa_thread_set_local_storage(t, tls.handle()));
  doca_check(doca_dpa_thread_start(t));
  /// notify_comp
  doca_check(doca_dpa_notification_completion_create(g.dev.dpa, t, &notify_comp));
  doca_check(doca_dpa_notification_completion_start(notify_comp));
  /// async_ops_comp
  // doca_check(doca_dpa_completion_create(g.dev.dpa, comp_queue_size, &async_ops_comp));
  // doca_check(doca_dpa_completion_set_thread(async_ops_comp, t));
  // doca_check(doca_dpa_completion_start(async_ops_comp));
  /// async_ops
  // doca_check(doca_dpa_async_ops_create(g.dev.dpa, async_queue_size, 0, &async_ops));
  // doca_check(doca_dpa_async_ops_attach(async_ops, async_ops_comp));
  // doca_check(doca_dpa_async_ops_start(async_ops));
  /// out_q
  doca_check(doca_comch_msgq_create(g.dev.dev, &out_q));
  doca_check(doca_comch_msgq_set_max_num_consumers(out_q, 1));
  doca_check(doca_comch_msgq_set_max_num_producers(out_q, 1));
  doca_check(doca_comch_msgq_set_dpa_producer(out_q, g.dev.dpa));
  doca_check(doca_comch_msgq_start(out_q));
  /// in_q
  doca_check(doca_comch_msgq_create(g.dev.dev, &in_q));
  doca_check(doca_comch_msgq_set_max_num_consumers(in_q, 1));
  doca_check(doca_comch_msgq_set_max_num_producers(in_q, 1));
  doca_check(doca_comch_msgq_set_dpa_consumer(in_q, g.dev.dpa));
  doca_check(doca_comch_msgq_start(in_q));
  /// cpu_cons
  doca_check(doca_comch_msgq_consumer_create(out_q, &cpu_cons));
  doca_check(doca_comch_consumer_task_post_recv_set_conf(cpu_cons, task_cb<comch::TaskType::Recv, TaskResult::Success>,
                                                         task_cb<comch::TaskType::Recv, TaskResult::Failure>,
                                                         comp_queue_size));
  auto cpu_cons_ctx = doca_comch_consumer_as_ctx(cpu_cons);
  doca_check(doca_ctx_set_state_changed_cb(cpu_cons_ctx, state_change_cb));
  doca_check(doca_ctx_set_user_data(cpu_cons_ctx, doca_data(this)));
  doca_check(doca_pe_connect_ctx(g.pe, cpu_cons_ctx));
  doca_check(doca_ctx_start(cpu_cons_ctx));
  /// cpu_prod
  doca_check(doca_comch_msgq_producer_create(in_q, &cpu_prod));
  doca_check(doca_comch_producer_task_send_set_conf(cpu_prod, task_cb<comch::TaskType::Send, TaskResult::Success>,
                                                    task_cb<comch::TaskType::Send, TaskResult::Failure>,
                                                    comp_queue_size));
  auto cpu_prod_ctx = doca_comch_producer_as_ctx(cpu_prod);
  doca_check(doca_ctx_set_state_changed_cb(cpu_prod_ctx, state_change_cb));
  doca_check(doca_ctx_set_user_data(cpu_prod_ctx, doca_data(this)));
  doca_check(doca_pe_connect_ctx(g.pe, cpu_prod_ctx));
  doca_check(doca_ctx_start(cpu_prod_ctx));
  /// cons_comp
  doca_check(doca_comch_consumer_completion_create(&cons_comp));
  doca_check(doca_comch_consumer_completion_set_max_num_consumers(cons_comp, 1));
  doca_check(doca_comch_consumer_completion_set_max_num_recv(cons_comp, comp_queue_size));
  doca_check(doca_comch_consumer_completion_set_dpa_thread(cons_comp, t));
  doca_check(doca_comch_consumer_completion_start(cons_comp));
  /// prod_comp
  doca_check(doca_dpa_completion_create(g.dev.dpa, comp_queue_size, &prod_comp));
  doca_check(doca_dpa_completion_set_thread(prod_comp, t));
  doca_check(doca_dpa_completion_start(prod_comp));
  /// dpa_cons
  doca_check(doca_comch_msgq_consumer_create(in_q, &dpa_cons));
  doca_check(doca_comch_consumer_set_dev_max_num_recv(dpa_cons, comp_queue_size));
  doca_check(doca_comch_consumer_set_completion(dpa_cons, cons_comp, 0));
  auto dpa_cons_ctx = doca_comch_consumer_as_ctx(dpa_cons);
  doca_check(doca_ctx_set_datapath_on_dpa(dpa_cons_ctx, g.dev.dpa));
  doca_check(doca_ctx_start(dpa_cons_ctx));
  /// dpa_prod
  doca_check(doca_comch_msgq_producer_create(out_q, &dpa_prod));
  doca_check(doca_comch_producer_set_dev_max_num_send(dpa_prod, comp_queue_size));
  doca_check(doca_comch_producer_dpa_completion_attach(dpa_prod, prod_comp));
  auto dpa_prod_ctx = doca_comch_producer_as_ctx(dpa_prod);
  doca_check(doca_ctx_set_datapath_on_dpa(dpa_prod_ctx, g.dev.dpa));
  doca_check(doca_ctx_start(dpa_prod_ctx));
}

DPAThread::~DPAThread() {
  //// Stop
  /// dpa_prod
  doca_check(doca_ctx_stop(doca_comch_producer_as_ctx(dpa_prod)));
  /// dpa_cons
  doca_check(doca_ctx_stop(doca_comch_consumer_as_ctx(dpa_cons)));
  /// prod_comp
  doca_check(doca_dpa_completion_stop(prod_comp));
  /// cons_comp
  doca_check(doca_comch_consumer_completion_stop(cons_comp));
  /// async_ops
  // doca_check(doca_dpa_async_ops_stop(async_ops));
  /// async_ops_comp
  // doca_check(doca_dpa_completion_stop(async_ops_comp));
  /// notify_comp
  doca_check(doca_dpa_notification_completion_stop(notify_comp));
  /// thread
  doca_check(doca_dpa_thread_stop(t));
  /// cpu_prod
  doca_check(doca_ctx_stop(doca_comch_producer_as_ctx(cpu_prod)));
  /// cpu_cons
  doca_check(doca_ctx_stop(doca_comch_consumer_as_ctx(cpu_cons)));

  //// Destroy
  /// dpa_prod
  doca_check(doca_comch_producer_destroy(dpa_prod));
  /// dpa_cons
  doca_check(doca_comch_consumer_destroy(dpa_cons));
  /// prod_comp
  doca_check(doca_dpa_completion_destroy(prod_comp));
  /// cons_comp
  doca_check(doca_comch_consumer_completion_destroy(cons_comp));
  /// cpu_cons
  doca_check(doca_comch_consumer_destroy(cpu_cons));
  /// cpu_prod
  doca_check(doca_comch_producer_destroy(cpu_prod));
  /// out_q
  doca_check(doca_comch_msgq_stop(out_q));
  doca_check(doca_comch_msgq_destroy(out_q));
  /// id_q
  doca_check(doca_comch_msgq_stop(in_q));
  doca_check(doca_comch_msgq_destroy(in_q));
  /// async_ops
  // doca_check(doca_dpa_async_ops_destroy(async_ops));
  /// async_ops_comp
  // doca_check(doca_dpa_completion_destroy(async_ops_comp));
  /// notify_comp
  doca_check(doca_dpa_notification_completion_destroy(notify_comp));
  /// thread
  doca_check(doca_dpa_thread_destroy(t));
}

}  // namespace dpx::doca
