#ifndef UTIL_H_
#define UTIL_H_
#include <cstdint>
#include <cstdarg>
#include <cstddef>
#include <string>
#include <string.h>
#include <vector>
#include <new>

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
  return static_cast<T*>(memcpy(dest,from,size*sizeof(T)));
}

template< typename T >
inline T* MemCopy( T* dest, const std::vector<T>& from ) {
  return static_cast<T*>(memcpy(dest,AsBuffer(&from,0),from.size()*sizeof(T)));
}

template< typename T >
inline void* BufferOffset( void* buffer , std::size_t offset ) {
  return reinterpret_cast<void*>(
      static_cast<T*>(buffer) + offset);
}

bool StringToInt    ( const char* , int* );
bool StringToReal   ( const char* , double* );
bool StringToBoolean( const char* , bool* );

/**
 * pretty print the real number. The issue with std::to_string is that
 * the tailing zeros are gonna show up. Example: 1.234 --> "1.234000".
 * We need to remove the tailing zeros to make the return value more
 * predictable
 */
std::string PrettyPrintReal( double );

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

#endif // UTIL_H_
