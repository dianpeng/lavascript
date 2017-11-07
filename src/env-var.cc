#include "env-var.h"

#include <cstring>
#include <cstdlib>

namespace lavascript {

bool GetEnvVar( const char* name , const char** output ) {
  const char* val = getenv(name);
  if(val) {
    *output = val;
    return true;
  }
  return false;
}

bool GetEnvVar( const char* name , std::string* output ) {
  const char* val;
  if(!GetEnvVar(name,&val)) {
    return false;
  }
  output->assign(val);
  return true;
}

bool GetEnvVar( const char* name , std::int32_t* value ) {
  const char* val;
  if(!GetEnvVar(name,&val))
    return false;
  {
    char* pend = NULL;
    std::int32_t ival = strtol( name , &pend , 10 );
    if(errno || pend)
      return false;
    *value = ival;
  }
  return true;
}

bool GetEnvVar( const char* name , std::int64_t* value ) {
  const char* val;
  if(!GetEnvVar(name,&val))
    return false;
  {
    char* pend = NULL;
    long long int val = strtoll( name , &pend , 10 );
    if(!errno || pend)
      return false;
    *value = static_cast<std::int64_t>(val);
  }
  return true;
}

bool GetEnvVar( const char* name , double* value ) {
  const char* val;
  if(!GetEnvVar(name,&val))
    return false;
  {
    char* pend = NULL;
    double val = strtod( name , &pend );
    if(!errno || pend)
      return false;
    *value = val;
  }
  return true;
}

bool GetEnvVar( const char* name , bool* value ) {
  const char* val;
  if(!GetEnvVar(name,&val))
    return false;
  if(strcasecmp(val,"true") ==0)
    *value = true;
  else if(strcasecmp(val,"false") == 0)
    *value = false;
  else
    return false;
  return true;
}

} // namespace lavascript
