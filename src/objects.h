#ifndef OBJECTS_H_
#define OBJECTS_H_

#include <string>
#include <cstdint>
#include <cstring>
#include <type_traits>

#include "common.h"
#include "bits.h"
#include "hash.h"
#include "trace.h"
#include "all-static.h"
#include "heap-object-header.h"
#include "source-code-info.h"

#include "interpreter/upvalue.h"
#include "interpreter/bytecode-iterator.h"

namespace lavascript {

namespace gc {
class SSOPool;
} // namespace gc

class Context;
class GC;
class Value;
class SSO;
class HeapObject;
class Iterator;
class List;
class Slice;
class Object;
class Map;
class String;
class Prototype;
class Closure;
class Extension;
class Script;
class ScriptBuilder;
class CallFrame;

namespace interpreter {
void SetValueFlag( Value* v , std::uint32_t );
std::uint32_t GetValueFlag( const Value& );
} // namespace interpreter

// Used in Assembly to modify field in an object. Based on C++ standard, offsetof
// must be used in class that is standard_layout due to sick C++ object modle. We
// must be sure that we can maintain this shitty concept. Another thing is the offset
// of certain field will be stored inside of std::uin32_t not std::size_t due to the
// fact that std::size_t is 64 bits which most of instruction cannot use 64 bits immediate.
// The common case for using offset is via [reg+reg*(1/2/4/8/0)+offset] addressing mode,
// this freaking addressing mode accepts a 32 bits offset. So *DO NOT STORE* in 64 bits.
//
// In another word, any code that will be used by assembly , please use explicit storage.
// Do not use type like std::size_t or int , which based on standard has variable length
// platform to platform
struct IteratorLayout;
struct ListLayout;
struct SliceLayout;
struct ObjectLayout;
struct MapLayout;
struct StringLayout;
struct PrototypeLayout;
struct ClosureLayout;
struct ExtensionLayout;
struct ScriptLayout;

namespace interpreter {
class BytecodeBuilder;
} // namespace interpreter


/*
 * A handler is a holder that holds a typped GCRef object. This save
 * us from cosntantly typping whether a certain GCRef is certain type
 * and also gurantee the type safe since it checks the type during
 * Handle creation
 */
template< typename T > class Handle {
 public:
  inline Handle();
  inline Handle( HeapObject** );
  inline Handle( T** );
  inline Handle( const Handle& );
  inline Handle& operator = ( const Handle& );

  bool IsEmpty() const { return *ref_ == NULL; }
  bool IsRefEmpty() const { return ref_ == NULL; }
  bool IsNull() const { return IsRefEmpty() || IsEmpty(); }
  operator bool () const { return !IsNull(); }

 public:
  inline HeapObject** heap_object() const;
  inline T* ptr() const;
  inline T** ref() const;

  inline T* operator -> ();
  inline const T* operator -> () const;
  inline T& operator * () ;
  inline const T& operator * () const;

 private:
  T** ref_;
};


/**
 * The value type in lavascript are categorized into 2 parts :
 *  1. primitive type --> value semantic
 *  2. heap type      --> reference semantic
 *
 * Value is a *BOX* type for everything , including GCRef inside
 * of lavascript. It can hold things like:
 *    1) primitive type or value type holding, and it is value copy
 *    3) GCRef hold, implicitly hold HeapObject , and it is reference copy
 *
 * The implementation of Value uses NaN-tagging
 */

class Value final {
  union {
    std::uint64_t raw_;
    void*         vptr_;
    double        real_;
  };

  friend void interpreter::SetValueFlag( Value*, std::uint32_t );
  friend std::uint32_t interpreter::GetValueFlag( const Value& );
 public:
  // For primitive type , we can tell it directly from *raw* value due to the
  // double never use the lower 53 bits.
  // For pointer type , since it only uses 48 bits on x64 platform, we can
  // reconstruct the pointer value from the full 64 bits machine word , but we
  // don't store the heap object type value directly inside of *Value* object
  // but put along with the heap object's common header HeapObject.
  //
  //
  // NOTES: The following *ORDER* matters and also the *VALUE* matters.
  enum {
    TAG_REAL   = 0xfff8000000000000,                        // Real
    TAG_TRUE   = 0xfff8100000000000,                        // True
    TAG_HEAP   = 0xfff9000000000000,                        // Heap ( normal heap pointer )

    /*
     * TAG_SSO    = 0xfffa000000000000,                     // SSO
     * TAG_LIST   = 0xfffb000000000000,                     // List
     * TAG_OBJECT = 0xfffc000000000000,                     // Object
     */

    TAG_FALSE  = 0xfffd000000000000,                        // False
    TAG_NULL   = 0xfffd100000000000,                        // NULL

    TAG_FLAGS  = 0xfffe000000000000                         // Used for internal flags
  };

  // The flag that avoids the lower 32 bits . It is mainly used in
  // assembly to set and test flag.
  enum {
    FLAG_REAL   = 0xfff80000,
    FLAG_TRUE   = 0xfff81000,

    FLAG_HEAP   = 0xfff9   ,       // pointer is 48 bits , so we only test the upper 16 bits
    FLAG_HEAP_UNMASK = ~FLAG_HEAP, // used to extract pointer from assembly

    /*
     * FLAG_SSO    = 0xfffa    ,
     * FLAG_LIST   = 0xfffb    ,
     * FLAG_OBJECT = 0xfffc    ,
     */

    FLAG_FALSE  = 0xfffd0000,
    FLAG_NULL   = 0xfffd1000,

    // reserved flag used by interpreter
    FLAG_1      = 0xfffe0000,
    FLAG_2      = 0xfffe1000
  };

  // A flag used to help assembly interpreter to decide which value should be treated
  // as *TRUE* regardless
  enum {
    FLAG_FALSECOND = FLAG_FALSE
  };

  enum {
    TAG_HEAP_STORE_MASK_HIGHER = 0xfff90000,
    TAG_HEAP_STORE_MASK_LOWER  = 0x00000000,
    TAG_HEAP_LOAD_MASK_HIGHER  = 0x0000ffff,
    TAG_HEAP_LOAD_MASK_LOWER   = 0xffffffff
  };

 private:
  // Masks
  static const std::uint64_t kPtrMask = 0x0000ffffffffffff;
  static const std::uint64_t kTagMask = 0xffff000000000000;

  // A mask that is used to check whether a pointer is a valid x64 pointer.
  // So not break our assumption. This assumption can be held always,I guess
  static const std::uint64_t kPtrCheckMask = ~kPtrMask;

  std::uint64_t tag() const { return (raw_ & kTagMask); }
  std::uint32_t flag()const { return static_cast<std::uint32_t>(raw_ >> 32); }

  bool IsTagReal()    const { return tag()  <  TAG_REAL; }
  bool IsTagTrue()    const { return flag() == FLAG_TRUE; }
  bool IsTagFalse()   const { return flag() == FLAG_FALSE; }
  bool IsTagNull()    const { return flag() == FLAG_NULL; }
  bool IsTagHeap()    const { return tag()  == TAG_HEAP; }

  inline HeapObject** heap_object() const;
 public:
  /**
   * Returns a type enumeration for this Value.
   * This function is not really performant , user should always prefer using
   * IsXXX function to test whether a handle is certain type
   */
  ValueType type() const;
  const char* type_name() const;

 public:
  // This function will check whether the value can be converted to boolean.
  // All value in lavascript can be converted to boolean.
  //
  // Except NULL and FALSE , any other value inside of lavascript is evaluated
  // to be True inside of the boolean context.
  bool AsBoolean() const {
    return flag() < FLAG_FALSECOND;
  }

 public:
  // Primitive types
  bool IsNull() const       { return IsTagNull(); }
  bool IsReal() const       { return IsTagReal(); }
  bool IsTrue() const       { return IsTagTrue(); }
  bool IsFalse()const       { return IsTagFalse();}
  bool IsBoolean() const    { return IsTagTrue() || IsTagFalse(); }
  bool IsHeapObject() const { return IsTagHeap(); }

  // Heap types
  inline bool IsString() const;
  inline bool IsList() const;
  inline bool IsSlice() const;
  inline bool IsObject() const;
  inline bool IsMap() const;
  inline bool IsClosure() const;
  inline bool IsPrototype() const;
  inline bool IsExtension() const;
  inline bool IsIterator() const;

  // String types
  inline bool IsSSO() const;
  inline bool IsLongString() const;

  // Getters
  inline bool GetBoolean() const;
  inline double GetReal() const;

  inline HeapObject** GetHeapObject() const;
  inline Handle<String> GetString() const;
  inline Handle<List> GetList() const;
  inline Handle<Slice> GetSlice() const;
  inline Handle<Object> GetObject() const;
  inline Handle<Map> GetMap() const;
  inline Handle<Prototype> GetPrototype() const;
  inline Handle<Closure> GetClosure() const;
  inline Handle<Extension> GetExtension() const;
  inline Handle<Iterator> GetIterator() const;
  inline Handle<Script> GetScript() const;

  template< typename T >
  inline Handle<T> GetHandle() const { return Handle<T>(GetHeapObject()); }

  // Setters
  inline void SetReal   ( double );
  inline void SetBoolean( bool );
  void SetNull   () { raw_ = TAG_NULL; }
  void SetTrue   () { raw_ = TAG_TRUE; }
  void SetFalse  () { raw_ = TAG_FALSE;}

  inline void SetHeapObject( HeapObject** );
  inline void SetString( const Handle<String>& );
  inline void SetList  ( const Handle<List>& );
  inline void SetSlice ( const Handle<Slice>& );
  inline void SetObject( const Handle<Object>& );
  inline void SetMap   ( const Handle<Map>& );
  inline void SetPrototype( const Handle<Prototype>& );
  inline void SetClosure  ( const Handle<Closure>& );
  inline void SetExtension( const Handle<Extension>& );
  inline void SetIterator ( const Handle<Iterator>& );
  inline void SetScript   ( const Handle<Script&> );

  template< typename T >
  void SetHandle( const Handle<T>& h ) { SetHeapObject(h.heap_object()); }

 public: // Helpers

  // This equality checks *identity* for heap object and value equality
  // for normal primitive types. i.e , for heap object the pointer must
  // be the same ; for primitive type the value must be the same.
  inline bool Equal( const Value& that ) const;

 public:
  Value() { SetNull(); }
  explicit Value( double v ) { SetReal(v); }
  explicit Value( std::int32_t v ) { SetReal( static_cast<double>(v) ); }
  explicit Value( bool v ) { SetBoolean(v); }
  explicit Value( HeapObject** v ) { SetHeapObject(v); }
  Value( const Value& that ): raw_(that.raw_) {}
  Value& operator = ( const Value& );
  template< typename T >
  explicit Value( const Handle<T>& h ) { SetHandle(h); }

  template< typename T > explicit Value( T** );
};

// The handle must be as long as a machine word , here for simplicitly we assume
// the machine word to be the size of uintptr_t type.
static_assert( sizeof(Value) == sizeof(std::uintptr_t) );


// Heap object's shared base object
//
// All heap object are gonna sit on our own managed heap. Each object will have
// an object header which has 8 bytes length. The layout is as following:
//
//  -------------------------------
//  | header | this | hidden data |
//  -------------------------------
//
//
//  Each object is *immutable* the growable data structure is accomplished via throw
//  the old one away and create a new one.
//
//
//  The user can get its header word by directly call function object_header() to
//  retrieve the heap header. This is done by (this - 8).
//
//  The HeapObject is an empty object at all and all the states it needs are stored
//  inside of the HeapObjectHeader object which can be referend via (this-8)

class HeapObject {
 public:
  bool IsString() const { return hoh().type() == TYPE_STRING; }
  bool IsList  () const { return hoh().type() == TYPE_LIST; }
  bool IsSlice () const { return hoh().type() == TYPE_SLICE; }
  bool IsObject() const { return hoh().type() == TYPE_OBJECT; }
  bool IsMap   () const { return hoh().type() == TYPE_MAP; }
  bool IsPrototype() const { return hoh().type() == TYPE_PROTOTYPE; }
  bool IsClosure() const { return hoh().type() == TYPE_CLOSURE; }
  bool IsExtension() const { return hoh().type() == TYPE_EXTENSION; }
  bool IsIterator() const { return hoh().type() == TYPE_ITERATOR; }
  bool IsScript() const { return hoh().type() == TYPE_SCRIPT; }

  // Generic way to check whether this HeapObject is certain type
  template< typename T > bool IsType() ;
  ValueType type() const { return hoh().type(); }

 public:
  HeapObjectHeader::Type* hoh_address() const {
    HeapObject* self = const_cast<HeapObject*>(this);
    return reinterpret_cast<HeapObjectHeader::Type*>(
        reinterpret_cast<char*>(self) - HeapObjectHeader::kHeapObjectHeaderSize);
  }

  HeapObjectHeader::Type hoh_raw() const {
    return *hoh_address();
  }

  HeapObjectHeader hoh() const {
    return HeapObjectHeader( hoh_raw() );
  }

  void set_hoh( const HeapObjectHeader& word ) {
    *hoh_address() = word.raw();
  }

  // Helper function for setting the GC state for this HeapObject
  inline void set_gc_state( GCState state );
};


/**
 * SSO represents short string object. Lavascript distinguish two types
 * of string inside the VM , though user don't need to be aware of it.
 *
 * The SSO is an optimization of *short string* and we turn short string
 * totally not GC collectable. So GC will reserve certain amounts of memory
 * for allocating SSO sololy and never collect them. And also SSO are deduped,
 * when a new string is allocated , we will pick up an existed string if we
 * can do so
 */

class SSO final {
 public:
  std::uint32_t hash() const { return hash_; }
  std::size_t size() const { return size_; }
  inline const void* data() const;
  std::string ToStdString() const {
    return std::string(static_cast<const char*>(data()),size());
  }
 public:
  inline bool operator == ( const char* ) const;
  inline bool operator == ( const std::string& ) const;
  inline bool operator == ( const String& ) const;
  inline bool operator == ( const SSO& ) const;

  inline bool operator != ( const char* ) const;
  inline bool operator != ( const std::string& ) const;
  inline bool operator != ( const String& ) const;
  inline bool operator != ( const SSO& ) const;

  inline bool operator >  ( const char* ) const;
  inline bool operator >  ( const std::string& ) const;
  inline bool operator >  ( const String& ) const;
  inline bool operator >  ( const SSO& ) const;

  inline bool operator >= ( const char* ) const;
  inline bool operator >= ( const std::string& ) const;
  inline bool operator >= ( const String& ) const;
  inline bool operator >= ( const SSO& ) const;

  inline bool operator <  ( const char* ) const;
  inline bool operator <  ( const std::string& ) const;
  inline bool operator <  ( const String& ) const;
  inline bool operator <  ( const SSO& ) const;

  inline bool operator <= ( const char* ) const;
  inline bool operator <= ( const std::string& ) const;
  inline bool operator <= ( const String& ) const;
  inline bool operator <= ( const SSO& ) const;

  // constructor
  SSO( std::size_t size , std::uint32_t hash ) : size_(size) , hash_(hash) {}

 private:
  // size of the data
  std::size_t size_;

  // hash value of this SSO
  std::uint32_t hash_;

  // memory pool for SSOs
  friend class gc::SSOPool;
  friend class SSOLayout;

  LAVA_DISALLOW_COPY_AND_ASSIGN(SSO);
};

static_assert( std::is_standard_layout<SSO>::value );

struct SSOLayout {
  static const std::uint32_t kSizeOffset = offsetof(SSO,size_);
  static const std::uint32_t kHashOffset = offsetof(SSO,hash_);
};

/**
 * Long string representation inside of VM
 *
 * We never use this LongString representation directly since it is embedded
 * by the String object internally. LongString is not a collectable object
 * but String is and it is held by String in place.
 */

struct LongString final {
  // Size of the LongString
  std::size_t size;
  // Return the stored the data inside of the LongString
  inline const void* data() const;

  LongString( std::size_t sz ) : size(sz) {}
};

static_assert( std::is_standard_layout<LongString>::value );

struct LongStringLayout {
  static const std::uint32_t kSizeOffset = offsetof(LongString,size);
  static const std::uint32_t kDataOffset = sizeof(LongString);
};

/**
 * A String object is a normal long string object that resides
 * on top the real heap and it is due to GC. String is an immutable
 * object and once it is allocated we cannot change its memory.
 * The String object directly uses memory that is after the *this*
 * pointer. Its memory layout is like this:
 *
 * ----------------------------------------------
 * |sizeof(String)|  union { LongString ; SSO } |
 * ----------------------------------------------
 *
 * Use HeapObjectHeader to tell whether we store a SSO or just a LongString
 * internally
 *
 */

class String final : public HeapObject {
 public:
  inline const void* data() const;
  inline std::size_t size() const;

  const SSO& sso() const;
  const LongString& long_string() const;

  bool IsSSO() const { return hoh().IsSSO(); }
  bool IsLongString() const { return hoh().IsLongString(); }

  // Obviously , we are not null terminated string , but with ToStdString,
  // we are able to gap the String object to real world string with a little
  // bit expensive operations.
  std::string ToStdString() const {
    return std::string(static_cast<const char*>(data()),size());
  }

 public:
  inline bool operator == ( const char* ) const;
  inline bool operator == ( const std::string& ) const;
  inline bool operator == ( const String& ) const;
  inline bool operator == ( const SSO& ) const;

  inline bool operator != ( const char* ) const;
  inline bool operator != ( const std::string& ) const;
  inline bool operator != ( const String& ) const;
  inline bool operator != ( const SSO& ) const;

  inline bool operator >  ( const char* ) const;
  inline bool operator >  ( const std::string& ) const;
  inline bool operator >  ( const String& ) const;
  inline bool operator >  ( const SSO& ) const;

  inline bool operator >= ( const char* ) const;
  inline bool operator >= ( const std::string& ) const;
  inline bool operator >= ( const String& ) const;
  inline bool operator >= ( const SSO& ) const;

  inline bool operator <  ( const char* ) const;
  inline bool operator <  ( const std::string& ) const;
  inline bool operator <  ( const String& ) const;
  inline bool operator <  ( const SSO& ) const;

  inline bool operator <= ( const char* ) const;
  inline bool operator <= ( const std::string& ) const;
  inline bool operator <= ( const String& ) const;
  inline bool operator <= ( const SSO& ) const;

 public: // Factory functions
  static Handle<String> New( GC* );
  static Handle<String> New( GC* , const char* , std::size_t );
  static Handle<String> New( GC* gc , const char* str ) {
    return New(gc,str,strlen(str));
  }
  static Handle<String> New( GC* gc , const std::string& str ) {
    return New(gc,str.c_str(),str.size());
  }
  static Handle<String> NewFromReal( GC* , double );
  static Handle<String> NewFromBoolean( GC* , bool );
 private:

  friend class StringLayout;
  friend class GC;
  LAVA_DISALLOW_COPY_AND_ASSIGN(String);
};

static_assert( std::is_standard_layout<String>::value );

/**
 * Represents a *list* of objects .
 *
 * A list object is simply a reference to a underlying slice object plus a
 * size field. A list is immutable in terms of its length so it is not really
 * *growable* but it can grow by creating a new slice object underlying and
 * then switch its internal pointer to the newly created slice object.
 */

class List final : public HeapObject {
 public:
  // Where the list's underlying Slice object are located
  Handle<Slice> slice() const { return slice_; };

  bool IsEmpty() const { return size_ == 0; }
  std::size_t size() const { return size_; }
  inline std::size_t capacity() const;
  inline const Value& Index( std::size_t ) const;
  inline Value& Index( std::size_t );
  Value& operator [] ( std::size_t index ) { return Index(index); }
  const Value& operator [] ( std::size_t index ) const { return Index(index); }

  inline Value& Last();
  inline const Value& Last() const;
  inline Value& First();
  inline const Value& First() const;

 public: // Mutator for the list object itself
  void Clear() { size_ = 0; }
  bool Push( GC* , const Value& );
  void Pop ();

  Handle<Iterator> NewIterator( GC* , const Handle<List>& ) const;

 public: // Factory functions
  static Handle<List> New( GC* );
  static Handle<List> New( GC* , std::size_t capacity );
  static Handle<List> New( GC* , const Handle<Slice>& slice );

  template< typename T >
  bool Visit( T* );

  List( const Handle<Slice>& slice ):
    size_(0),
    slice_(slice)
  {}

 private:

  std::uint32_t size_;
  Handle<Slice> slice_;

  friend struct ListLayout;
  friend class GC;
  LAVA_DISALLOW_COPY_AND_ASSIGN(List);
};

static_assert( std::is_standard_layout<List>::value );

struct ListLayout {
  static const std::uint32_t kSizeOffset = offsetof(List,size_);
  static const std::uint32_t kSliceOffset= offsetof(List,slice_);
};

/**
 * Represents a slice of objects.
 *
 * This is the internal object a list manipulates and for end user it is not
 * of there interests to know it. In lavascript, we don't expose anything called
 * slice to users.
 *
 * It is a tuple of (capacity,array of object[])
 */
class Slice final : public HeapObject {
  inline void* array() const;
 public:
  bool IsEmpty() const { return capacity() == 0; }
  std::size_t capacity() const { return capacity_; }
  const Value* data() const;
  Value* data();
  inline Value& Index( std::size_t );
  inline const Value& Index( std::size_t ) const;
  Value& operator [] ( std::size_t index ) { return Index(index); }
  const Value& operator [] ( std::size_t index ) const { return Index(index); }

 public: // Factory functions
  static Handle<Slice> Extend( GC* , const Handle<Slice>& old );
  static Handle<Slice> New( GC* );
  static Handle<Slice> New( GC* , std::size_t cap );

  template< typename T >
  bool Visit( T* );

  Slice( std::size_t capacity ) : capacity_(capacity) {}

 private:

  std::uint32_t capacity_;

  friend struct SliceLayout;
  friend class GC;
  LAVA_DISALLOW_COPY_AND_ASSIGN(Slice);
};

static_assert( std::is_standard_layout<Slice>::value );

struct SliceLayout {
  static const std::uint32_t kCapacityOffset = offsetof(Slice,capacity_);
  static const std::uint32_t kArrayOffset    = sizeof(Slice);
};

/**
 * Represents an Object.
 *
 * A object is really just a shim object points to a *MAP* object and an MAP
 * object is immutable in terms of itself , since we don't use chain resolution
 * but use open addressing hash.
 */

class Object final : public HeapObject {
 public:
   Handle<Map> map() const { return map_; }
 public:
  inline std::size_t capacity() const;
  inline std::size_t size() const;
  inline bool IsEmpty() const;

 public:
  inline bool Get ( const Handle<String>& , Value* ) const;
  inline bool Get ( const char* , Value* ) const;
  inline bool Get ( const std::string& , Value* ) const;

  inline bool Set ( GC* , const Handle<String>& , const Value& );
  inline bool Set ( GC* , const char*   , const Value& );
  inline bool Set ( GC* , const std::string& , const Value& );

  inline bool Update ( GC* , const Handle<String>& , const Value& );
  inline bool Update ( GC* , const char*   , const Value& );
  inline bool Update ( GC* , const std::string& , const Value& );

  inline void Put ( GC* , const Handle<String>& , const Value& );
  inline void Put ( GC* , const char*   , const Value& );
  inline void Put ( GC* , const std::string& , const Value& );

  inline bool Delete ( const Handle<String>& );
  inline bool Delete ( const char*   );
  inline bool Delete ( const std::string& );

  void Clear  ( GC* );
 public:
  Handle<Iterator> NewIterator( GC* , const Handle<Object>& ) const;

 public: // Factory functions
  static Handle<Object> New( GC* );
  static Handle<Object> New( GC* , std::size_t capacity );
  static Handle<Object> New( GC* , const Handle<Map>& );

  template< typename T >
  bool Visit( T* );

  Object( const Handle<Map>& map ):map_(map) {}

 private:

  Handle<Map> map_;

  friend struct ObjectLayout;
  friend class GC;
  LAVA_DISALLOW_COPY_AND_ASSIGN(Object);
};

static_assert( std::is_standard_layout<Object>::value );

struct ObjectLayout {
  static const std::uint32_t kMapOffset = offsetof(Object,map_);
};


/**
 * Represents an Map object used by Object internally.
 *
 * It is an open addressing hash map. It is fixed length and will not grow.
 * However we don't use this Map object direclty but use Object. Object will
 * take care of the Grow internally.
 */

class Map final : public HeapObject {
  inline void* entry() const;
 public:

  /**
   * Entry for the open addressing hash object.
   *
   * As you can see we have largest size for allocating an array
   * internally used for Map object due to the fact that we use
   * 30 bits to rerepsent *next* pointer , so the map's entry cannot
   * be bigger than 2^30 which I think is enough for most of the cases
   */

  struct Entry {
    String** key;
    Value value;
    std::uint32_t hash;
    std::uint32_t next : 29;
    std::uint32_t more :  1; // 1<<29
    std::uint32_t del  :  1; // 1<<30
    std::uint32_t use  :  1; // 1<<31

    static const std::uint32_t kMoreBit = ((1<<29));
    static const std::uint32_t kDelBit  = ((1<<30));
    static const std::uint32_t kUseBit  = ((1<<31));
    // Used to test whether the entry is *used* but not *del*
    static const std::uint32_t kUseButNotDelBit = kUseBit;

    bool active() const { return use && !del; }
  };

  static const std::size_t kMaximumMapSize = 1<<29;

  // How many slots has been reserved for this Map object
  std::size_t capacity() const { return capacity_; }

  // How many live objects are inside of the Map
  std::size_t size() const { return size_; }

  // How many slots has been occupied , since deletion is marked with
  // tombstone
  std::size_t slot_size() const { return slot_size_; }

  // Do we need to do rehashing now
  bool NeedRehash() const { return slot_size() == capacity(); }

  // Is the map empty
  bool IsEmpty() const { return size() == 0; }

  inline Entry* data();
  inline const Entry* data() const;

 public: // Mutators
  inline bool Get ( const Handle<String>& , Value* ) const;
  inline bool Get ( const char*, Value* ) const;
  inline bool Get ( const std::string& , Value* ) const;

  inline bool Set ( GC* , const Handle<String>& , const Value& );
  inline bool Set ( GC* , const char*   , const Value& );
  inline bool Set ( GC* , const std::string& , const Value& );

  inline bool Update ( GC* , const Handle<String>& , const Value& );
  inline bool Update ( GC* , const char*   , const Value& );
  inline bool Update ( GC* , const std::string& , const Value& );

  inline void Put ( GC* , const Handle<String>& , const Value& );
  inline void Put ( GC* , const char*   , const Value& );
  inline void Put ( GC* , const std::string& , const Value& );

  inline bool Delete ( const Handle<String>& );
  inline bool Delete ( const char*   );
  inline bool Delete ( const std::string& );

  Handle<Iterator> NewIterator( GC* , const Handle<Map>& ) const;

 public: // Factory functions
  static Handle<Map> New( GC* );
  static Handle<Map> New( GC* , std::size_t capacity );
  static Handle<Map> Rehash( GC* , const Handle<Map>&);
  template< typename T > bool Visit( T* );

  Map( std::size_t capacity ):
    capacity_(capacity),
    mask_(capacity-1),
    size_(0),
    slot_size_(0)
  {
    lava_debug(NORMAL,lava_verify(capacity);lava_verify(!(capacity &(capacity-1))););
  }

 private:

  /**
   * Helper function for fetching the slot inside of Entry
   * array with certain option.
   */
  enum Option {
    FIND = 0 ,               // Find , do not do linear probing
    INSERT=1 ,               // If not found, do linear probing
    UPDATE=2                 // Find , return it otherwise return a probing position

  };

  inline static std::uint32_t Hash( const char* );
  inline static std::uint32_t Hash( const std::string& );
  inline static std::uint32_t Hash( const Handle<String>& );

  static bool Equal( const Handle<String>& lhs , const Handle<String>& rhs ) {
    return *lhs == *rhs;
  }

  static bool Equal( const Handle<String>& lhs , const char* rhs ) {
    return *lhs == rhs;
  }

  static bool Equal( const Handle<String>& lhs , const std::string& rhs ) {
    return *lhs == rhs;
  }

  template< typename T >
  Entry* FindEntry( const T& , std::uint32_t , Option ) const;

 private:
  std::uint32_t capacity_;
  std::uint32_t mask_;    // capacity_ - 1
  std::uint32_t size_;
  std::uint32_t slot_size_;

  friend struct MapLayout;
  friend class GC;
  LAVA_DISALLOW_COPY_AND_ASSIGN(Map);
};

static_assert( std::is_standard_layout<Map>::value );
static_assert( sizeof(Map::Entry) == 24 );

struct MapLayout {
  static const std::uint32_t kCapacityOffset = offsetof(Map,capacity_);
  static const std::uint32_t kMaskOffset     = offsetof(Map,mask_);
  static const std::uint32_t kSizeOffset     = offsetof(Map,size_);
  static const std::uint32_t kSlotSize       = offsetof(Map,slot_size_);
  static const std::uint32_t kArrayOffset    = sizeof(Map);
};

struct MapEntryLayout {
  static const std::uint32_t kKeyOffset = offsetof(Map::Entry,key);
  static const std::uint32_t kValueOffset = offsetof(Map::Entry,value);
  static const std::uint32_t kHashOffset = offsetof(Map::Entry,hash);
  static const std::uint32_t kFlagOffset = sizeof(Value) + sizeof(Value) +
                                                           sizeof(std::uint32_t);
};

/**
 * Iterator represents a specific iterator on the heap.
 *
 * The iterator object mearly represents how we should iterate an object,
 * (which we don't know by looking at iterator). The iterator also stays
 * on the heap and it is used both by user via Extension and also by objects
 * like List and Object.
 */

class Iterator : public HeapObject {
 public:

  /**
   * Whether this iterator has next object or not
   */
  virtual bool HasNext() const = 0;

  /**
   * Deref the iterator to get the 1) key and 2) value
   */
  virtual void Deref( Value* , Value* ) const = 0;

  /**
   * Move the iterator to the next available slots
   * if this operation can be performed return true;
   * otherwise return false
   */
  virtual bool Move() = 0;

 public:
  virtual ~Iterator() {}

  friend class GC;
};


/**
 * Prototype represents a meta data structure for script side function unit
 *
 *
 * Each proto will be created automatically while function is defined and
 * it is referenced by Closure which will be created during runtime of script
 * execution
 */

class Prototype final : public HeapObject {
 public:
   // Used to store SSO string inside of the Prototype. This structure also
   // keeps a record of a String** which encapsulate the SSO* pointer
   struct SSOTableEntry {
     const SSO* sso;
     String**   str;
     SSOTableEntry(): sso(NULL), str(NULL) {}
     SSOTableEntry( const SSO* _1st , String** _2nd ): sso(_1st) , str(_2nd) {}
   };
   static_assert(sizeof(SSOTableEntry) == 16);

 public:
  Handle<String> proto_string() const { return proto_string_; }
  std::uint8_t argument_size() const { return argument_size_; }

  // Used to maintain correct GC status
  std::uint8_t max_local_var_size() const { return max_local_var_size_; }

 public: // Mutator
  void set_proto_string( const Handle<String>& str ) { proto_string_ = str; }
  void set_argument_size( std::size_t arg) { argument_size_ = arg; }

 public: // Size
  std::uint8_t real_table_size() const { return real_table_size_; }
  std::uint8_t string_table_size() const { return string_table_size_; }
  std::uint8_t sso_table_size() const { return sso_table_size_; }
  std::uint8_t upvalue_size() const { return upvalue_size_; }
  std::uint32_t code_buffer_size() const { return code_buffer_size_; }
  std::uint32_t sci_size() const { return code_buffer_size_; }
  std::uint32_t reg_offset_size() const { return code_buffer_size_; }

 public: // Constant table
  inline double GetReal( std::size_t ) const;
  inline Handle<String> GetString( std::size_t ) const;
  inline SSOTableEntry* GetSSO( std::size_t ) const;
  std::uint8_t GetUpValue( std::size_t , interpreter::UpValueState* ) const;
  interpreter::BytecodeIterator GetBytecodeIterator() const {
    return interpreter::BytecodeIterator( code_buffer(), code_buffer_size() );
  }
  const std::uint32_t* code_buffer() const { return code_buffer_; }
  inline const SourceCodeInfo& GetSci( std::size_t i ) const;
  inline std::uint8_t GetRegOffset( std::size_t i ) const;

  // Check whether this prototype is a closure , which means have upvalues
  bool IsClosure() const { return upvalue_table_ != NULL; }
  // Whether this function is pure function , means we don't need a closure
  // to interpret this function
  bool IsPureFunction() const { return !IsClosure(); }

  template< typename T >
  bool Visit( T* );
 public:
  void Dump( DumpWriter* writer , const std::string& source ) const;

 public:
  Prototype( const Handle<String>& pp , std::uint8_t argument_size ,
                                        std::uint8_t max_local_var_size,
                                        std::uint8_t real_table_size,
                                        std::uint8_t string_table_size,
                                        std::uint8_t sso_table_size,
                                        std::uint8_t upvalue_size,
                                        std::uint32_t code_buffer_size,
                                        double* rtable,
                                        String*** stable,
                                        SSOTableEntry* ssotable,
                                        std::uint32_t* utable,
                                        std::uint32_t* cb,
                                        SourceCodeInfo* sci,
                                        std::uint8_t* reg_offset_table );
 private:
  inline const double* real_table() const;
  String*** string_table() const { return string_table_; }
  SSOTableEntry* sso_table()    const { return sso_table_;  }
  const std::uint32_t* upvalue_table() const { return upvalue_table_; }
  const SourceCodeInfo* sci_buffer() const { return sci_buffer_; }
  const std::uint8_t* reg_offset_table() const { return reg_offset_table_; }

 private:
  Handle<String> proto_string_;
  std::uint8_t argument_size_;
  std::uint8_t max_local_var_size_;

  // Constant table size
  std::uint8_t real_table_size_;
  std::uint8_t string_table_size_;
  std::uint8_t sso_table_size_;

  // Upvalue slot size
  std::uint8_t upvalue_size_;

  // Code buffer size
  std::uint32_t code_buffer_size_;

  /**
   * For prototype, we don't use implicit layout since there are
   * too many members here and also it is hard to maintain this
   * kind of code when the member is too many. We need to adjust
   * alignment in between
   *
   *
   * NOTE: real_table is *stored* implicitly not due to the fact
   *       this saves me one memory hit instruction in interpreter
   *       for loading the real number. Obviously real number loading
   *       is a hot code path
   */

  String*** string_table_;
  SSOTableEntry* sso_table_;

  std::uint32_t* upvalue_table_;
  std::uint32_t* code_buffer_;
  SourceCodeInfo* sci_buffer_;
  std::uint8_t* reg_offset_table_;

  friend struct PrototypeLayout;
  friend class GC;
  friend class interpreter::BytecodeBuilder;

  LAVA_DISALLOW_COPY_AND_ASSIGN(Prototype);
};

static_assert( std::is_standard_layout<Prototype>::value );

struct PrototypeLayout {
  static const std::uint32_t kProtoStringOffset = offsetof(Prototype,proto_string_);
  static const std::uint32_t kArgumentSizeOffset= offsetof(Prototype,argument_size_);
  static const std::uint32_t kMaxLocalVarSizeOffset = offsetof(Prototype,max_local_var_size_);
  static const std::uint32_t kRealTableSizeOffset = offsetof(Prototype,real_table_size_);
  static const std::uint32_t kStringTableSizeOffset = offsetof(Prototype,string_table_size_);
  static const std::uint32_t kSSOTableSizeOffset = offsetof(Prototype,sso_table_size_);
  static const std::uint32_t kUpValueSizeOffset  = offsetof(Prototype,upvalue_size_);
  static const std::uint32_t kCodeBufferSizeOffset = offsetof(Prototype,code_buffer_size_);
  static const std::uint32_t kStringTableOffset = offsetof(Prototype,string_table_);
  static const std::uint32_t kSSOTableOffset = offsetof(Prototype,sso_table_);
  static const std::uint32_t kUpValueTableOffset= offsetof(Prototype,upvalue_table_);
  static const std::uint32_t kCodeBufferOffset = offsetof (Prototype,code_buffer_);
  static const std::uint32_t kSciBufferOffset  = offsetof (Prototype,sci_buffer_);
  static const std::uint32_t kRegOffsetTableOffset = offsetof(Prototype,reg_offset_table_);

  // GC will guarantee this , always put the constant table for real right after the
  // object in terms of memory layout
  //
  // Currently we don't have one single literal table, we may be able to do that since
  // this will save string loading and real number loading at same time , but I am not
  // sure whether worth it or not.
  static const std::uint32_t kRealTableOffset = sizeof(Prototype);
};

struct PrototypeSSOTableEntryLayout {
  static const std::uint32_t kSSOOffset = offsetof(Prototype::SSOTableEntry,sso);
  static const std::uint32_t kStrOffset = offsetof(Prototype::SSOTableEntry,str);
};
static_assert(PrototypeSSOTableEntryLayout::kSSOOffset == 0); // SSO must be at very first

/**
 * Closure represents a function that defined at script side. A closure *doesn't*
 * have a name since we are value based functional language. Closure can be
 * able to access its *upvalue* . The upvalue is a value that is outside of its
 * function's lexical scope but in its upper lexical scope. The way we implement
 * upvalue is via stack collapsing. Upvalue has its own array to be used to index
 * and they are per closure related resource. And each Upvalue index will directly
 * goes to the closure.
 *
 * Each closure will have a pointer points to its *proto* object which has all the
 * meta information for this function/closure
 */

class Closure final : public HeapObject {
 public:
  Handle<Prototype> prototype() const { return prototype_; }

  // Unsafe function since it doesn't check index and oob
  Value* upvalue() {
    return reinterpret_cast<Value*>(
        reinterpret_cast<char*>(this)+sizeof(Closure));
  }

  const Value* upvalue() const {
    return reinterpret_cast<const Value*>(
        reinterpret_cast<const char*>(this)+sizeof(Closure));
  }

  Value GetUpValue( std::uint8_t idx ) const {
    lava_debug(NORMAL,lava_verify(idx < prototype_->upvalue_size()););
    return upvalue()[idx];
  }

  template< typename T >
  bool Visit( T* );

 public:
  // cached attribute accessor
  const std::uint32_t* code_buffer() const { return code_buffer_; }
  std::uint8_t argument_size() const { return argument_size_; }
 public:
  // Create a closure that is used to wrap *main* prototype
  static Handle<Closure> New( GC* , const Handle<Prototype>& );

  Closure( const Handle<Prototype>& proto ):
    prototype_(proto),
    code_buffer_(proto->code_buffer()),
    argument_size_(proto->argument_size())
  {}

 private:
  Handle<Prototype> prototype_;

  /** cached value from Prototype object to avoid pointer chasing
   *  The value put here must be persisten across the GC boundary
   */
  const std::uint32_t* code_buffer_; // *cached* code buffer pointer to avoid too much
                                     // pointer chasing inside of interpreter. The code
                                     // cache is not gc with the normal heap and it is
                                     // persistent across heap compaction , so we can
                                     // cache it as long as the prototype is alive which
                                     // is always true

  std::uint8_t argument_size_;       // Argument size of the attached protocol
  friend struct ClosureLayout;
  friend class GC;
  LAVA_DISALLOW_COPY_AND_ASSIGN(Closure);
};
static_assert( std::is_standard_layout<Closure>::value );

struct ClosureLayout {
  static const std::uint32_t kPrototypeOffset = offsetof(Closure,prototype_);
  static const std::uint32_t kCodeBufferOffset= offsetof(Closure,code_buffer_);
  static const std::uint32_t kArgumentSizeOffset = offsetof(Closure,argument_size_);
  static const std::uint32_t kUpValueOffset   = sizeof(Closure);
};

/**
 * Extension is the only interfaces that user can be used to extend the whole world
 * of lavascript. An extension is a specialized HeapObject that is used to customization.
 */

class Extension : public HeapObject {
 public:
   // Arithmetic handler for extension.
  virtual bool Add( const Value& , const Value& , Value* , std::string* );
  virtual bool Sub( const Value& , const Value& , Value* , std::string* );
  virtual bool Mul( const Value& , const Value& , Value* , std::string* );
  virtual bool Div( const Value& , const Value& , Value* , std::string* );
  virtual bool Mod( const Value& , const Value& , Value* , std::string* );
  virtual bool Pow( const Value& , const Value& , Value* , std::string* );

  // Comparison handler for extension
  virtual bool Lt ( const Value& , const Value& , Value* , std::string* );
  virtual bool Le ( const Value& , const Value& , Value* , std::string* );
  virtual bool Gt ( const Value& , const Value& , Value* , std::string* );
  virtual bool Ge ( const Value& , const Value& , Value* , std::string* );
  virtual bool Eq ( const Value& , const Value& , Value* , std::string* );
  virtual bool Ne ( const Value& , const Value& , Value* , std::string* );

  // Accessor
  virtual bool GetProp ( const Value& , const Value& , Value* , std::string* ) const;
  virtual bool SetProp ( const Value& , const Value& , const Value& , std::string* );

  // Iterator
  virtual Handle<Iterator> NewIterator( GC* , const Handle<Extension>& , std::string* ) const;

  virtual bool Size( std::uint32_t* , std::string* ) const;

  // Function Call
  virtual bool Call( CallFrame* call_frame , std::string* error );


  // Unique type name
  virtual const char* name() const = 0;

 public:
  virtual ~Extension() = 0;
};

/**
 * Script is a representation of a *source code* file. Each source code will be represented
 * by Script object and sleeps on the heap. Like all other stuff on the heap. The Script
 * object is immutable and it is generated by ScriptBuilder object.
 */
class Script final : public HeapObject {
 public:
  Handle<String> source() const { return source_; }
  Handle<String> filename() const { return filename_; }
  Context* context() const { return context_; }
 public:
  Handle<Prototype> main() const { return main_; }

 public:
  /* -------------------------------
   * Function table                |
   * ------------------------------*/
  std::size_t function_table_size() const { return function_table_size_; }

  struct FunctionTableEntry {
    Handle<String> name; // Can be empty, if it is empty then it is a anonymous function
    Handle<Prototype> prototype;
    FunctionTableEntry(): name() , prototype() {}
    FunctionTableEntry( const Handle<String>& n ,
                        const Handle<Prototype>& p ):
      name(n),
      prototype(p)
    {}
  };

  inline const FunctionTableEntry& GetFunction( std::size_t ) const;

 public:
  template< typename T > bool Visit( T* ); // For GC

 public:
  // Create a new Script object
  static Handle<Script> New( GC* , Context* , const ScriptBuilder& );

  Script( Context* context,
          const Handle<String>& source ,
          const Handle<String>& filename ,
          const Handle<Prototype>& main ,
          std::size_t function_table_size ):
    source_(source),
    filename_(filename),
    main_(main),
    function_table_size_(function_table_size),
    context_(context)
  {}

 private:
  inline FunctionTableEntry* fte_array() const;

  Handle<String> source_;
  Handle<String> filename_;
  Handle<Prototype> main_;
  std::size_t function_table_size_;
  Context* context_;

  friend class GC;
  LAVA_DISALLOW_COPY_AND_ASSIGN(Script);
};

/* =================================================================================
 *
 * Inline functions definitions
 *
 * ================================================================================*/

/* --------------------------------------------------------------------
 * Handle
 * ------------------------------------------------------------------*/
template< typename T > inline Handle<T>::Handle():
  ref_(NULL) {
  static_assert( std::is_base_of<HeapObject,T>::value );
}
template< typename T > inline Handle<T>::Handle( HeapObject** ref ) {
  static_assert( std::is_base_of<HeapObject,T>::value );
  lava_debug(NORMAL,
      lava_verify(ref && *ref);
      lava_verify((*ref)->IsType<T>());
    );
  ref_ = reinterpret_cast<T**>(ref);
}

template< typename T > inline Handle<T>::Handle( T** ref ):
  ref_(ref) {
  static_assert( std::is_base_of<HeapObject,T>::value );
}

template< typename T > inline Handle<T>::Handle( const Handle& that ):
  ref_(that.ref_)
{}

template< typename T >
inline Handle<T>& Handle<T>::operator = ( const Handle<T>& that ) {
  ref_ = that.ref_;
  return *this;
}

template< typename T >
inline T* Handle<T>::operator -> () {
  lava_debug(NORMAL,lava_verify(!IsNull()););
  return *ref_;
}

template< typename T >
inline const T* Handle<T>::operator -> () const {
  lava_debug(NORMAL,lava_verify(!IsNull()););
  return *ref_;
}

template< typename T >
inline T& Handle<T>::operator * () {
  lava_debug(NORMAL,lava_verify(!IsNull()););
  return **ref_;
}

template< typename T >
inline const T& Handle<T>::operator * () const {
  lava_debug(NORMAL,lava_verify(!IsNull()););
  return **ref_;
}

template< typename T >
inline HeapObject** Handle<T>::heap_object() const {
  lava_debug(NORMAL,lava_verify(!IsNull()););
  return reinterpret_cast<HeapObject**>(ref_);
}

template< typename T >
inline T** Handle<T>::ref() const {
  lava_debug(NORMAL,lava_verify(!IsRefEmpty()););
  return ref_;
}

template< typename T >
inline T* Handle<T>::ptr() const {
  lava_debug(NORMAL,lava_verify(!IsNull()););
  return *ref_;
}

/* --------------------------------------------------------------------
 * Value
 * ------------------------------------------------------------------*/
template< typename T >
Value::Value( T** obj ) {
  static_assert( std::is_base_of<Extension,T>::value );
  SetHeapObject( reinterpret_cast<HeapObject**>(obj) );
}

inline Value& Value::operator = ( const Value& that ) {
  if(this != &that) {
    raw_ = that.raw_;
  }
  return *this;
}

inline HeapObject** Value::heap_object() const {
  return reinterpret_cast<HeapObject**>(raw_ & kPtrMask);
}

inline void Value::SetHeapObject( HeapObject** ptr ) {
  /**
   * Checking whether the input pointer is valid pointer
   * with our assumptions
   */
  lava_debug(NORMAL,
      lava_assertF( (reinterpret_cast<std::uintptr_t>(ptr)&kPtrCheckMask) == 0 ,
                    "the pointer %x specified here is not a valid pointer,"
                    "upper 16 bits is not 0s", ptr );
    );

  raw_ = static_cast<std::uint64_t>(
      reinterpret_cast<std::uintptr_t>(ptr) | TAG_HEAP);
}

inline ValueType Value::type() const {
  if(IsTagReal()) {
    return TYPE_REAL;
  } else {
    /**
     * The code here is extreamly sensitive since it is coded based on
     * the order of the type and actual number literal of each type defined
     * above.
     *
     * This function has some simple optimization to avoid too much comparison
     * but it is of best efforts and not really importantant. This function should
     * be avoided to use IsXXX test function to decide the type instead of calling
     * *type()*.
     */
    static const std::uint64_t kMask = 0x000f000000000000;
    static const std::uint64_t kSMask= 0x0000f00000000000;
    static const std::uint32_t kShift= 48;
    static const std::uint32_t kSShift=44;
    static const std::uint64_t kBase = 8;
    // Order sensitive , if change other parts this needs to be changed
    static const ValueType kFlags[] = {
      SIZE_OF_VALUE_TYPES, // not used
      SIZE_OF_VALUE_TYPES, // 1
      TYPE_STRING,         // 2
      TYPE_LIST,           // 3
      TYPE_OBJECT,         // 4
      SIZE_OF_VALUE_TYPES  // 5
    };

    static const ValueType kSFlags[] = {
      SIZE_OF_VALUE_TYPES,
      TYPE_BOOLEAN
    };
    const std::size_t idx = ((raw_ & kMask) >> kShift) - kBase;
    if(idx ==0) {
      const std::size_t subidx = ((raw_ & kSMask) >> kSShift);
      return kSFlags[subidx];
    } else if(idx == 5) {
      return IsTagNull() ? TYPE_NULL : TYPE_BOOLEAN;
    } else {
      ValueType flag = kFlags[idx];
      if(flag == SIZE_OF_VALUE_TYPES) {
        lava_debug(NORMAL,lava_verify(IsHeapObject()););
        return (*heap_object())->type();
      } else {
        return flag;
      }
    }
  }
}

inline const char* Value::type_name() const {
  return GetValueTypeName( type() );
}


inline bool Value::IsString() const {
  return IsHeapObject() && (*heap_object())->IsString();
}

inline bool Value::IsSSO() const {
  return GetString()->IsSSO();
}

inline bool Value::IsLongString() const {
  return GetString()->IsLongString();
}

inline bool Value::IsList() const {
  return IsHeapObject() && (*heap_object())->IsList();
}

inline bool Value::IsSlice() const {
  return IsHeapObject() && (*heap_object())->IsSlice();
}

inline bool Value::IsObject() const {
  return IsHeapObject() && (*heap_object())->IsObject();
}

inline bool Value::IsMap() const {
  return IsHeapObject() && (*heap_object())->IsMap();
}

inline bool Value::IsPrototype() const {
  return IsHeapObject() && (*heap_object())->IsPrototype();
}

inline bool Value::IsClosure() const {
  return IsHeapObject() && (*heap_object())->IsClosure();
}

inline bool Value::IsExtension() const {
  return IsHeapObject() && (*heap_object())->IsExtension();
}

inline bool Value::IsIterator() const {
  return IsHeapObject() && (*heap_object())->IsIterator();
}

inline bool Value::GetBoolean() const {
  lava_debug(NORMAL,lava_verify(IsBoolean()););
  return IsTagTrue();
}

inline double Value::GetReal() const {
  lava_debug(NORMAL,lava_verify(IsReal()););
  return real_;
}

inline HeapObject** Value::GetHeapObject() const {
  lava_debug(NORMAL,lava_verify(IsHeapObject()););
  return heap_object();
}

inline Handle<String> Value::GetString() const {
  lava_debug(NORMAL,lava_verify(IsHeapObject()););
  return Handle<String>(heap_object());
}

inline Handle<List> Value::GetList() const {
  lava_debug(NORMAL,lava_verify(IsHeapObject()););
  return Handle<List>(heap_object());
}

inline Handle<Slice> Value::GetSlice() const {
  lava_debug(NORMAL,lava_verify(IsHeapObject()););
  return Handle<Slice>(heap_object());
}

inline Handle<Object> Value::GetObject() const {
  lava_debug(NORMAL,lava_verify(IsHeapObject()););
  return Handle<Object>(heap_object());
}

inline Handle<Map> Value::GetMap() const {
  lava_debug(NORMAL,lava_verify(IsHeapObject()););
  return Handle<Map>(heap_object());
}

inline Handle<Prototype> Value::GetPrototype() const {
  lava_debug(NORMAL,lava_verify(IsHeapObject()););
  return Handle<Prototype>(heap_object());
}

inline Handle<Closure> Value::GetClosure() const {
  lava_debug(NORMAL,lava_verify(IsHeapObject()););
  return Handle<Closure>(heap_object());
}

inline Handle<Extension> Value::GetExtension() const {
  lava_debug(NORMAL,lava_verify(IsHeapObject()););
  return Handle<Extension>(heap_object());
}

inline Handle<Iterator> Value::GetIterator() const {
  lava_debug(NORMAL,lava_verify(IsHeapObject()););
  return Handle<Iterator>(heap_object());
}

inline void Value::SetReal( double value ) {
  real_ = value;
}

inline void Value::SetBoolean( bool value ) {
  if(value)
    raw_ = TAG_TRUE;
  else
    raw_ = TAG_FALSE;
}

inline void Value::SetString( const Handle<String>& str ) {
  SetHeapObject(str.heap_object());
}

inline void Value::SetList( const Handle<List>& list ) {
  SetHeapObject(list.heap_object());
}

inline void Value::SetSlice( const Handle<Slice>& slice ) {
  SetHeapObject(slice.heap_object());
}

inline void Value::SetObject( const Handle<Object>& object ) {
  SetHeapObject(object.heap_object());
}

inline void Value::SetMap( const Handle<Map>& map ) {
  SetHeapObject(map.heap_object());
}

inline void Value::SetPrototype( const Handle<Prototype>& proto ) {
  SetHeapObject(proto.heap_object());
}

inline void Value::SetClosure( const Handle<Closure>& closure ) {
  SetHeapObject(closure.heap_object());
}

inline void Value::SetExtension( const Handle<Extension>& ext ) {
  SetHeapObject(ext.heap_object());
}

inline void Value::SetIterator( const Handle<Iterator>& itr ) {
  SetHeapObject(itr.heap_object());
}

inline bool Value::Equal( const Value& that ) const {
  if(this == &that) return true;

  if(that.IsBoolean() && IsBoolean())
    return that.GetBoolean() == GetBoolean();
  else if(that.IsNull() && IsNull())
    return true;
  else if(that.IsReal() && IsReal())
    return that.GetReal() == GetReal();
  else {
    lava_debug(NORMAL,lava_verify(that.IsHeapObject()););
    return IsHeapObject() ? (that.GetHeapObject() == GetHeapObject()) : false;
  }
}

/* --------------------------------------------------------------------
 * HeapObject
 * ------------------------------------------------------------------*/

template< typename T > bool HeapObject::IsType() {
  if(std::is_same<T,String>::value)
    return IsString();
  else if(std::is_same<T,List>::value)
    return IsList();
  else if(std::is_same<T,Slice>::value)
    return IsSlice();
  else if(std::is_same<T,Object>::value)
    return IsObject();
  else if(std::is_same<T,Map>::value)
    return IsMap();
  else if(std::is_same<T,Iterator>::value)
    return IsIterator();
  else if(std::is_same<T,Prototype>::value)
    return IsPrototype();
  else if(std::is_same<T,Closure>::value)
    return IsClosure();
  else if(std::is_same<T,Extension>::value)
    return IsExtension();
  else
    lava_die();
  return false;
}

inline void HeapObject::set_gc_state( GCState state ) {
  HeapObjectHeader hdr( hoh() );
  hdr.set_gc_state(state);
  set_hoh(hdr);
}


/* --------------------------------------------------------------------
 * SSO
 * ------------------------------------------------------------------*/

inline const void* SSO::data() const {
  return reinterpret_cast<const void*>(
      reinterpret_cast<const char*>(this) + sizeof(SSO));
}

inline bool SSO::operator == ( const char* str ) const {
  const std::size_t l = strlen(str);
  const std::size_t len = std::min(l,size_);
  int r = std::memcmp(data(),str,len);
  return r == 0 ? (len == l) : false;
}

inline bool SSO::operator == ( const std::string& str ) const {
  const std::size_t len = std::min(size_,str.size());
  int r = std::memcmp(data(),str.c_str(),len);
  return r == 0 ? (size_ == len) : false;
}

inline bool SSO::operator == ( const String& str ) const {
  const std::size_t len = std::min(size_,str.size());
  int r = std::memcmp(data(),str.data(),len);
  return r == 0 ? (size_ == len) : false;
}

inline bool SSO::operator == ( const SSO& str ) const {
  return this == &str;
}

inline bool SSO::operator != ( const char* str ) const {
  const std::size_t l = strlen(str);
  const std::size_t len = std::min(size_,l);
  int r = std::memcmp(data(),str,len);
  return r == 0 ? (size_ != len) : true;
}

inline bool SSO::operator != ( const std::string& str ) const {
  const std::size_t len = std::min(size_,str.size());
  int r = std::memcmp(data(),str.c_str(),len);
  return r == 0 ? (size_ != len) : true;
}

inline bool SSO::operator != ( const String& str ) const {
  const std::size_t len = std::min(size_,str.size());
  int r = std::memcmp(data(),str.data(),len);
  return r == 0 ? (size_ != len ) : true;
}

inline bool SSO::operator != ( const SSO& str ) const {
  return this != &str;
}

inline bool SSO::operator >  ( const char* str ) const {
  const std::size_t l = strlen(str);
  const std::size_t len = std::min(size_,l);
  int r = std::memcmp(data(),str,len);
  if(r > 0 || (r == 0 && size_ > len))
    return true;
  else
    return false;
}

inline bool SSO::operator > ( const std::string& str ) const {
  const std::size_t len = std::min(size_,str.size());
  int r = std::memcmp(data(),str.c_str(),len);
  if(r > 0 || (r == 0 && size_ > len))
    return true;
  else
    return false;
}

inline bool SSO::operator > ( const String& str ) const {
  const std::size_t len = std::min(size_,str.size());
  int r = std::memcmp(data(),str.data(),len);
  if(r > 0 || (r == 0 && size_ > len))
    return true;
  else
    return false;
}

inline bool SSO::operator > ( const SSO& str ) const {
  const std::size_t len = std::min(size_,str.size());
  int r = std::memcmp(data(),str.data(),len);
  if(r > 0 || (r == 0 && size_ > len))
    return true;
  else
    return false;
}

inline bool SSO::operator >= ( const char* str ) const {
  const std::size_t l = strlen(str);
  const std::size_t len = std::min(size_,l);
  int r = std::memcmp(data(),str,len);
  if(r > 0 || (r == 0 && size_ >= len))
    return true;
  else
    return false;
}

inline bool SSO::operator >= ( const std::string& str ) const {
  const std::size_t len = std::min(size_,str.size());
  int r = std::memcmp(data(),str.c_str(),len);
  if(r > 0 || (r == 0 && size_ >= len))
    return true;
  else
    return false;
}

inline bool SSO::operator >= ( const String& str ) const {
  const std::size_t len = std::min(size_,str.size());
  int r = std::memcmp(data(),str.data(),len);
  if(r > 0 || (r == 0 && size_ >= len))
    return true;
  else
    return false;
}

inline bool SSO::operator >= ( const SSO& str ) const {
  return (*this == str) || (*this > str);
}

inline bool SSO::operator < ( const char* str ) const {
  const std::size_t l = strlen(str);
  const std::size_t len = std::min(size_,l);
  int r = std::memcmp(data(),str,len);
  if(r < 0 || (r == 0 && size_ < l))
    return true;
  else
    return false;
}

inline bool SSO::operator < ( const std::string& str ) const {
  const std::size_t len = std::min(size_,str.size());
  int r = std::memcmp(data(),str.c_str(),len);
  if(r < 0 || (r == 0 && size_ < str.size()))
    return true;
  else
    return false;
}

inline bool SSO::operator < ( const String& str ) const {
  const std::size_t len = std::min(size_,str.size());
  int r = std::memcmp(data(),str.data(),len);
  if(r < 0 || (r == 0 && size_ < str.size()))
    return true;
  else
    return false;
}

inline bool SSO::operator < ( const SSO& str ) const {
  const std::size_t len = std::min(size_,str.size());
  int r = std::memcmp(data(),str.data(),len);
  if(r < 0 || (r == 0 && size_ < str.size()))
    return true;
  else
    return false;
}

inline bool SSO::operator <= ( const char* str ) const {
  const std::size_t l = strlen(str);
  const std::size_t len = std::min(size_,l);
  int r = std::memcmp(data(),str,len);
  if(r < 0 || (r == 0 && size_ <= l))
    return true;
  else
    return false;
}

inline bool SSO::operator <= ( const std::string& str ) const {
  const std::size_t len = std::min(size_,str.size());
  int r = std::memcmp(data(),str.c_str(),len);
  if(r < 0 || (r == 0 && size_ <= str.size()))
    return true;
  else
    return false;
}

inline bool SSO::operator <= ( const String& str ) const {
  const std::size_t len = std::min(size_,str.size());
  int r = std::memcmp(data(),str.data(),len);
  if(r < 0 || (r == 0 && size_ <= str.size()))
    return true;
  else
    return false;
}

inline bool SSO::operator <= ( const SSO& str ) const {
  return (*this == str) || (*this < str);
}

/* --------------------------------------------------------------------
 * Long String
 * ------------------------------------------------------------------*/

inline const void* LongString::data() const {
  return reinterpret_cast<const void*>(
      reinterpret_cast<const char*>(this) + sizeof(LongString));
}


/* --------------------------------------------------------------------
 * String
 * ------------------------------------------------------------------*/

inline const void* String::data() const {
  if(IsSSO())
    return sso().data();
  else
    return long_string().data();
}

inline std::size_t String::size() const {
  if(IsSSO())
    return sso().size();
  else
    return long_string().size;
}

inline const SSO& String::sso() const {
  lava_debug(NORMAL,lava_verify(IsSSO()););

  // The SSO are actually stored inside of SSOPool.
  // For a String object, it only stores a pointer
  // to the actual SSO object
  return **reinterpret_cast<SSO**>(const_cast<String*>(this));
}

inline const LongString& String::long_string() const {
  lava_debug(NORMAL,lava_verify(IsLongString()););
  return *reinterpret_cast<const LongString*>(this);
}

inline bool String::operator == ( const char* str ) const {
  const std::size_t l = strlen(str);
  const std::size_t len = std::min(size(),l);
  int r = std::memcmp(data(),str,len);
  return r == 0 ? size() == l : false;
}

inline bool String::operator == ( const std::string& str ) const {
  const std::size_t len = std::min(size(),str.size());
  int r = std::memcmp(data(),str.c_str(),len);
  return r == 0 ? size() == str.size() : false;
}

inline bool String::operator == ( const String& str ) const {
  const std::size_t len = std::min(size(),str.size());
  int r = std::memcmp(data(),str.data(),len);
  return r == 0 ? size() == str.size() : false;
}

inline bool String::operator == ( const SSO& str ) const {
  if(IsSSO()) {
    return str == sso();
  } else {
    const LongString& lstr = long_string();
    const std::size_t len = std::min(lstr.size,str.size());
    int r = std::memcmp(lstr.data(),str.data(),len);
    return r == 0 ? lstr.size == str.size() : false;
  }
}

inline bool String::operator != ( const char* str ) const {
  const std::size_t l = strlen(str);
  const std::size_t len = std::min(size(),l);
  int r = std::memcmp(data(),str,len);
  return r == 0 ? size() != l : true;
}

inline bool String::operator != ( const std::string& str ) const {
  const std::size_t len = std::min(size(),str.size());
  int r = std::memcmp(data(),str.c_str(),len);
  return r == 0 ? size() != str.size() : true;
}

inline bool String::operator != ( const String& str ) const {
  const std::size_t len = std::min(size(),str.size());
  int r = std::memcmp(data(),str.data(),len);
  return r == 0 ? size() != str.size() : true;
}

inline bool String::operator != ( const SSO& str ) const {
  if(IsSSO()) {
    return str != sso();
  } else {
    const LongString& lstr = long_string();
    const std::size_t len = std::min(lstr.size,str.size());
    int r = std::memcmp(lstr.data(),str.data(),len);
    return r == 0 ? lstr.size != str.size() : true;
  }
}

inline bool String::operator > ( const char* str ) const {
  const std::size_t l = strlen(str);
  const std::size_t len = std::min(size(),l);
  int r = std::memcmp(data(),str,len);
  if(r > 0 || (r == 0 && size() > l))
    return true;
  else
    return false;
}

inline bool String::operator > ( const std::string& str ) const {
  const std::size_t len = std::min(size(),str.size());
  int r = std::memcmp(data(),str.c_str(),len);
  return (r > 0 ||(r == 0 && size() > str.size()));
}

inline bool String::operator > ( const String& str ) const {
  const std::size_t len = std::min(size(),str.size());
  int r = std::memcmp(data(),str.data(),len);
  return (r > 0 ||(r == 0 && size() > str.size()));
}

inline bool String::operator > ( const SSO& str ) const {
  const std::size_t len = std::min(size(),str.size());
  int r = std::memcmp(data(),str.data(),len);
  return (r > 0 ||(r == 0 && size() > str.size()));
}

inline bool String::operator >= ( const char* str ) const {
  const std::size_t l = strlen(str);
  const std::size_t len = std::min(size(),l);
  int r = std::memcmp(data(),str,len);
  return (r > 0 ||(r == 0 && size() >= l));
}

inline bool String::operator >= ( const std::string& str ) const {
  const std::size_t len = std::min(size(),str.size());
  int r = std::memcmp(data(),str.c_str(),len);
  return (r > 0 ||(r == 0 && size() >= str.size()));
}

inline bool String::operator >= ( const String& str ) const {
  const std::size_t len = std::min(size(),str.size());
  int r = std::memcmp(data(),str.data(),len);
  return (r > 0 ||(r == 0 && size() >= str.size()));
}

inline bool String::operator >= ( const SSO& str ) const {
  const std::size_t len = std::min(size(),str.size());
  int r = std::memcmp(data(),str.data(),len);
  return (r > 0 ||(r == 0 && size() >= str.size()));
}

inline bool String::operator <  ( const char* str ) const {
  const std::size_t l = strlen(str);
  const std::size_t len = std::min(size(),l);
  int r = std::memcmp(data(),str,len);
  return (r < 0||(r == 0 && size() < l));
}

inline bool String::operator <  ( const std::string& str ) const {
  const std::size_t len = std::min(size(),str.size());
  int r = std::memcmp(data(),str.c_str(),len);
  return (r < 0 ||(r == 0 && size() < str.size()));
}

inline bool String::operator < ( const String& str ) const {
  const std::size_t len = std::min(size(),str.size());
  int r = std::memcmp(data(),str.data(),len);
  return (r < 0 ||(r == 0 && size() < str.size()));
}

inline bool String::operator <  ( const SSO& str ) const {
  const std::size_t len = std::min(size(),str.size());
  int r = std::memcmp(data(),str.data(),len);
  return (r < 0 ||(r == 0 && size() < str.size()));
}

inline bool String::operator <= ( const char* str ) const {
  const std::size_t l = strlen(str);
  const std::size_t len = std::min(size(),l);
  int r = std::memcmp(data(),str,len);
  return (r < 0 || (r == 0 && size() <= l));
}

inline bool String::operator <= ( const std::string& str ) const {
  const std::size_t len = std::min(size(),str.size());
  int r = std::memcmp(data(),str.c_str(),len);
  return (r < 0 || (r == 0 && size() <= str.size()));
}

inline bool String::operator <= ( const String& str ) const {
  const std::size_t len = std::min(size(),str.size());
  int r = std::memcmp(data(),str.data(),len);
  return (r < 0 || (r == 0 && size() <= str.size()));
}

inline bool String::operator <= ( const SSO& str ) const {
  const std::size_t len = std::min(size(),str.size());
  int r = std::memcmp(data(),str.data(),len);
  return (r < 0 || (r == 0 && size() <= str.size()));
}

/* --------------------------------------------------------------------
 * List
 * ------------------------------------------------------------------*/

inline std::size_t List::capacity() const {
  return slice()->capacity();
}

inline const Value& List::Index( std::size_t index ) const {
  return slice()->Index(index);
}

inline Value& List::Index( std::size_t index ) {
  return slice()->Index(index);
}

inline Value& List::Last() {
  lava_debug(NORMAL,lava_verify(size() >0););
  return slice()->Index( size() - 1 );
}

inline const Value& List::Last() const {
  lava_debug(NORMAL,lava_verify(size() >0););
  return slice()->Index( size() - 1 );
}

inline Value& List::First() {
  lava_debug(NORMAL,lava_verify(size() >0););
  return slice()->Index(0);
}

inline const Value& List::First() const {
  lava_debug(NORMAL,lava_verify(size() >0););
  return slice()->Index(0);
}

inline bool List::Push( GC* gc , const Value& value ) {
  if(size_ == slice_->capacity()) {
    // We run out of memory for this slice , just dump it
    slice_ = Slice::Extend(gc,slice_);

    // We cannot allocate a larger Slice , return false directly
    if(!slice_) return false;
  }

  slice_->Index(size_) = value;
  ++size_;
  return true;
}

inline void List::Pop() {
  lava_debug(NORMAL,lava_verify(size_ > 0););
  --size_;
}

template< typename T > bool List::Visit( T* visitor ) {
  if(visitor->Begin(this)) {
    if(visitor->VisitSlice(slice()))
      return visitor->End(this);
  }
  return false;
}

/* --------------------------------------------------------------------
 * Slice
 * ------------------------------------------------------------------*/

inline void* Slice::array() const {
  Slice* self = const_cast<Slice*>(this);
  return reinterpret_cast<void*>(
      reinterpret_cast<char*>(self) + sizeof(Slice));
}

inline Value* Slice::data() {
  return static_cast<Value*>(array());
}

inline const Value* Slice::data() const {
  return static_cast<const Value*>(array());
}

inline const Value& Slice::Index( std::size_t index ) const {
  lava_debug(NORMAL,lava_verify(index < capacity()););
  return data()[index];
}

inline Value& Slice::Index( std::size_t index ) {
  lava_debug(NORMAL,lava_verify(index < capacity()););
  return data()[index];
}

template< typename T > bool Slice::Visit( T* visitor ) {
  if(visitor->Begin(this)) {
    for( std::size_t i = 0 ; i < capacity() ; ++i ) {
      if(!visitor->VisitValue(Index(i))) return false;
    }
    return visitor->End(this);
  }
  return false;
}

/* --------------------------------------------------------------------
 * Slice
 * ------------------------------------------------------------------*/

inline std::size_t Object::capacity() const {
  return map()->capacity();
}

inline std::size_t Object::size() const {
  return map()->size();
}

inline bool Object::IsEmpty() const {
  return map()->IsEmpty();
}

inline bool Object::Get( const Handle<String>& key , Value* output ) const {
  return map()->Get(key,output);
}

inline bool Object::Get( const char* key , Value* output ) const {
  return map()->Get(key,output);
}

inline bool Object::Get( const std::string& key , Value* output ) const {
  return map()->Get(key,output);
}

inline bool Object::Set( GC* gc , const Handle<String>& key ,
                                  const Value& val ) {
  if(map_->NeedRehash()) {
    map_ = Map::Rehash(gc,map_);
    if(!map_) return false;
  }

  return map_->Set(gc,key,val);
}

inline bool Object::Set( GC* gc , const char* key , const Value& val ) {
  if(map_->NeedRehash()) {
    map_ = Map::Rehash(gc,map_);
    if(!map_) return false;
  }
  return map_->Set(gc,key,val);
}

inline bool Object::Set( GC* gc , const std::string& key ,
    const Value& val ) {
  if(map_->NeedRehash()) {
    map_ = Map::Rehash(gc,map_);
    if(!map_) return false;
  }
  return map_->Set(gc,key,val);
}

inline bool Object::Update( GC* gc , const Handle<String>& key ,
                                     const Value& val ) {
  if(map_->NeedRehash()) {
    map_ = Map::Rehash(gc,map_);
    if(!map_) return false;
  }
  return map_->Update(gc,key,val);
}

inline bool Object::Update( GC* gc , const char* key ,
                                     const Value& val ) {
  if(map_->NeedRehash()) {
    map_ = Map::Rehash(gc,map_);
    if(!map_) return false;
  }
  return map_->Update(gc,key,val);
}

inline bool Object::Update( GC* gc , const std::string& key ,
                                     const Value& val ) {
  if(map_->NeedRehash()) {
    map_ = Map::Rehash(gc,map_);
    if(!map_) return false;
  }
  return map_->Update(gc,key,val);
}

inline void Object::Put( GC* gc , const Handle<String>& key ,
                                  const Value& val ) {
  if(map_->NeedRehash()) {
    map_ = Map::Rehash(gc,map_);
  }
  map_->Put(gc,key,val);
}

inline void Object::Put( GC* gc , const char* key ,
                                  const Value& val ) {
  if(map_->NeedRehash()) {
    map_ = Map::Rehash(gc,map_);
  }
  map_->Put(gc,key,val);
}

inline void Object::Put( GC* gc , const std::string& key,
                                  const Value& val ) {
  if(map_->NeedRehash()) {
    map_ = Map::Rehash(gc,map_);
  }
  map_->Put(gc,key,val);
}

inline bool Object::Delete( const Handle<String>& key ) {
  return map_->Delete(key);
}

inline bool Object::Delete( const char* key ) {
  return map_->Delete(key);
}

inline bool Object::Delete( const std::string& key ) {
  return map_->Delete(key);
}

template<typename T> bool Object::Visit( T* visitor ) {
  if(visitor->Begin(this)) {
    if(visitor->VisitMap(map()))
      return visitor->End(this);
  }
  return false;
}

/* --------------------------------------------------------------------
 * Map
 * ------------------------------------------------------------------*/

inline std::uint32_t Map::Hash( const Handle<String>& key ) {
  if(key->IsSSO()) {
    // Use default hash when it is SSO and this behavior *should* not change
    return key->sso().hash();
  } else {
    return Hasher::Hash(key->long_string().data(),key->long_string().size);
  }
}

inline std::uint32_t Map::Hash( const char* key ) {
  return Hasher::Hash(key,strlen(key));
}

inline std::uint32_t Map::Hash( const std::string& key ) {
  return Hasher::Hash(key.c_str(),key.size());
}

template< typename T >
Map::Entry* Map::FindEntry( const T& key , std::uint32_t fullhash ,
                                           Option opt ) const {
  lava_debug(NORMAL,
      lava_verify(capacity() && bits::NextPowerOf2(capacity()) == capacity());
    );

  Map* self = const_cast<Map*>(this);
  int main_position = fullhash & mask_;
  Entry* main = self->data()+main_position;
  if(!main->use) return opt == FIND ? NULL : main;

  // Okay the main entry is been used or at least it is a on chain of the
  // collision list. So we need to chase down the link to see whats happening
  Entry* cur = main;
  do {
    if(!cur->del) {
      if(cur->hash == fullhash && Equal(Handle<String>(cur->key),key)) {
        return opt == INSERT ? NULL : cur;
      }
    }
    if(cur->more)
      cur = self->data()+(cur->next);
    else
      break;
  } while(true);

  if(opt == FIND) return NULL;

  // linear probing to find the next available new_slot
  Entry* new_slot = NULL;
  std::uint32_t h = fullhash;
  while( (new_slot = self->data()+(++h &(capacity()-1)))->use )
    ;

  cur->more = 1;

  // linked the previous Entry to the current found one
  cur->next = (new_slot - self->data());

  return new_slot;
}

inline void* Map::entry() const {
  return reinterpret_cast<void*>(
      reinterpret_cast<char*>(const_cast<Map*>(this)) + sizeof(Map));
}

inline Map::Entry* Map::data() { return static_cast<Entry*>(entry()); }

inline const Map::Entry* Map::data() const {
  return static_cast<const Entry*>(entry());
}

inline bool Map::Get( const Handle<String>& key , Value* output ) const {
  if(size_ == 0) return false;

  Entry* entry = FindEntry(key,Hash(key),FIND);
  if(entry) {
    *output = entry->value;
    return true;
  }
  return false;
}

inline bool Map::Get( const char* key , Value* output ) const {
  if(size_ == 0) return false;

  Entry* entry = FindEntry(key,Hash(key),FIND);
  if(entry) {
    *output = entry->value;
    return true;
  }
  return false;
}

inline bool Map::Get( const std::string& key , Value* output ) const {
  if(size_ == 0) return false;

  Entry* entry = FindEntry(key,Hash(key),FIND);
  if(entry) {
    *output = entry->value;
    return true;
  }
  return false;
}

inline bool Map::Set( GC* gc , const Handle<String>& key , const Value& value ) {
  (void)gc;

  lava_debug(NORMAL,lava_verify(!NeedRehash()););

  std::uint32_t f = Hash(key);
  Entry* entry = FindEntry(key,f,INSERT);
  if(entry) {
    lava_debug(NORMAL,lava_verify(!entry->use););
    entry->use = 1;
    entry->value = value;
    entry->key = key.ref();
    entry->hash = f;
    ++size_;
    ++slot_size_;
    return true;
  }
  return false;
}

inline bool Map::Set( GC* gc , const char* key , const Value& value ) {
  lava_debug(NORMAL,lava_verify(!NeedRehash()););

  std::uint32_t f = Hash(key);
  Entry* entry = FindEntry(key,f,INSERT);
  if(entry) {
    lava_debug(NORMAL,lava_verify(!entry->use););
    entry->use = 1;
    entry->value = value;
    entry->key = String::New(gc,key).ref();
    entry->hash = f;
    ++size_;
    ++slot_size_;
    return true;
  }
  return false;
}

inline bool Map::Set( GC* gc , const std::string& key , const Value& value ) {
  lava_debug(NORMAL,lava_verify(!NeedRehash()););

  std::uint32_t f = Hash(key);
  Entry* entry = FindEntry(key,f,INSERT);
  if(entry) {
    lava_debug(NORMAL,lava_verify(!entry->use););
    entry->use = 1;
    entry->value = value;
    entry->key =  String::New(gc,key).ref();
    entry->hash = f;
    ++size_;
    ++slot_size_;
    return true;
  }
  return false;
}

inline void Map::Put( GC* gc , const Handle<String>& key , const Value& value ) {
  lava_debug(NORMAL,lava_verify(!NeedRehash()););

  std::uint32_t f = Hash(key);
  Entry* entry = FindEntry(key,f,UPDATE);
  if(!entry->use) {
    ++slot_size_;
    ++size_;
  }

  entry->del = 0;
  entry->use = 1;
  entry->value = value;
  entry->key = key.ref();
  entry->hash = f;
}

inline void Map::Put( GC* gc , const char* key , const Value& value ) {
  lava_debug(NORMAL,lava_verify(!NeedRehash()););

  std::uint32_t f = Hash(key);
  Entry* entry = FindEntry(key,f,UPDATE);
  if(!entry->use) {
    ++slot_size_;
    ++size_;
  }

  entry->del = 0;
  entry->use = 1;
  entry->value = value;
  entry->key = (String::New(gc,key)).ref();
  entry->hash = f;
}

inline void Map::Put( GC* gc , const std::string& key , const Value& value ) {
  lava_debug(NORMAL,lava_verify(!NeedRehash()););

  std::uint32_t f = Hash(key);
  Entry* entry = FindEntry(key,f,UPDATE);
  if(!entry->use) {
    ++slot_size_;
    ++size_;
  }

  entry->del = 0;
  entry->use = 1;
  entry->value = value;
  entry->key = (String::New(gc,key)).ref();
  entry->hash = f;
}

inline bool Map::Update( GC* gc , const Handle<String>& key , const Value& value ) {
  if(size_ == 0) return false;

  std::uint32_t f = Hash(key);
  Entry* entry = FindEntry(key,f,FIND);

  if(entry) {
    entry->value = value;
    lava_debug(NORMAL,
        lava_verify( entry->hash == f  );
        lava_verify( *key == **(entry->key) );
      );
    return true;
  }
  return false;
}

inline bool Map::Update( GC* gc , const char* key , const Value& value ) {
  if(size_ == 0) return false;

  std::uint32_t f = Hash(key);
  Entry* entry = FindEntry(key,f,FIND);

  if(entry) {
    entry->value = value;
    lava_debug(NORMAL,
        lava_verify( entry->hash == f  );
        lava_verify( **(entry->key) == key );
      );
    return true;
  }
  return false;
}

inline bool Map::Update( GC* gc , const std::string& key , const Value& value ) {
  if(size_ == 0) return false;

  std::uint32_t f = Hash(key);
  Entry* entry = FindEntry(key,f,FIND);

  if(entry) {
    entry->value = value;
    lava_debug(NORMAL,
        lava_verify( entry->hash == f  );
        lava_verify( **(entry->key) == key );
      );
    return true;
  }
  return false;
}

inline bool Map::Delete( const Handle<String>& key ) {
  if(size_ == 0) return false;

  Entry* entry = FindEntry(key,Hash(key),FIND);
  if(entry) {
    entry->del = 1;
    --size_;
    return true;
  }
  return false;
}

inline bool Map::Delete( const char*  key ) {
  if(size_ == 0) return false;

  Entry* entry = FindEntry(key,Hash(key),FIND);
  if(entry) {
    entry->del = 1;
    --size_;
    return true;
  }
  return false;
}

inline bool Map::Delete( const std::string& key ) {
  if(size_ == 0) return false;

  Entry* entry = FindEntry(key,Hash(key),FIND);
  if(entry) {
    entry->del = 1;
    --size_;
    return true;
  }
  return false;
}

template< typename T > bool Map::Visit( T* visitor ) {
  if(visitor->Begin(this)) {
    for( std::size_t i = 0 ; i < capacity() ; ++i ) {
      Entry* e = data()[i];
      if(e->active()) {
        if(!visitor->VisitString( Handle<String>(e->key)) ||
           !visitor->VisitValue ( e->value ))
          return false;
      }
    }
    return visitor->End(this);
  }
  return false;
}

/* --------------------------------------------------------------------
 * Prototype
 * ------------------------------------------------------------------*/
inline const double* Prototype::real_table() const {
  if(real_table_size_) {
    const char* p = reinterpret_cast<const char*>(this);
    return reinterpret_cast<const double*>(p+sizeof(Prototype));
  } else {
    return NULL;
  }
}

inline double Prototype::GetReal( std::size_t index ) const {
  const double* arr = real_table();
  lava_debug(NORMAL,lava_verify(arr && index < real_table_size_ ););
  return arr[index];
}

inline Handle<String> Prototype::GetString( std::size_t index ) const {
  String*** arr = string_table();
  lava_debug(NORMAL,lava_verify(arr && index < string_table_size_ ););
  return Handle<String>(arr[index]);
}

inline Prototype::SSOTableEntry* Prototype::GetSSO( std::size_t index ) const {
  lava_debug(NORMAL,lava_verify(sso_table_ && index < sso_table_size_););
  return sso_table_ + index;
}

inline const SourceCodeInfo& Prototype::GetSci( std::size_t index ) const {
  lava_debug(NORMAL,lava_verify(index < sci_size()););
  return sci_buffer()[index];
}

inline std::uint8_t Prototype::GetRegOffset( std::size_t index ) const {
  lava_debug(NORMAL,lava_verify(index < reg_offset_size()););
  return reg_offset_table()[index];
}

template< typename T >
bool Prototype::Visit( T* visitor ) {
  if(visitor->Begin(this)) {
    if(!visitor->VisitString(proto_string_)) return false;

    {
      String*** arr = string_table();
      for( std::size_t i = 0 ; i < string_table_size_ ; ++i ) {
        if(!visitor->VisitString(Handle<String>(arr[i])))
          return false;
      }
    }
    return visitor->End(this);
  }
  return false;
}

/* --------------------------------------------------------------------
 * Script
 * ------------------------------------------------------------------*/
inline Script::FunctionTableEntry* Script::fte_array() const {
  char* base = reinterpret_cast<char*>(const_cast<Script*>(this));
  return reinterpret_cast<FunctionTableEntry*>(base + sizeof(Script));
}

inline const Script::FunctionTableEntry&
Script::GetFunction( std::size_t index ) const {
  lava_debug(NORMAL,lava_verify(index < function_table_size_););
  const FunctionTableEntry* e = fte_array();
  return e[index];
}

template< typename T > bool Script::Visit( T* visitor ) {
  if(visitor->Begin(this)) {
    for( std::size_t i = 0 ; i < function_table_size() ; ++i ) {
      const FunctionTableEntry& e = GetFunction(i);
      if(e.name && !visitor->VisitString(e.name)) return false;
      if(!visitor->VisitPrototype(e.prototype)) return false;
    }
    return visitor->End(this);
  }
  return false;
}

} // namespace lavascript
#endif // OBJECTS_H_
