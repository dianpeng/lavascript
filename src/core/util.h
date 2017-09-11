#ifndef CORE_UTIL_H_
#define CORE_UTIL_H_

#include <cstdarg>
#include <cstddef>
#include <string>

#define LAVA_DISALLOW_COPY_AND_ASSIGN(X) \
  void operator = (const X&); \
  X(const X&)

namespace lavascript {
namespace core {

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

} // namespace core
} // namespace lavascript

#endif // CORE_UTIL_H_
