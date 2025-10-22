#include "sd/native/jenv_util.hxx"

namespace dpx::sd {

std::optional<std::unordered_map<std::string, std::string>> java_str2str_hashmap_cvt_stl_map(JNIEnv *j_env,
                                                                                             jobject j_hash_map) {
  jclass mapClass = j_env->FindClass("java/util/Map");
  if (mapClass == nullptr) {
    return std::nullopt;
  }
  jmethodID entrySet = j_env->GetMethodID(mapClass, "entrySet", "()Ljava/util/Set;");
  if (entrySet == nullptr) {
    return std::nullopt;
  }
  jobject set = j_env->CallObjectMethod(j_hash_map, entrySet);
  if (set == nullptr) {
    return std::nullopt;
  }
  jclass setClass = j_env->FindClass("java/util/Set");
  if (setClass == nullptr) {
    return std::nullopt;
  }
  jmethodID iterator = j_env->GetMethodID(setClass, "iterator", "()Ljava/util/Iterator;");
  if (iterator == nullptr) {
    return std::nullopt;
  }
  jobject iter = j_env->CallObjectMethod(set, iterator);
  if (iter == nullptr) {
    return std::nullopt;
  }
  jclass iteratorClass = j_env->FindClass("java/util/Iterator");
  if (iteratorClass == nullptr) {
    return std::nullopt;
  }
  jmethodID hasNext = j_env->GetMethodID(iteratorClass, "hasNext", "()Z");
  if (hasNext == nullptr) {
    return std::nullopt;
  }
  jmethodID next = j_env->GetMethodID(iteratorClass, "next", "()Ljava/lang/Object;");
  if (next == nullptr) {
    return std::nullopt;
  }
  jclass entryClass = j_env->FindClass("java/util/Map$Entry");
  if (entryClass == nullptr) {
    return std::nullopt;
  }
  jmethodID getKey = j_env->GetMethodID(entryClass, "getKey", "()Ljava/lang/Object;");
  if (getKey == nullptr) {
    return std::nullopt;
  }
  jmethodID getValue = j_env->GetMethodID(entryClass, "getValue", "()Ljava/lang/Object;");
  if (getValue == nullptr) {
    return std::nullopt;
  }

  std::unordered_map<std::string, std::string> m;
  jboolean isCopy = JNI_FALSE;
  while (j_env->CallBooleanMethod(iter, hasNext)) {
    jobject e = j_env->CallObjectMethod(iter, next);
    jstring k = (jstring)j_env->CallObjectMethod(e, getKey);
    jstring v = (jstring)j_env->CallObjectMethod(e, getValue);
    const char *rawKey = j_env->GetStringUTFChars(k, &isCopy);
    if (!rawKey) {
      return std::nullopt;
    }
    const char *rawValue = j_env->GetStringUTFChars(v, &isCopy);
    if (!rawValue) {
      j_env->ReleaseStringUTFChars(k, rawKey);
      return std::nullopt;
    }

    m.emplace(std::string_view(rawKey), std::string_view(rawValue));

    j_env->DeleteLocalRef(e);
    j_env->ReleaseStringUTFChars(k, rawKey);
    j_env->DeleteLocalRef(k);
    j_env->ReleaseStringUTFChars(v, rawValue);
    j_env->DeleteLocalRef(v);
  }
  j_env->DeleteLocalRef(iter);
  j_env->DeleteLocalRef(entryClass);
  j_env->DeleteLocalRef(iteratorClass);
  j_env->DeleteLocalRef(setClass);
  j_env->DeleteLocalRef(set);
  j_env->DeleteLocalRef(mapClass);
  return m;
}

std::optional<std::vector<jobject>> java_object_hashset_cvt_stl_vector(JNIEnv *j_env, jobject j_hash_set) {
  jclass hashSetClass = j_env->GetObjectClass(j_hash_set);
  if (hashSetClass == nullptr) {
    return std::nullopt;
  }
  jmethodID iteratorMethod = j_env->GetMethodID(hashSetClass, "iterator", "()Ljava/util/Iterator;");
  if (iteratorMethod == nullptr) {
    return std::nullopt;
  }
  jobject iterator = j_env->CallObjectMethod(j_hash_set, iteratorMethod);
  if (iterator == nullptr) {
    return std::nullopt;
  }
  jclass iteratorClass = j_env->GetObjectClass(iterator);
  if (iteratorClass == nullptr) {
    return std::nullopt;
  }
  jmethodID hasNextMethod = j_env->GetMethodID(iteratorClass, "hasNext", "()Z");
  if (hasNextMethod == nullptr) {
    return std::nullopt;
  }
  jmethodID nextMethod = j_env->GetMethodID(iteratorClass, "next", "()Ljava/lang/Object;");
  if (nextMethod == nullptr) {
    return std::nullopt;
  }
  std::vector<jobject> v;
  while (j_env->CallBooleanMethod(iterator, hasNextMethod)) {
    jobject element = j_env->CallObjectMethod(iterator, nextMethod);
    v.push_back(element);
    // WARN: donot release local ref!
    // env->DeleteLocalRef(element);
  }
  j_env->DeleteLocalRef(iterator);
  j_env->DeleteLocalRef(hashSetClass);
  j_env->DeleteLocalRef(iteratorClass);
  return v;
}


jarray new_array(JNIEnv *j_env, basic_type_t t, uint32_t length) {
  switch (t) {
    case T_BOOLEAN:
      return j_env->NewBooleanArray(length);
    case T_CHAR:
      return j_env->NewCharArray(length);
    case T_FLOAT:
      return j_env->NewFloatArray(length);
    case T_DOUBLE:
      return j_env->NewDoubleArray(length);
    case T_BYTE:
      return j_env->NewByteArray(length);
    case T_SHORT:
      return j_env->NewShortArray(length);
    case T_INT:
      return j_env->NewIntArray(length);
    case T_LONG:
      return j_env->NewLongArray(length);
    default:
      unreachable();
  }
}

jarray new_array(JNIEnv *j_env, const FakeKlass *elem_klass, uint32_t length) {
  return j_env->NewObjectArray(length, elem_klass->clazz(), nullptr);
}

}  // namespace dpx::sd
