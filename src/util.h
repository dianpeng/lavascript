#ifndef UTIL_H_
#define UTIL_H_

#include <cstdarg>
#include <cstddef>
#include <string>
#include <new>

namespace lavascript {

template< size_t N , typename T >
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

inline char* AsBuffer( std::string* output , size_t off )
{ return &(*output->begin()) + off; }

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

} // namespace lavascript

#endif // UTIL_H_
