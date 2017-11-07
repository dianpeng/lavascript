#ifndef CONTEXT_H_
#define CONTEXT_H_
#include "gc.h"

namespace lavascript {

class Context {
 public:
  Context() : gc_(this) {}
  GC* gc() { return &gc_; }
 private:
  GC gc_;
};

} // namespace lavascript

#endif // CONTEXT_H_
