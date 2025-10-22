#pragma once

#include <jni.h>

#include "memory/naive_buffer.hxx"
#include "sd/native/class_info.hxx"
#include "sd/native/fake.hxx"
#include "sd/native/map.hxx"
#include "sd/native/options.hxx"
#include "sd/native/rw_buffer.hxx"

namespace dpx::sd {

class ClassResolver;

class ObjectReviver : Noncopyable, Nonmovable {
 public:
  static jobject revive(JNIEnv *j_env, const FakeKlass *klass, ClassResolver &resolver, const Options &o,
                        naive::BorrowedBuffer ctx, naive::BorrowedBuffer in);

 private:
  ObjectReviver(JNIEnv *j_env, ClassResolver &r, const Options &o, naive::BorrowedBuffer ctx, naive::BorrowedBuffer in)
      : j_env(j_env), r(r), o(o), b(in), root(nullptr)
      // , off2ref(ctx)
      {}
  ~ObjectReviver() = default;

  ObjWithHandle do_parse_object(uint32_t base, ClassInfo info);
  ObjWithHandle do_parse_array(uint32_t base, ClassInfo info);
  ObjWithHandle parse(class_id_t expected_id);
  void cvt_utf8_to_utf16(const FakeObject *obj, ClassInfo info, uint32_t length);

  JNIEnv *j_env;
  ClassResolver &r;
  const Options &o;
  RWBuffer b;
  jobject root;                              // we only need to return the root object
  // Map<uint32_t, jobject, 4096, 23> off2ref;  // 72 KB+
};

}  // namespace dpx::sd
