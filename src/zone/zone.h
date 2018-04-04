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

// A general arena based allocator interface. Used with Zone based container , in the
// future we may extend the STL to support Zone based allocator by wrapping Zone into
// STL allocator interface.
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

  BumpZone( std::size_t minimum = kMinimum, std::size_t maximum = kMaximum, HeapAllocator* allocator = NULL ):
    allocator_(minimum,maximum,allocator)
  {}

  virtual ~Zone() {}
 public:
  // Malloc a chunk of memory , no free needed once This zone is destroyed or reset, the memory
  // is freed automatically
  virtual void* Malloc( std::size_t size ) { return allocator_.Grab(Align(size,kAlignment)); }
  // Reset the zone object, this will mark all malloced memory from this zone object to be invalid
  virtual void Reset() { allocator_.Reset(); }
 public: // helper
  template< typename T > T* Malloc() { return static_cast<T*>(Malloc(sizeof(T))); }
  template< typename T , typename ... ARGS >
  T* New( ARGS ...args ) {
    void* mem = Malloc(sizeof(T));
    return ConstructFromBuffer<T>(mem,args...);
  }
 public: // statistics
  // How many memory has been allocated
  virtual std::size_t size()             const { return allocator_.size(); }
  // How many memory has been requested from underlying system API for manage memory
  virtual std::size_t total_bytes()      const { return allocator_.total_bytes(); }
 private:
  BumpAllocator allocator_;   // internal bump allocator
  LAVA_DISALLOW_COPY_AND_ASSIGN(Zone);
};

// ===============================================================================
// StackZone
//
// Zone allocator that uses stack based allocation at first and then if runs out of the
// stack memory fallback to whatever zone object you gives
template< std::size_t Size >
class StackZone : public Zone {
 public:
  StackZone( Zone* fallback ): fallback_(fallback), buffer_() , size_(0) {}
  bool UseFallback() const { return size_ == Size; }
  Zone* fallback  () const { return fallback_; }
 protected:
  virtual inline void* Malloc( std::size_t );
  virtual std::size_t size() const { return size_ + fallback_->size(); }
 private:
  Zone* fallback_;
  char  buffer_[Size];
  std::size_t size_  ;
};

template< std::size_t Size >
inline void* StackZone::Malloc( std::size_t size ) {
  if((Size - size_) >= size) {
    void* ret = static_cast<void*>(buffer_ + size_);
    size_ += size;
    return ret;
  } else {
    size_ = Size;
    return fallback_->Malloc(size);
  }
}

// ==============================================================================
// Small zone is just yet another zone object configuration that is good enough
// for small temporary usage.
class SmallZone : public BumpZone {
 public:
  static const std::size_t kMinimum = 64;
  static const std::size_t kMaximum = 1024;
  SmallZone(): Zone( kMinimum , kMaximum ) {}
};

// =====================================================================================
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
