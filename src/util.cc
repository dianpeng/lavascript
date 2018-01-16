#include "util.h"

#include <cstring>
#include <cstdio>
#include <cerrno>
#include <cstdlib>

namespace lavascript {

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

bool StringToInt( const char* source , std::int32_t * output ) {
  char* pend = NULL;
  errno = 0;
  std::int32_t val = std::strtol(source,&pend,10);
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

std::string PrettyPrintReal( double real ) {
  std::string result = std::to_string(real);
  /**
   * Now we try to remove all the tailing zeros in the returned
   * std::string result value.
   */
  size_t npos = result.find_last_of('.');
  if(npos == std::string::npos) {
    return result;
  } else {
    npos = result.find_last_not_of('0');
    if(npos != std::string::npos) {
      result.erase(npos+1,std::string::npos);
    }

    // remove the trailing dot if we need to
    if(result[result.size()-1] == '.') {
      result.pop_back();
    }
    return result;
  }
}

} // namespace lavascript
