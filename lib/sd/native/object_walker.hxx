#pragma once

#include "memory/naive_buffer.hxx"
#include "sd/native/class_info.hxx"
#include "sd/native/fake.hxx"
#include "sd/native/map.hxx"
#include "sd/native/options.hxx"
#include "sd/native/rw_buffer.hxx"

namespace dpx::sd {

class ClassResolver;

class ObjectWalker : Noncopyable, Nonmovable {
 public:
  static size_t walk(const FakeObject *obj, ClassResolver &resolver, const Options &o, naive::BorrowedBuffer ctx,
                     naive::BorrowedBuffer out);

 private:
  ObjectWalker(ClassResolver &r, const Options &o, naive::BorrowedBuffer ctx, naive::BorrowedBuffer out)
      : r(r),
        o(o),
        b(out)
  // , ref2off(ctx)
  {}
  ~ObjectWalker() = default;

  void do_walk_object(const FakeObject *obj, ClassInfo info);
  void do_walk_array(const FakeObject *obj, ClassInfo info);
  uint32_t walk(const FakeObject *obj, class_id_t id);
  void cvt_utf16_to_utf8(const FakeObject *obj, ClassInfo info, uint32_t length);

  ClassResolver &r;
  const Options &o;
  RWBuffer b;
  // TODO make the map size variant
  // Map<const FakeObject *, uint32_t, 4096, 23> ref2off;
};

}  // namespace dpx::sd
