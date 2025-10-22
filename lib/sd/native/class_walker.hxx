#pragma once

#include <jni.h>

#include "sd/common/basic_type.h"
#include "sd/native/class_info.hxx"
#include "util/noncopyable.hxx"
#include "util/nonmovable.hxx"

namespace dpx::sd {

class ClassResolver;
struct FakeKlass;
struct FakeArrayKlass;
struct FakeInstanceKlass;

// TODO: take jenv into more consideration

class ClassWalker : Noncopyable, Nonmovable {
  friend class ClassResolver;

 private:
  class_id_t walk_array_klass(JNIEnv *j_env, const FakeArrayKlass *klass);
  class_id_t walk_instance_klass(JNIEnv *j_env, const FakeInstanceKlass *klass);
  class_id_t walk_enum_klass(JNIEnv *j_env, const FakeInstanceKlass *klass, const jclass j_class);

  void register_class_info(ClassInfo info);

  // only construct by resolver
  ClassWalker(ClassResolver &resolver) : r(resolver) {}
  ~ClassWalker() = default;

  ClassResolver &r;
};

}  // namespace dpx::sd
