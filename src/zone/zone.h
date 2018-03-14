#ifndef ZONE_ZONE_H_
#define ZONE_ZONE_H_
#include "src/common.h"
#include "src/util.h"
#include "src/trace.h"
#include "src/bump-allocator.h"

#include <type_traits>
#include <cstddef>

namespace lavascript {
namespace zone {

// Just a wrapper around BumpAllocator. In the future the Zone may have a better
// implementation like a slab than a simple BumpAllocator.
class Zone {
 public:
  // for future , we need to align the memory since some arch may have panelty
  // for accessing not aligned memory
  static const std::size_t kAlignment = 8;

  static const std::size_t kMaximum =
#ifndef LAVA_ZONE_MAXIMUM_SIZE
    1024*1024*4; // 4MB
#else
    LAVA_ZONE_MAXIMUM_SIZE;
#endif // LAVA_ZONE_MAXIMUM_SIZE

  static const std::size_t kMinimum =
#ifndef LAVA_ZONE_MINIMUM_SIZE
    1024;        // 1KB
#else
    LAVA_ZONE_MINIMUM_SIZE;
#endif // LAVA_ZONE_MINIMUM_SIZE

  inline Zone( std::size_t minimum = kMinimum, std::size_t maximum = kMaximum,
                                               HeapAllocator* allocator = NULL );

  void* Malloc( std::size_t size ) { return allocator_.Grab(Align(size,kAlignment)); }

  template< typename T >
  T* Malloc() { return static_cast<T*>(Malloc(sizeof(T))); }

  template< typename T , typename ... ARGS >
  T* New( ARGS ...args ) {
    void* mem = Malloc(sizeof(T));
    return ConstructFromBuffer<T>(mem,args...);
  }

 public:
  std::size_t size()             const { return allocator_.size(); }
  std::size_t maximum_size()     const { return allocator_.maximum_size(); }
  std::size_t segment_size()     const { return allocator_.segment_size(); }
  std::size_t current_capacity() const { return allocator_.current_capacity(); }
  std::size_t total_bytes()      const { return allocator_.total_bytes(); }

 public:
  // Reset the zone memory pool
  void Reset() { allocator_.Reset(); }

 private:
  BumpAllocator allocator_;   // internal bump allocator

  LAVA_DISALLOW_COPY_AND_ASSIGN(Zone);
};

inline Zone::Zone( std::size_t minimum , std::size_t maximum ,
                                         HeapAllocator* allocator ):
  allocator_(minimum,maximum,allocator)
{}

// All object that is gonna allocated from *zone* must be derived from the *ZoneObject*.
//
// And user needs to use placement new to construct an zone object since Zone allocator
// only gives you memory and it *wont* call object's constructor.
class ZoneObject {
 public:
  static void* operator new( std::size_t size , Zone* zone ) {
    return zone->Malloc(size);
  }
  static void* operator new( std::size_t ) { lava_die(); return NULL; }
  static void* operator new[] ( std::size_t ) { lava_die(); return NULL; }
  static void  operator delete( void* ) { lava_die(); }
  static void  operator delete[](void*) { lava_die(); }
};

// Helper to check whether the type meets zone object's requirements
#define LAVASCRIPT_ZONE_CHECK_TYPE(T)                         \
  static_assert( std::is_pod<T>::value                    ||  \
                 std::is_base_of<ZoneObject,T>::value     ||  \
                 std::is_trivially_destructible<T>::value )

} // namespace zone
} // namespace lavascript

#endif // ZONE_ZONE_H_
