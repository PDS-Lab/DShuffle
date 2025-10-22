#pragma once

#include <jni.h>

#include <cstdint>
#include <string>

#include "util/fatal.hxx"
#include "util/unreachable.hxx"

namespace dpx {

inline std::string get_j_string(JNIEnv* j_env, jstring j_string) {
  jboolean is_copy = false;
  size_t len = j_env->GetStringLength(j_string);
  const char* str = j_env->GetStringUTFChars(j_string, &is_copy);
  if (str == nullptr) {
    die("Fail to get j_string");
  }
  std::string copy_s(str, len);
  j_env->ReleaseStringUTFChars(j_string, str);
  return copy_s;
}

// clang-format off
template <typename U, typename... Ts>
inline constexpr bool in_v = (std::is_same_v<U, Ts> || ...);

template <typename T>
using JArray =
    std::conditional_t<in_v<T,  uint8_t,  int8_t>, jbyteArray,
    std::conditional_t<in_v<T, uint16_t, int16_t>, jshortArray,
    std::conditional_t<in_v<T, uint32_t, int32_t>, jintArray,
    std::conditional_t<in_v<T, uint64_t, int64_t>, jlongArray, void>>>>;
// clang-format on

template <typename T>
inline std::span<T> get_raw_j_array(JNIEnv* j_env, JArray<T> j_array) {
  size_t length = j_env->GetArrayLength(j_array);
  void* raw_array = nullptr;
  jboolean is_copy = false;
  if constexpr (in_v<T, uint8_t, int8_t>) {
    raw_array = j_env->GetByteArrayElements(j_array, &is_copy);
  } else if constexpr (in_v<T, uint16_t, int16_t>) {
    raw_array = j_env->GetShortArrayElements(j_array, &is_copy);
  } else if constexpr (in_v<T, uint32_t, int32_t>) {
    raw_array = j_env->GetIntArrayElements(j_array, &is_copy);
  } else if constexpr (in_v<T, uint64_t, int64_t>) {
    raw_array = j_env->GetLongArrayElements(j_array, &is_copy);
  } else {
    static_unreachable;
  }
  return {reinterpret_cast<T*>(raw_array), length};
}

template <typename T>
inline void release_raw_j_array(JNIEnv* j_env, JArray<T> j_array, std::span<T> raw_array) {
  if constexpr (in_v<T, uint8_t, int8_t>) {
    j_env->ReleaseByteArrayElements(j_array, (jbyte*)raw_array.data(), 0);
  } else if constexpr (in_v<T, uint16_t, int16_t>) {
    j_env->ReleaseShortArrayElements(j_array, (jshort*)raw_array.data(), 0);
  } else if constexpr (in_v<T, uint32_t, int32_t>) {
    j_env->ReleaseIntArrayElements(j_array, (jint*)raw_array.data(), 0);
  } else if constexpr (in_v<T, uint64_t, int64_t>) {
    j_env->ReleaseLongArrayElements(j_array, (jlong*)raw_array.data(), 0);
  } else {
    static_unreachable;
  }
}

}  // namespace dpx
