#include "sd/native/class_walker.hxx"

#include "sd/native/class_info.hxx"
#include "sd/native/class_resolver.hxx"

namespace dpx::sd {

class_id_t ClassWalker::walk_enum_klass(JNIEnv *j_env, const FakeInstanceKlass *klass, const jclass j_class) {
  if (auto info = r.get_class_info(klass); !info.is_dummy()) {
    return info.id();  // registered
  }
  // TODO: we shall pre-register the java/lang/Enum into env, here we can get
  // the correct the field offset, and access the field properly.
  // sizeof(marker) + sizeof(cptr) same with header size
  uint32_t ordinal_offset = sizeof(FakeObject) + sizeof(uint32_t);
  [[maybe_unused]] auto class_object = FakeObject::from_clazz(j_class);
  auto info = r.from_instance_klass(klass, true);
  info.set_header_size(ordinal_offset);
  info.set_obj_size(ordinal_offset + sizeof(uint32_t));
  assert(info.n_non_static_field() == 0);
  // NOTICE: the last static field is the array type of this enum class, and contains all of them.
  assert(info.n_static_field() > 1);
  // TODO: just copy the last array, here we copy one by one to check ordinal
  auto sig = std::string(info.signature());
  auto cp = klass->constant_pool;
  for (uint32_t i = 0; i < info.n_static_field() - 1; i++) {
    auto &f = klass->field(i);
    auto j_field_id = j_env->GetStaticFieldID(klass->clazz(), f.name(cp).c_str(), sig.c_str());
    auto enum_instance_handle = j_env->GetStaticObjectField(klass->clazz(), j_field_id);
    TRACE("field name: {} signature: {} j_field_id: {:X} handle: {:}", f.name(cp), info.signature(),
          (uintptr_t)j_field_id, (void *)enum_instance_handle);
    // NOTICE: we used to get enum instance in a hack way
    // auto enum_instance =
    // class_object->reference_at(klass->field(i).offset());
    auto enum_instance = FakeObject::from_jobject(enum_instance_handle);
    auto enum_global_instance_handle = j_env->NewGlobalRef(enum_instance_handle);
    // NOTICE: If the enum instance does not used before, they will not be
    // created. So we will trigger "getEnumConstants" in java code to ensure
    // they are created.
    assert(enum_instance != nullptr);
    auto ordinal = enum_instance->enum_ordinal();
    assert(i == ordinal);  // maybe not
    info.set_enum(ordinal, enum_instance, enum_global_instance_handle);
    TRACE("ordinal: {} instance: {:}", ordinal, (void *)enum_instance);
  }
  register_class_info(info);
  return info.id();
}

class_id_t ClassWalker::walk_instance_klass(JNIEnv *j_env, const FakeInstanceKlass *klass) {
  if (auto info = r.get_class_info(klass); !info.is_dummy()) {
    return info.id();  // registered
  }

  // TODO super klass
  auto cp = klass->constant_pool;
  auto info = r.from_instance_klass(klass, false);
  TRACE("{} with {} fields", info.signature(), info.n_field());
  std::vector<uint32_t> idx;
  idx.reserve(info.n_non_static_field());
  // collect all non-static fields and calculate field offset
  for (uint32_t i = 0; i < info.n_field(); i++) {
    auto &jfield = klass->field(i);
    if (jfield.is_internal()) {
      // WARN do not touch
      TRACE("internal field {}, offset: {}", i, jfield.offset());
    } else if (jfield.is_static()) {
      // WARN do not touch
      TRACE("static field {}, signature: {}, offset: {}", i, jfield.signature_view(cp), jfield.offset());
    } else {
      // non-static
      TRACE("non-static field {}, signature: {}, offset: {}", i, jfield.signature_view(cp), jfield.offset());
      idx.push_back(i);
      // the minimum of field.offset() is the object header size.
      auto offset = jfield.offset();
      if (offset < info.object_header_size()) {
        info.set_header_size(offset);
      }
    }
  }
  TRACE("object header size: {}", info.object_header_size());

  // sort by field offset
  std::ranges::sort(
      idx, [klass](uint32_t l, uint32_t r) -> bool { return klass->field(l).offset() < klass->field(r).offset(); });

  // resolve fields
  for (uint32_t i = 0; i < info.n_non_static_field(); i++) {
    auto &field = info.field(i);
    auto &jfield = klass->field(idx[i]);
    // remove header size here
    field.offset = jfield.offset() - info.object_header_size();
    field.type = jfield.type(cp);
    field.flag = jfield.access_flags;
    TRACE("resolve field {}, offset: {}, type: {}", field.id, field.offset, type2str(field.type));
    if (is_reference_type(field.type)) {
      auto sig = jfield.signature(cp);
      auto name = jfield.name(cp);
      auto j_field_id = j_env->GetFieldID(klass->clazz(), name.c_str(), sig.c_str());
      TRACE("name: {}, signature: {}, j_field_id: {:X}", name, sig, (uintptr_t)j_field_id);
      field.j_field_id = (uintptr_t)j_field_id;
      auto info = r.get_class_info(sig);
      if (info.is_dummy()) {
        field.id = UNREGISTERED_CLASS_ID;
        if (auto iter = r.unresolved.find(sig); iter != r.unresolved.end()) {
          iter->second.push_back(&field);
        } else {
          r.unresolved.emplace(std::string(sig), std::vector<field_info_t *>{&field});
        }
      } else {
        field.id = info.id();
      }
    } else if (is_primitive_type(field.type)) {
      field.j_field_id = (uintptr_t)j_env->GetFieldID(klass->clazz(), jfield.name(cp).c_str(), type2sig(field.type));
    } else {
      unreachable();
    }
  }

  register_class_info(info);
  return info.id();
}

class_id_t ClassWalker::walk_array_klass(JNIEnv *j_env, const FakeArrayKlass *klass) {
  if (auto info = r.get_class_info(klass); !info.is_dummy()) {
    return info.id();  // registered
  }
  auto info = r.from_array_klass(klass);
  auto &elem_info = info.field(0);
  if (klass->dimension == 1) {  // 1-dim array is special
    auto t = klass->lh.element_type();
    elem_info.type = t;
    if (t == T_OBJECT) {
      auto elem_klass = ((const FakeObjectArrayKlass *)klass)->element_klass;
      if (elem_klass->is_enum()) {
        elem_info.id = walk_enum_klass(j_env, elem_klass, elem_klass->clazz());
      } else {
        elem_info.id = walk_instance_klass(j_env, elem_klass);
      }
    }
  } else {
    elem_info.type = T_ARRAY;
    elem_info.id = walk_array_klass(j_env, klass->lower_dimension);
  }
  register_class_info(info);
  return info.id();
}

void ClassWalker::register_class_info(ClassInfo info) {
  TRACE("register {}, klass: {:}, id: {}", info.signature(), (void *)info.klass(), info.id());
  // WARN: do not check registered info
  info.set_id(ClassResolver::index2id(r.info_by_id.size()));
  r.info_by_id.push_back(info);
  r.info_by_klass.insert(
      std::ranges::lower_bound(r.info_by_klass, info,
                               [](const ClassInfo &l, const ClassInfo &r) -> bool { return l.klass() < r.klass(); }),
      info);
  r.sig2id.emplace(std::string(info.signature()), info.id());

  if (auto iter = r.unresolved.find(info.signature()); iter != r.unresolved.end()) {
    for (auto f : iter->second) {
      f->id = info.id();
    }
    r.unresolved.erase(iter);
  }
}

}  // namespace dpx::sd
