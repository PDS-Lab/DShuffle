#include "sd/native/object_walker.hxx"

#include <simdutf.h>

#include <cassert>

#include "sd/common/basic_type.h"
#include "sd/native/class_resolver.hxx"

namespace dpx::sd {

void ObjectWalker::do_walk_object(const FakeObject *obj, ClassInfo info) {
  TRACE("walk {} id: {} n non static field: {}", info.signature(), info.id(), info.n_non_static_field());
  TRACE("current offset: {}", b.offset());
  assert(b.offset() % 8 == 0);
  b.put(info.id());
  b.put(OBJECT_FLAG);
  b.fill_next_align_8();
  auto obj_base = b.offset();
  b.put(obj->raw(info.object_header_size()), info.object_body_size());
  for (uint32_t i = 0; i < info.n_non_static_field(); i++) {
    auto &f = info.get_field(i);
    TRACE("field: {} id: {}", i, f.id);
    if (is_primitive_type(f.type)) {
      continue;
    }
    assert(is_reference_type(f.type));
    // here we can access the buffer instead
    auto f_offset = obj_base + f.offset;
    auto member_cptr = b.get_at<uint32_t>(f_offset);
    auto member = (const FakeObject *)JVMArgs::parse_heap_cptr(member_cptr);
    auto member_offset = walk(member, f.id);
    b.put_at(member_offset, f_offset);
  }
}

void ObjectWalker::do_walk_array(const FakeObject *obj, ClassInfo info) {
  TRACE("walk {} id: {}", info.signature(), info.id());
  TRACE("current offset: {}", b.offset());
  uint32_t length = obj->array_length(info.array_header_size());
  assert(b.offset() % 8 == 0);
  b.put(info.id());
  b.put(ARRAY_FLAG);
  b.put(length);
  b.fill_next_align_8();
  auto &elem = info.get_field(0);
  if (is_reference_type(elem.type)) {
    for (uint32_t i = 0; i < length; i++) {
      auto elem_obj = obj->array_elem_ref(info.array_header_size(), i);
      [[maybe_unused]] auto elem_off = walk(elem_obj, elem.id);
      TRACE("element offset: {}", elem_off);
    }
  } else if (is_primitive_type(elem.type)) {
    assert(info.dim() == 1);
    if (elem.type == T_CHAR && o.enable_utf16_to_utf8) {
      cvt_utf16_to_utf8(obj, info, length);
    } else {
      b.put(obj->raw(info.array_header_size()), info.array_body_size(length));
    }
  } else {
    unreachable();
  }
}
// NOTICE:
//  instance layout:
//   | id | object flag | end off | padding | body | padding |
//  enum layout:
//   | id | enum flag | ordinal | padding |
//  null layout:
//   | id | null flag | padding |
//  redirect layout:
//   | id | redirect flag | redirect off | padding |
//  array layout:
//   | id | array flag | length | padding | elements | padding |
uint32_t ObjectWalker::walk(const FakeObject *obj, class_id_t expected_id) {
  b.fill_next_align_8();
  auto base = b.offset();
  TRACE("base: {} expected id: {}", base, expected_id);
  // check null
  if (obj == nullptr) [[unlikely]] {
    assert(expected_id != UNREGISTERED_CLASS_ID);
    b.put<int16_t>(expected_id);
    b.put<uint16_t>(NULL_FLAG);
    return base;
  }
  // check visited
  // if (auto [off, found] = ref2off.lookup_v(obj); found) {
  //   TRACE("visited at offset {}", off);
  //   b.put<int16_t>(expected_id);
  //   b.put<uint16_t>(REDIRECT_FLAG);
  //   b.put<uint32_t>(off);
  //   return base;
  // }
  // check resolved
  auto info = r.get_class_info(obj->klass_pointer());
  assert(!info.is_dummy());
  // mark visited
  // ref2off.insert_kv(obj, base);
  // do walk
  if (info.is_enum()) {
    b.put(info.id());
    b.put((flag_t)(ENUM_FLAG | OBJECT_FLAG));
    b.put(obj->enum_ordinal());
    assert(b.offset() % 8 == 0);
  } else if (info.is_object()) {
    do_walk_object(obj, info);
  } else if (info.is_array()) {
    do_walk_array(obj, info);
  } else {
    unreachable();
  }
  return base;
}

size_t ObjectWalker::walk(const FakeObject *obj, ClassResolver &resolver, const Options &o, naive::BorrowedBuffer ctx,
                          naive::BorrowedBuffer out) {
  ObjectWalker w(resolver, o, ctx, out);
  w.b.skip(OBJECT_DATA_OFFSET);
  TRACE("start offset: {}", w.b.offset());
  w.walk(obj, UNREGISTERED_CLASS_ID);
  TRACE("end offset: {}", w.b.offset());
  auto header = (meta_header_t *)w.b.raw_at(0);
  header->total_length = w.b.offset();
  TRACE("total length: {}", header->total_length);
  return header->total_length;
}

void ObjectWalker::cvt_utf16_to_utf8(const FakeObject *obj, ClassInfo info, uint32_t length) {
  // NOTICE:
  //  data layout:
  //   | origin byte size | actual byte size | utf-8 data | padding |
  auto origin_byte_size = info.array_body_size(length);
  b.put(origin_byte_size);
  auto u16_raw = (const char16_t *)obj->raw(info.array_header_size());
  auto u8_raw = b.raw_at(b.offset() + sizeof(uint32_t));
  uint32_t actual_byte_size = simdutf::convert_utf16le_to_utf8(u16_raw, length, reinterpret_cast<char *>(u8_raw));
  TRACE("origin size: {}, actual size: {}, length: {}", origin_byte_size, actual_byte_size, length);
  b.put(actual_byte_size);
  b.skip(actual_byte_size);
}

}  // namespace dpx::sd
