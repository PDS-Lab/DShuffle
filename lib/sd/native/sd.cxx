#include "sd/native/sd.hxx"

#include "sd/native/object_reviver.hxx"
#include "sd/native/object_walker.hxx"

namespace dpx::sd {

jbyteArray Context::serialize(JNIEnv* j_env, jobject j_obj) {
  auto obj = FakeObject::from_jobject(j_obj);
  auto total_length = ObjectWalker::walk(obj, r, o, ctx_buffer.borrow(), out_buffer.borrow());
  DEBUG("total length: {}", total_length);
  auto j_output = j_env->NewByteArray(total_length);
  auto j_output_obj = FakeObject::from_jobject(j_output);
  memcpy(j_output_obj->raw(j_output_obj->array_header_size()), out_buffer.data(), total_length);
  TRACE("{}", Hexdump(out_buffer.data(), total_length));
  return j_output;
}

jobject Context::deserialize(JNIEnv* j_env, jbyteArray j_input, jclass j_cls) {
  auto klass = FakeKlass::from_clazz(j_cls);
  // auto j_input_obj = FakeObject::from_jobject(j_input);
  // auto in_buffer =
  // naive::BorrowedBuffer(reinterpret_cast<uint8_t*>(j_input_obj->raw(j_input_obj->array_header_size())),
  //                                        j_input_obj->array_length(j_input_obj->array_header_size()));
  jboolean is_copy = false;
  auto raw_input = j_env->GetPrimitiveArrayCritical(j_input, &is_copy);
  auto raw_input_length = j_env->GetArrayLength(j_input);
  auto in_buffer = naive::BorrowedBuffer((uint8_t*)raw_input, raw_input_length);
  auto j_obj = ObjectReviver::revive(j_env, klass, r, o, ctx_buffer.borrow(), in_buffer);
  j_env->ReleasePrimitiveArrayCritical(j_input, raw_input, 0);
  return j_obj;
}

jbyteArray DPAContext::do_serialize(JNIEnv* j_env, jobject j_obj) {
  auto object = FakeObject::from_jobject(j_obj);
  auto info = r.get_class_info(object->klass_pointer());
  if (info.is_dummy()) {
    return nullptr;
  }
  if (info.is_array() && o.max_device_threads > 1) {
    auto& f = info.get_field(0);
    if (is_reference_type(f.type)) {
      return do_array_obj_serialize(j_env, object, info);
    }
  }
  return do_obj_serialize(j_env, object);
}

jbyteArray DPAContext::do_obj_serialize(JNIEnv* j_env, FakeObject* object) {
  doca::TaskContext ctx;
  se_input_t in = {.h_object = (uintptr_t)object};
  TRACE("{:X}", in.h_object);
  ctx.set_input(in);
  TRACE("trigger");
  g.trigger(ctx).get();
  TRACE("trigger done");
  auto out = ctx.get_output<se_output_t>();
  TRACE("dpa done");
  jbyteArray j_result = j_env->NewByteArray(out.length);
  auto result_obj = FakeObject::from_jobject(j_result);
  auto out_buffer = reinterpret_cast<const void*>(out.h_output);
  memcpy(result_obj->raw(result_obj->array_header_size()), out_buffer, out.length);
  DEBUG("{}", Hexdump(out_buffer, out.length));
  return j_result;
}

jbyteArray DPAContext::do_array_obj_serialize(JNIEnv* j_env, FakeObject* object, ClassInfo info) {
  uint32_t length = object->array_length(info.array_header_size());
  RWBuffer b(out.borrow());
  b.skip(OBJECT_DATA_OFFSET);
  assert(b.offset() % 8 == 0);
  b.put(info.id());
  b.put(ARRAY_FLAG);
  b.put(length);
  b.fill_next_align_8();
  uint32_t batch_size = o.max_device_threads;
  uint32_t n_batch = (length + batch_size - 1) / batch_size;
  TRACE("batch_size: {}, n_batch: {}", batch_size, n_batch);
  for (uint32_t i = 0; i < n_batch; ++i) {
    TRACE("batch {} begin", i);
    uint32_t n_task = std::min(batch_size, length - i * batch_size);
    std::vector<doca::TaskContext> ctxs(n_task);
    std::vector<boost::fibers::future<void>> fs;
    for (uint32_t j = 0; j < n_task; ++j) {
      auto elem_obj = object->array_elem_ref(info.array_header_size(), i * batch_size + j);
      se_input_t in = {.h_object = (uintptr_t)elem_obj};
      ctxs[j].set_input(in);
      fs.push_back(g.trigger(ctxs[j]));
    }
    for (uint32_t j = 0; j < n_task; ++j) {
      fs[j].get();
      auto out = ctxs[j].get_output<se_output_t>();
      b.fill_next_align_8();
      auto out_buffer = reinterpret_cast<const void*>(out.h_output + OBJECT_DATA_OFFSET);
      auto out_length = out.length - OBJECT_DATA_OFFSET;
      b.put(out_buffer, out_length);
      DEBUG("{}", Hexdump(out_buffer, out_length));
    }
    TRACE("batch {} end with length {}", i, b.offset());
  }
  uint32_t total_length = b.offset();
  ((meta_header_t*)b.raw_at(0))->total_length = total_length;
  jbyteArray j_result = j_env->NewByteArray(total_length);
  auto result_obj = FakeObject::from_jobject(j_result);
  memcpy(result_obj->raw(result_obj->array_header_size()), out.data(), total_length);
  return j_result;
}

}  // namespace dpx::sd
