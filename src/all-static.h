#ifndef ALL_STATIC_H_
#define ALL_STATIC_H_

#include "core/trace.h"
#include <cstddef>

namespace lavascript {

class AllStatic {
 private:
  static void* operator new ( size_t );
  static void* operator new[] ( size_t );
  static void operator delete( void* );
  static void operator delete[]( void* ptr );
};

class DoNotAllocateOnNormalHeap : AllStatic {};

} // namespace lavascript

#endif // ALL_STATIC_H_
