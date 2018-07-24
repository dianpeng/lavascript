#ifndef HEAP_OBJECT_HEADER_H_
#define HEAP_OBJECT_HEADER_H_

#include <cstdint>
#include <type_traits>

#include "object-type.h"
#include "all-static.h"
#include "bits.h"

namespace lavascript {

class HeapObject;

enum GCState {
  GC_WHITE    = 0,
  GC_BLACK    = 1,
  GC_GRAY     = 2,
  GC_RESERVED = 3
};

const char* GetGCStateName( GCState );

/**
 *
 * HeapObjectHeader is an object represents many states for a certain object.
 * The state are all stored inside of this 64 bits objects. This object will
 * always be placement new on the header of an object and user can mutate it
 * and then store it back.
 *
 *
 * The layout of HeapObjectHeader is as follow :
 *
 * [ -- 4 -- , -- 4 -- ]
 *    Low    ,   High
 *
 *
 * Low : Used for storing the size of the HeapObject , this means we can store
 *       object as large as 2^32 (4GB) , which is way more than enough
 *
 * High: Used for storing bit flags and other stuffs .
 *
 * [
 *   2 bytes reserved [3rd - 4th bytes]
 *   ---------------------------
 *   2nd byte
 *   --------
 *   bit 8:short/long string ;
 *   bit 7-0:heap object type ;
 *   ---------------------------
 *   1st byte
 *   --------
 *   bit   3:end of chunk
 *   bit 2-1:gc mark state
 *   ---------------------------
 * ]
 *
 * The last 2 bits are used for storing flag for GC cycle
 */

class HeapObjectHeader : DoNotAllocateOnNormalHeap {
 public:
  typedef std::uint64_t Type;

  static const size_t kHeapObjectHeaderSize = 16;

  static const std::uint32_t kGCStateMask = 3;         // 0b11
  static const std::uint32_t kLongStringMask = (1<<7); // 0b10000000
  static const std::uint32_t kEndOfChunkMask = (1<<3); // 0b00000100

  // Mask for getting the heap object type , should be 0b01111111
  static const std::uint32_t kHeapObjectTypeMask  = bits::BitOn<std::uint32_t,0,7>::value;

  GCState gc_state() const { return static_cast<GCState>(high() & kGCStateMask); }
  void set_gc_state( GCState state ) {
    std::uint32_t v = high();
    v &= ~kGCStateMask;
    v |= static_cast<std::uint32_t>(state);
    set_high(v);
  }

  bool IsGCBlack() const { return gc_state() == GC_BLACK; }
  bool IsGCWhite() const { return gc_state() == GC_WHITE; }
  bool IsGCGray() const  { return gc_state() == GC_GRAY ; }
  void set_gc_black() { set_gc_state( GC_BLACK ); }
  void set_gc_white() { set_gc_state( GC_WHITE ); }
  void set_gc_gray () { set_gc_state( GC_GRAY  ); }

 public:

  // Check if it is end of the chunk. Used when iterating through object on heap
  bool IsEndOfChunk() const { return (high() & kEndOfChunkMask); }
  void set_end_of_chunk() { set_high( high() | kEndOfChunkMask); }
  void set_not_end_of_chunk() { set_high( high() & ~kEndOfChunkMask); }

 public:
  // Check whether this object is a short string or long string if this
  // object is a heap object there
  void set_sso() {
    std::uint8_t v = high<1>();
    v &= ~kLongStringMask;
    set_high<1>(v);
  }

  void set_long_string()  {
    std::uint8_t v = high<1>();
    v |= kLongStringMask;
    set_high<1>(v);
  }

  bool IsSSO() const { return type() == TYPE_STRING && !(high<1>() & kLongStringMask); }
  bool IsLongString() const  { return type() == TYPE_STRING && (high<1>() & kLongStringMask); }

  // HeapObject's type are stored inside of this HeapObjectHeader
  ValueType type() const {
    std::uint8_t h = high<1>();
    return static_cast<ValueType>((h & kHeapObjectTypeMask));
  }
  bool IsString() const { return type() == TYPE_STRING; }
  bool IsList() const { return type() == TYPE_LIST; }
  bool IsSlice() const { return type() == TYPE_SLICE; }
  bool IsObject() const { return type() == TYPE_OBJECT; }
  bool IsMap() const { return type() == TYPE_MAP; }
  bool IsIterator() const { return type() == TYPE_ITERATOR; }
  bool IsPrototype() const { return type() == TYPE_PROTOTYPE; }
  bool IsClosure() const { return type() == TYPE_CLOSURE; }
  bool IsExtension() const { return type() == TYPE_EXTENSION; }
  bool IsScript() const { return type() == TYPE_SCRIPT; }

  void set_type( ValueType type ) {
    std::uint8_t v = high<1>();
    v &= ~kHeapObjectTypeMask;
    v |= static_cast<std::uint32_t>(type);
    set_high<1>(v);
  }

  // Size of the object + the header size
  //
  // NOTES:
  // implementation uses size() which has std::size_t larger
  // range than std::uint32_t catch corner case that size is
  // std::uint32_t::max()
  std::size_t total_size() const { return size() + kHeapObjectHeaderSize; }

  // Size of the object in byte
  std::size_t size() const { return low() ; }

  // Set the object's size in byte
  void set_size( std::uint32_t size ) { set_low(size); }

  // Return the heap object header in raw format basically a 64 bits number
  Type raw () const { return raw_; }
 public:

  static void SetHeader( void* here , const HeapObjectHeader& hdr ) {
    *reinterpret_cast<Type*>(here) = hdr.raw();
  }

  explicit HeapObjectHeader( Type raw ) :
    raw_(raw),
    id_ ()
  {}

  explicit HeapObjectHeader( void* raw ):
    raw_ ( *reinterpret_cast<Type*>(raw) ),
    id_  ()
  {}

 private:
  std::uint32_t high() const { return high_; }
  std::uint32_t low () const { return low_ ; }
  void set_high( std::uint32_t h ) { high_ = h; }
  void set_low( std::uint32_t l ) { low_  = l; }

  template< std::size_t index >
  inline std::uint8_t high() const;

  template< std::size_t index >
  inline void set_high( std::uint8_t v );

 private:
  union {
    Type raw_;
    // Though we don't support big endian but suppose we want to extend
    // it to more platform so we leave this here to make our code easier
    // to be extended in the future
#ifdef LAVA_LITTLE_ENDIAN
    struct {
      std::uint32_t low_;
      std::uint32_t high_;
    };
#else
    struct {
      std::uint32_t high_;
      std::uint32_t low_ ;
    };
#endif // LAVA_LTTILE_ENDIAN
  };
  std::uint64_t id_;
};

static_assert( std::is_standard_layout<HeapObjectHeader>::value );
static_assert( sizeof(HeapObjectHeader::Type)*2 == sizeof(HeapObjectHeader) );

template< std::size_t index >
inline std::uint8_t HeapObjectHeader::high() const {
  static_assert( index >= 0 && index < 4 );
  std::uint32_t v = high_;
  switch(index) {
    case 0:
      return static_cast<std::uint8_t>(v);
    case 1:
      return static_cast<std::uint8_t>(v>>8);
    case 2:
      return static_cast<std::uint8_t>(v>>16);
    default:
      return static_cast<std::uint8_t>(v>>24);
  }
}

template< std::size_t index >
inline void HeapObjectHeader::set_high( std::uint8_t v ) {
  static_assert( index >= 0 && index < 4 );
  switch(index) {
    case 0:
      high_ = (high_ & 0xffffff00) | static_cast<std::uint32_t>(v);
      break;
    case 1:
      high_ = (high_ & 0xffff00ff) | (static_cast<std::uint32_t>(v)<<8);
      break;
    case 2:
      high_ = (high_ & 0xff00ffff) | (static_cast<std::uint32_t>(v)<<16);
      break;
    default:
      high_ = (high_ & 0x00ffffff) | (static_cast<std::uint32_t>(v)<<24);
      break;
  }
}

} // namespace lavascript
#endif // HEAP_OBJECT_HEADER_H_
