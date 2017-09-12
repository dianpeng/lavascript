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
 * [ 3 bytes reserved |8:short/long string|7-3:heap object type |2-1:gc mark state]
 *
 * The last 2 bits are used for storing flag for GC cycle
 */

class HeapObjectHeader : DoNotAllocateOnNormalHeap {
 public:
  typedef std::uint64_t Type;
  static const size_t kHeapObjectHeaderSize = 8;

  static const std::uint32_t kGCStateMask = 3;              // 0b11
  static const std::uint32_t kShortLongStringMask = (1<<7); // 0b10000000

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
  inline void set_short_string();
  inline void set_long_string();
  inline bool IsShortString() const;
  inline bool IsLongString() const;

  // HeapObject's type are stored inside of this HeapObjectHeader
  inline ValueType type() const;
  inline void set_type( ValueType );

  // This represents the *size in bytes* so not the actual size stored
  // there since the number store there needs to * 8 or << 3
  size_t SizeInBytes() const { return low_ * 8; }

  std::uint64_t raw () const { return (static_cast<uint64_t>(low_) |
                                      (static_cast<uint64_t>(high_)<<32)); }

  operator std::uint64_t const () { return raw(); }

 private:
  HeapObjectHeader( std::uint64_t raw ) :
    high_( core::High64(raw) ),
    low_ ( core::Low64 (raw) )
  {}

  std::uint32_t high_;
  std::uint32_t low_;

  friend class HeapObject;
};

} // namespace lavascript
#endif // HEAP_OBJECT_HEADER_H_
