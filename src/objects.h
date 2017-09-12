#ifndef OBJECTS_H_
#define OBJECTS_H_

#include <cstdint>

#include "vm/bytecode.h"
#include "vm/constant-table.h"
#include "core/trace.h"
#include "all-static.h"

/**
 * Objects are representation of all runtime objects in C++ side and
 * script side. It is designed to be efficient and also friendly to
 * assembly code since we will write interpreter and JIT mostly in the
 * assembly code
 */

namespace lavascript {

class GC;
class GCRef;
class Handle;
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
template< typename T > class Value;

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
  __( TYPE_SSO     , SSO     , "string"  )                            \


#define LAVASCRIPT_VALUE_TYPE_LIST(__)                                \
  LAVASCRIPT_PRIMITIVE_TYPE_LIST(__)                                  \
  LAVASCRIPT_HEAP_OBJECT_LIST(__)


enum ValueType {
#define __(A,B,C) A,
  LAVASCRIPT_VALUE_TYPE_LIST(__)
  SIZE_OF_VALUE_TYPES
#undef __ // __
};

/**
 * Get the name of certain value type via its enum index
 */
const char* GetValueTypeName( ValueType );

/**
 * A value is a wrapper classes for GCRef to allow us to easily operate
 * on different kinds of objects. User can directly use GCRef to use
 * different heap object , but with Value it is much more safer
 */
template< typename T > class Value {};

#define DECLARE_SPECIALIZE_VALUE(T)                                       \
  template<> class Value<T> {                                             \
   public:                                                                \
    Value( const Value& that ):ref_(that.ref_){}                          \
    Value& operator == ( const Value& that )                              \
    { if(&that != this) ref_ = that.ref_; return *this; }                 \
    explicit Value(GCRef* ref):ref_(ref){}                                \
    Value():ref_(NULL){}                                                  \
    bool IsEmpty() const { return ref_ == NULL; }                         \
    GCRef* ref  () const { return ref_; }                                 \
    inline T* operator -> ();                                             \
    inline const T* operator -> () const                                  \
    inline T& operator* ();                                               \
    inline const T& operator* () const                                    \
    void Clear() { ref_ = NULL; }                                         \
   public:                                                                \
    bool operator == ( const Value& that ) const                          \
    { return ref_ == that.ref_; }                                         \
    bool operator != ( const Value& that ) const                          \
    { return ref_ != that.ref_; }                                         \
    bool operator == ( GCRef* that ) const { return ref_ == that; }       \
    bool operator != ( GCRef* that ) const { return ref_ != that; }       \
                                                                          \
   private:                                                               \
    GCRef* ref_;                                                          \
  };


/** Declare specialized Value object for all HeapObject */
#define __(A,B,C) DECLARE_SPECIALIZE_VALUE(B)
LAVASCRIPT_HEAP_OBJECT_LIST(__)
#undef __ // __

#undef DECLARE_SPECIALIZE_VALUE // DECLARE_SPECIALIZE_VALUE

/**
 *
 * The value type in lavascript are categorized into 2 parts :
 *  1. primitive type --> value semantic
 *  2. heap type      --> reference semantic
 *
 * All the value type can be boxed via boxing handler called Handle
 * Handle is essentially the value type that gonna be used throughout
 * the vm and JIT.
 *
 * The Handle object is implemented via NAN-tagging which makes Handle
 * to be only as large as a machine word. This is good since we only
 * need the one load to fetch the Handle object on stack and also it
 * can be held by register.
 *
 */

class Handle {
  union {
    std::uintptr_t raw_;
    void* vptr_;
    double real_;
    std::int32_t integer_;
  };

  // For primitive type , we can tell it directly from *raw* value due to the
  // double never use the lower 53 bits.
  // For pointer type , since it only uses 48 bits on x64 platform, we can
  // reconstruct the pointer value from the full 64 bits machine word , but we
  // don't store the heap object type value directly inside of *Handle* object
  // but put along with the heap object's common header HeapObject.
  enum {
    TAG_REAL   = 0xfff800000000000 ,                        // Real
    TAG_INTEGER= 0xfff910000000000 ,                        // Integer
    TAG_TRUE   = 0xfff920000000000 ,                        // True
    TAG_FALSE  = 0xfff930000000000 ,                        // False
    TAG_NULL   = 0xfff940000000000 ,                        // Null
    TAG_HEAP   = 0xfffa00000000000 ,                        // Heap Object
    TAG_SSO    = 0xfffb00000000000                          // Short String Object
  };

  // Masks
  static const std::uintptr_t kTagMask = 0xffff000000000000;
  static const std::uintptr_t kIntMask = 0x00000000ffffffff;
  static const std::uintptr_t kPtrMask = 0x0000ffffffffffff;

  // A mask that is used to check whether a pointer is a valid x64 pointer.
  // So not break our assumption. This assumption can be held always,I guess
  static const std::uintptr_t kPtrCheckMask = ~kPtrMask;

  int tag() const { return (raw_&kTagMask); }

  bool IsTagReal()    const { return tag() <  TAG_REAL; }
  bool IsTagInteger() const { return tag() == TAG_INTEGER; }
  bool IsTagTrue()    const { return tag() == TAG_TRUE; }
  bool IsTagFalse()   const { return tag() == TAG_FALSE;}
  bool IsTagNull()    const { return tag() == TAG_NULL;}
  bool IsTagHeap()    const { return tag() == TAG_HEAP;}
  bool IsTagSSO ()    const { return tag() == TAG_SSO; }

 public:
  // Primitive types
  bool IsNull() const       { return IsTagNull(); }
  bool IsReal() const       { return IsTagReal(); }
  bool IsInteger() const    { return IsTagInteger(); }
  bool IsTrue() const       { return IsTagTrue(); }
  bool IsFalse()const       { return IsTagFalse();}
  bool IsBoolean() const    { return IsTagTrue() || IsTagFalse(); }
  bool IsSSO () const       { return IsTagSSO(); }

  // Heap types
  bool IsHeapObject() const { return IsTagHeap(); }
  inline bool IsString() const;
  inline bool IsList  () const;
  inline bool IsObject() const;
  inline bool IsClosure()const;
  inline bool IsExtension() const;

  /** Getters for all boxed value */
  inline double GetReal() const;
  inline std::int32_t GetInteger() const;
  inline bool GetBoolean() const;
  inline const SSO& GetSSO() const;

  /** Setters for all boxed value */
  void SetReal( double real ) { real_ = real; }
  inline void SetInteger( std::int32_t );
  void SetTrue() { raw_ = TAG_TRUE; }
  void SetFalse(){ raw_ = TAG_FALSE;}
  void SetBoolean( bool val ) { raw_ = val ? TAG_TRUE : TAG_FALSE; }
  void SetNull() { raw_ = TAG_NULL; }
  inline void SetSSO ( SSO* sso );

  /** Setters for pointer type */
  inline void SetGCRef( GCRef* );
  inline GCRef* GetGCRef() const;
  inline template< typename T > Value<T> GetGCRef() const;

  /**
   * Returns a type enumeration for this Handle.
   * This function is not really performant , user should always prefer using
   * IsXXX function to test whether a handle is certain type
   */
  ValueType type() const;
  const char* type_name() const;

 public:
  Handle();
  inline explicit Handle( double );
  inline explicit Handle( std::int32_t );
  inline explicit Handle( bool );
  inline explicit Handle( SSO* );
  inline explicit Handle( GCRef* );
};

// The handle must be as long as a machine word , here for simplicitly we assume
// the machine word to be the size of uintptr_t type.
static_assert( sizeof(Handle) == sizeof(std::uintptr_t) );

// Heap object's shared base object
class HeapObject {
 public:
  bool IsString() const { return type_ == TYPE_STRING; }
  bool IsList  () const { return type_ == TYPE_LIST;   }
  bool IsObject() const { return type_ == TYPE_OBJECT; }
  bool IsClosure()const { return type_ == TYPE_CLOSURE;}
  bool IsExtension() const { return type_ == TYPE_EXNTESION; }
  ValueType type() const { return type_; }

  virtual void Mark(GC*) {}
  virtual ~HeapObject() = 0;

 protected:
  HeapObject( ValueType type ) : type_(type) {}

 private:
  ValueType type_;

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
  size_t size() const { return sizez_; }

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
  size_t size_;

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
  inline GCRef* slice() const;
  inline bool empty() const;
  inline size_t size() const;
  inline size_t capacity() const;
  inline const Handle& Index( size_t ) const;
  inline Handle& Index( size_t );
  Handle& operator [] ( size_t index ) { return Index(index); }
  const Handle& operator [] ( size_t index ) const { return Index(index); }

  inline Handle& Last();
  inline const Handle& Last() const;
  inline Handle& First();
  inline const Handle& First() const;

 public: // Mutator for the list object itself
  void Clear( GC* );
  bool Push( GC* , const Handle& );
  void Pop ();

 private:
  virtual void Mark( GC* );

  size_t size_;
  GCRef* slice_;

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
 public:
  bool empty() const { return capacity() != 0; }
  size_t capacity() const;
  const Handle* data() const;
  Handle* data();
  inline Handle& Index( size_t );
  inline const Handle& Index( size_t ) const;
  Handle& operator [] ( size_t index ) { return Index(index); }
  const Handle& operator [] ( size_t index ) const { return Index(index); }
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
  inline size_t capacity() const;
  inline size_t size() const;
  inline bool empty() const;
  inline GCRef* map() const;

 public:
  inline bool Get ( const String& , Handle* ) const;
  inline bool Get ( const char*   , Handle* ) const;
  inline bool Get ( const std::string& , Handle* ) const;
  inline bool Get ( const SSO& , Handle* ) const;

  inline bool Set ( GC* , const String& , const Handle& );
  inline bool Set ( GC* , const char*   , const Handle& );
  inline bool Set ( GC* , const std::string& , const Handle& );
  inline bool Set ( GC* , const SSO& , const Handle& );

  inline bool Update ( GC* , const String& , const Handle& );
  inline bool Update ( GC* , const char*   , const Handle& );
  inline bool Update ( GC* , const std::string& , const Handle& );
  inline bool Update ( GC* , const SSO& , const Handle& );

  inline void Put ( GC* , const String& , const Handle& );
  inline void Put ( GC* , const char*   , const Handle& );
  inline void Put ( GC* , const std::string& , const Handle& );
  inline void Put ( GC* , const SSO& , const Handle& );

  inline bool Delete ( const String& );
  inline bool Delete ( const char*   );
  inline bool Delete ( const std::string& );
  inline bool Delete ( const SSO& );

 private:
  virtual void Mark(GC*);
  GCRef* map_;

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
    GCRef* key;
    Handle value;
    std::uint32_t hash;
    std::uint32_t next : 30;
    std::uint32_t del  :  1;
    std::uint32_t use  :  1;
  };

  static const size_t kMaximumMapSize = 1<<30;

  inline size_t capacity() const;
  inline size_t size() const;
  inline Entry* data();
  inline const Entry* data() const;

 public: // Mutators
  inline bool Get ( const String& , Handle* ) const;
  inline bool Get ( const char*   , Handle* ) const;
  inline bool Get ( const std::string& , Handle* ) const;
  inline bool Get ( const SSO& , Handle* ) const;

  inline bool Set ( const String& , const Handle& );
  inline bool Set ( const char*   , const Handle& );
  inline bool Set ( const std::string& , const Handle& );
  inline bool Set ( const SSO& , const Handle& );

  inline bool Update ( const String& , const Handle& );
  inline bool Update ( const char*   , const Handle& );
  inline bool Update ( const std::string& , const Handle& );
  inline bool Update ( const SSO& , const Handle& );

  inline void Put ( const String& , const Handle& );
  inline void Put ( const char*   , const Handle& );
  inline void Put ( const std::string& , const Handle& );
  inline void Put ( const SSO& , const Handle& );

  inline bool Delete ( const String& );
  inline bool Delete ( const char*   );
  inline bool Delete ( const std::string& );
  inline bool Delete ( const SSO& );

 private:
  virtual void Mark(GC*);

  size_t capacity_;
  size_t size_;

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

  virtual bool Deref( Handle* , Handle* ) const = 0;

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
  GCRef* proto_string_gcref() const { return proto_string_; }
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
  GCRef* proto_string_;
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
  GCRef* prototype() const { return prototype_; }
  inline Handle* upvalue();
  inline const Handle* upvalue() const;

 private:
  virtual void Mark(GC*);

  GCRef* prototype_;

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



/* GC related objects --------------------------------------------------
 *
 * We will implement a simple stop the world GC right now and the later
 * on migrate to a better generational GC.
 *
 * The handler in GC are 2 level of indirection, or we don't directly
 * store the pointer to an object that is on the *heap* but instead we
 * store the *pointer of pointer* to the object on the heap. This object
 * is called a GCPointer. A GCPointer is a pointer of pointer to an object
 * on the heap.
 *
 * A GCPointer is a just a wrapper class around an address which resides
 * inside of GCRef array. A GCRef array is a special array
 * that is used to allocate all the heap object's reference and it is also
 * part of our heap but it always grow and never shrinks. The GCRef
 * array is a simple free list and we don't free memory underlying of
 * GCRef array and this piece of memory is dedicated for GC
 */

enum GCState {
  GC_MARK_WHITE = 0,
  GC_MARK_GRAY ,
  GC_MARK_BLACK,
  GC_MARK_RESERVED
};

/**
 * A GCRef object is a tagged pointer and it can only be allocated via
 * GCRefAllocator and cannot be allocated with *normal* C++/C heap mechanism.
 *
 * The GCRef will be always as large as a machine word and it contains:
 *  1) pointer points to normal heap that has all the HeapObject
 *  2) GC state embedded here (as last 3 bits in the pointer).
 *
 * A GC scan process will *not* need to touch the HeapObject as long as the object
 * is not an extension.
 *
 * Our allocator will *ALWAYS* allign any memory allocation to be 8 , so we have
 * last 3 bits left out for our personal use.
 *
 */

class GCRef : DoNotAllocateOnNormalHeap {
  union {
    HeapObject* object_;
    std::uintptr_t ptr_;
  };

  // A pointer points to the next *available* GCRef object in the GCRefPool.
  // This field is used for scanning during the GC cycle.
  GCRef* next_;

 private:
  static const std::uintptr_t kFlagMask      =  7;  // 0b111
  static const std::uintptr_t kPtrMask       = ~7;  // 0b11....100

  void* raw_pointer() const { return reinterpret_cast<void*>(ptr_ & kPtrMask); }
  template< typename T >
  T* pointer() const { return reinterpret_cast<T*>(raw_pointer()); }
  inline void set_pointer( HeapObject* );

 public:
  GCRef( String* ptr, GCRef* n ):next_(n) { set_pointer(ptr); }
  GCRef( List* ptr  , GCRef* n ):next_(n) { set_pointer(ptr); }
  GCRef( Slice* ptr , GCRef* n ):next_(n) { set_pointer(ptr); }
  GCRef( Object* ptr  , GCRef* n ):next_(n) { set_pointer(ptr); }
  GCRef( Map* ptr , GCRef* n ):next_(n)   { set_pointer(ptr); }
  GCRef( Prototype* ptr , GCRef* n ):next_(n) { set_pointer(ptr); }
  GCRef( Closure* ptr , GCRef* n ):next_(n) { set_pointer(ptr); }
  GCRef( Extension* ptr ):next_(n) { set_pointer(ptr); }

 public:
  GCState gc_state() const { return static_cast<GCState>(ptr_ & kFlagMask); }
  void set_gc_state( GCState state ) { ptr_ = ptr_ | state; }
  ValueType type() const { return heap_object()->type(); }
  GCRef*  next() const { return next_; }
  HeapObject* heap_object() const { return pointer<HeapObject>(); }

 public:
  bool IsString() const { return heap_object()->IsString(); }
  bool IsList  () const { return heap_object()->IsList()  ; }
  bool IsSlice () const { return heap_object()->IsSlice() ; }
  bool IsObject() const { return heap_object()->IsObject(); }
  bool IsMap   () const { return heap_object()->IsMap();    }
  bool IsPrototype() const { return heap_object()->IsPrototype(); }
  bool IsClosure()const { return heap_object()->IsClosure();}
  bool IsExtension() const { return heap_object()->IsExtension(); }

  void SetString( String* ptr ) { set_pointer(ptr); }
  void SetList  ( List* ptr   ) { set_pointer(ptr); }
  void SetSlice ( Slice* ptr  ) { set_pointer(ptr); }
  void SetObject( Object* ptr ) { set_pointer(ptr); }
  void SetMap   ( Map* ptr )    { set_pointer(ptr); }
  void SetPrototype( Prototype* ptr ) { set_pointer(ptr); }
  void SetClosure( Closure* ptr ) { set_pointer(ptr); }
  void SetExtension( Extension* ptr ) { set_pointer(ptr); }

  inline void SetHeapObject( HeapObject* );

  inline String*    GetString() const;
  inline List*      GetList  () const;
  inline Slice*     GetSlice () const;
  inline Object*    GetObject() const;
  inline Map*       GetMap()    const;
  inline Prototype* GetPrototype() const;
  inline Closure*   GetClosure() const;
  inline Extension* GetExtension() const;
};



/* -------------------------------------------------------------
 *
 * Inline functions definition
 *
 * ------------------------------------------------------------*/


#define DEFINE_SPECIALIZE_VALUE(T) \
  inline template<> T& Value<T>::operator* ()                             \
  { lava_verify( !IsEmpty() ); return *ref_->Get##T(); }                  \
  inline template<> const T& Value<T>::operator* () const                 \
  { lava_verify( !IsEmpty() ); return *ref_->Get##T(); }                  \
  inline template<> T* Value<T>::operator -> ()                           \
  { lava_verify( !IsEmpty() ); return ref_->Get##T();  }                  \
  inline template<> const T* Value<T>::operator -> () const               \
  { lava_verify( !IsEmpty() ); return ref_->Get##T();  }

#define __(A,B,C) DEFINE_SPECIALIZE_VALUE(B)

LAVASCRIPT_HEAP_OBJECT_LIST(__)

#undef __ // __

#undef DEFINE_SPECIALIZE_VALUE // DEFINE_SPECIALIZE_VALUE


} // namespace lavascript
#endif // OBJECTS_H_
