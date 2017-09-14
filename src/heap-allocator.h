#ifndef HEAP_ALLOCATOR_H_
#define HEAP_ALLOCATOR_H_
#include <cstdint>

namespace lavascript {

/**
 * Heap allocator
 *
 * Used to allocate chunk memory by Heap
 */

class HeapAllocator {
 public:
  virtual void* Malloc( size_t size ) = 0;
  virtual void  Free  ( void* ) = 0;
  virtual ~HeapAllocator() {}
};

} // namespace lavascript

#endif // HEAP_ALLOCATOR_H_
