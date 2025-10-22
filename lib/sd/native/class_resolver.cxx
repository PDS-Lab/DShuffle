#include "sd/native/class_resolver.hxx"

#include "doca/buffer.hxx"
#include "sd/native/class_walker.hxx"
#include "sd/native/jenv_util.hxx"

namespace dpx::sd {

void ClassResolver::show_class_infos() const {
  std::string result = "\nRegistered:\n";
  for (auto [sig, id] : sig2id) {
    auto info = get_class_info(id);
    result += std::format("{}\n", info);
  }
  result += std::format("\nUnresolved:\n");
  for (auto &[sig, _] : unresolved) {
    result += sig;
    result += '\n';
  }
  INFO(result);
}

ClassInfo ClassResolver::from_instance_klass(const FakeInstanceKlass *klass, bool is_enum) {
  auto [n_field, n_static_field] = klass->fields_count();
  uint8_t n_non_static_field = n_field - n_static_field;
  TRACE("n field: {}, n static field: {}, n non static field: {}", (int)n_field, (int)n_static_field,
        (int)n_non_static_field);

  uint32_t needed_size = sizeof(class_info_t) + n_non_static_field * sizeof(field_info_t);
  uint16_t enum_ref_arr_off = 0;
  if (is_enum) {
    assert(n_static_field > 1 && n_non_static_field == 0);
    enum_ref_arr_off = needed_size;
    needed_size += (n_static_field - 1) * sizeof(obj_with_handle_t);
  }
  uint16_t sig_off = needed_size;
  needed_size += MAX_SIGNATURE_LENGTH;

  TRACE("enum ref arr off: {} sig off: {} needed size: {}", enum_ref_arr_off, sig_off, needed_size);

  auto raw_info = (char *)a.allocate(needed_size);

  auto info = (class_info_t *)raw_info;
  info->id = UNREGISTERED_CLASS_ID;
  info->obj_size = klass->lh.object_size();
  // NOTICE: instance header size will be set in the ClassWalker
  // actually, it will be the offset of the first field.
  info->header_size = UINT8_MAX;
  info->n_non_static_field = n_non_static_field;
  info->n_static_field = n_static_field;
  info->dim = 0;  // not array
  info->klass = (void *)klass;
  info->sig_off = sig_off;
  info->enum_ref_arr_off = enum_ref_arr_off;
  info->klass_cptr = JVMArgs::compress_metaspace_ptr(klass);
  // set sig
  auto sig = klass->signature();
  assert(sig.length() + 2 < MAX_SIGNATURE_LENGTH);
  auto info_sig = &raw_info[sig_off];
  // fill dummy bytes
  memset(info_sig, '?', MAX_SIGNATURE_LENGTH);
  info_sig[0] = 'L';
  sig.copy(&info_sig[1], sig.length());
  info_sig[1 + sig.length()] = ';';
  info_sig[2 + sig.length()] = '\0';
  memset(info->fields, -1, sizeof(field_info_t) * n_non_static_field);
  return ClassInfo{info};
}

ClassInfo ClassResolver::from_array_klass(const FakeArrayKlass *klass) {
  // array class has only one field, save element type, id and offset
  uint32_t needed_size = sizeof(class_info_t) + sizeof(field_info_t) + MAX_SIGNATURE_LENGTH;
  auto raw_info = (char *)a.allocate(needed_size);
  auto info = (class_info_t *)raw_info;
  info->id = UNREGISTERED_CLASS_ID;
  // obj_size of array type is same with with header_size
  info->obj_size = klass->lh.array_header_size();
  info->header_size = klass->lh.array_header_size();
  info->n_non_static_field = 1;
  info->n_static_field = 0;
  info->dim = klass->dimension;
  info->klass = (void *)klass;
  info->sig_off = sizeof(class_info_t) + sizeof(field_info_t);
  info->enum_ref_arr_off = 0;
  info->klass_cptr = JVMArgs::compress_metaspace_ptr(klass);
  // set sig
  auto sig = klass->signature();
  auto info_sig = &raw_info[info->sig_off];
  assert(sig.length() < MAX_SIGNATURE_LENGTH);
  // fill dummy bytes
  memset(info_sig, '?', MAX_SIGNATURE_LENGTH);
  sig.copy(info_sig, sig.length());
  info_sig[sig.length()] = '\0';
  // NOTICE: field type will be set in the ClassWalker
  info->fields[0].type = T_ILLEGAL;  // to be set
  info->fields[0].id = -1;           // to be set
  info->fields[0].offset = 0;
  return ClassInfo{info};
}

void ClassResolver::register_classes(JNIEnv *j_env, jobject j_set_classes) {
  auto classes = java_object_hashset_cvt_stl_vector(j_env, j_set_classes).value();
  for (auto cls : classes) {
    register_class(j_env, (jclass)cls);
    j_env->DeleteLocalRef(cls);
  }
}

class_id_t ClassResolver::register_class(JNIEnv *j_env, const jclass j_class) {
  auto target_klass = FakeKlass::from_clazz(j_class);
  TRACE("register {}", target_klass->signature());
  if (auto info = get_class_info(target_klass); !info.is_dummy()) {
    TRACE("registered");
    return info.id();  // registered
  }
  ClassWalker w(*this);
  if (target_klass->lh.is_instance()) {
    auto instance = (const FakeInstanceKlass *)target_klass;
    if (instance->is_enum()) {
      return w.walk_enum_klass(j_env, instance, j_class);
    } else {
      return w.walk_instance_klass(j_env, instance);
    }
  } else if (target_klass->lh.is_array()) {
    return w.walk_array_klass(j_env, (const FakeObjectArrayKlass *)target_klass);
  } else {
    unreachable();
  }
}

extern "C" doca_dpa_func_t register_class_infos;

void ClassResolver::export_class_infos(doca::Device &dev, doca::DPABuffer &dev_class_infos) {
  INFO("export class infos to device");
  dev_class_infos.device_memset(0);

  doca::MappedRegion mapped_infos(dev, infos.data(), infos.size(), DOCA_ACCESS_FLAG_PCI_READ_WRITE);

  doca::OwnedBuffer class_infos_by_id_offsets(dev, sizeof(uint64_t) * info_by_id.size(),
                                              DOCA_ACCESS_FLAG_PCI_READ_WRITE);
  {
    uint64_t *offsets = reinterpret_cast<uint64_t *>(class_infos_by_id_offsets.data());
    for (auto i = 0uz; i < info_by_id.size(); ++i) {
      offsets[i] = reinterpret_cast<uint8_t *>(info_by_id[i].i) - infos.data();
      TRACE("{:X}", offsets[i]);
    }
  }

  doca::OwnedBuffer class_infos_by_klass_offsets(dev, sizeof(uint64_t) * info_by_klass.size(),
                                                 DOCA_ACCESS_FLAG_PCI_READ_WRITE);
  {
    uint64_t *offsets = reinterpret_cast<uint64_t *>(class_infos_by_klass_offsets.data());
    for (auto i = 0uz; i < info_by_klass.size(); ++i) {
      offsets[i] = reinterpret_cast<uint8_t *>(info_by_klass[i].i) - infos.data();
      TRACE("{:X}", offsets[i]);
    }
  }

  doca::launch_rpc(dev, register_class_infos, dev_class_infos.handle(), mapped_infos.get_mmap_handle(),
                   mapped_infos.handle(), a.allocated(), info_by_id.size(),
                   class_infos_by_id_offsets.get_mmap_handle(dev), class_infos_by_id_offsets.handle(),
                   class_infos_by_klass_offsets.get_mmap_handle(dev), class_infos_by_klass_offsets.handle());
}

}  // namespace dpx::sd
