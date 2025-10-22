#pragma once

#include <atomic>

#include "trans/common/defs.hxx"
#include "util/logger.hxx"
#include "util/noncopyable.hxx"
#include "util/nonmovable.hxx"

namespace dpx {

class EndpointBase : Noncopyable, Nonmovable {
 public:
  explicit EndpointBase(Status s_ = Status::Idle) : s(s_) {}
  ~EndpointBase() = default;

  bool idle() const { return s == Status::Idle; }
  bool ready() const { return s == Status::Ready; }
  bool running() const { return s == Status::Running; }
  bool stopping() const { return s == Status::Stopping; }
  bool exited() const { return s == Status::Exited; }

 protected:
  void prepare() {
    assert(idle());
    s = Status::Ready;
    INFO("Endpoint status change: Idle -> Ready");
  }

  void run() {
    assert(ready());
    s = Status::Running;
    INFO("Endpoint status change: Ready -> Running");
  }

  void stop() {
    assert(running());
    s = Status::Stopping;
    INFO("Endpoint status change: Running -> Stopped");
  }

  void shutdown() {
    assert(stopping());
    s = Status::Exited;
    INFO("Endpoint status change: Stopped -> Exited");
  }
  std::atomic<Status> s;
};

}  // namespace dpx
