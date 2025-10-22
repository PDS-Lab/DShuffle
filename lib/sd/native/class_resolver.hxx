#pragma once

#include "doca/dpa_buffer.hxx"
#include "memory/naive_buffer.hxx"
#include "memory/simple_allocator.hxx"
#include "sd/native/class_info.hxx"
#include "sd/native/fake.hxx"
#include "sd/native/options.hxx"
#include "util/string_hash.hxx"

// #include <codecvt>
#include <vector>

namespace dpx::sd {

class ClassResolver : Noncopyable, Nonmovable {
  friend class ClassWalker;

 public:
  ClassResolver(Options &o_) : o(o_), infos(o.max_class_info_size, 64), a(infos) {}
  ~ClassResolver() = default;

  // setter
  void register_classes(JNIEnv *j_env, jobject j_set_classes);
  class_id_t register_class(JNIEnv *j_env, const jclass j_class);

  // getters
  const ClassInfo get_class_info(class_id_t id) const {
    auto idx = id2index(id);
    return idx < info_by_id.size() ? info_by_id[idx] : ClassInfo::dummy();
  }
  const ClassInfo get_class_info(std::string_view sig) const {
    auto iter = sig2id.find(sig);
    return iter != sig2id.end() ? info_by_id[id2index(iter->second)] : ClassInfo::dummy();
  }
  const ClassInfo get_class_info(const FakeKlass *klass) const {
    auto iter = std::ranges::lower_bound(info_by_klass, klass, std::less<const FakeKlass *>(),
                                         [](const ClassInfo &i) -> const FakeKlass * { return i.klass(); });
    return iter != info_by_klass.end() ? ((*iter).klass() == klass ? *iter : ClassInfo::dummy()) : ClassInfo::dummy();
  }
  const ClassInfo get_class_info(uint32_t klass_cptr) const {
    return get_class_info((const FakeKlass *)JVMArgs::parse_metaspace_cptr(klass_cptr));
  }

  void export_class_infos(doca::Device &dev, doca::DPABuffer &dev_class_infos);

  void show_class_infos() const;

 private:  // for walker
  ClassInfo from_instance_klass(const FakeInstanceKlass *klass, bool is_enum);
  ClassInfo from_array_klass(const FakeArrayKlass *klass);

  // NOTICE class_id > BasicType
  static size_t id2index(class_id_t id) {
    assert(id >= MIN_CLASS_ID);
    return id - MIN_CLASS_ID;
  }
  static class_id_t index2id(uint32_t i) {
    assert(i + MIN_CLASS_ID <= MAX_CLASS_ID);
    return i + MIN_CLASS_ID;
  }

 private:
  Options &o;
  naive::OwnedBuffer infos;
  SimpleAllocator a;

  std::vector<ClassInfo> info_by_id;
  std::vector<ClassInfo> info_by_klass;

  // TODO replace with ART
  std::unordered_map<std::string, class_id_t, string_hash, std::equal_to<>> sig2id;
  // TODO use unordered_dense
  std::unordered_map<std::string, std::vector<field_info_t *>, string_hash, std::equal_to<>> unresolved;
};

}  // namespace dpx::sd
