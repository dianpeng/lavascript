#ifndef HEAP_ALLOCATOR_H_
#define HEAP_ALLOCATOR_H_
#include <cstdint>
#include <cstdlib>

namespace lavascript {

/**
 * Heap allocator
 *
 * Used to allocate chunk memory by Heap
 */

class HeapAllocator {
 public:
  virtual void* Malloc( std::size_t ) = 0;
  virtual void  Free  ( void* ) = 0;
  virtual ~HeapAllocator() {}
};

inline void* Malloc( HeapAllocator* allocator , std::size_t size ) {
  return allocator ? allocator->Malloc(size) : ::malloc(size);
}

inline void Free( HeapAllocator* allocator , void* ptr ) {
  if(allocator)
    allocator->Free(ptr);
  else
    ::free(ptr);
}

} // namespace lavascript

#endif // HEAP_ALLOCATOR_H_
