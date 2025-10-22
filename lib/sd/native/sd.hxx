#pragma once

#include <jni.h>

#include "doca/buffer.hxx"
#include "doca/device.hxx"
#include "doca/dpa_thread.hxx"
#include "memory/naive_buffer.hxx"
#include "sd/common/args.h"
#include "sd/native/class_resolver.hxx"
#include "sd/native/options.hxx"

extern "C" doca_dpa_func_t serialize;
extern "C" doca_dpa_func_t register_jvm_heap;

namespace dpx::sd {

class DPAContext : Noncopyable, Nonmovable {
  struct ThreadContext : Noncopyable, Nonmovable {
    doca::OwnedBuffer h_out_buffer;
    doca::DPABuffer d_ctx_buffer;
    doca::DPABuffer d_out_buffer;
    sd_thread_args_t sd_thread_args;

    ThreadContext(doca::Device& dev, Options& o)
        : h_out_buffer(dev, o.max_task_out_buffer_size / o.max_device_threads, DOCA_ACCESS_FLAG_PCI_READ_WRITE),
          d_ctx_buffer(dev, o.max_task_ctx_buffer_size),
          d_out_buffer(dev, o.max_task_out_buffer_size / o.max_device_threads),
          sd_thread_args({
              .d_ctx = d_ctx_buffer.handle(),
              .d_ctx_len = d_ctx_buffer.size(),
              .d_output = d_out_buffer.handle(),
              .d_output_len = d_out_buffer.size(),
              .h_output = h_out_buffer.handle(),
              .h_output_h = h_out_buffer.get_mmap_handle(dev),
          }) {}

    ~ThreadContext() = default;
  };

 public:
  DPAContext(doca::Device& dev_, Options& o_, ClassResolver& r_)
      : dev(dev_),
        o(o_),
        r(r_),
        infos(dev, o.max_class_info_size),
        out(o.max_task_out_buffer_size),
        g(dev, o.max_device_threads, ::serialize) {
    for (uint32_t i = 0; i < o.max_device_threads; ++i) {
      t_ctxs.emplace_back(new ThreadContext(dev, o));
      g.add(t_ctxs.back()->sd_thread_args);
    }
  }

  ~DPAContext() {
    for (auto ctx : t_ctxs) {
      delete ctx;
    }
  }

  void export_class_infos() { r.export_class_infos(dev, infos); }

  jbyteArray serialize(JNIEnv* j_env, jobject j_obj) {
    bool done = false;
    boost::fibers::fiber poller([&]() {
      TRACE("begin poller");
      while (!done) {
        if (!g.progress()) {
          boost::this_fiber::yield();
        }
      }
      TRACE("end poller");
    });
    TRACE("begin serialize");
    auto result = do_serialize(j_env, j_obj);
    TRACE("end serialize");
    done = true;
    poller.join();
    return result;
  }

 private:
  jbyteArray do_serialize(JNIEnv* j_env, jobject j_obj);
  jbyteArray do_obj_serialize(JNIEnv* j_env, FakeObject* object);
  jbyteArray do_array_obj_serialize(JNIEnv* j_env, FakeObject* object, ClassInfo info);

  doca::Device& dev;
  Options& o;
  ClassResolver& r;
  doca::DPABuffer infos;
  naive::OwnedBuffer out;
  std::vector<ThreadContext*> t_ctxs;
  doca::DPAThreadGroup g;
};

class Context : Noncopyable, Nonmovable {
 public:
  Context(Options& o_, ClassResolver& r_)
      : o(o_), r(r_), ctx_buffer(o.max_task_ctx_buffer_size), out_buffer(o.max_task_out_buffer_size) {}

  ~Context() = default;

  jbyteArray serialize(JNIEnv* j_env, jobject j_obj);
  jobject deserialize(JNIEnv* j_env, jbyteArray j_input, jclass j_cls);

 private:
  Options& o;
  ClassResolver& r;
  naive::OwnedBuffer ctx_buffer;
  naive::OwnedBuffer out_buffer;
};

}  // namespace dpx::sd
