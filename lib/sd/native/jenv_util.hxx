#pragma once

#include <jni.h>

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "sd/common/basic_type.h"
#include "sd/native/fake.hxx"

namespace dpx::sd {

std::optional<std::unordered_map<std::string, std::string>> java_str2str_hashmap_cvt_stl_map(JNIEnv *j_env,
                                                                                             jobject j_hash_map);

std::optional<std::vector<jobject>> java_object_hashset_cvt_stl_vector(JNIEnv *j_env, jobject j_hash_set);

// primitive array
jarray new_array(JNIEnv *j_env, basic_type_t t, uint32_t length);

// object array
jarray new_array(JNIEnv *j_env, const FakeKlass *klass, uint32_t length);

}  // namespace dpx::sd
