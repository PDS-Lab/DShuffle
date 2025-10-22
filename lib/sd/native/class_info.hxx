#pragma once

#include "sd/common/class_info.h"
#include "sd/native/fake.hxx"

namespace dpx::sd {

using ObjWithHandle = std::pair<FakeObject *, jobject>;

struct ClassInfo {
  class_info_t *i = nullptr;

  // we do not copy the signature to the device
  static ClassInfo dummy() { return ClassInfo(); }
  bool is_dummy() const { return i == nullptr; }
  uint32_t mirror_size() const {
    return sizeof(class_info_t) + i->n_non_static_field * sizeof(field_info_t) +
           (is_enum() ? i->n_static_field * sizeof(FakeObject *) : 0);
  }
  uint32_t size() const { return mirror_size() + MAX_SIGNATURE_LENGTH; }

  // char *raw(uint32_t offset) { return (char *)&i + offset; }
  // setters
  void set_id(class_id_t id) { i->id = id; }
  void set_header_size(uint8_t size) { i->header_size = size; }
  void set_obj_size(uint16_t size) { i->obj_size = size; }
  void set_enum(uint32_t ordinal, const FakeObject *enum_instance, jobject global_handle) {
    auto &obj_with_handle = ((obj_with_handle_t *)raw(i->enum_ref_arr_off))[ordinal];
    obj_with_handle.object = (uint64_t)enum_instance;
    obj_with_handle.handle = (uint64_t)global_handle;
  }

  field_info_t &field(uint32_t idx) { return i->fields[idx]; }

  // getters
  class_id_t id() const { return i->id; }
  const char *raw(uint32_t offset = 0) const { return ((const char *)i) + offset; }
  const FakeKlass *klass() const { return (const FakeKlass *)i->klass; }
  uint32_t klass_cptr() const { return i->klass_cptr; }
  uint32_t dim() const { return i->dim; }

  uint32_t n_non_static_field() const { return i->n_non_static_field; }
  uint32_t n_static_field() const { return i->n_static_field; }
  uint32_t n_field() const { return i->n_non_static_field + i->n_static_field; }
  // uint32_t n_ref_field() const { return i->n_ref_field; }
  // uint32_t n_pri_field() const { return i->n_pri_field; }

  uint32_t object_size() const { return i->obj_size; }
  uint32_t object_header_size() const { return i->header_size; }
  uint32_t object_body_size() const { return i->obj_size - i->header_size; }

  uint32_t array_size(uint32_t length) const { return i->header_size + array_body_size(length); }
  uint32_t array_header_size() const { return i->header_size; }
  uint32_t array_body_size(uint32_t length) const { return length * type_size(i->fields[0].type); }

  bool is_enum() const { return i->enum_ref_arr_off != 0; }
  bool is_array() const { return i->dim >= 1; }
  bool is_object() const { return i->dim == 0; }

  ObjWithHandle get_enum(uint32_t ordinal) const {
    auto &h = ((const obj_with_handle_t *)raw(i->enum_ref_arr_off))[ordinal];
    return {(FakeObject *)h.object, (jobject)h.handle};
  }

  const field_info_t &get_field(uint32_t idx) const { return i->fields[idx]; }

  std::string signature() const { return std::string(raw(i->sig_off)); }
};

}  // namespace dpx::sd

template <>
struct std::formatter<dpx::sd::ClassInfo> : std::formatter<std::string> {
  template <typename Context>
  Context::iterator format(dpx::sd::ClassInfo info, Context out) const {
    std::string o;
    if (info.is_object()) {
      if (info.is_enum()) {
        std::string eo;
        for (auto i = 0uz; i < info.n_static_field(); ++i) {
          if (i != 0) {
            eo += ", ";
          }
          auto oh = info.get_enum(0);
          eo += std::format("<ordinal: {}, handle: 0x{:X}>", i, (uintptr_t)oh.second);
        }
        o = std::format("<id: {}, signature: {}, enum: [{}]>", info.id(), info.signature(), eo);
      } else {
        std::string fo;
        for (auto i = 0uz; i < info.n_non_static_field(); ++i) {
          if (i != 0) {
            fo += ", ";
          }
          auto &f = info.field(i);
          fo += std::format("<id: {}, type: {}, offset: {}, flag: 0b{:016b}>", f.id, dpx::sd::type2str(f.type),
                            f.offset, f.flag);
        }
        o = std::format(
            "<id: {}, "
            "signature: {}, "
            "klass: 0x{:X}, "
            "size：{}, "
            "header size: {}, "
            "body size: {}, "
            "n non-static field: {}, "
            "n static field: {}, "
            "fields: [{}]>",
            info.id(), info.signature(), info.klass_cptr(), info.object_size(), info.object_header_size(),
            info.object_body_size(), info.n_non_static_field(), info.n_static_field(), fo);
      }

    } else if (info.is_array()) {
      o = std::format(
          "<id: {}, "
          "signature: {}, "
          "klass: 0x{:X}, "
          "header size: {}, "
          "dim: {}>",
          info.id(), info.signature(), info.klass_cptr(), info.array_header_size(), info.dim());
    } else {
      dpx::unreachable();
    }
    return std::formatter<std::string>::format(o, out);
  }
};
