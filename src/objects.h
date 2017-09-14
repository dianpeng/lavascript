#ifndef OBJECTS_H_
#define OBJECTS_H_

#include <cstdint>
#include <cstring>
#include <type_traits>

#include "hash.h"
#include "trace.h"
#include "all-static.h"
#include "heap-object-header.h"

/**
 * Objects are representation of all runtime objects in C++ side and
 * script side. It is designed to be efficient and also friendly to
 * assembly code since we will write interpreter and JIT mostly in the
 * assembly code
 */

namespace lavascript {

namespace gc {
class SSOPool;
} // namespace gc

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


/*
 * A handler is a holder that holds a typped GCRef object. This save
 * us from cosntantly typping whether a certain GCRef is certain type
 * and also gurantee the type safe since it checks the type during
 * Handler creation
 */
template< typename T > class Handler {
 public:
  static_assert( std::is_base_of<HeapObject,T>::value );

  Handler():ref_(NULL) {}
  inline Handler( HeapObject** );
  inline Handler( T** );
  inline Handler( const Handler& );
  inline Handler& operator = ( const Handler& );

  bool IsEmpty() const { return *ref_ != NULL; }
  bool IsRefEmpty() const { return ref_ != NULL; }
  bool IsNull() const { return !IsRefEmpty() && !IsEmpty(); }
  operator bool () const { return !IsNull(); }

 public:
  inline HeapObject** heap_object() const;
  inline T** ref() const;

  inline T* operator -> ();
  inline T* operator -> () const;
  inline T& operator -> () ;
  inline const T& operator -> () const;

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
 */

class Value final {
  union {
    std::uint64_t raw_;
    void* vptr_;
    double real_;
  };

  // For primitive type , we can tell it directly from *raw* value due to the
  // double never use the lower 53 bits.
  // For pointer type , since it only uses 48 bits on x64 platform, we can
  // reconstruct the pointer value from the full 64 bits machine word , but we
  // don't store the heap object type value directly inside of *Value* object
  // but put along with the heap object's common header HeapObject.
  enum {
    TAG_REAL   = 0xfff800000000000 ,                        // Real
    TAG_INTEGER= 0xfff910000000000 ,                        // Integer
    TAG_TRUE   = 0xfff920000000000 ,                        // True
    TAG_FALSE  = 0xfff930000000000 ,                        // False
    TAG_NULL   = 0xfff940000000000 ,                        // Null
    TAG_HEAP   = 0xfffa00000000000                          // Heap
  };

  // Masks
  static const std::uint64_t kPtrCheckMask = 0xffff000000000000;
  static const std::uint64_t kTagMask = 0xffff000000000000;
  static const std::uint64_t kIntMask = 0x00000000ffffffff;
  static const std::uint64_t kPtrMask = 0x0000ffffffffffff;

  // A mask that is used to check whether a pointer is a valid x64 pointer.
  // So not break our assumption. This assumption can be held always,I guess
  static const std::uint64_t kPtrCheckMask = ~kPtrMask;

  std::uint64_t tag() const { return (raw_&kTagMask); }

  bool IsTagReal()    const { return tag() <  TAG_REAL; }
  bool IsTagInteger() const { return tag() == TAG_INTEGER; }
  bool IsTagTrue()    const { return tag() == TAG_TRUE; }
  bool IsTagFalse()   const { return tag() == TAG_FALSE; }
  bool IsTagNull()    const { return tag() == TAG_NULL; }
  bool IsTagHeap()    const { return tag() == TAG_HEAP; }

  inline HeapObject** heap_object() const;
  inline void set_heap_object( HeapObject** );

 public:
  /**
   * Returns a type enumeration for this Value.
   * This function is not really performant , user should always prefer using
   * IsXXX function to test whether a handle is certain type
   */
  ValueType type() const;
  const char* type_name() const;

 public:
  // Primitive types
  bool IsNull() const       { return IsTagNull(); }
  bool IsReal() const       { return IsTagReal(); }
  bool IsInteger() const    { return IsTagInteger(); }
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

  // Getters --------------------------------------------
  inline std::uint32_t GetInteger() const;
  inline bool GetBoolean() const;
  inline double GetReal() const;

  inline HeapObject** GetHeapObject() const;
  inline Handler<String> GetString() const;
  inline Handler<List> GetList() const;
  inline Handler<Slice> GetSlice() const;
  inline Handler<Object> GetObject() const;
  inline Handler<Map> GetMap() const;
  inline Handler<Prototype> GetPrototype() const;
  inline Handler<Closure> GetClosure() const;
  inline Handler<Extension> GetExtension() const;

  template< typename T >
  inline Handler<T> GetHandler() const { return Handler<T>(GetHeapObject()); }

  // Setters ---------------------------------------------
  inline void SetInteger( std::uint32_t );
  inline void SetBoolean( bool );
  inline void SetReal   ( double );
  void SetNull   () { raw_ = TAG_NULL; }
  void SetTrue   () { raw_ = TAG_TRUE; }
  void SetFalse  () { raw_ = TAG_FALSE;}

  inline void SetHeapObject( HeapObject** );
  inline void SetString( const Handler<String>& );
  inline void SetList  ( const Handler<List>& );
  inline void SetSlice ( const Handler<Slice>& );
  inline void SetObject( const Handler<Object>& );
  inline void SetMap   ( const Handler<Map>& );
  inline void SetPrototype( const Handler<Prototype>& );
  inline void SetClosure  ( const Handler<Closure>& );
  inline void SetExtension( const Handler<Extension>& );

  template< typename T >
  void SetHandler( const Handler<T>& h ) { set_heap_object(h.heap_object()); }

 public:
  Value() { SetNull(); }
  inline explicit Value( double v ) { SetReal(v); }
  inline explicit Value( std::int32_t v ) { SetInteger(v); }
  inline explicit Value( bool v ) { SetBoolean(v); }
  inline explicit Value( HeapObject** v ) { SetHeapObject(v); }
  template< typename T >
  explicit Value( const Handler<T>& h ) { SetGCRef(h.ref()); }
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
class HeapObject : DoNotAllocateOnNormalHeap {
 public:
  bool IsString() const { return hoh().type() == VALUE_STRING; }
  bool IsList  () const { return hoh().type() == VALUE_LIST; }
  bool IsSlice () const { return hoh().type() == VALUE_SLICE; }
  bool IsObject() const { return hoh().type() == VALUE_OBJECT; }
  bool IsMap   () const { return hoh().type() == VALUE_MAP; }
  bool IsPrototype() const { return hoh().type() == VALUE_PROTOTYPE; }
  bool IsClosure() const { return hoh().type() == VALUE_CLOSURE; }
  bool IsExtension() const { return hoh().type() == VALUE_EXTENSION; }

  // Generic way to check whether this HeapObject is certain type
  template< typename T > bool IsType() const;

 public:
  HeapObjectHeader::Type* hoh_address() const {
    return reinterpret_cast<HeapObjectHeader::Type*>(
        static_cast<std::uint8_t*>(this) - HeapObjectHeader::kHeapObjectHeaderSize);
  }

  HeapObjectHeader::Type hoh_raw() const {
    return *hoh_address();
  }

  HeapObjectHeader hoh() const {
    return HeapObjectHeader( hoh_raw() );
  }

  void set_hoh( const HeapObjectHeader& word ) {
    *heap_word_address() = word.raw();
  }

  // Helper function for setting the GC state for this HeapObject
  inline void set_gc_state( GCState state );

  virtual ~HeapObject() = 0;
 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(HeapObject);
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
  size_t size() const { return size_; }
  inline void* data() const;

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

 private:
  // size of the data
  size_t size_;

  // hash value of this SSO
  std::uint32_t hash_;

  // memory pool for SSOs
  friend class gc::SSOPool;

  LAVA_DISALLOW_COPY_AND_ASSIGN(SSO);
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
  size_t size;

  LongString() : size(0) {}
  // Return the stored the data inside of the LongString
  inline void* data() const;
};

/**
 * A String object is a normal long string object that resides
 * on top the real heap and it is due to GC. String is an immutable
 * object and once it is allocated we cannot change its memory.
 * The String object directly uses memory that is after the *this*
 * pointer. Its memory layout is like this:
 *
 * ---------------------------------------------
 * |sizeof(String)|  union { LongString ; SSO } |
 * ---------------------------------------------
 *
 * Use HeapObjectHeader to tell whether we store a SSO or just a LongString
 * internally
 *
 */

class String final : public HeapObject {
 public:
  void*  data() const { return sso.data(); }
  size_t size() const { return long_string().data(); }

  const SSO& sso() const;
  const LongString& long_string() const;

  bool IsSSO() const { return hoh().IsSSO(); }
  bool IsLongString() const { return hoh().IsLongString(); }

  // Obviously , we are not null terminated string , but with ToStdString,
  // we are able to gap the String object to real world string with a little
  // bit expensive operations.
  std::string ToStdString() const { return std::string(data(),size()); }

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
  static Handler<String> New( GC* );
  static Handler<String> New( GC* , const char* );
  static Handler<String> New( GC* , const char* , size_t );
  static Handler<String> New( GC* , const std::string& );

  template< typename T , typename DATA >
  bool Visit( const T& , const Data& d );

 private:

  friend class GC;
  LAVA_DISALLOW_COPY_AND_ASSIGN(String);
};

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
  Handler<Slice> slice() const { return slice_ };

  inline bool IsEmpty() const;
  inline size_t size() const;
  inline size_t capacity() const;
  inline const Value& Index( size_t ) const;
  inline Value& Index( size_t );
  Value& operator [] ( size_t index ) { return Index(index); }
  const Value& operator [] ( size_t index ) const { return Index(index); }

  inline Value& Last();
  inline const Value& Last() const;
  inline Value& First();
  inline const Value& First() const;

 public: // Mutator for the list object itself
  void Clear() { size_ = 0; }
  bool Push( GC* , const Value& );
  void Pop ();

 public: // Factory functions
  static Handler<List> New( GC* );
  static Handler<List> New( GC* , size_t capacity );
  static Handler<List> New( GC* , const Handler<Slice>& slice );

  template< typename T , typename DATA >
  bool Visit( const T& , const Data& d );

 private:

  size_t size_;
  Handler<Slice> slice_;

  friend class GC;
  LAVA_DISALLOW_COPY_AND_ASSIGN(List);
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
  bool IsEmpty() const { return capacity() != 0; }
  size_t capacity() const { return capacity_; }
  const Value* data() const;
  Value* data();
  inline Value& Index( size_t );
  inline const Value& Index( size_t ) const;
  Value& operator [] ( size_t index ) { return Index(index); }
  const Value& operator [] ( size_t index ) const { return Index(index); }

 public: // Factory functions
  static Handler<Slice> New( GC* );
  static Handler<Slice> New( GC* , size_t capacity );
  static Handler<Slice> Extend( GC* , const Handler<Slice>& old , size_t new_cap );

  template< typename T , typename DATA >
  bool Visit( const T& , const Data& d );

 private:

  size_t capacity_;

  friend class GC;
  LAVA_DISALLOW_COPY_AND_ASSIGN(Slice);
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
   Handler<Map> map() const { return map_; }
 public:
  inline size_t capacity() const;
  inline size_t size() const;
  inline bool IsEmpty() const;

 public:
  inline bool Get ( const Handler<String>& , Value* ) const;
  inline bool Get ( const char* , Value* ) const;
  inline bool Get ( const std::string& , Value* ) const;

  inline bool Set ( GC* , const Handler<String>& , const Value& );
  inline bool Set ( GC* , const char*   , const Value& );
  inline bool Set ( GC* , const std::string& , const Value& );

  inline bool Update ( GC* , const Handler<String>& , const Value& );
  inline bool Update ( GC* , const char*   , const Value& );
  inline bool Update ( GC* , const std::string& , const Value& );

  inline void Put ( GC* , const Handler<String>& , const Value& );
  inline void Put ( GC* , const char*   , const Value& );
  inline void Put ( GC* , const std::string& , const Value& );

  inline bool Delete ( const Handler<String>& );
  inline bool Delete ( const char*   );
  inline bool Delete ( const std::string& );

 public: // Factory functions
  static Handler<Object> New( GC* );
  static Handler<Object> New( GC* , size_t capacity );
  static Handler<Object> New( GC* , const Handler<Map>& );

  template< typename T , typename DATA >
  bool Visit( const T& , const Data& d );

 private:
  Handler<Map> map_;

  friend class GC;
  LAVA_DISALLOW_COPY_AND_ASSIGN(Object);
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
    Value key;
    Value value;
    std::uint32_t hash;
    std::uint32_t next : 29;
    std::uint32_t more :  1;
    std::uint32_t del  :  1;
    std::uint32_t use  :  1;
  };

  static const size_t kMaximumMapSize = 1<<29;

  // How many slots has been reserved for this Map object
  size_t capacity() const { return capacity_; }

  // How many live objects are inside of the Map
  size_t size() const { return size_; }

  // How many slots has been occupied , since deletion is marked with
  // tombstone
  size_t slot_size() const { return slot_size_; }

  // Do we need to do rehashing now
  bool NeedRehash() const { return slot_size() == capacity(); }

  // Is the map empty
  bool IsEmpty() const { return size() == 0; }

  inline Entry* data();
  inline const Entry* data() const;

 public: // Mutators
  inline bool Get ( const Handler<String>& , Value* ) const;
  inline bool Get ( const char*, Value* ) const;
  inline bool Get ( const std::string& , Value* ) const;

  inline bool Set ( GC* , const Handler<String>& , const Value& );
  inline bool Set ( GC* , const char*   , const Value& );
  inline bool Set ( GC* , const std::string& , const Value& );

  inline bool Update ( GC* , const Handler<String>& , const Value& );
  inline bool Update ( GC* , const char*   , const Value& );
  inline bool Update ( GC* , const std::string& , const Value& );

  inline void Put ( GC* , const Handler<String>& , const Value& );
  inline void Put ( GC* , const char*   , const Value& );
  inline void Put ( GC* , const std::string& , const Value& );

  inline bool Delete ( const Handler<String>& );
  inline bool Delete ( const char*   );
  inline bool Delete ( const std::string& );

 public:
  inline static std::uint32_t Hash( const char* );
  inline static std::uint32_t Hash( const std::string& );
  inline static std::uint32_t Hash( const Handler<String>& );

 public: // Factory functions
  static Handler<Map> New( GC* );
  static Handler<Map> New( GC* , size_t capacity );
  static Handler<Map> Rehash( GC* , const Handler<Map>& , size_t );

  template< typename T , typename DATA >
  bool Visit( const T& , const Data& d );

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

  template< typename T >
  Entry* FindEntry( const T& , std::uint32_t , Option );

 private:

  size_t capacity_;
  size_t size_;
  size_t slot_size_;

  friend class GC;
  LAVA_DISALLOW_COPY_AND_ASSIGN(Map);
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
  virtual bool Deref( Value* , Value* ) const = 0;

  /**
   * Move the iterator to the next available slots
   * if this operation can be performed return true;
   * otherwise return false
   */
  virtual bool Move() = 0;

 public:
  virtual ~Iterator() {}

  friend class GC;

  LAVA_DISALLOW_COPY_AND_ASSIGN(Iterator);
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
  // const vm::Bytecode& bytecode() const { return bytecode_; }
  // const vm::ConstantTable& constant_table() const { return constant_table_; }
  // const vm::UpValueIndexArray& upvalue_array() const { return upvalue_array_; }
  Handle<String> proto_string() const { return proto_string_; }
  size_t argument_size() const { return argument_size_; }

 public: // Mutator
  // vm::Bytecode& bytecode() { return bytecode_; }
  // vm::ConstantTable& constant_table() { return constant_table_; }
  // vm::UpValueIndexArray& upvalue_array() { return upvalue_array_; }
  void set_proto_string( const Handle<String>& str ) { proto_string_ = str; }
  void set_argument_size( size_t arg) { argument_size_ = arg;}

  template< typename T , typename DATA >
  bool Visit( const T& , const Data& d );

 private:
  // vm::BytecodeArray code_;
  // vm::ConstantTable const_table_;
  // vm::UpValueIndexArray upvalue_array_;
  Handle<String> proto_string_;
  size_t argument_size_;

  friend class GC;

  LAVA_DISALLOW_COPY_AND_ASSIGN(Prototype);
};

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
  GCRef  prototype() const { return prototype_; }
  inline Value* upvalue();
  inline const Value* upvalue() const;

  template< typename T , typename DATA >
  bool Visit( const T& , const Data& d );

 private:

  GCRef prototype_;

  friend class GC;
  LAVA_DISALLOW_COPY_AND_ASSIGN(Closure);
};

/**
 * Extension is the only interfaces that user can be used to extend the whole world
 * of lavascript. An extension is a specialized HeapObject that is used to customization.
 */
class Extension : public HeapObject {
 public:
};



/* =================================================================================
 *
 * Inline functions definitions
 *
 * ================================================================================*/

/* --------------------------------------------------------------------
 * Handler
 * ------------------------------------------------------------------*/

template< typename T > inline Handler<T>::Handler( HeapObject** ref ) {
#ifdef LAVASCRIPT_CHECK_OBJECTS
  lava_verify(ref && *ref);
  lava_verify(*ref->Is<T>());
#endif // LAVASCRIPT_CHECK_OBJECTS
  ref_ = reinterpret_cast<T**>(ref);
}

template< typename T > inline Handler<T>::Handler( T** ref ):
  ref_(ref)
{}

template< typename T > inline Handler<T>::Handler( const Handler& that ):
  ref_(that.ref_)
{}

template< typename T >
inline Handler<T>& Handler<T>::operator = ( const Handler<T>& that ) {
  ref_ = that.ref_;
  return *this;
}

template< typename T >
inline T* Handler<T>::operator -> () {
#ifdef LAVASCRIPT_CHECK_OBJECTS
  lava_verify(!IsNull());
#endif // LAVASCRIPT_CHECK_OBJECTS
  return *ref_;
}

template< typename T >
inline const T* Handler<T>::operator -> () const {
#ifdef LAVASCRIPT_CHECK_OBJECTS
  lava_verify(!IsNull());
#endif // LAVASCRIPT_CHECK_OBJECTS
  return *ref_;
}

template< typename T >
inline T& Handler<T>::operator * () {
#ifdef LAVASCRIPT_CHECK_OBJECTS
  lava_verify(!IsNull());
#endif // LAVASCRIPT_CHECK_OBJECTS
  return **ref_;
}

template< typename T >
inline const T& Handler<T>::operator * () const {
#ifdef LAVASCRIPT_CHECK_OBJECTS
  lava_verify(!IsNull());
#endif // LAVASCRIPT_CHECK_OBJECTS
  return **ref_;
}

/* --------------------------------------------------------------------
 * Value
 * ------------------------------------------------------------------*/

inline HeapObject** Value::heap_object() const {
  return reinterpret_cast<HeapObject**>(raw_ & kPtrMask);
}

inline void Value::set_heap_object( HeapObject** ptr ) {

#ifdef LAVASCRIPT_CHECK_OBJECTS
  /**
   * Checking whether the input pointer is valid pointer
   * with our assumptions
   */
  lava_assert( (static_cast<std::uintptr_t>(ptr)&kPtrCheckMask) == 0 ,
               "the pointer %x specified here is not a valid pointer,"
               " upper 16 bits is not 0s", ptr );
#endif // LAVASCRIPT_CHECK_OBJECTS

  raw_ = static_cast<std::uint64_t>(
      static_cast<std::uintptr_t>(ptr) | tag);
}

inline ValueType Value::type() const {
  if(IsTagHeap()) {
    return heap_object()->type();
  } else if(IsTagTrue() || IsTagFalse()) {
    return VALUE_BOOLEAN;
  } else if(IsTagNull()) {
    return VALUE_NULL;
  } else if(IsTagInteger()) {
    return VALUE_INTEGER;
  } else {
    return VALUE_REAL;
  }
}

inline const char* Value::type_name() const {
  return GetValueTypeName( type() );
}

inline bool Value::IsString() const {
  return IsHeapObject() && (*heap_object())->IsString();
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

inline std::uint32_t Value::GetInteger() const {
#ifdef LAVASCRIPT_CHECK_OBJECTS
  lava_verify(IsInteger());
#endif // LAVASCRIPT_CHECK_OBJECTS

  /**
   * This can be optimized out by defineing a structure which is
   * properly aligned on this 64 bits machine word with things like:
   * struct {
   *   int32_t high;
   *   int32_t low;
   * };
   *
   * Then based on endianess you can return low or high accordingly
   *
   * Here we just use a mask , hopefully compiler will optimize it out
   * for case like Value is in register than we know that we have high
   * and low alias for this register instead of wasting cycle to do the
   * bitmask
   */
  return static_cast<std::uint32_t>(raw_ & kIntMask);
}

inline bool Value::GetBoolean() const {
#ifdef LAVASCRIPT_CHECK_OBJECTS
  lava_verify(IsBoolean());
#endif // LAVASCRIPT_CHECK_OBJECTS
  return IsTagTrue();
}


inline double Value::GetReal() const {
#ifdef LAVASCRIPT_CHECK_OBJECTS
  lava_verify(IsReal());
#endif // LAVASCRIPT_CHECK_OBJECTS
  return real_;
}

inline HeapObject** Value::GetHeapObject() const {
#define LAVASCRIPT_CHECK_OBJECTS
  lava_verify(IsHeapObject());
#endif // LAVASCRIPT_CHECK_OBJECTS
  return heap_object();
}

inline Handler<String> Value::GetString() const {
#define LAVASCRIPT_CHECK_OBJECTS
  lava_verify(IsHeapObject());
#endif // LAVASCRIPT_CHECK_OBJECTS
  return Handler<String>(heap_object());
}

inline Handler<List> Value::GetList() const {
#define LAVASCRIPT_CHECK_OBJECTS
  lava_verify(IsHeapObject());
#endif // LAVASCRIPT_CHECK_OBJECTS
  return Handler<List>(heap_object());
}

inline Handler<Slice> Value::GetSlice() const {
#define LAVASCRIPT_CHECK_OBJECTS
  lava_verify(IsHeapObject());
#endif // LAVASCRIPT_CHECK_OBJECTS
  return Handler<Slice>(heap_object());
}

inline Handler<Object> Value::GetObject() const {
#define LAVASCRIPT_CHECK_OBJECTS
  lava_verify(IsHeapObject());
#endif // LAVASCRIPT_CHECK_OBJECTS
  return Handler<Object>(heap_object());
}

inline Handler<Map> Value::GetMap() const {
#define LAVASCRIPT_CHECK_OBJECTS
  lava_verify(IsHeapObject());
#endif // LAVASCRIPT_CHECK_OBJECTS
  return Handler<Map>(heap_object());
}

inline Handler<Prototype> Value::GetPrototype() const {
#define LAVASCRIPT_CHECK_OBJECTS
  lava_verify(IsHeapObject());
#endif // LAVASCRIPT_CHECK_OBJECTS
  return Handler<Prototype>(heap_object());
}

inline Handler<Closure> Value::GetClosure() const {
#define LAVASCRIPT_CHECK_OBJECTS
  lava_verify(IsHeapObject());
#endif // LAVASCRIPT_CHECK_OBJECTS
  return Handler<Closure>(heap_object());
}

inline Handler<Extension> Value::GetExtension() const {
#define LAVASCRIPT_CHECK_OBJECTS
  lava_verify(IsHeapObject());
#endif // LAVASCRIPT_CHECK_OBJECTS
  return Handler<Extension>(heap_object());
}

inline void Value::SetInteger( std::uint32_t value ) {
  raw_ = value | TAG_INTEGER;
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

inline void Value::SetHeapObject( HeapObject** ptr ) {
  set_pointer(reinterpret_cast<void*>(ptr),TAG_HEAP);
}

inline void Value::SetString( const Handler<String>& str ) {
  SetHeapObject(str.heap_object());
}

inline void Value::SetList( const Handler<List>& list ) {
  SetHeapObject(list.heap_object());
}

inline void Value::SetSlice( const Handler<Slice>& slice ) {
  SetHeapObject(slice.heap_object());
}

inline void Value::SetObject( const Handler<Object>& object ) {
  SetHeapObject(object.heap_object());
}

inline void Value::SetMap( const Handler<Map>& map ) {
  SetHeapObject(map.heap_object());
}

inline void Value::SetPrototype( const Handler<Prototype>& proto ) {
  SetHeapObject(proto.heap_object());
}

inline void Value::SetClosure( const Handler<Closure>& closure ) {
  SetHeapObject(closure.heap_object());
}

inline void Value::SetExtension( const Handler<Extension>& ext ) {
  SetHeapObject(set.heap_object());
}

/* --------------------------------------------------------------------
 * HeapObject
 * ------------------------------------------------------------------*/

template< typename T >
bool HeapObject::IsType<T>() const {
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
  else if(std;:is_same<T,Prototype>::value)
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

inline void* SSO::data() const {
  SSO* self = const_cast<SSO*>(this);
  return reinterpret_cast<void*>(
      static_cast<char*>(self) + sizeof(SSO));
}

inline bool SSO::operator == ( const char* str ) const {
  const size_t l = strlen(str);
  const size_t len = std::min(l,size_);
  int r = std::memcmp(data(),str,len);
  return r == 0 ? (len == l) : false;
}

inline bool SSO::operator == ( const std::string& str ) const {
  const size_t len = std::min(size_,str.size());
  int r = std::memcmp(data(),str.c_str(),len);
  return r == 0 ? (size_ == len) : false;
}

inline bool SSO::operator == ( const String& str ) const {
  const size_t len = std::min(size_,str.size());
  int r = std::memcmp(data(),str.data(),len);
  return r == 0 ? (size_ == len) : false;
}

inline bool SSO::operator == ( const SSO& str ) const {
  return this == &str;
}

inline bool SSO::operator != ( const char* str ) const {
  const size_t l = strlen(str);
  const size_t len = std::min(size_,l);
  int r = std::memcmp(data(),str,len);
  return r == 0 ? (size_ != len) : true;
}

inline bool SSO::operator != ( const std::string& str ) const {
  const size_t len = std::min(size_,str.size());
  int r = std::memcmp(data(),str.c_str(),len);
  return r == 0 ? (size_ != len) : true;
}

inline bool SSO::operator != ( const String& str ) const {
  const size_t len = std::min(size_,str.size());
  int r = std::memcmp(data(),str.data(),len);
  return r == 0 ? (size_ != len ) : true;
}

inline bool SSO::operator != ( const SSO& str ) const {
  const size_t len = std::min(size_,str.size());
  int r = std::memcmp(data(),str.data(),len);
  return r == 0 ? (size_ != len) : true;
}

inline bool SSO::operator >  ( const char* str ) const {
  const size_t l = strlen(str);
  const size_t len = std::min(size_,l);
  int r = std::memcmp(data(),str.data(),len);
  if(r > 0 || (r == 0 && size_ > len))
    return true;
  else
    return false;
}

inline bool SSO::operator > ( const std::string& str ) const {
  const size_t len = std::min(size_,str.size());
  int r = std::memcmp(data(),str.c_str(),len);
  if(r > 0 || (r == 0 && size_ > len))
    return true;
  else
    return false;
}

inline bool SSO::operator > ( const String& str ) const {
  const size_t len = std::min(size_,str.size());
  int r = std::memcmp(data(),str.c_str(),len);
  if(r > 0 || (r == 0 && size_ > len))
    return true;
  else
    return false;
}

inline bool SSO::operator > ( const SSO& str ) const {
  const size_t len = std::min(size_,str.size());
  int r = std::memcmp(data(),str.data(),len);
  if(r > 0 || (r == 0 && size_ > len))
    return true;
  else
    return false;
}

inline bool SSO::operator >= ( const char* str ) const {
  const size_t l = strlen(str);
  const size_t len = std::min(size_,l);
  int r = std::memcmp(data(),str,len);
  if(r > 0 || (r == 0 && size_ >= len))
    return true;
  else
    return false;
}

inline bool SSO::operator >= ( const std::string& str ) const {
  const size_t len = std::min(size_,str.size());
  int r = std::memcmp(data(),str.c_str(),len);
  if(r > 0 || (r == 0 && size_ >= len))
    return true;
  else
    return false;
}

inline bool SSO::operator >= ( const String& str ) const {
  const size_t len = std::min(size_,str.size());
  int r = std::memcmp(data(),str.data(),len);
  if(r > 0 || (r == 0 && size_ >= len))
    return true;
  else
    return false;
}

inline bool SSO::operator >= ( const SSO& str ) const {
  const size_t len = std::min(size_,str.size());
  int r = std::memcmp(data(),str.data(),len);
  if(r > 0 || (r == 0 && size_ >= len ))
    return true;
  else
    return false;
}

inline bool SSO::operator < ( const char* str ) const {
  const size_t l = strlen(str);
  const size_t len = std::min(size_,l);
  int r = std::memcmp(data(),str,len);
  if(r < 0 || (r == 0 && size_ < l))
    return true;
  else
    return false;
}

inline bool SSO::operator < ( const std::string& str ) const {
  const size_t len = std::min(size_,str.size());
  int r = std::memcmp(data(),str.c_str(),len);
  if(r < 0 || (r == 0 && size_ < str.size()))
    return true;
  else
    return false;
}

inline bool SSO::operator < ( const String& str ) const {
  const size_t len = std::min(size_,str.size());
  int r = std::memcmp(data(),str.c_str(),len);
  if(r < 0 || (r == 0 && size_ < str.size()))
    return true;
  else
    return false;
}

inline bool SSO::operator < ( const SSO& str ) const {
  const size_t len = std::min(size_,str.size());
  int r = std::memcmp(data(),str.data(),len);
  if(r < 0 || (r == 0 && size_ < str.size()))
    return true;
  else
    return false;
}

inline bool SSO::operator <= ( const char* str ) const {
  const size_t l = strlen(str);
  const size_t len = std::min(size_,l);
  int r = std::memcmp(data(),str,len);
  if(r < 0 || (r == 0 && size_ <= l))
    return true;
  else
    return false;
}

inline bool SSO::operator <= ( const std::string& str ) const {
  const size_t len = std::min(size_,str.size());
  int r = std::memcmp(data(),str.c_str(),len);
  if(r < 0 || (r == 0 && size_ <= str.size()))
    return true;
  else
    return false;
}

inline bool SSO::operator <= ( const String& str ) const {
  const size_t len = std::min(size_,str.size());
  int r = std:memcmp(data(),str.c_str(),len);
  if(r < 0 || (r == 0 && size_ <= str.size()))
    return true;
  else
    return false;
}

inline bool SSO::operator <= ( const SSO& str ) const {
  const size_t len = std::min(size_,str.size());
  int r = std::memcmp(data(),str.size(),len);
  if(r < 0 || (r == 0 && size_ <= str.size()))
    return true;
  else
    return false;
}

/* --------------------------------------------------------------------
 * String
 * ------------------------------------------------------------------*/

inline const SSO& String::sso() const {
#ifdef LAVASCRIPT_CHECK_OBJECTS
  lava_verify(IsSSO());
#endif // LAVASCRIPT_CHECK_OBJECTS

  // The SSO are actually stored inside of SSOPool.
  // For a String object, it only stores a pointer
  // to the actual SSO object
  return **reinterpret_cast<const SSO**>(this);
}

inline const LongString& String::long_string() const {
#ifdef LAVASCRIPT_CHECK_OBJECTS
  lava_verify(IsLongString());
#endif // LAVASCRIPT_CHECK_OBJECTS
  return *reinterpret_cast<const LongString*>(this);
}

inline bool String::operator == ( const char* str ) const {
  const size_t l = strlen(str);
  const std::size_t len = std::min(size(),l);
  int r = std::memcmp(data(),str,len);
  return r == 0 ? size() == len : false;
}

inline bool String::operator == ( const std::string& str ) const {
  const std::size_t len = std::min(size(),str.size());
  int r = std::memcmp(data(),str.c_str(),len);
  return r == 0 ? size() == len : false;
}

inline bool String::operator == ( const String& str ) const {
  const std::size_t len = std::min(size(),str.size());
  int r = std::memcmp(data(),str.data(),len);
  return r == 0 ? size() == len : false;
}

inline bool String::operator == ( const SSO& str ) const {
  if(IsSSO()) {
    return str == sso();
  } else {
    const LongString& lstr = long_string();
    const std::size_t len = std::min(lstr.size,str.size());
    int r = std::memcmp(lstr.data(),str.data(),len);
    return r == 0 ? lstr.size == len : false;
  }
}

inline bool String::operator != ( const char* str ) const {
  const std::sizez_t l = strlen(str);
  const std::size_t len = std::min(size(),l);
  int r = std::memcmp(data(),str,len);
  return r == 0 ? size() != len : true;
}

inline bool String::operator != ( const std::string& str ) const {
  const std::size_t len = std::min(size(),str.size());
  int r = std::memcmp(data(),str.c_str(),len);
  return r == 0 ? size() != len : true;
}

inline bool String::operator != ( const String& str ) const {
  const std::size_t len = std::min(size(),str.size());
  int r = std::memcmp(data(),str.data(),len);
  return r == 0 ? size() != len : true;
}

inline bool String::operator != ( const SSO& str ) const {
  if(IsSSO()) {
    return str != sso();
  } else {
    const LongString& lstr = long_string();
    const std::size_t len = std::min(lstr.size,str.size());
    int r = std::memcmp(lstr.data(),str.data(),len);
    return r == 0 ? lstr.size != len : true;
  }
}

inline bool String::operator > ( const char* str ) const {
  const size_t l = strlen(str);
  const std::size_t len = std::min(size(),l);
  int r = std::memcmp(data(),str,len);
  if(r > 0 || (r == 0 && size() > len))
    return true;
  else
    return false;
}

inline bool String::operator > ( const std::string& str ) const {
  const std::size_t len = std::min(size(),str.size());
  int r = std::memcmp(data(),str.c_str(),len);
  return (r > 0 ||(r == 0 && size() > len));
}

inline bool String::operator > ( const String& str ) const {
  const std::size_t len = std::min(size(),str.size());
  int r = std::memcmp(data(),str.data(),len);
  return (r > 0 ||(r == 0 && size() > len));
}

inline bool String::operator > ( const SSO& str ) const {
  const std::size_t len = std::min(size(),str.size());
  int r = std::memcmp(data(),str.data(),len);
  return (r > 0 ||(r == 0 && size() > len));
}

inline bool String::operator >= ( const char* str ) const {
  const std::size_t l = strlen(str);
  const std::size_t len = std::min(size(),l);
  int r = std::memcmp(data(),str.data(),len);
  return (r > 0 ||(r == 0 && size() >= len));
}

inline bool String::operator >= ( const std::string& str ) const {
  const std::size_t len = std::min(size(),str.size());
  int r = std::memcmp(data(),str.c_str(),len);
  return (r > 0 ||(r == 0 && size() >= len));
}

inline bool String::operator >= ( const String& str ) const {
  const std::size_t len = std::min(size(),str.size());
  int r = std::memcmp(data(),str.data(),len);
  return (r > 0 ||(r == 0 && size() >= len));
}

inline bool String::operator >= ( const SSO& str ) const {
  const std::size_t len = std::min(size(),str.size());
  int r = std::memcmp(data(),str.data(),len);
  return (r > 0 ||(r == 0 && size() >= len));
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
  return (r < 0 || (r == 0 && size() < str.size()));
}

inline bool String::operator <= ( const SSO& str ) const {
  const std::size_t len = std::min(size(),str.size());
  int r = std::memcmp(data(),str.data(),len);
  return (r < 0 || (r == 0 && size() < str.size()));
}

/* --------------------------------------------------------------------
 * List
 * ------------------------------------------------------------------*/

inline bool List::IsEmpty() const {
  return slice()->IsEmpty();
}

inline bool List::size() const {
  return slice()->size();
}

inline bool List::capacity() const {
  return slice()->capacity();
}

inline const Value& List::Index( size_t index ) const {
  return slice()->Index(index);
}

inline Value& List::Index( size_t index ) {
  return slice()->Index(index);
}

inline Value& List::Last() {
#ifdef LAVASCRIPT_CHECK_OBJECTS
  lava_verify(size() >0);
#endif // LAVASCRIPT_CHECK_OBJECTS
  return slice()->Index( size() - 1 );
}

inline Value& List::Last() const {
#ifdef LAVASCRIPT_CHECK_OBJECTS
  lava_verify(size() >0);
#endif // LAVASCRIPT_CHECK_OBJECTS
  return slice()->Index( size() - 1 );
}

inline Value& List::First() {
#ifdef LAVASCRIPT_CHECK_OBJECTS
  lava_verify(size() >0);
#endif // LAVASCRIPT_CHECK_OBJECTS
  return slice()->Index(0);
}

inline Value& List::First() const {
#ifdef LAVASCRIPT_CHECK_OBJECTS
  lava_verify(size() >0);
#endif // LAVASCRIPT_CHECK_OBJECTS
  return slice()->Index(0);
}

inline bool List::Push( GC* gc , const Value& value ) {
  if(size_ == slice_->capacity()) {
    // We run out of memory for this slice , just dump it
    slice_ = Slice::Extend(gc,slice_,2*size_);

    // We cannot allocate a larger Slice , return false directly
    if(!slice_) return false;
  }

  slice_->Index(size_) = value;
  ++size_;
  return true;
}

inline void List::Pop() {
#ifdef LAVASCRIPT_CHECK_OBJECTS
  lava_verify(size_ > 0);
#endif // LAVASCRIPT_CHECK_OBJECTS
  --size_;
}


/* --------------------------------------------------------------------
 * Slice
 * ------------------------------------------------------------------*/

inline void* Slice::array() const {
  return reinterpret_cast<void*>(
      static_cast<char*>(this) + sizeof(Slice));
}

inline Value* Slice::data() {
  return static_cast<Value*>(array());
}

inline const Value* Slice::data() const {
  return static_cast<const Value*>(array());
}

inline const Value& Slice::Index( size_t index ) const {
  return data()[index];
}

inline Value& Slice::Index( size_t index ) {
  return data()[index];
}


/* --------------------------------------------------------------------
 * Slice
 * ------------------------------------------------------------------*/

inline size_t Object::capacity() const {
  return map()->capacity();
}

inline size_t Object::size() const {
  return map()->size();
}

inline size_t Object::IsEmpty() const {
  return map()->IsEmpty();
}

inline bool Object::Get( const String& key , Value* output ) const {
  return map()->Get(key,output);
}

inline bool Object::Get( const char* key , Value* output ) const {
  return map()->Get(key,output);
}

inline bool Object::Get( const std::string& key , Value* output ) const {
  return map()->Get(key,output);
}

inline bool Object::Set( GC* gc , const String& key ,
    const Value& val ) {
  if(map_->NeedRehash()) {
    map_ = Map::Rehash(gc,map_);
    if(!map_) return false;
  }

  return map_->Set(gc,key,val);
}

inline bool Object::Set( GC* gc , const char* key ,
    const Value& val ) {
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

inline bool Object::Update( GC* gc , const String& key ,
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

inline bool Object::Put( GC* gc , const String& key ,
    const Value& val ) {
  if(map_->NeedRehash()) {
    map_ = Map::Rehash(gc,map_);
    if(!map_) return false;
  }
  return map_->Put(gc,key,val);
}

inline bool Object::Put( GC* gc , const char* key ,
    const Value& val ) {
  if(map_->NeedRehash()) {
    map_ = Map::Rehash(gc,map_);
    if(!map_) return false;
  }
  return map_->Put(gc,key,val);
}

inline bool Object::Put( GC* gc , const std::string& key,
    const Value& val ) {
  if(map_->NeedRehash()) {
    map_ = Map::Rehash(gc,map_);
    if(!map_) return false;
  }
  return map_->Put(gc,key,val);
}

inline bool Object::Delete( const String& key ) {
  return map_->Delete(key);
}

inline bool Object::Delete( const char* key ) {
  return map_->Delete(key);
}

inline bool Object::Delete( const std::string& key ) {
  return map_->Delete(key);
}

/* --------------------------------------------------------------------
 * Map
 * ------------------------------------------------------------------*/

inline std::uint32_t Map::Hash( const Handle<String>& key ) {
  if(key.IsSSO()) {
    return key.sso().hash();
  } else {
    return Hasher::Hash(key.long_string().data(),key.long_string().size());
  }
}

inline std::uint32_t Map::Hash( const char* key ) {
  return Hasher::Hash(key);
}

inline std::uint32_t Map::Hash( const std::string& key ) {
  return Hasher::Hash(key.c_str(),key.size());
}

template< typename T >
Entry* Map::FindEntry<T>( const T& key , std::uint32_t fullhash ,
                                         Option opt ) {
  int main_position = fullhash & (capacity()-1);
  Entry* main = entry()[main_position];
  if(!main->use) return opt == FIND ? NULL : main;

  // Okay the main entry is been used or at least it is a on chain of the
  // collision list. So we need to chase down the link to see whats happening
  Entry* cur = main;
  do {
    if(!cur->del) {
      // The current entry is not deleted, so we can try check wether it is a
      // matched key or not
#ifdef LAVASCRIPT_CHECK_OBJECTS
      lava_verify( cur->key.IsString() );
#endif // LAVASCRIPT_CHECK_OBJECTS

      if(cur->hash == fullhash && *cur->key.GetString() == key) {
        return opt == INSERT ? NULL : cur;
      }
    }
    if(cur->more)
      cur = entry()[cur->next];
    else
      break;
  } while(true);

  if(opt == FIND) return NULL;

  // linear probing to find the next available new_slot
  Entry* new_slot = NULL;
  std::uint32_t h = fullhash;
  while( (new_slot = entry()[ ++h &(capacity()-1)])->use )
    ;

  // linked the previous Entry to the current found one
  cur->next = (new_slot - entry());

  return new_slot;
}

inline void* Map::array() const {
  return reinterpret_cast<void*>(
      static_cast<char*>(this) + sizeof(Map));
}

inline Entry* Map::data() { return static_cast<Entry*>(array()); }

inline const Entry* Map::data() const {
  return static_cast<const Entry*>(array());
}

inline bool Map::Get( const Handler<String>& key , Value* output ) const {
  Entry* entry = FindEntry(key,Hash(key),FIND);
  if(entry) {
    *output = entry->value;
    return true;
  }
  return false;
}

inline bool Map::Get( const char* key , Value* output ) const {
  Entry* entry = FindEntry(key,Hash(key),FIND);
  if(entry) {
    *output = entry->value;
    return true;
  }
  return false;
}

inline bool Map::Get( const std::string& key , Value* output ) const {
  Entry* entry = FindEntry(key,Hash(key),FIND);
  if(entry) {
    *output = entry->Value;
    return true;
  }
  return false;
}

inline bool Map::Set( GC* gc , const Handler<String>& key , const Value& value ) {
  (void)gc;

#ifdef LAVASCRIPT_CHECK_OBJECTS
  lava_verify(!NeedRehash());
#endif // LAVASCRIPT_CHECK_OBJECTS

  std::uint32_t f = Hash(key);
  Entry* entry = FindEntry(key,f,INSERT);
  if(entry) {
#ifdef LAVASCRIPT_CHECK_OBJECTS
    lava_verify(!entry->use);
#endif // LAVASCRIPT_CHECK_OBJECTS
    entry->use = 1;
    entry->value = value;
    entry->key.SetHandler(key);
    entry->hash = h;
    ++size_;
    ++slot_size_;
    return true;
  }
  return false;
}

inline bool Map::Set( GC* gc , const char* key , const Value& value ) {

#ifdef LAVASCRIPT_CHECK_OBJECTS
  lava_verify(!NeedRehash());
#endif // LAVASCRIPT_CHECK_OBJECTS

  std::uint32_t f = Hash(key);
  Entry* entry = FindEntry(key,f,INSERT);
  if(entry) {
#ifdef LAVASCRIPT_CHECK_OBJECTS
    lava_verify(!entry->use);
#endif // LAVASCRIPT_CHECK_OBJECTS
    entry->use = 1;
    entry->value = value;
    entry->key.SetString( String::New(gc,key) );
    entry->hash = h;
    ++size_;
    ++slot_size_;
    return true;
  }
  return false;
}

inline bool Map::Set( GC* gc , const std::string& key , const Value& value ) {

#ifdef LAVASCRIPT_CHECK_OBJECTS
  lava_verify(!NeedRehash());
#endif // LAVASCRIPT_CHECK_OBJECTS

  std::uint32_t f = Hash(key);
  Entry* entry = FindEntry(key,f,INSERT);
  if(entry) {
#ifdef LAVASCRIPT_CHECK_OBJECTS
    lava_verify(!entry->use);
#endif // LAVASCRIPT_CHECK_OBJECTS
    entry->use = 1;
    entry->value = value;
    entry->key.SetString( String::New(gc,key) );
    entry->hash = h;
    ++size_;
    ++slot_size_;
    return true;
  }
  return false;
}

inline void Map::Put( GC* gc , const Handler<String>& key , const Value& value ) {

#ifdef LAVASCRIPT_CHECK_OBJECTS
  lava_verify(!NeedRehash());
#endif // LAVASCRIPT_CHECK_OBJECTS

  std::uint32_t f = Hash(key);
  Entry* entry = FindEntry(key,f,UPDATE);

  entry->del = 0;
  entry->use = 1;
  entry->value = value;
  entry->key.SetString(key);
  entry->hash = h;
  ++size_;
  ++slot_size_;
}

inline void Map::Put( GC* gc , const char* key , const Value& value ) {

#ifdef LAVASCRIPT_CHECK_OBJECTS
  lava_verify(!NeedRehash());
#endif // LAVASCRIPT_CHECK_OBJECTS

  std::uint32_t f = Hash(key);
  Entry* entry = FindEntry(key,f,UPDATE);

  entry->del = 0;
  entry->use = 1;
  entry->value = value;
  entry->key.SetString(String::New(gc,key));
  entry->hash = h;
  ++size_;
  ++slot_size_;
}

inline void Map::Put( GC* gc , const std::string& key , const Value& value ) {

#ifdef LAVASCRIPT_CHECK_OBJECTS
  lava_verify(!NeedRehash());
#endif // LAVASCRIPT_CHECK_OBJECTS

  std::uint32_t f = Hash(key);
  Entry* entry = FindEntry(key,f,UPDATE);

  entry->del = 0;
  entry->use = 1;
  entry->value = value;
  entry->key.SetString(String::New(gc,key));
  entry->hash = h;
  ++size_;
  ++slot_size_;
}

inline bool Map::Delete( const Handler<String>& key ) {

  Entry* entry = FindEntry(key,Hash(key),FIND);
  if(entry) {
    entry->del = 1;
    --size_;
    return true;
  }
  return false;
}

inline bool Map::Delete( const char*  key ) {
  Entry* entry = FindEntry(key,Hash(key),FIND);
  if(entry) {
    entry->del = 1;
    --size_;
    return true;
  }
  return false;
}

inline bool Map::Delete( const std::string& key ) {
  Entry* entry = FindEntry(key,Hash(key),FIND);
  if(entry) {
    entry->del = 1;
    --size_;
    return true;
  }
  return false;
}

} // namespace lavascript
#endif // OBJECTS_H_
