#include "sd/native/object_reviver.hxx"

#include <simdutf.h>

#include "sd/native/class_resolver.hxx"
#include "sd/native/jenv_util.hxx"

namespace dpx::sd {

ObjWithHandle ObjectReviver::do_parse_object(uint32_t base, ClassInfo info) {
  b.skip_next_align_8();
  auto handle = j_env->AllocObject(info.klass()->clazz());
  if (handle == nullptr) {
    j_env->ExceptionDescribe();
    die("Fail to allocate object");
  }
  auto obj = FakeObject::from_jobject(handle);
  TRACE("revive at: {} object: {} handle: {}", base, (void *)obj, (void *)handle);
  // off2ref.insert_kv(base, handle);
  if (root == nullptr) {
    root = handle;
  }
  // b.get(obj->raw(info.object_header_size()), info.object_body_size());
  // INFO("{}", Hexdump(b.raw(), info.object_body_size()));
  auto field_base = b.offset();
  b.skip(info.object_body_size());
  // INFO("field_base: {}", field_base);
  TRACE("revive {} id: {} n non static field {}", info.signature(), info.id(), info.n_non_static_field());
  for (uint32_t i = 0; i < info.n_non_static_field(); i++) {
    auto &f = info.get_field(i);
    TRACE("field: {} id: {} offset: {} type: {}", i, f.id, f.offset, type2str(f.type));
    if (is_primitive_type(f.type)) {
      switch (f.type) {
        case T_BOOLEAN: {
          jboolean v = *(jboolean *)b.raw_at(field_base + f.offset);
          j_env->SetBooleanField(handle, (jfieldID)f.j_field_id, v);
          break;
        }
        case T_BYTE: {
          jbyte v = *(jbyte *)b.raw_at(field_base + f.offset);
          j_env->SetByteField(handle, (jfieldID)f.j_field_id, v);
          break;
        }
        case T_CHAR: {
          jchar v = *(jchar *)b.raw_at(field_base + f.offset);
          j_env->SetCharField(handle, (jfieldID)f.j_field_id, v);
          break;
        }
        case T_SHORT: {
          jshort v = *(jshort *)b.raw_at(field_base + f.offset);
          j_env->SetShortField(handle, (jfieldID)f.j_field_id, v);
          break;
        }
        case T_INT: {
          jint v = *(jint *)b.raw_at(field_base + f.offset);
          j_env->SetIntField(handle, (jfieldID)f.j_field_id, v);
          break;
        }
        case T_FLOAT: {
          jfloat v = *(jfloat *)b.raw_at(field_base + f.offset);
          j_env->SetFloatField(handle, (jfieldID)f.j_field_id, v);
          break;
        }
        case T_LONG: {
          jlong v = *(jlong *)b.raw_at(field_base + f.offset);
          j_env->SetLongField(handle, (jfieldID)f.j_field_id, v);
          break;
        }
        case T_DOUBLE: {
          jdouble v = *(jdouble *)b.raw_at(field_base + f.offset);
          j_env->SetDoubleField(handle, (jfieldID)f.j_field_id, v);
          break;
        }
        default: {
          unreachable();
        }
      }
    } else if (is_reference_type(f.type)) {
      // NOTICE: as we cannot modify the input buffer, so we have to use place_at
      // to change the reference in the object, but in dpa, we can.
      auto [member, member_handle] = parse(f.id);
      j_env->SetObjectField(handle, (jfieldID)f.j_field_id, member_handle);
      j_env->DeleteLocalRef(member_handle);
    }
  }
  if (j_env->ExceptionCheck()) {
    j_env->ExceptionDescribe();
    die("Meet exception");
  }
  return {obj, handle};
}

ObjWithHandle ObjectReviver::do_parse_array(uint32_t base, ClassInfo info) {
  auto length = b.get<uint32_t>();
  TRACE("revive array with length: {}", length);
  b.skip_next_align_8();
  auto &elem = info.get_field(0);
  jobject handle = nullptr;
  if (is_primitive_type(elem.type)) {
    handle = new_array(j_env, elem.type, length);
  } else {
    auto elem_info = r.get_class_info(elem.id);
    handle = new_array(j_env, elem_info.klass(), length);
  }
  if (handle == nullptr) {
    j_env->ExceptionDescribe();
    die("Fail to allocate array");
  }
  auto obj = FakeObject::from_jobject(handle);
  TRACE("revive at: {} object: {} handle: {}", base, (void *)obj, (void *)handle);
  // off2ref.insert_kv(base, handle);
  if (root == nullptr) {
    root = handle;
  }
  TRACE("revive {} id: {}", info.signature(), info.id());
  if (is_primitive_type(elem.type)) {
    assert(info.dim() == 1);
    if (elem.type == T_CHAR && o.enable_utf16_to_utf8) {
      cvt_utf8_to_utf16(obj, info, length);
    } else {
      size_t j_length = j_env->GetArrayLength((jarray)handle);
      if (length != j_length) {
        die("{} {}", length, j_length);
      }
      // b.get(obj->raw(info.array_header_size()), info.array_body_size(length));
      j_env->SetByteArrayRegion((jbyteArray)handle, 0, length, (const jbyte *)b.raw_at(b.offset()));
      b.skip(info.array_body_size(length));
    }
  } else if (is_reference_type(elem.type)) {
    for (uint32_t i = 0; i < length; i++) {
      auto [member, member_handle] = parse(elem.id);
      j_env->SetObjectArrayElement((jobjectArray)handle, i, member_handle);
      j_env->DeleteLocalRef(member_handle);
    }
  } else {
    unreachable();
  }
  if (j_env->ExceptionCheck()) {
    j_env->ExceptionDescribe();
    die("Meet exception");
  }
  return {obj, handle};
}

ObjWithHandle ObjectReviver::parse(class_id_t expected_id) {
  b.skip_next_align_8();
  auto base = b.offset();
  auto id = b.get<class_id_t>();
  auto flag = b.get<flag_t>();
  TRACE("expected id: {} base: {}, id: {}, flag: {:X}", expected_id, base, id, flag);
  if (is_null_f(flag)) {
    return {nullptr, nullptr};
  }
  if (is_redirect_f(flag)) {
    // auto offset = b.get<uint32_t>();
    // TRACE("redirect offset: {}", offset);
    // auto [handle, found] = off2ref.lookup_v(offset);
    // assert(found);
    // return {FakeObject::from_jobject(handle), handle};
    die("can not be redirect");
  }
  auto info = r.get_class_info(id);
  assert(!info.is_dummy());
  // TODO check expected_id & id compatible here
  if (info.is_enum()) {
    assert(is_enum_f(flag));
    auto h = info.get_enum(b.get<uint32_t>());
    // off2ref.insert_kv(base, h.second);
    return h;
    return {nullptr, nullptr};
  } else if (info.is_object()) {
    assert(is_object_f(flag));
    return do_parse_object(base, info);
  } else if (info.is_array()) {
    assert(is_array_f(flag));
    return do_parse_array(base, info);
  } else {
    unreachable();
  }
}

jobject ObjectReviver::revive(JNIEnv *j_env, const FakeKlass *klass, ClassResolver &resolver, const Options &o,
                              naive::BorrowedBuffer ctx, naive::BorrowedBuffer in) {
  auto info = resolver.get_class_info(klass);
  assert(!info.is_dummy());
  ObjectReviver r(j_env, resolver, o, ctx, in);
  auto h = (meta_header_t *)r.b.raw_at(0);
  r.b.skip(OBJECT_DATA_OFFSET);
  // TRACE("{}", Hexdump(in.data(), h->total_length));
  TRACE("begin offset: {}", r.b.offset());
  r.parse(info.id());
  TRACE("end offset: {}", r.b.offset());
  return r.root;
}

void ObjectReviver::cvt_utf8_to_utf16(const FakeObject *obj, ClassInfo info, uint32_t length) {
  auto origin_byte_size [[maybe_unused]] = b.get<uint32_t>();
  auto u16_raw = (char16_t *)obj->raw(info.array_header_size());
  auto actual_byte_size = b.get<uint32_t>();
  auto u8_raw = b.raw();
  uint32_t actual_length =
      simdutf::convert_utf8_to_utf16le(reinterpret_cast<char *>(u8_raw), actual_byte_size, u16_raw);
  TRACE("origin size: {}, actual size: {}, length: {}", origin_byte_size, actual_byte_size, length);
  assert(actual_length == length);
  b.skip(actual_byte_size);
}

}  // namespace dpx::sd
