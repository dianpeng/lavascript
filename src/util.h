#ifndef UTIL_H_
#define UTIL_H_

#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS
#endif // __STDC_FORMAT_MACROS

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

#include "trace.h"

#define LAVA_FMTI64 PRId64
#define LAVA_FMTU64 PRIu64

namespace lavascript {

// template function to get the size of an C array
template< std::size_t N , typename T >
size_t ArraySize( const T (&arr)[N] ) { (void)arr; return N; }

// format a vararg into a std::string
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

// convert std::string to a raw c buffer
inline char* AsBuffer( std::string* output , std::size_t off )
{ return &(*output->begin()) + off; }

// convert std::vector to raw c buffer
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

// object level memcpy/memmove/memset functions
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
  return reinterpret_cast<void*>(
      static_cast<T*>(buffer) + offset);
}

// lexcial cast to convert string representation of number into primitive type
inline bool LexicalCast( const char* , std::int32_t* );
inline bool LexicalCast( const char* , std::uint32_t* );
inline bool LexicalCast( const char* , std::int64_t* );
inline bool LexicalCast( const char* , std::uint64_t* );
inline bool LexicalCast( const char* , double* );
inline bool LexicalCast( double , std::string* output );
inline bool LexicalCast( bool   , std::string* output );

// Try to narrow a real number into a 32 bits integer. It will fail
// if the double has exponent part , and also fail if its size cannot
// be put into the int32_t value.
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
double CastRealAndStoreAsReal( double real ) { return static_cast<double>( CastReal<T>(real) ); }

// align a number against another number std::align is kind of hard to use
template< typename T > T Align( T value , T alignment ) {
  return (value + (alignment-1)) & ~(alignment-1);
}

// placement new wrapper , use c++11 perfect forwarding mechanism
template< typename T , typename Allocator , typename ... ARGS >
T* Construct( Allocator* allocator , ARGS ...args ) {
  return ::new (allocator->Grab(sizeof(T))) T(std::forward(args)...);
}

template< typename T , typename Allocator >
T* Construct( Allocator* allocator ) {
  return ::new (allocator->Grab(sizeof(T))) T();
}

template< typename T , typename ... ARGS >
T* ConstructFromBuffer( void* buffer , ARGS ...args ) {
  return ::new (buffer) T(args...);
}

template< typename T >
T* ConstructFromBuffer( void* buffer ) {
  return ::new (buffer) T();
}

template< typename T > void Destruct( T* object ) {
  object->~T();
}

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

// Wrapper of Slice style string, basically a const char* and a length field
// No memory ownership is managed with this wrapper
struct Str {
  const void* data;
  std::size_t length;
  Str() : data(NULL) , length() {}
  Str( const void* d , std::size_t l ) : data(d), length(l) {}
  // do a comparison of two chunk of memory/Str
  static inline int Cmp( const Str& , const Str& );
};

} // namespace lavascript

#include "util-inl.h"

#endif // UTIL_H_
