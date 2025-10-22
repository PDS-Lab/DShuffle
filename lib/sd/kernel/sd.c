// clang-format off
#include "doca/kernel/helper.h"
// clang-format on

#include <doca_dpa_dev.h>
#include <doca_dpa_dev_buf.h>
#include <doca_dpa_dev_comch_msgq.h>
#include <doca_dpa_dev_sync_event.h>

// #include "doca/kernel/map.h"
#include "doca/kernel/mem.h"
#include "doca/kernel/thread_args.h"
#include "sd/common/args.h"
#include "sd/common/meta.h"

// global variables
static meta_idx_t meta_idx;
static jvm_args_t jvm_args;

// context structure
typedef struct {
  char *p;
  char *cur_p;
  uint64_t lim;
} buffer_t;

typedef struct {
  buffer_t outbuf;
  // map_t ref2off;
} se_task_ctx_t;

// meta
UNUSED __forceinline class_info_t *get_class_info_by_klass(void *klass) {
  LOG_DBG("search %lX", (uint64_t)klass);
  uint32_t l = 0;
  uint32_t r = meta_idx.class_infos_by_klass_len;
  uint32_t m = 0;
  class_info_t *info = 0;
  LOG_DBG("%d %d", l, r);
  while (l < r) {
    m = (r - l) / 2 + l;
    info = meta_idx.class_infos_by_klass[m];
    if (info->klass == klass) {
      return info;
    } else if (info->klass < klass) {
      l = m + 1;
    } else {
      r = m;
    }
  }
  UNREACHABLE_CRIT;
  return 0;
}

UNUSED __forceinline uint64_t get_enum(class_info_t *info, uint32_t ordinal) {
  return ((uint64_t *)((uintptr_t)info + info->enum_ref_arr_off))[ordinal];
}

__forceinline bool is_enum(class_info_t *info) {
  return info->enum_ref_arr_off != 0;
}
__forceinline bool is_array(class_info_t *info) { return info->dim >= 1; }
__forceinline bool is_object(class_info_t *info) { return info->dim == 0; }

__forceinline void show_class_infos(void) {
  LOG_INFO("sorted by id, %ld", meta_idx.class_infos_by_id_len);
  for (uint32_t i = 0; i < meta_idx.class_infos_by_id_len; i++) {
    class_info_t *info = meta_idx.class_infos_by_id[i];
    LOG_INFO("%d %s", info->id, ((const char *)(info) + info->sig_off));
  }
  LOG_INFO("sorted by klass, %ld", meta_idx.class_infos_by_klass_len);
  for (uint32_t i = 0; i < meta_idx.class_infos_by_klass_len; i++) {
    class_info_t *info = meta_idx.class_infos_by_klass[i];
    LOG_INFO("%lX %s", (uint64_t)info->klass,
             ((const char *)(info) + info->sig_off));
  }
}

UNUSED __forceinline void hex_dump(uint8_t *p, uint64_t size) {
  uint64_t offset = 0;
  uint64_t remain = size;
  while (remain > 0) {
    uint8_t *q = &p[offset];
    LOG_INFO("%p %02X %02X %02X %02X %02X %02X %02X %02X", (void *)q, q[0],
             q[1], q[2], q[3], q[4], q[5], q[6], q[7]);
    offset += 8;
    remain -= 8;
  }
}

// buffer helper

__forceinline uint32_t parse_u32_at(void *p, uint32_t off) {
  return *(uint32_t *)&((const char *)p)[off];
}

UNUSED __forceinline uint64_t parse_u64_at(void *p, uint32_t off) {
  return *(uint64_t *)&((const char *)p)[off];
}

UNUSED __forceinline void place_u32_at(void *p, uint32_t v, uint32_t off) {
  *(uint32_t *)&((const char *)p)[off] = v;
}

UNUSED __forceinline void place_u64_at(void *p, uint64_t v, uint32_t off) {
  *(uint64_t *)&((const char *)p)[off] = v;
}

__forceinline void *parse_cptr(uint32_t cptr, uint64_t base, uint32_t mode,
                               uint32_t shift) {
  uint64_t ptr = cptr;
  switch (mode) {
  case JVM_COMPRESS_PTR_MODE_RAW32:
    return (void *)ptr;
  case JVM_COMPRESS_PTR_MODE_ZERO_BASED:
    return (void *)(ptr << shift);
  case JVM_COMPRESS_PTR_MODE_NON_ZERO_BASED:
    return (void *)(base + (ptr << shift));
  case JVM_COMPRESS_PTR_MODE_RAW64:
  default:
    __builtin_unreachable();
  }
}

__forceinline uint32_t compress_ptr(uint64_t ptr, uint64_t base, uint32_t mode,
                                    uint32_t shift) {
  switch (mode) {
  case JVM_COMPRESS_PTR_MODE_RAW32:
    return ptr;
  case JVM_COMPRESS_PTR_MODE_ZERO_BASED:
    return ptr >> shift;
  case JVM_COMPRESS_PTR_MODE_NON_ZERO_BASED:
    return (ptr >> shift) - base;
  case JVM_COMPRESS_PTR_MODE_RAW64:
  default:
    __builtin_unreachable();
  }
}

__forceinline void *parse_heap_cptr(uint64_t cptr) {
  return parse_cptr(cptr, jvm_args.h_heap_base, jvm_args.heap_compress_ptr_mode,
                    jvm_args.heap_compress_ptr_shift);
}

UNUSED __forceinline void *parse_metaspace_cptr(uint64_t cptr) {
  return parse_cptr(cptr, jvm_args.h_compressed_class_space_base,
                    jvm_args.metaspace_compress_ptr_mode,
                    jvm_args.metaspace_compress_ptr_shift);
}

UNUSED __forceinline uint32_t compress_heap_ptr(uint64_t ptr) {
  return compress_ptr(ptr, jvm_args.h_heap_base,
                      jvm_args.heap_compress_ptr_mode,
                      jvm_args.heap_compress_ptr_shift);
}

UNUSED __forceinline uint32_t compress_metaspace_ptr(uint64_t ptr) {
  return compress_ptr(ptr, jvm_args.h_compressed_class_space_base,
                      jvm_args.metaspace_compress_ptr_mode,
                      jvm_args.metaspace_compress_ptr_shift);
}

UNUSED __forceinline void *get_klass_pointer(void *d_object) {
  return parse_metaspace_cptr(parse_u32_at(d_object, sizeof(uint64_t)));
}
static uint32_t get_enum_ordinal(void *d_object) {
  return parse_u32_at(d_object, sizeof(uint64_t) + sizeof(uint32_t));
}
__forceinline uint32_t get_array_length(void *d_array, uint32_t header_size) {
  return parse_u32_at(d_array, header_size - sizeof(uint32_t));
}
__forceinline uint32_t get_array_elem_cptr(void *d_array, uint32_t header_size,
                                           uint32_t i) {
  return parse_u32_at(d_array, header_size + i * sizeof(uint32_t));
}

UNUSED __forceinline bool is_null_f(flag_t f) {
  return (f & CTRL_FLAG_MASK) == NULL_FLAG;
}
__forceinline bool is_object_f(flag_t f) {
  return (f & TYPE_FLAG_MASK) == OBJECT_FLAG;
}
UNUSED __forceinline bool is_array_f(flag_t f) {
  return (f & TYPE_FLAG_MASK) == ARRAY_FLAG;
}
UNUSED __forceinline bool is_enum_f(flag_t f) {
  return is_object_f(f) && (f & CTRL_FLAG_MASK) == ENUM_FLAG;
}
UNUSED __forceinline bool is_redirect_f(flag_t f) {
  return (f & CTRL_FLAG_MASK) == REDIRECT_FLAG;
}
__forceinline bool is_primitive_type(basic_type_t t) {
  return t >= T_BOOLEAN && t <= T_LONG;
}
__forceinline bool is_reference_type(basic_type_t t) {
  return t >= T_OBJECT && t <= T_ARRAY;
}

__forceinline uint32_t type_size(uint16_t t) {
  switch (t) {
  case T_BOOLEAN:
  case T_BYTE:
    return 1;
  case T_CHAR:
  case T_SHORT:
    return 2;
  case T_INT:
  case T_FLOAT:
    return 4;
  case T_LONG:
  case T_DOUBLE:
    return 8;
  case T_OBJECT:
  case T_ARRAY:
    return 4; // WARN we assume that CompressedOops is on
  case T_VOID:
  case T_ADDRESS:
  case T_NARROWOOP:
  case T_METADATA:
  case T_NARROWKLASS:
  case T_CONFLICT:
  case T_ILLEGAL:
    return 0;
  }
  __builtin_unreachable();
  return -1;
}

// buffer util

__forceinline uint32_t cur_off(buffer_t *b) { return b->cur_p - b->p; }

#define U_TYPE(w) uint##w##_t
#define PUT_DECL(w)                                                            \
  UNUSED __forceinline void put_u##w##_at(buffer_t *b, U_TYPE(w) v,            \
                                          uint32_t off) {                      \
    *(U_TYPE(w) *)(b->p + off) = v;                                            \
  }                                                                            \
  UNUSED __forceinline void put_u##w(buffer_t *b, U_TYPE(w) v) {               \
    *(U_TYPE(w) *)(b->cur_p) = v;                                              \
    b->cur_p += sizeof(U_TYPE(w));                                             \
  }

PUT_DECL(8)
PUT_DECL(16)
PUT_DECL(32)
PUT_DECL(64)

__forceinline void put(buffer_t *b, const char *d_src, uint32_t len) {
  d_memcpy(b->cur_p, d_src, len);
  b->cur_p += len;
}

#define U_TYPE(w) uint##w##_t
#define GET_DECL(w)                                                            \
  UNUSED __forceinline U_TYPE(w) get_u##w##_at(buffer_t *b, uint32_t off) {    \
    return *(U_TYPE(w) *)(b->p + off);                                         \
  }                                                                            \
  UNUSED __forceinline U_TYPE(w) get_u##w(buffer_t *b) {                       \
    U_TYPE(w) v = *(U_TYPE(w) *)(b->cur_p);                                    \
    b->cur_p += sizeof(U_TYPE(w));                                             \
    return v;                                                                  \
  }

GET_DECL(8)
GET_DECL(16)
GET_DECL(32)
GET_DECL(64)

UNUSED __forceinline void get(buffer_t *b, char *d_dest, uint32_t len) {
  d_memcpy(d_dest, b->cur_p, len);
  b->cur_p += len;
}
UNUSED __forceinline void get_at(buffer_t *b, char *d_dest, uint32_t len,
                                 uint32_t off) {
  d_memcpy(d_dest, b->p + off, len);
}
UNUSED __forceinline void skip(buffer_t *b, uint32_t n) { b->cur_p += n; }
UNUSED __forceinline void skip_next_align_8(buffer_t *b) {
  b->cur_p = align_up_8(b->cur_p);
}
__forceinline void fill_next_align_8(buffer_t *b) {
  uint32_t padding = align_up_8(b->cur_p) - b->cur_p;
  switch (padding) {
  case 0:
    break;
  case 7:
    put_u8(b, 0);
    put_u16(b, 0);
    put_u32(b, 0);
    break;
  case 6:
    put_u16(b, 0);
    put_u32(b, 0);
    break;
  case 5:
    put_u8(b, 0);
    put_u32(b, 0);
    break;
  case 4:
    put_u32(b, 0);
    break;
  case 3:
    put_u8(b, 0);
    put_u16(b, 0);
    break;
  case 2:
    put_u16(b, 0);
    break;
  case 1:
    put_u8(b, 0);
    break;
  }
}

// forward declaration
__forceinline uint32_t _do_serialize_recur(se_task_ctx_t *ctx,
                                           uint64_t h_object,
                                           class_id_t expected_id);

__forceinline void _do_serialize_object(se_task_ctx_t *ctx, uint64_t h_object,
                                        class_info_t *info) {
  LOG_DBG("_do_serialize_object");
  // TICK0;
  // count_object(ctx, info->id);
  put_u16(&ctx->outbuf, info->id);
  put_u16(&ctx->outbuf, OBJECT_FLAG);
  // uint32_t u32 = OBJECT_FLAG;
  // u32 = (u32 << 16) | info->id;
  // put_u32(&ctx->outbuf, u32);
  fill_next_align_8(&ctx->outbuf);
  uint32_t obj_base = cur_off(&ctx->outbuf);
  // TICK1;
  put(&ctx->outbuf, (char *)h2d(h_object + info->header_size),
      info->obj_size - info->header_size);
  // TICK2;
  // PRINT_TICK(2);
  for (uint32_t i = 0; i < info->n_non_static_field; i++) {
    const field_info_t *f = &info->fields[i];
    if (is_primitive_type(f->type)) {
      continue;
    }
    uint32_t f_offset = obj_base + f->offset;
    uint32_t h_member_cptr = get_u32_at(&ctx->outbuf, f_offset);
    uint64_t h_member = (uint64_t)parse_heap_cptr(h_member_cptr);
    uint32_t member_offset = _do_serialize_recur(ctx, h_member, f->id);
    put_u32_at(&ctx->outbuf, member_offset, f_offset);
  }
}

__forceinline void _do_serialize_array(se_task_ctx_t *ctx, uint64_t h_array,
                                       class_info_t *info) {
  LOG_DBG("_do_serialize_array");
  // TICK0;
  void *d_array = h2d(h_array);
  uint32_t length = get_array_length(d_array, info->header_size);
  // count_array(ctx, info->id, length);
  put_u16(&ctx->outbuf, info->id);
  put_u16(&ctx->outbuf, ARRAY_FLAG);
  put_u32(&ctx->outbuf, length);
  // uint64_t u64 = length;
  // u64 = (u64 << 16) | ARRAY_FLAG;
  // u64 = (u64 << 16) | info->id;
  // put_u64(&ctx->outbuf, u64);
  fill_next_align_8(&ctx->outbuf);
  // TICK1;
  const field_info_t *elem = &info->fields[0];
  if (is_reference_type(elem->type)) {
    for (uint32_t i = 0; i < length; i++) {
      uint32_t h_elem_cptr = get_array_elem_cptr(d_array, info->header_size, i);
      uint64_t h_elem = (uint64_t)parse_heap_cptr(h_elem_cptr);
      uint32_t elem_offset UNUSED = _do_serialize_recur(ctx, h_elem, elem->id);
    }
  } else if (is_primitive_type(elem->type)) {
    const char *d_src = (const char *)d_array + info->header_size;
    put(&ctx->outbuf, d_src, length * type_size(elem->type));
  } else {
    UNREACHABLE_CRIT;
  }
  // PRINT_TICK(1);
}

__forceinline uint32_t _do_serialize_recur(se_task_ctx_t *ctx,
                                           uint64_t h_object,
                                           class_id_t expected_id) {
  LOG_DBG("_do_serialize_recur");
  // TICK0;
  fill_next_align_8(&ctx->outbuf);
  uint32_t base = cur_off(&ctx->outbuf);
  // check null
  LOG_DBG("%d", base);
  if (h_object == 0) {
    put_u16(&ctx->outbuf, expected_id);
    put_u16(&ctx->outbuf, NULL_FLAG);
    // uint32_t u32 = NULL_FLAG;
    // u32 = (u32 << 16) | expected_id;
    // put_u32(&ctx->outbuf, u32);
    return base;
  }
  // check visited
  // TICK1;
  // uint32_t off = map_lookup(&ctx->ref2off, h_object);
  // if (off != invalid_value_u32) {
  //   put_u16(&ctx->outbuf, expected_id);
  //   put_u16(&ctx->outbuf, REDIRECT_FLAG);
  //   put_u32(&ctx->outbuf, off);
  //   // uint64_t u64 = off;
  //   // u64 = (u64 << 16) | REDIRECT_FLAG;
  //   // u64 = (u64 << 16) | expected_id;
  //   // put_u64(&ctx->outbuf, u64);
  //   return base;
  // }
  void *d_object = h2d(h_object);
  // TICK2;
  // check resovled
  class_info_t *info = get_class_info_by_klass(get_klass_pointer(d_object));
  // TICK3;
  // int ok = map_insert(&ctx->ref2off, h_object, base);
  // if (!ok) {
  //   UNREACHABLE_CRIT;
  // }
  // TICK4;
  // dispatch
  if (is_enum(info)) {
    // count_object(ctx, info->id);
    put_u16(&ctx->outbuf, info->id);
    put_u16(&ctx->outbuf, ENUM_FLAG | OBJECT_FLAG);
    // put_u32(&ctx->outbuf, get_ordinal(info, h_object));
    put_u32(&ctx->outbuf, get_enum_ordinal(d_object));
    // uint64_t u64 = get_ordinal(info, h_object);
    // u64 = (u64 << 16) | ENUM_FLAG | OBJECT_FLAG;
    // u64 = (u64 << 16) | info->id;
    // put_u64(&ctx->outbuf, u64);
  } else if (is_object(info)) {
    _do_serialize_object(ctx, h_object, info);
  } else if (is_array(info)) {
    _do_serialize_array(ctx, h_object, info);
  } else {
    UNREACHABLE_CRIT;
  }
  // PRINT_TICK(4);
  return base;
}

__forceinline uint64_t _do_serialize(se_task_ctx_t *ctx, uint64_t h_object) {
  LOG_DBG("_do_serialize");

  _do_serialize_recur(ctx, h_object, UNREGISTERED_CLASS_ID);
  meta_header_t *header = (meta_header_t *)(ctx->outbuf.p);
  header->total_length = ctx->outbuf.cur_p - ctx->outbuf.p;
  return header->total_length;
}

// communication with Host
__forceinline void post_recv(doca_dpa_dev_comch_consumer_t cons_h, uint32_t n) {
  doca_dpa_dev_comch_consumer_ack(cons_h, n);
}

__forceinline void post_send(doca_dpa_dev_comch_producer_t prod_h,
                             uint32_t cons_id, uint8_t *imm, uint32_t imm_len) {
  doca_dpa_dev_comch_producer_post_send_imm_only(
      prod_h, cons_id, imm, imm_len, DOCA_DPA_DEV_SUBMIT_FLAG_FLUSH);
}

__forceinline se_output_t do_serialize(sd_thread_args_t *args, se_input_t in) {
  se_task_ctx_t *ctx = (se_task_ctx_t *)args->d_ctx;
  LOG_DBG("thread args: %lX %ld %lX %ld", args->d_output, args->d_output_len,
          args->d_ctx, args->d_ctx_len);
  LOG_DBG("se input: %lX", in);
  // init_map(&ctx->ref2off);
  ctx->outbuf.p = (char *)args->d_output;
  ctx->outbuf.cur_p = ctx->outbuf.p + OBJECT_DATA_OFFSET;
  ctx->outbuf.lim = args->d_output_len;
  doca_dpa_dev_mmap_get_external_ptr(jvm_args.heap_mmap_h,
                                     jvm_args.h_heap_base);
  uint64_t length = _do_serialize(ctx, in.h_object);
  void *h_output = (void *)doca_dpa_dev_mmap_get_external_ptr(args->h_output_h,
                                                              args->h_output);
  d_memcpy(h_output, ctx->outbuf.p, length);
  __dpa_thread_window_writeback();
  se_output_t out;
  out.length = length;
  out.h_output = args->h_output;
  return out;
}

__forceinline void comch_cons_comp(dpa_thread_args_t *p) {
  doca_dpa_dev_comch_consumer_completion_element_t cons_e;
  int got = doca_dpa_dev_comch_consumer_get_completion(p->cons_comp_h, &cons_e);
  if (got) {
    LOG_DBG("cons comp");
    uint32_t imm_len = 0;
    payload_t *input =
        (payload_t *)doca_dpa_dev_comch_consumer_get_completion_imm(cons_e,
                                                                    &imm_len);
    se_input_t in = *(se_input_t *)input->payload;
    se_output_t out = do_serialize((sd_thread_args_t *)p->d_ctx, in);
    payload_t output;
    output.id = input->id;
    *(se_output_t *)(output.payload) = out;
    doca_dpa_dev_comch_consumer_completion_ack(p->cons_comp_h, 1);
    doca_dpa_dev_comch_consumer_completion_request_notification(p->cons_comp_h);
    post_recv(p->cons_h, 1);
    post_send(p->prod_h, p->cpu_cons_id, (uint8_t *)(&output),
              sizeof(payload_t));
  }
}

__forceinline void comch_prod_comp(dpa_thread_args_t *p) {
  doca_dpa_dev_completion_element_t prod_e;
  int got = doca_dpa_dev_get_completion(p->prod_comp_h, &prod_e);
  if (got) {
    LOG_DBG("prod comp");
    LOG_DBG("send serialize result done");
    doca_dpa_dev_completion_ack(p->prod_comp_h, 1);
    doca_dpa_dev_completion_request_notification(p->prod_comp_h);
  }
}

// RPCs

__dpa_rpc__ static uint64_t
register_class_infos(doca_dpa_dev_uintptr_t dev_class_infos_p,
                     doca_dpa_dev_mmap_t host_class_infos_h,
                     doca_dpa_dev_uintptr_t host_class_infos_base,
                     uint64_t class_infos_len, uint64_t n_class_infos,
                     doca_dpa_dev_mmap_t host_by_id_offsets_h,
                     doca_dpa_dev_uintptr_t host_by_id_offsets_base,
                     doca_dpa_dev_mmap_t host_by_klass_offsets_h,
                     doca_dpa_dev_uintptr_t host_by_klass_offsets_base) {
  uint64_t offsets_length = n_class_infos * sizeof(uint64_t);

  meta_idx.class_infos_base = (void *)dev_class_infos_p;
  meta_idx.class_infos_lim = class_infos_len;
  meta_idx.class_infos_by_id =
      (class_info_t **)(dev_class_infos_p + class_infos_len);
  meta_idx.class_infos_by_id_len = n_class_infos;
  meta_idx.class_infos_by_klass =
      (class_info_t **)(dev_class_infos_p + class_infos_len + offsets_length);
  meta_idx.class_infos_by_klass_len = n_class_infos;

  uint8_t *host_class_infos_base_ptr =
      (uint8_t *)doca_dpa_dev_mmap_get_external_ptr(host_class_infos_h,
                                                    host_class_infos_base);
  d_memcpy(meta_idx.class_infos_base, host_class_infos_base_ptr,
           class_infos_len);
  uint64_t *host_by_id_offsets_base_ptr =
      (uint64_t *)doca_dpa_dev_mmap_get_external_ptr(host_by_id_offsets_h,
                                                     host_by_id_offsets_base);
  d_memcpy(meta_idx.class_infos_by_id, host_by_id_offsets_base_ptr,
           offsets_length);
  uint64_t *host_by_klass_offsets_base_ptr =
      (uint64_t *)doca_dpa_dev_mmap_get_external_ptr(
          host_by_klass_offsets_h, host_by_klass_offsets_base);
  d_memcpy(meta_idx.class_infos_by_klass, host_by_klass_offsets_base_ptr,
           offsets_length);

  for (uint32_t i = 0; i < n_class_infos; i++) {
    LOG_DBG("%lX %lX", (uintptr_t)meta_idx.class_infos_by_id[i],
            (uintptr_t)meta_idx.class_infos_by_klass[i]);
    *(uintptr_t *)(&meta_idx.class_infos_by_id[i]) += dev_class_infos_p;
    *(uintptr_t *)(&meta_idx.class_infos_by_klass[i]) += dev_class_infos_p;
    LOG_DBG("%lX %lX", (uintptr_t)meta_idx.class_infos_by_id[i],
            (uintptr_t)meta_idx.class_infos_by_klass[i]);
  }

  show_class_infos();

  return 0;
}

__dpa_rpc__ static uint64_t
register_jvm_heap(uint64_t jvm_heap_base, uint64_t jvm_heap_size,
                  uint64_t jvm_compressed_class_space_base,
                  uint64_t jvm_compressed_class_space_size,
                  doca_dpa_dev_mmap_t jvm_heap_mmap_h,
                  uint32_t jvm_heap_compress_ptr_mode,
                  uint32_t jvm_metaspace_compress_ptr_mode,
                  uint32_t jvm_heap_compress_ptr_shift,
                  uint32_t jvm_metaspace_compress_ptr_shift) {
  jvm_args.h_heap_base = jvm_heap_base;
  jvm_args.h_heap_size = jvm_heap_size;
  jvm_args.h_compressed_class_space_base = jvm_compressed_class_space_base;
  jvm_args.h_compressed_class_space_size = jvm_compressed_class_space_size;
  jvm_args.heap_mmap_h = jvm_heap_mmap_h;
  jvm_args.heap_compress_ptr_mode = jvm_heap_compress_ptr_mode;
  jvm_args.metaspace_compress_ptr_mode = jvm_metaspace_compress_ptr_mode;
  jvm_args.heap_compress_ptr_shift = jvm_heap_compress_ptr_shift;
  jvm_args.metaspace_compress_ptr_shift = jvm_metaspace_compress_ptr_shift;

  LOG_V(jvm_args.h_heap_base, "%lX");
  LOG_V(jvm_args.h_heap_size, "%lX");
  LOG_V(jvm_args.h_compressed_class_space_base, "%lX");
  LOG_V(jvm_args.h_compressed_class_space_size, "%lX");
  LOG_V(jvm_args.heap_mmap_h, "%X");
  LOG_V(jvm_args.heap_compress_ptr_mode, "%X");
  LOG_V(jvm_args.metaspace_compress_ptr_mode, "%X");
  LOG_V(jvm_args.heap_compress_ptr_shift, "%X");
  LOG_V(jvm_args.metaspace_compress_ptr_shift, "%X");
  return 0;
}

// Main Function

__dpa_global__ static void serialize(void) {
  LOG_DBG("trigger");
  dpa_thread_args_t *p =
      (dpa_thread_args_t *)doca_dpa_dev_thread_get_local_storage();
  comch_cons_comp(p);
  comch_prod_comp(p);
  LOG_DBG("reschedule");
  doca_dpa_dev_thread_reschedule();
}
