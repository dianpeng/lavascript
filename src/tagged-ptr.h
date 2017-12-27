#ifndef TAGGED_PTR_H_
#define TAGGED_PTR_H_
#include <cstddef>
#include <cstdint>

#include "src/trace.h"
#include "src/bits.h"

namespace lavascript {

// ----------------------------------------------------------------------
// Tagged pointer.
//
// This is just a normal tagged pointer which gonan steal some
// bits from its ptr and be able to allow you to set some
// states there.
// ----------------------------------------------------------------------
template< typename T > class TaggedPtr {

  static constexpr std::size_t CacluateBits( std::size_t size ) {
    return (size == 4 ? 2 : 3);
  }

 public:
  // we don't support tagged pointer on type that has size less than 4
  static_assert(sizeof(T) >= 4);

  static const std::size_t kBit = CacluateBits(sizeof(T));

  static const std::uintptr_t kPtrMask   = bits::BitOn<std::uintptr_t,kBit,sizeof(void*)*8>::value;

  static const std::uintptr_t kStateMask = bits::BitOn<std::uintptr_t,0,kBit>::value;

 public:
  explicit TaggedPtr( T* p = NULL ) {
    CheckPtr(p);
    ptr_ = p;
  }

  TaggedPtr( T* p , std::uint32_t state ) {
    reset(p,state);
  }

  // get the usable address/pointer
  T* ptr() const {
    return reinterpret_cast<T*>(raw_ & kPtrMask);
  }

  // this will *only* reset the pointer part
  void set_ptr( T* p ) {
    CheckPtr(p);
    std::uintptr_t temp = reinterpret_cast<std::uintptr_t>(p);
    temp |= static_cast<std::uintptr_t>(state());
    raw_ = temp;
  }

  // this will reset everything
  void reset( T* p ) {
    CheckPtr(p); ptr_ = p;
  }

  void reset( T* p , std::uint32_t value ) {
    CheckPtr(p);
    CheckState(value);
    raw_ = (reinterpret_cast<std::uintptr_t>(p) | static_cast<std::uintptr_t>(value));
  }

  // set/get the state of this tagged pointer
  void set_state( std::uint32_t value ) {
    CheckState(value);
    raw_ &= ~kStateMask;
    raw_ |= value;
  }

  // get the state of this tagged pointer
  std::uint32_t state() const {
    return static_cast<std::uint32_t>(raw_ & kStateMask);
  }

  // check if the TaggedPtr is an NULL pointer
  operator bool () const { return !IsNull(); }

  bool IsNull() const { return ptr() == NULL; }

 private:
  void CheckPtr( T* p ) {
    lava_debug(NORMAL,lava_verify((reinterpret_cast<std::uintptr_t>(p) & kStateMask) == 0 ););
  }

  void CheckState( std::uint32_t value ) {
    lava_debug(NORMAL,lava_verify(value <= kStateMask););
  }

 private:

  union {
   std::uintptr_t raw_;
   T* ptr_;
  };
};

static_assert( sizeof(TaggedPtr<int>) == sizeof(void*) );

} // namespace

#endif // TAGGED_PTR_H_
