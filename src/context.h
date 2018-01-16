#ifndef CONTEXT_H_
#define CONTEXT_H_
#include "trace.h"
#include "gc.h"
#include "jit-profile-data.h"

namespace lavascript {

/**
 * Context is a single thread or isolated execution container for
 * everything needed for executing a script.
 */
class Context {
 public:
  inline Context();

 public:

  // ------------------------------------------------------------
  // GC field
  GC* gc() {
    return &gc_;
  }

  const GC* gc() const {
    return &gc_;
  }

  // ------------------------------------------------------------
  // Runtime pointer
  interpreter::Runtime* runtime() {
    return runtime_;
  }

  const interpreter::Runtime* runtime() const {
    return runtime_;
  }

  void PushCurrentRuntime( interpreter::Runtime* runtime ) {
    runtime_ = runtime;
  }

  void PopCurrentRuntime();

  // ------------------------------------------------------------
  // JIT hot conut data field
  compiler::JITHotCountData* hotcount_data() {
    return &hotcount_data_;
  }

  const compiler::JITHotCountData* hotcount_data() const {
    return &hotcount_data_;
  }

 private:
  // GC interfaces
  GC gc_;

  // current interpreter runtime. This field will be set if we are in
  // an interpreted frame or an interpreter is executed. Otherwise,it
  // is NULL
  interpreter::Runtime* runtime_;

  // hot count for recording JIT compiler's profile data
  compiler::JITHotCountData hotcount_data_;
};

inline Context::Context():
  gc_                (this),
  runtime_           (NULL),
  hotcount_data_     ()
{}

} // namespace lavascript

#endif // CONTEXT_H_
