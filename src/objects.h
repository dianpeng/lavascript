#ifndef OBJECTS_H_
#define OBJECTS_H_

#include <cstdint>
#include <cstring>

#include "vm/bytecode.h"
#include "vm/constant-table.h"
#include "core/trace.h"
#include "all-static.h"
#include "heap-object-header.h"

/**
 * Objects are representation of all runtime objects in C++ side and
 * script side. It is designed to be efficient and also friendly to
 * assembly code since we will write interpreter and JIT mostly in the
 * assembly code
 */

namespace lavascript {

class GC;
class Value;
class SSO;
class SSOPool;
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


/**
 *
 * A GCRef is just a pointer to an HeapObject pointer. A GCRef is a level
 * of indirection which allow us to do compaction. A GCRef will be traced
 * on all C++ side object traced by LocalScope object.
 *
 * An object will be reclaimed if 1) No script side object points to it, and
 * 2) no GCRef points to it. Since GCRef is traced by our runtime, so we know
 * where C++ code are still using certain HeapObject
 */
class GCRef {
 public:
  typedef HeapObject** RefType;

  GCRef():ref_(NULL) {}
  GCRef( RefType value ):ref_(value) {}
  GCRef( const GCRef& ref ):ref_(ref.ref_) {}
  GCRef& operator = ( const GCRef& ref ) {
    if(this != &ref) {
      ref_ = ref.ref_;
    }
    return *this;
  }

 public:
  bool IsEmpty() const { lava_verify(!IsRefEmpty()); return *ref_ == NULL; }
  bool IsRefEmpty() const { return ref_ == NULL; }
  bool IsNull() const { return ref_ == NULL || *ref_ == NULL; }
  HeapObject* heap_object() { lava_verify(!IsRefEmpty()); return *ref_; }
  RefType ref() const { return ref_; }

  HeapObject* operator * () { return heap_object(); }
  const HeapObject* operator * () const { return heap_object(); }

  RefType operator -> () { return ref_; }
  const RefType operator -> () const { return ref_; }

 private:
  RefType ref_;
};

/**
 * A helper template class to help us work with GCRef.
 *
 * This is sololy to get rid of the verbose syntax and checking around the
 * pointer to pointer GCRef object
 */
template< typename T > class Handler {};

#define DECLARE_GCHANDLER(T) \
  template<> class Handler<T> {                                            \
   public:                                                                 \
    GCRef ref() const { return ref_; }                                     \
    bool IsEmpty() const { return ref_.IsEmpty(); }                        \
    bool IsRefEmpty() const { return ref_.IsRefEmpty(); }                  \
    bool IsNull() const { return ref_.IsNull(); }                          \
    inline T& operator* ();                                                \
    inline const T& operator* () const;                                    \
    inline T* operator -> ();                                              \
    inline const T* operator -> () const;                                  \
   public:                                                                 \
    Handler( GCRef ref ):ref_(ref){}                                       \
    Handler():ref_(){}                                                     \
    Handler( const Handler& handler ):ref_(handler.ref){}                  \
    Handler& operator = ( const Handler& that ) {                          \
      if(this != &that) {                                                  \
        ref_ = that.ref_;                                                  \
      }                                                                    \
      return *this;                                                        \
    }                                                                      \
   private:                                                                \
    GCRef ref_;                                                            \
  };

#define __(A,B,C) DECLARE_GCHANDLER(B)

LAVASCRIPT_HEAP_OBJECT_LIST(__)

#undef __ // __

#undef // DECLARE_GCHANDLER

/**
 * The value type in lavascript are categorized into 2 parts :
 *  1. primitive type --> value semantic
 *  2. heap type      --> reference semantic
 *
 * Value is a *BOX* type for everything , including GCRef inside
 * of lavascript. It can hold things like:
 *    1) primitive type or value type holding, and it is value copy
 *    2) directly hold HeapObject pointer , and it is reference copy
 *    3) GCRef hold, implicitly hold HeapObject , and it is reference copy
 */

class Value {
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
    TAG_HEAP   = 0xfffa00000000000 ,                        // Heap Object
    TAG_GCREF  = 0xfffc00000000000                          // GCRef pointer
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
  bool IsTagGCRef()   const { return tag() == TAG_GCREF; }

  inline HeapObject* heap_object() const;
  inline GCRef::RefType gcref_ptr  () const;
  inline void set_pointer( void* ptr , int tag );

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
  bool IsGCRef() const      { return IsTagGCRef(); }
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

  inline HeapObject* GetHeapObject() const;
  inline String* GetString() const;
  inline List* GetList() const;
  inline Slice* GetSlice() const;
  inline Object* GetObject() const;
  inline Map* GetMap() const;
  inline Prototype* GetPrototype() const;
  inline Closure* GetClosure() const;
  inline Extension* GetExtension() const;

  inline GCRef GetGCRef() const;

  template< typename T >
  inline Handler<T> GetHandler() const { return Handler<T>(GetGCRef()); }

  // Setters ---------------------------------------------
  inline void SetInteger( std::uint32_t );
  inline void SetBoolean( bool );
  inline void SetReal   ( double );
  void SetNull   () { raw_ = TAG_NULL; }
  void SetTrue   () { raw_ = TAG_TRUE; }
  void SetFalse  () { raw_ = TAG_FALSE;}

  inline void SetHeapObject( HeapObject* );
  inline void SetString( String* );
  inline void SetList  ( List* );
  inline void SetSlice ( Slice* );
  inline void SetObject( Object*);
  inline void SetMap   ( Map* );
  inline void SetPrototype( Prototype* );
  inline void SetClosure  ( Closure* );
  inline void SetExtension( Extension* );

  inline void SetGCRef ( GCRef );
  template< typename T >
  void SetHandler( const Handler<T>& h ) { SetGCRef(h.ref()); }

 public:
  Value() { SetNull(); }
  inline explicit Value( double v ) { SetReal(v); }
  inline explicit Value( std::int32_t v ) { SetInteger(v); }
  inline explicit Value( bool v ) { SetBoolean(v); }
  inline explicit Value( HeapObject* v ) { SetHeapObject(v); }
  inline explicit Value( GCRef v ) { SetGCRef(v); }
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
class HeapObject {
 public:
  bool IsString() const { return type_ == TYPE_STRING; }
  bool IsList  () const { return type_ == TYPE_LIST;   }
  bool IsSlice () const { return type_ == TYPE_SLICE; }
  bool IsObject() const { return type_ == TYPE_OBJECT; }
  bool IsMap   () const { return type_ == TYPE_MAP; }
  bool IsPrototype() const { return type_ == TYPE_PROTOTYPE; }
  bool IsClosure()const { return type_ == TYPE_CLOSURE;}
  bool IsExtension() const { return type_ == TYPE_EXNTESION; }

 public:
  ValueType type() const { return type_; }

  HeapObjectHeader::Type* heap_object_header_address() const {
    return reinterpret_cast<HeapObjectHeader::Type*>(
        static_cast<std::uint8_t*>(this) - HeapObjectHeader::kHeapObjectHeaderSize);
  }

  HeapObjectHeader::Type heap_object_header_word() const {
    return *heap_object_header_address();
  }

  HeapObjectHeader heap_object_header() const {
    return HeapObjectHeader( heap_object_header_word() );
  }

  void set_heap_object_header( const HeapObjectHeader& word ) {
    *heap_word_address() = word.raw();
  }

  // helper function for retrieving some GC states
  GCState gc_state() const { return heap_object_header().gc_state(); }

  void set_gc_state( GCState state ) {
    HeapObjectHeader h = heap_object_header();
    h.set_gc_state( state );
    set_heap_object_header(h);
  }

  virtual void Mark(GC*) {}
  virtual ~HeapObject() = 0;
 protected:
  HeapObject( ValueType type ) : type_(type) {}

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

class SSO {
 public:
  std::uint32_t hash() const { return hash_; }
  void* data() const { return data_; }
  size_t size() const { return size_; }

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
  // data that held by SSO
  void* data_;

  // size of the data
  size_t size_;

  // hash value of this SSO
  std::uint32_t hash_;

  // next SSO entry if collision happened
  SSO* next_;

  // memory pool for SSOs
  friend class SSOPool;

  LAVA_DISALLOW_COPY_AND_ASSIGN(SSO);
};

/**
 * Long string representation inside of VM
 */
struct LongString {
  size_t size;
  LongString() : size(0) {}
};

/**
 * A String object is a normal long string object that resides
 * on top the real heap and it is due to GC. String is an immutable
 * object and once it is allocated we cannot change its memory.
 * The String object directly uses memory that is after the *this*
 * pointer. Its memory layout is like this:
 *
 * ------------------------------------------
 * |sizeof(String)|  size      | data ..... |
 * ------------------------------------------
 *
 */

class String : public HeapObject {
 public:
  inline void*  data() const;
  inline size_t size() const;

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

class List : public HeapObject {
 public:
  // Where the list's underlying Slice object are located
  Handler<Slice> slice() const { return Handler<Slice>(slice_); }
  GCRef slice_ref() const { return slice_; }

  inline bool empty() const;
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

 private:
  virtual void Mark( GC* );

  size_t size_;
  GCRef  slice_;

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
class Slice : public HeapObject {
  inline void* array() const;
 public:
  bool empty() const { return capacity() != 0; }
  size_t capacity() const { return capacity_; }
  const Value* data() const;
  Value* data();
  inline Value& Index( size_t );
  inline const Value& Index( size_t ) const;
  Value& operator [] ( size_t index ) { return Index(index); }
  const Value& operator [] ( size_t index ) const { return Index(index); }
 private:
  virtual void Mark( GC* );

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

class Object : public HeapObject {
 public:
   GCRef map_ref() const { return map_; }
   Handler<Map> map() const { return Handler<Map>(map_); }
 public:
  inline size_t capacity() const;
  inline size_t size() const;
  inline bool empty() const;

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

 private:
  virtual void Mark(GC*);
  GCRef map_;

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

class Map : public HeapObject {
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

  size_t capacity() const { return capacity_; }
  size_t size() const { return size_; }
  size_t slot_size() const { return slot_size_; }
  bool empty() const { return size() == 0; }

  inline Entry* data();
  inline const Entry* data() const;

 public: // Mutators
  inline bool Get ( const Handler<String>& , Value* ) const;
  inline bool Get ( const char*, Value* ) const;
  inline bool Get ( const std::string& , Value* ) const;

  inline bool Set ( const Handler<String>& , const Value& );
  inline bool Set ( const char*   , const Value& );
  inline bool Set ( const std::string& , const Value& );

  inline bool Update ( const Handler<String>& , const Value& );
  inline bool Update ( const char*   , const Value& );
  inline bool Update ( const std::string& , const Value& );

  inline void Put ( const Handler<String>& , const Value& );
  inline void Put ( const char*   , const Value& );
  inline void Put ( const std::string& , const Value& );

  inline bool Delete ( const Handler<String>& );
  inline bool Delete ( const char*   );
  inline bool Delete ( const std::string& );

 public:
  static std::uint32_t Hash( const char* );
  static std::uint32_t Hash( const std::string& );
  static std::uint32_t Hash( const Handler<String>& );

  static bool KeyEqual( const char* , const Value& );
  static bool KeyEqual( const std::string& , const Value& );
  static bool KeyEqual( const Handler<String>& , const Value& );

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
  virtual void Mark(GC*);

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

class Prototype : public HeapObject {
 public:
  const vm::Bytecode& bytecode() const { return bytecode_; }
  const vm::ConstantTable& constant_table() const { return constant_table_; }
  const vm::UpValueIndexArray& upvalue_array() const { return upvalue_array_; }
  GCRef  proto_string_gcref() const { return proto_string_; }
  inline proto_string() const;
  size_t argument_size() const { return argument_size_; }

 public: // Mutator
  vm::Bytecode& bytecode() { return bytecode_; }
  vm::ConstantTable& constant_table() { return constant_table_; }
  vm::UpValueIndexArray& upvalue_array() { return upvalue_array_; }
  void set_proto_string( GCRef* str ) { proto_string_ = str; }
  void set_argument_size( size_t arg) { argument_size_ = arg;}

 private:
  virtual void Mark(GC*);

  vm::BytecodeArray code_;
  vm::ConstantTable const_table_;
  vm::UpValueIndexArray upvalue_array_;
  GCRef  proto_string_;
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

class Closure : public HeapObject {
 public:
  GCRef  prototype() const { return prototype_; }
  inline Value* upvalue();
  inline const Value* upvalue() const;

 private:
  virtual void Mark(GC*);

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

#define DEFINE_GCHANDLER(T) \
  template<> inline T& Handler<T>::operator* ()                                         \
  { return *ref_.heap_object()->Get##T(); }                                             \
  template<> inline const T& Handler<T>::operator* () const                             \
  { return *ref_.heap_object()->Get##T(); }                                             \
  template<> inline T* Handler<T>::operator -> ()                                       \
  { return ref_.heap_object()->Get##T(); }                                              \
  template<> inline const T* Handler<T>::operator -> () const                           \
  { return ref_.heap_object()->Get##T(); }

#define __(A,B,C) DEFINE_GCHANDLER(B) \

LAVASCRIPT_HEAP_OBJECT_LIST(__)

#undef __ // __

#undef DEFINE_GCHANDLER // DEFINE_GCHANDLER

inline HeapObject* Value::heap_object() const {
  return reinterpret_cast<HeapObject*>(raw_ & kPtrMask);
}

inline GCRef::RefType Value::gcref_ptr() const {
  return reinterpret_cast<GCRef::RefType>(raw_ & kPtrMask);
}

inline void Value::set_pointer( void* ptr , int tag ) {
#ifdef LAVASCRIPT_CHECK_OBJECTS
  /**
   * Checking whether the input pointer is valid pointer
   * with our assumptions
   */
#ifdef LAVASCRIPT_ARCH_X64
  lava_assert( (static_cast<std::uintptr_t>(ptr)&kPtrCheckMask) == 0 ,
               "the pointer %x specified here is not a valid pointer,"
               " upper 16 bits is not 0s", ptr );
#endif // LAVASCRIPT_ARCH_X64
#endif // LAVASCRIPT_CHECK_OBJECTS

  raw_ = static_cast<std::uintptr_t>(ptr) | tag;
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
  return IsHeapObject() && heap_object()->IsString();
}

inline bool Value::IsList() const {
  return IsHeapObject() && heap_object()->IsList();
}

inline bool Value::IsSlice() const {
  return IsHeapObject() && heap_object()->IsSlice();
}

inline bool Value::IsObject() const {
  return IsHeapObject() && heap_object()->IsObject();
}

inline bool Value::IsMap() const {
  return IsHeapObject() && heap_object()->IsMap();
}

inline bool Value::IsPrototype() const {
  return IsHeapObject() && heap_object()->IsPrototype();
}

inline bool Value::IsClosure() const {
  return IsHeapObject() && heap_object()->IsClosure();
}

inline bool Value::IsExtension() const {
  return IsHeapObject() && heap_object()->IsExtension();
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

inline HeapObject* Value::GetHeapObject() const {
#define LAVASCRIPT_CHECK_OBJECTS
  lava_verify(IsHeapObject());
#endif // LAVASCRIPT_CHECK_OBJECTS
  return heap_object();
}

inline String* Value::GetString() const {
#define LAVASCRIPT_CHECK_OBJECTS
  lava_verify(IsString());
#endif // LAVASCRIPT_CHECK_OBJECTS
  return static_cast<String*>(heap_object());
}

inline List* Value::GetList() const {
#define LAVASCRIPT_CHECK_OBJECTS
  lava_verify(IsList());
#endif // LAVASCRIPT_CHECK_OBJECTS
  return static_cast<List*>(heap_object());
}

inline Slice* Value::GetSlice() const {
#define LAVASCRIPT_CHECK_OBJECTS
  lava_verify(IsSlice());
#endif // LAVASCRIPT_CHECK_OBJECTS
  return static_cast<Slice*>(heap_object());
}

inline Object* Value::GetObject() const {
#define LAVASCRIPT_CHECK_OBJECTS
  lava_verify(IsObject());
#endif // LAVASCRIPT_CHECK_OBJECTS
  return static_cast<Object*>(heap_object());
}

inline Map* Value::GetMap() const {
#define LAVASCRIPT_CHECK_OBJECTS
  lava_verify(IsMap());
#endif // LAVASCRIPT_CHECK_OBJECTS
  return static_cast<Map*>(heap_object());
}

inline Prototype* Value::GetPrototype() const {
#define LAVASCRIPT_CHECK_OBJECTS
  lava_verify(IsPrototype());
#endif // LAVASCRIPT_CHECK_OBJECTS
  return static_cast<Prototype*>(heap_object());
}

inline Closure* Value::GetClosure() const {
#define LAVASCRIPT_CHECK_OBJECTS
  lava_verify(IsClosure());
#endif // LAVASCRIPT_CHECK_OBJECTS
  return static_cast<Closure*>(heap_object());
}

inline Extension* Value::GetExtension() const {
#define LAVASCRIPT_CHECK_OBJECTS
  lava_verify(IsExtension());
#endif // LAVASCRIPT_CHECK_OBJECTS
  return static_cast<Extension*>(heap_object());
}

inline GCRef Value::GetGCRef() const {
#define LAVASCRIPT_CHECK_OBJECTS
  lava_verify(IsGCRef());
#endif // LAVASCRIPT_CHECK_OBJECTS
  return GCRef( gcref_ptr() );
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

inline void Value::SetHeapObject( HeapObject* ptr ) {
  set_pointer(ptr,TAG_HEAP);
}

inline void Value::SetString( String* ptr ) {
  set_pointer(static_cast<HeapObject*>(ptr),TAG_HEAP);
}

inline void Value::SetList( List* ptr ) {
  set_pointer(static_cast<HeapObject*>(ptr),TAG_HEAP);
}

inline void Value::SetSlice( Slice* ptr ) {
  set_pointer(static_cast<HeapObject*>(ptr),TAG_HEAP);
}

inline void Value::SetObject( Object* ptr ) {
  set_pointer(static_cast<HeapObject*>(ptr),TAG_HEAP);
}

inline void Value::SetMap( Map* ptr ) {
  set_pointer(static_cast<HeapObject*>(ptr),TAG_HEAP);
}

inline void Value::SetPrototype( Prototype* ptr ) {
  set_pointer(static_cast<HeapObject*>(ptr),TAG_HEAP);
}

inline void Value::SetClosure( Closure* ptr ) {
  set_pointer(static_cast<HeapObject*>(ptr),TAG_HEAP);
}

inline void Value::SetExtension( Extension* ptr ) {
  set_pointer(static_cast<HeapObject*>(ptr),TAG_HEAP);
}

inline void Value::SetGCRef( GCRef ref ) {
#ifdef LAVASCRIPT_CHECK_OBJECTS
  lava_verify(*ref != NULL);
#endif
  set_pointer(static_cast<GCRef::RefType>(ref.ref()),TAG_GCREF);
}

inline bool SSO::operator == ( const char* str ) const {
  const size_t l = strlen(str);
  const size_t len = std::min(l,size_);
  int r = std::memcmp(data_,str,len);
  return r == 0 ? (len == l) : false;
}

inline bool SSO::operator == ( const std::string& str ) const {
  const size_t len = std::min(size_,str.size());
  int r = std::memcmp(data_,str.c_str(),len);
  return r == 0 ? (size_ == len) : false;
}

inline bool SSO::operator == ( const String& str ) const {
  const size_t len = std::min(size_,str.size());
  int r = std::memcmp(data_,str.data(),len);
  return r == 0 ? (size_ == len) : false;
}

inline bool SSO::operator == ( const SSO& str ) const {
  return this == &str;
}

inline bool SSO::operator != ( const char* str ) const {
  const size_t l = strlen(str);
  const size_t len = std::min(size_,l);
  int r = std::memcmp(data_,str,len);
  return r == 0 ? (size_ != len) : true;
}

inline bool SSO::operator != ( const std::string& str ) const {
  const size_t len = std::min(size_,str.size());
  int r = std::memcmp(data_,str.c_str(),len);
  return r == 0 ? (size_ != len) : true;
}

inline bool SSO::operator != ( const String& str ) const {
  const size_t len = std::min(size_,str.size());
  int r = std::memcmp(data_,str.data(),len);
  return r == 0 ? (size_ != len ) : true;
}

inline bool SSO::operator != ( const SSO& str ) const {
  const size_t len = std::min(size_,str.size());
  int r = std::memcmp(data_,str.data(),len);
  return r == 0 ? (size_ != len) : true;
}

inline bool SSO::operator >  ( const char* str ) const {
  const size_t l = strlen(str);
  const size_t len = std::min(size_,l);
  int r = std::memcmp(data_,str.data(),len);
  if(r > 0 || (r == 0 && size_ > len))
    return true;
  else
    return false;
}

inline bool SSO::operator > ( const std::string& str ) const {
  const size_t len = std::min(size_,str.size());
  int r = std::memcmp(data_,str.c_str(),len);
  if(r > 0 || (r == 0 && size_ > len))
    return true;
  else
    return false;
}

inline bool SSO::operator > ( const String& str ) const {
  const size_t len = std::min(size_,str.size());
  int r = std::memcmp(data_,str.c_str(),len);
  if(r > 0 || (r == 0 && size_ > len))
    return true;
  else
    return false;
}

inline bool SSO::operator > ( const SSO& str ) const {
  const size_t len = std::min(size_,str.size());
  int r = std::memcmp(data_,str.data(),len);
  if(r > 0 || (r == 0 && size_ > len))
    return true;
  else
    return false;
}

inline bool SSO::operator >= ( const char* str ) const {
  const size_t l = strlen(str);
  const size_t len = std::min(size_,l);
  int r = std::memcmp(data_,str,len);
  if(r > 0 || (r == 0 && size_ >= len))
    return true;
  else
    return false;
}

inline bool SSO::operator >= ( const std::string& str ) const {
  const size_t len = std::min(size_,str.size());
  int r = std::memcmp(data_,str.c_str(),len);
  if(r > 0 || (r == 0 && size_ >= len))
    return true;
  else
    return false;
}

inline bool SSO::operator >= ( const String& str ) const {
  const size_t len = std::min(size_,str.size());
  int r = std::memcmp(data_,str.data(),len);
  if(r > 0 || (r == 0 && size_ >= len))
    return true;
  else
    return false;
}

inline bool SSO::operator >= ( const SSO& str ) const {
  const size_t len = std::min(size_,str.size());
  int r = std::memcmp(data_,str.data(),len);
  if(r > 0 || (r == 0 && size_ >= len ))
    return true;
  else
    return false;
}

inline bool SSO::operator < ( const char* str ) const {
  const size_t l = strlen(str);
  const size_t len = std::min(size_,l);
  int r = std::memcmp(data_,str,len);
  if(r < 0 || (r == 0 && size_ < l))
    return true;
  else
    return false;
}

inline bool SSO::operator < ( const std::string& str ) const {
  const size_t len = std::min(size_,str.size());
  int r = std::memcmp(data_,str.c_str(),len);
  if(r < 0 || (r == 0 && size_ < str.size()))
    return true;
  else
    return false;
}

inline bool SSO::operator < ( const String& str ) const {
  const size_t len = std::min(size_,str.size());
  int r = std::memcmp(data_,str.c_str(),len);
  if(r < 0 || (r == 0 && size_ < str.size()))
    return true;
  else
    return false;
}

inline bool SSO::operator < ( const SSO& str ) const {
  const size_t len = std::min(size_,str.size());
  int r = std::memcmp(data_,str.data(),len);
  if(r < 0 || (r == 0 && size_ < str.size()))
    return true;
  else
    return false;
}

inline bool SSO::operator <= ( const char* str ) const {
  const size_t l = strlen(str);
  const size_t len = std::min(size_,l);
  int r = std::memcmp(data_,str,len);
  if(r < 0 || (r == 0 && size_ <= l))
    return true;
  else
    return false;
}

inline bool SSO::operator <= ( const std::string& str ) const {
  const size_t len = std::min(size_,str.size());
  int r = std::memcmp(data_,str.c_str(),len);
  if(r < 0 || (r == 0 && size_ <= str.size()))
    return true;
  else
    return false;
}

inline bool SSO::operator <= ( const String& str ) const {
  const size_t len = std::min(size_,str.size());
  int r = std:memcmp(data_,str.c_str(),len);
  if(r < 0 || (r == 0 && size_ <= str.size()))
    return true;
  else
    return false;
}

inline bool SSO::operator <= ( const SSO& str ) const {
  const size_t len = std::min(size_,str.size());
  int r = std::memcmp(data_,str.size(),len);
  if(r < 0 || (r == 0 && size_ <= str.size()))
    return true;
  else
    return false;
}

inline void* String::data() const {
  return reinterpret_cast<void*>(
      reinterpret_cast<char*>(this) + sizeof(String));
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
  const std::size_t len = std::min(size(),str.size());
  int r = std::memcmp(data(),str.data(),len);
  return r == 0 ? size() == len : false;
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
  const std::size_t len = std::min(size(),str.size());
  int r = std::memcmp(data(),str.data(),len);
  return r == 0 ? size() != len : true;
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

inline bool List::empty() const {
  return slice()->empty();
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

inline size_t Object::capacity() const {
  return map()->capacity();
}

inline size_t Object::size() const {
  return map()->size();
}

inline size_t Object::empty() const {
  return map()->empty();
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
  return map()->Set(gc,key,val);
}

inline bool Object::Set( GC* gc , const char* key ,
    const Value& val ) {
  return map()->Set(gc,key,val);
}

inline bool Object::Set( GC* gc , const std::string& key ,
    const Value& val ) {
  return map()->Set(gc,key,val);
}

inline bool Object::Update( GC* gc , const String& key ,
    const Value& val ) {
  return map()->Update(gc,key,val);
}

inline bool Object::Update( GC* gc , const char* key ,
    const Value& val ) {
  return map()->Update(gc,key,val);
}

inline bool Object::Update( GC* gc , const std::string& key ,
    const Value& val ) {
  return map()->Update(gc,key,val);
}

inline bool Object::Put( GC* gc , const String& key ,
    const Value& val ) {
  return map()->Put(gc,key,val);
}

inline bool Object::Put( GC* gc , const char* key ,
    const Value& val ) {
  return map()->Put(gc,key,val);
}

inline bool Object::Put( GC* gc , const std::string& key,
    const Value& val ) {
  return map()->Put(gc,key,val);
}

inline bool Object::Delete( const String& key ) {
  return map()->Delete(key);
}

inline bool Object::Delete( const char* key ) {
  return map()->Delete(key);
}

inline bool Object::Delete( const std::string& key ) {
  return map()->Delete(key);
}

std::uint32_t Map::Hash( const String& key ) {}
std::uint32_t Map::Hash( const char* key ) {}
std::uint32_t Map::Hash( const std::string& key ) {}

std::uint32_t Map::Hash( const SSO& key ) {
  return key.hash();
}

template< typename T >
Entry* Map::FindEntry<T>( const T& key , std::uint32_t fullhash , Option opt ) {
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
      if(cur->hash == fullhash && KeyEqual(key,cur->key)) {
        return opt == INSERT ? NULL : cur;
      }
    }
    if(cur->more)
      cur = entry()[cur->next];
    else
      break;
  } while(true);

  if(opt == FIND) return NULL;
  // Linear probing
  Entry* new_slot = NULL;
  std::uint32_t h = fullhash;
  while( (new_slot = entry()[ ++h &(capacity()-1)])->use )
    ;

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
    return true;
  }
  return false;
}

inline bool Map::Set( GC* gc , const char* key , const Value& value ) {
  std::uint32_t f = Hash(key);
  Entry* entry = FindEntry(key,f,INSERT);
  if(entry) {
#ifdef LAVASCRIPT_CHECK_OBJECTS
    lava_verify(!entry->use);
#endif // LAVASCRIPT_CHECK_OBJECTS
    entry->use = 1;
    entry->value = value;
  }
}



























} // namespace lavascript
#endif // OBJECTS_H_
