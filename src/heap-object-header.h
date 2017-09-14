#ifndef HEAP_OBJECT_HEADER_H_
#define HEAP_OBJECT_HEADER_H_

#include <cstdint>
#include "all-static.h"

#include "core/bits.h"

namespace lavascript {

class HeapObject;

enum GCState {
  GC_RESERVED = 0,
  GC_BLACK    = 1,
  GC_WHITE    = 2,
  GC_GRAY     = 3
};

const char* GetGCStateName( GCState );


#define LAVASCRIPT_HEAP_OBJECT_LIST(__)                               \
  __( TYPE_ITERATOR,  Iterator, "iterator")                           \
  __( TYPE_LIST    ,  List    , "list"    )                           \
  __( TYPE_SLICE   ,  Slice   , "slice"   )                           \
  __( TYPE_OBJECT  ,  Object  , "object"  )                           \
  __( TYPE_MAP     ,  Map     , "map"     )                           \
  __( TYPE_STRING  ,  String  , "string"  )                           \
  __( TYPE_PROTOTYPE, Prototype, "prototype")                         \
  __( TYPE_CLOSURE ,  Closure , "closure" )                           \
  __( TYPE_EXTENSION, Extension , "extension")

#define LAVASCRIPT_PRIMITIVE_TYPE_LIST(__)                            \
  __( TYPE_INTEGER , Integer , "integer" )                            \
  __( TYPE_REAL    , Real    , "real"    )                            \
  __( TYPE_BOOLEAN , Boolean , "boolean" )                            \
  __( TYPE_NULL    , Null    , "null"    )                            \


#define LAVASCRIPT_VALUE_TYPE_LIST(__)                                \
  LAVASCRIPT_HEAP_OBJECT_LIST(__)                                     \
  LAVASCRIPT_PRIMITIVE_TYPE_LIST(__)


enum ValueType {
#define __(A,B,C) A,
  LAVASCRIPT_VALUE_TYPE_LIST(__)
  SIZE_OF_VALUE_TYPES
#undef __ // __
};

const char* GetValueTypeName( ValueType );

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
 *       object as large as 2^32^8 , since all the pointer on Heap is aligned with 8
 *
 * High: Used for storing bit flags and other stuffs .
 *
 * [
 *   3 bytes reserved ;
 *   bit 8:short/long string ;
 *   bit 7:end of chunk;
 *   bit 6-3:heap object type ;
 *   bit 2-1:gc mark state
 * ]
 *
 * The last 2 bits are used for storing flag for GC cycle
 */

class HeapObjectHeader : DoNotAllocateOnNormalHeap {
 public:
  typedef std::uint64_t Type;

  static const size_t kHeapObjectHeaderSize = 8;

  static const std::uint32_t kGCStateMask = 3;         // 0b11
  static const std::uint32_t kLongStringMask = (1<<7); // 0b10000000
  static const std::uint32_t kEndOfChunkMask = (1<<6); // 0b01000000

  // Mask for getting the heap object type , should be 0b0011100
  static const std::uint32_t kHeapObjectTypeMask  = core::BitOn<std::uint32_t,2,5>::value;

  GCState gc_state() const { return static_cast<GCState>(low_ & kGCStateMask); }
  void set_gc_state( GCState state ) { low_ |= static_cast<std::uint32_t>(state); }

  bool IsGCBlack() const { return gc_state() == GC_BLACK; }
  bool IsGCWhite() const { return gc_state() == GC_WHITE; }
  bool IsGCGray() const  { return gc_state() == GC_GRAY ; }
  void set_gc_black() { set_gc_state( GC_BLACK ); }
  void set_gc_white() { set_gc_state( GC_WHITE ); }
  void set_gc_gray () { set_gc_state( GC_GRAY  ); }

  // Check whether this object is a short string or long string if this
  // object is a heap object there
  void set_short_string() { high_ &= ~kLongStringMask;  }
  void set_long_string()  { high_ |= kLongStringMask;   }

  // These two functions doesn't test whether the HeapObjectType is a TYPE_STRING but
  // assume it is a string
  bool IsSSO() const { return !IsLongString(); }
  bool IsLongString() const  { return (high_ & kLongStringMask); }

  // Check if it is end of the chunk. Used when iterating through object on heap
  bool IsEndOfChunk() const { return (high_ & kEndOfChunkMask); }
  void set_end_of_chunk() { high_ |= kEndOfChunkMask; }
  void set_not_end_of_chunk() { high_ &= ~kEndOfChunkMask; }

  // HeapObject's type are stored inside of this HeapObjectHeader
  ValueType type() const {
    return static_cast<ValueType>( (high_ & kHeapObjectTypeMask) >> 2 );
  }
  void set_type( ValueType type ) {
    high_ |= (static_cast<std::uint32_t>(type) << 2);
  }

  // The size field of this object here
  std::size_t size() const { return low_ ; }
  void set_size( std::uint32_t size ) { low_ = size; }

  // Return the heap object header in raw format basically a 64 bits number
  std::uint64_t raw () const { return (static_cast<uint64_t>(low_) |
                                      (static_cast<uint64_t>(high_)<<32)); }

  operator std::uint64_t const () { return raw(); }

  explicit HeapObjectHeader( std::uint64_t raw ) :
    high_( core::High64(raw) ),
    low_ ( core::Low64 (raw) )
  {}

  explicit HeapObjectHeader( void* raw ):
    high_( core::High64(*reinterpret_cast<std::uint64_t*>(raw)) ),
    low_ ( core::Low64 (*reinterpret_cast<std::uint64_t*>(raw)) )
  {}

  HeapObjectHeader(): high_(0), low_(0) {}

 private:
  std::uint32_t high_;
  std::uint32_t low_;

};

} // namespace lavascript
#endif // HEAP_OBJECT_HEADER_H_
