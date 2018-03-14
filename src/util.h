#ifndef UTIL_H_
#define UTIL_H_

#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS
#endif // __STDC_FORMAT_MACROS

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

template< std::size_t N , typename T >
size_t ArraySize( const T (&arr)[N] ) { (void)arr; return N; }

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

template< typename T >
inline void* BufferOffset( void* buffer , std::size_t offset ) {
  return reinterpret_cast<void*>(
      static_cast<T*>(buffer) + offset);
}

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
T CastReal( double real ) {
  return static_cast<T>(real);
}

template< typename T >
double CastRealAndStoreAsReal( double real ) {
  return static_cast<double>( CastReal<T>(real) );
}

template< typename T > T Align( T value , T alignment ) {
  return (value + (alignment-1)) & ~(alignment-1);
}

template< typename T , typename Allocator , typename ... ARGS >
T* Construct( Allocator* allocator , ARGS ...args ) {
  return ::new (allocator->Grab(sizeof(T))) T(args...);
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

template< typename T >
class Optional {
 public:
  Optional():value_(),has_(false) {}
  Optional( const T& value ):value_(),has_(false) { Set(value); }
  Optional( const Optional& opt ):
    value_(),
    has_  (opt.has_) {
    if(has_) Copy(opt.Get());
  }
  Optional& operator = ( const Optional& that ) {
    if(this != &that) {
      Clear();
      if(that.Has()) Set(that.Get());
    }
    return *this;
  }
  ~Optional() { Clear(); }
 public:
  void Set( const T& value ) {
    Clear();
    Copy(value);
    has_ = true;
  }
  void Clear() {
    if(has_) {
      Destruct( reinterpret_cast<T*>(value_) );
      has_ = false;
    }
  }
  T& Get() {
    lava_verify(has_);
    return *reinterpret_cast<T*>(value_);
  }
  const T& Get() const {
    lava_verify(has_);
    return *reinterpret_cast<const T*>(value_);
  }
  bool Has() const { return has_; }
  operator bool () const { return has_; }
 private:
  void Copy( const T& value ) {
    ConstructFromBuffer<T>(reinterpret_cast<T*>(value_),value);
  }

  std::uint8_t value_[sizeof(T)];
  bool has_;
};

} // namespace lavascript

#include "util-inl.h"

#endif // UTIL_H_
