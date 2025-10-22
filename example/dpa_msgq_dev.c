// clang-format off
#include "doca/kernel/helper.h"
// clang-format on

#include <doca_dpa_dev.h>
#include <doca_dpa_dev_buf.h>
#include <doca_dpa_dev_comch_msgq.h>
#include <doca_dpa_dev_rdma.h>
// #include <doca_dpa_dev_sync_event.h>

#include "doca/kernel/thread_args.h"

__dpa_global__ static void dpa_se_func(void) {
  //   struct dpa_thread_args_t *p = (struct dpa_thread_args_t *)doca_dpa_dev_thread_get_local_storage();
  //   DOCA_DPA_DEV_LOG_CRIT("notify");
  //   uint64_t v = 0;
  //   doca_dpa_dev_sync_event_wait_gt(p->wake_se_h, 0, -1);
  //   doca_dpa_dev_sync_event_get(p->wake_se_h, &v);
  //   DOCA_DPA_DEV_LOG_CRIT("wait %lu", v);
  //   uint32_t acc = 0;
  //   for (uint32_t i = 0; i < 10000; ++i) {
  //     acc += i;
  //   }
  //   DOCA_DPA_DEV_LOG_CRIT("%u", acc);
  //   doca_dpa_dev_sync_event_update_set(p->exit_se_h, 1);
  //   DOCA_DPA_DEV_LOG_CRIT("exit");
  //   doca_dpa_dev_thread_reschedule();
}

typedef void (*fn_t)(struct dpa_thread_args_t *p);

static void rdma_comp(struct dpa_thread_args_t *p, fn_t fn) {
  doca_dpa_dev_completion_element_t comp_e;
  int got = doca_dpa_dev_get_completion(p->rdma_comp_h, &comp_e);
  if (got) {
    LOG_DBG("rdma comp");
    doca_dpa_dev_completion_type_t comp_t = doca_dpa_dev_get_completion_type(comp_e);
    switch (comp_t) {
      case DOCA_DPA_DEV_COMP_SEND: {
        LOG_DBG("send comp");
        fn(p);
      } break;
      case DOCA_DPA_DEV_COMP_RECV_SEND: {
        LOG_DBG("recv comp");
      } break;
      case DOCA_DPA_DEV_COMP_RECV_RDMA_WRITE_IMM: {
        LOG_DBG("recv write imm comp");
      } break;
      case DOCA_DPA_DEV_COMP_RECV_SEND_IMM: {
        LOG_DBG("recv send imm comp");
      } break;
      case DOCA_DPA_DEV_COMP_SEND_ERR: {
        LOG_DBG("send err");
      } break;
      case DOCA_DPA_DEV_COMP_RECV_ERR: {
        LOG_DBG("recv err");
      } break;
    }
    doca_dpa_dev_completion_ack(p->rdma_comp_h, 1);
    doca_dpa_dev_completion_request_notification(p->rdma_comp_h);
  }
}

static void post_send(struct dpa_thread_args_t *p, uint32_t idx) {
  doca_dpa_dev_buf_arr_t buf = doca_dpa_dev_buf_array_get_buf(p->buf_arr_h, idx);
  LOG_DBG("post send addr: %lx len: %ld idx: %d", doca_dpa_dev_buf_get_addr(buf), doca_dpa_dev_buf_get_len(buf), idx);
  doca_dpa_dev_rdma_post_buf_send(p->rdma_h, p->rdma_conn_id, buf, DOCA_DPA_DEV_SUBMIT_FLAG_FLUSH);
}
static void post_recv(struct dpa_thread_args_t *p, uint32_t idx) {
  doca_dpa_dev_buf_arr_t buf = doca_dpa_dev_buf_array_get_buf(p->buf_arr_h, idx);
  LOG_DBG("post recv addr: %lx len: %ld idx: %d", doca_dpa_dev_buf_get_addr(buf), doca_dpa_dev_buf_get_len(buf), idx);
  doca_dpa_dev_rdma_post_buf_receive(p->rdma_h, buf);
}

static void notify_host(struct dpa_thread_args_t *p) {
  p->ok++;
  LOG_DBG("%d", p->ok);
  if (p->ok == 32) {
    uint64_t imm_data = 1919810;
    doca_dpa_dev_comch_producer_post_send_imm_only(p->prod_h, p->cpu_cons_id, (uint8_t *)(&imm_data), sizeof(uint64_t),
                                                   DOCA_DPA_DEV_SUBMIT_FLAG_FLUSH);
  } else {
    // post_recv(p, 0);
    post_send(p, 1);
  }
}

static void do_rdma(struct dpa_thread_args_t *p) {
  // if (!p->passive) {
  // post_recv(p, 0);
  post_send(p, 1);
  // } else {
  // post_recv(p, 0);
  // }
}

static void finish(struct dpa_thread_args_t *p UNUSED) { doca_dpa_dev_thread_finish(); }

__dpa_global__ static void dpa_rdma_func(void) {
  struct dpa_thread_args_t *p = (struct dpa_thread_args_t *)doca_dpa_dev_thread_get_local_storage();
  LOG_DBG("trigger");
  comch_cons_comp(p, do_rdma);
  rdma_comp(p, notify_host);
  comch_prod_comp(p, finish);
  LOG_DBG("reschedule");
  doca_dpa_dev_thread_reschedule();
}

__dpa_global__ static void dpa_msgq_func(void) {
  struct dpa_thread_args_t *p = (struct dpa_thread_args_t *)doca_dpa_dev_thread_get_local_storage();
  int got = 0;
  doca_dpa_dev_comch_consumer_completion_element_t cons_e;
  got = doca_dpa_dev_comch_consumer_get_completion(p->cons_comp_h, &cons_e);
  if (got) {
    uint32_t imm_len = 0;
    const uint8_t *imm_data = doca_dpa_dev_comch_consumer_get_completion_imm(cons_e, &imm_len);
    // uint32_t prod_id = doca_dpa_dev_comch_consumer_get_completion_producer_id(cons_e);
    // DOCA_DPA_DEV_LOG_CRIT("got %d %ld %u", got, *(const uint64_t *)imm_data, imm_len);
    doca_dpa_dev_comch_consumer_completion_ack(p->cons_comp_h, 1);
    doca_dpa_dev_comch_consumer_completion_request_notification(p->cons_comp_h);

    // if (doca_dpa_dev_comch_producer_is_consumer_empty(p->prod_h, p->cpu_cons_id)) {
    //   DOCA_DPA_DEV_LOG_CRIT("consumer is not empty");
    // } else {
    //   uint64_t imm_data = 1919810;
    doca_dpa_dev_comch_producer_post_send_imm_only(p->prod_h, p->cpu_cons_id, (uint8_t *)(&imm_data), sizeof(uint64_t),
                                                   DOCA_DPA_DEV_SUBMIT_FLAG_FLUSH);
    //   DOCA_DPA_DEV_LOG_CRIT("send");
    // }
    //   } else {
    // DOCA_DPA_DEV_LOG_CRIT("no recv");
  }
  doca_dpa_dev_completion_element_t prod_e;
  got = doca_dpa_dev_get_completion(p->prod_comp_h, &prod_e);
  if (got) {
    // DOCA_DPA_DEV_LOG_CRIT("send done, got %d", got);
    doca_dpa_dev_completion_ack(p->prod_comp_h, 1);
    doca_dpa_dev_completion_request_notification(p->prod_comp_h);
    //   } else {
    // DOCA_DPA_DEV_LOG_CRIT("no send");
  }
  //   DOCA_DPA_DEV_LOG_CRIT("recheduled");
  doca_dpa_dev_thread_reschedule();
}

__dpa_rpc__ static uint64_t trigger(doca_dpa_dev_comch_consumer_t cons_handle) {
  LOG_DBG("wake up");
  doca_dpa_dev_comch_consumer_ack(cons_handle, 32);
  return 0;
}

__dpa_rpc__ static uint64_t notify(doca_dpa_dev_notification_completion_t h) {
  doca_dpa_dev_thread_notify(h);
  return 0;
}
