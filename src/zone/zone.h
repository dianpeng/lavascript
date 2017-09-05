#ifndef ZONE_ZONE_H_
#define ZONE_ZONE_H_
#include <src/core/util.h>
#include <src/core/trace.h>

#include <cstddef>

namespace lavascript {
namespace zone {

// A simple bump allocator. Used to construct all the AST Node
class Zone {
 public:
  Zone();
  Zone( size_t minimum , size_t maximum );

  ~Zone();

  static const size_t kMaximum =
#ifndef LAVA_ZONE_MAXIMUM_SIZE
    1024*1024*4; // 4MB
#else
    LAVA_ZONE_MAXIMUM_SIZE;
#endif // LAVA_ZONE_MAXIMUM_SIZE

  static const size_t kMinimum =
#ifndef LAVA_ZONE_MINIMUM_SIZE
    1024;        // 1KB
#else
    LAVA_ZONE_MINIMUM_SIZE;
#endif // LAVA_ZONE_MINIMUM_SIZE

  inline void* Malloc( size_t );
  template< typename T > T* Malloc() { return static_cast<T*>(Malloc(sizeof(T))); }

 private:
  void Grow( size_t require );

 private:
  struct Chunk {
    Chunk* next;     // Pointer points to the next chunk
  };

  Chunk* first_chunk_;   // First avaiable chunk
  size_t capacity_;      // Current capacity
  void* pool_;           // Current avaiable pointer in our pool
  size_t left_;          // How many memory left in the current pool
  size_t minimum_;       // Minimum pool size
  size_t maximum_;       // Maximum pool size

  LAVA_DISALLOW_COPY_AND_ASSIGN(Zone);
};

inline void* Zone::Malloc( size_t size ) {
  if( left_ < size ) Grow(size);
  lava_assert ( size <= left_ , "" );
  void* ret = pool_; pool_ = ((char*)pool_ + size);
  left_ -= size;
  return ret;
}

// All object that is gonna allocated from *zone* must be derived from the *ZoneObject*.
//
// And user needs to use placement new to construct an zone object since Zone allocator
// only gives you memory and it *wont* call object's constructor.
class ZoneObject {
 public:
  static void* operator new( size_t size , Zone* zone ) {
    return zone->Malloc(size);
  }
  static void* operator new( size_t );
  static void* operator new[] ( size_t );
  static void  operator delete( void* );
  static void  operator delete[](void*);
};

} // namespace zone
} // namespace lavascript

#endif // ZONE_ZONE_H_
