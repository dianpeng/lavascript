#ifndef UTIL_H_
#define UTIL_H_

#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS
#endif // __STDC_FORMAT_MACROS

#include <memory>
#include <functional>
#include <optional>
#include <cstdint>
#include <cstdarg>
#include <cstddef>
#include <string>
#include <cstring>
#include <vector>
#include <new>
#include <cmath>
#include <limits>
#include <inttypes.h>

#include "macro.h"
#include "hash.h"
#include "trace.h"

#define LAVA_FMTI64 PRId64
#define LAVA_FMTU64 PRIu64

namespace lavascript {

// Boost foreach style foreach statement for support customized iterator type.
// Requires C++ 17 if statement
#define lava_foreach(T,ITR)                                   \
  for( auto __itr = (ITR); __itr.HasNext(); __itr.Move() )    \
    if( T = __itr.value() ; true )

// A iterator adpater to limit how many steps this iterator should have
template< typename ITR > class CountedIterator {
 public:
  typedef typename ITR::ValueType ValueType;
  CountedIterator( const ITR& itr , std::size_t limit ) : itr_(itr), limit_(limit) {}
  const ValueType& value() const { return itr_.value(); }
  bool  HasNext         () const { return limit_ == 0 || itr_.HasNext(); }
  bool  Move            () const {
    lava_debug(NORMAL,lava_verify(HasNext()););
    if(--limit_ == 0) return false;
    return itr_.Move();
  }
 private:
  ITR itr_;
  mutable std::size_t limit_;
};

// ----------------------------------------------------------------
// Iterator Algorithm
// ----------------------------------------------------------------
template< typename ITR >
ITR FindIf( ITR itr , const std::function< bool ( typename ITR::ConstReferenceType ) >& predicate ) {
  for( ; itr.HasNext(); itr.Move() ) {
    if(predicate(itr.value())) return itr;
  }
  return itr;
}

template< typename ITR >
ITR Find( ITR itr , typename ITR::ConstReferenceType value ) {
  return FindIf(itr,[=]( typename ITR::ConstReferenceType v ) { return v == value; });
}

// Get the array's size
template< std::size_t N , typename T >
size_t ArraySize( const T (&arr)[N] ) { (void)arr; return N; }

// ----------------------------------------------------------------
// String Formatting
// ----------------------------------------------------------------
void FormatV( std::string* , const char*  , va_list );
inline std::string FormatV( const char* format , va_list vl ) {
  std::string temp;
  FormatV(&temp,format,vl);
  return temp;
}
inline std::string Format( const char * format , ... ) {
  va_list vl; va_start(vl,format);
  return FormatV(format,vl);
}
inline void Format( std::string* buffer , const char* format , ... ) {
  va_list vl; va_start(vl,format);
  FormatV(buffer,format,vl);
}

// ----------------------------------------------------------------
// Misc
// ----------------------------------------------------------------
inline char* AsBuffer( std::string* output , std::size_t off )
{ return &(*output->begin()) + off; }

template< typename T >
inline T* AsBuffer( std::vector<T>* output , std::size_t off ) {
  lava_verify(!output->empty());
  return &(*output->begin()) + off;
}

template< typename T >
inline const T* AsBuffer( const std::vector<T>* output , std::size_t off ) {
  lava_verify(!output->empty());
  return &(*output->begin()) + off;
}

// Object level memcpy and memmove wrapper
template< typename T >
inline T* MemCopy( T* dest , const T* from , std::size_t size ) {
  return static_cast<T*>(std::memcpy(dest,from,size*sizeof(T)));
}
template< typename T >
inline T* MemCopy( T* dest, const std::vector<T>& from ) {
  return static_cast<T*>(std::memcpy(dest,AsBuffer(&from,0),from.size()*sizeof(T)));
}
template< typename T >
inline T* MemMove( T* dest , const T* source , std::size_t size ) {
  return static_cast<T*>(std::memmove(dest,source,size*sizeof(T)));
}
template< typename T >
inline T* ZeroOut( T* dest , std::size_t size ) {
  memset(dest,0,sizeof(T)*size);
  return dest;
}

// object oriented buffer offset
template< typename T >
inline void* BufferOffset( void* buffer , std::size_t offset ) {
  return reinterpret_cast<void*>(static_cast<T*>(buffer) + offset);
}

// ---------------------------------------------------------------------
// lexical cast style conversion
// ---------------------------------------------------------------------
inline bool LexicalCast( const char* , std::int32_t* );
inline bool LexicalCast( const char* , std::uint32_t* );
inline bool LexicalCast( const char* , std::int64_t* );
inline bool LexicalCast( const char* , std::uint64_t* );
inline bool LexicalCast( const char* , double* );
inline bool LexicalCast( double , std::string* output );
inline bool LexicalCast( bool   , std::string* output );

// ---------------------------------------------------------------------
// Real number cast
// ---------------------------------------------------------------------
// Try to narrow a real number into a another types integer. It will fail
// if the double has exponent part or when there's a overflow
template< typename T >
bool NarrowReal     ( double real , T* output ) {
  double ipart;
  double rpart = std::modf(real,&ipart);
  if(rpart == 0.0) {
    if( static_cast<double>( std::numeric_limits<T>::max() ) >= ipart &&
        static_cast<double>( std::numeric_limits<T>::min() ) <= ipart ) {
      *output = static_cast<T>(ipart);
      return true;
    }
  }
  return false;
}

template< typename T >
bool TryCastReal( double real , T* output ) {
  double dmax = static_cast<double>(std::numeric_limits<T>::max());
  double dmin = static_cast<double>(std::numeric_limits<T>::min());
  if( real >= dmin && real <= dmax ) {
    *output = static_cast<T>(real);
    return true;
  }
  return false;
}

template< typename T >
T CastReal( double real ) { return static_cast<T>(real); }

template< typename T >
double CastRealAndStoreAsReal( double real ) {
  return static_cast<double>( CastReal<T>(real) );
}

// Cast a float64/double into a index number that is acceptable by lavascript.
// Inside of the lavascript, we use std::uint32_t as index to index into any
// types of memory , which obviously post a upper bound of how much memory you
// are allowed to index into
inline bool CastToIndex( double v , std::uint32_t* idx ) {
  // the following code is the same as what has been implemented inside of the
  // interpreter. and we should not overflow with std::int64_t when convert to
  // double and we ignore any float point part during conversion which is the
  // same as the interpreter does.
  std::int64_t temp = static_cast<std::int64_t>(v);
  // check the boundary of the value type , it is not possible to have index
  // larger than std::uint32_t's max range since we don't support this type
  // of memory allocation internally at all
  if(temp < 0) return false;
  if(temp > std::numeric_limits<std::uint32_t>::max()) return false;
  *idx = static_cast<std::uint32_t>(temp);
  return true;
}

// ---------------------------------------------------------------------
// Memory
// ---------------------------------------------------------------------
template< typename T > T Align( T value , T alignment ) {
  return (value + (alignment-1)) & ~(alignment-1);
}

// Placement new
template< typename T , typename Allocator , typename ... ARGS >
T* Construct( Allocator* allocator , ARGS&& ...args ) {
  return ::new (allocator->Grab(sizeof(T))) T(std::forward<ARGS>(args)...);
}

template< typename T , typename Allocator >
T* Construct( Allocator* allocator ) { return ::new (allocator->Grab(sizeof(T))) T(); }

template< typename T , typename ... ARGS >
T* ConstructFromBuffer( void* buffer , ARGS&& ...args ) {
  return ::new (buffer) T(std::forward<ARGS>(args)...);
}

template< typename T , typename ... ARGS >
T* ConstructArrayFromBuffer( std::size_t n , void* buffer , ARGS&& ...args ) {
  T* b = static_cast<T*>(buffer);
  for( std::size_t i = 0 ; i < n ; ++i ) {
    ::new (b+i) T(std::forward<ARGS>(args)...);
  }
  return b;
}

// Destructor , wrapper aroud std::destroy_at now
template< typename T > void Destruct( T* object ) { std::destroy_at(object); }

// ----------------------------------------------------------------------
// LazyInstance
//   Instantiate via a manual call instead of direct constructor. But the
//   memory is embedded into enclosed object
//
//   The object itself will not check whether this object is already been
//   initialized or not, user needs to make sure not call its Initialize
//   multiple times
// ----------------------------------------------------------------------
template< typename T >
class LazyInstance {
 public:
  LazyInstance() = default;
  template< typename ... ARGS >
  void Init  ( ARGS ...args )  { ConstructFromBuffer<T>(buffer_,args...); }
  void Deinit()                { Destruct(ptr()); }
  T*       ptr()               { return reinterpret_cast<T*>(buffer_); }
  const T* ptr() const         { return reinterpret_cast<const T*>(buffer_); }
  T&       operator * ()       { return *ptr(); }
  const T& operator * () const { return *ptr(); }
  T*       operator ->()       { return ptr();  }
  const T* operator ->() const { return ptr();  }
 private:
  std::uint8_t buffer_[sizeof(T)];

  LAVA_DISALLOW_COPY_AND_ASSIGN(LazyInstance);
};

// ----------------------------------------------------------------------
// CheckedLazyInstance
//   A LazyInstance object that records whether the object is intialized
//   or not. Provides more automation to LazyInstance. Prefer this object
//   than raw LazyInstance.
// ----------------------------------------------------------------------
template< typename T >
class CheckedLazyInstance {
 public:
  CheckedLazyInstance() : instance_() , init_(false) {}
  template< typename ... ARGS >
  void Init( ARGS ... args ) { lava_verify(!init_); instance_.Init(args...); init_ = true;  }
  void Deinit()              { lava_verify(init_) ; instance_.Deinit();      init_ = false; }
  // check whether it is initialized or not
  bool init()          const { return init_; }
 public:
  // accessors
  T*       ptr()               { lava_verify(init_); return instance_.ptr(); }
  const T* ptr() const         { lava_verify(init_); return instance_.ptr(); }
  T*       checked_ptr()       { return init() ? ptr() : NULL; }
  const T* checked_ptr() const { return init() ? ptr() : NULL; }
  T&       operator * ()       { return *ptr(); }
  const T& operator * () const { return *ptr(); }
  T*       operator ->()       { return ptr();  }
  const T* operator ->() const { return ptr();  }
 private:
  LazyInstance<T> instance_;
  bool            init_;

  LAVA_DISALLOW_COPY_AND_ASSIGN(CheckedLazyInstance);
};

// ----------------------------------------------------------------------
// Optional
// ----------------------------------------------------------------------
// Old legacy code, before std::optional we do have a implementation, now it is just
// a wrapper around std::optional. For any future code, we should just use std::optional
template< typename T >
class Optional {
 public:
  Optional() : val_() {}
  explicit Optional( const T& value ): val_(value) {}
  Optional( const Optional& that ) : val_(that.val_) {}
  Optional& operator = ( const Optional& that ) { val_ = that.val_; return *this; }
  void Set( const T& value ) { val_ = value; }
  void Clear()               { val_.reset(); }
  T& Get()                   { lava_verify(Has()); return val_.value(); }
  const T& Get()      const  { lava_verify(Has()); return val_.value(); }
  bool Has()          const  { return val_.has_value(); }
  operator bool ()    const  { return Has(); }
 private:
  std::optional<T> val_;
};

template< typename T >
class SingleNodeLink {
 protected:
  SingleNodeLink() : next_(NULL) {}
  // get the pointer points to next object in chain
  T* NextLink() const { return next_; }
  // add a link *before* the input that node
  void AddLink( T* that ) { next_ = that; }
 private:
  T* next_;
};

// ----------------------------------------------------------------------
// Slice style string comparison
// ----------------------------------------------------------------------
inline
int SliceCmp( const void* , std::size_t , const void* , std::size_t );

// ----------------------------------------------------------------------
// Raw C-Style string
// ----------------------------------------------------------------------
// Wrapper of Slice style string, basically a const void* and a length field
// No memory ownership is managed with this wrapper
//
// TODO:: Replace it with std::string_view ??
struct Str {
  const void* data;
  std::size_t length;
  Str() : data(NULL) , length() {}
  Str( const void* d , std::size_t l ) : data(d), length(l) {}
  bool operator == ( const Str& that ) const { return Str::Cmp(*this,that) == 0; }
  bool operator != ( const Str& that ) const { return Str::Cmp(*this,that) != 0; }
  bool operator <  ( const Str& that ) const { return Str::Cmp(*this,that) <  0; }
  bool operator <= ( const Str& that ) const { return Str::Cmp(*this,that) <= 0; }
  bool operator >  ( const Str& that ) const { return Str::Cmp(*this,that) >  0; }
  bool operator >= ( const Str& that ) const { return Str::Cmp(*this,that) >= 0; }
 public:
  // do a comparison of two chunk of memory/Str
  static inline int Cmp( const Str& , const Str& );
  static inline std::uint32_t Hash( const Str& str ) { return Hasher::Hash(str.data,str.length); }
  static std::string ToStdString  ( const Str& str ) {
    return std::string(static_cast<const char*>(str.data),str.length);
  }
};

} // namespace lavascript

// Hash wrapper for Str object to be used by std::unordered_xxx
namespace std {

template<> struct hash<::lavascript::Str> {
  typedef ::lavascript::Str type;
  std::size_t operator () ( const type& expr ) const {
    return static_cast<std::uint32_t>(type::Hash(expr));
  }
};

template<> struct equal_to<::lavascript::Str> {
  typedef ::lavascript::Str type;
  bool operator () ( const type& lhs , const type& rhs ) const {
    return type::Cmp(lhs,rhs) == 0;
  }
};

} // namespace std

#include "util-inl.h"

#endif // UTIL_H_
