#include "util.h"

#include <cstring>
#include <cstdio>
#include <cerrno>
#include <cstdlib>

namespace lavascript {
namespace core {


void FormatV(std::string* buffer, const char* format, va_list vl) {
  va_list backup;
  size_t old_size = buffer->size();

  va_copy(backup, vl);
  buffer->resize(old_size + 128);

  int ret = vsnprintf(AsBuffer(buffer, old_size), 128, format, vl);
  if (ret >= 128) {
    buffer->resize(old_size + ret);
    ret = vsnprintf(AsBuffer(buffer, old_size), ret, format, backup);
  }
  buffer->resize(old_size + ret);
}

bool StringToInt( const char* source , int* output ) {
  char* pend = NULL;
  errno = 0;
  int val = std::strtol(source,&pend,10);
  if(errno || *pend) return false;
  *output = val;
  return true;
}

bool StringToReal( const char* source, double* output ) {
  char* pend = NULL;
  errno = 0;
  double val = std::strtod(source,&pend);
  if(errno || *pend) return false;
  *output = val;
  return true;
}

bool StringToBoolean( const char* source , bool* output ) {
  if(strcmp(source,"true") ==0) {
    *output = true;
  } else if(strcmp(source,"false") == 0) {
    *output = false;
  } else {
    return false;
  }
  return true;
}


} // namespace core
} // namespace lavascript
