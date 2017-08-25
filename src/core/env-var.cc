#include "env-var.h"

#include <cstring>
#include <cstdlib>

namespace lavascript {
namespace core {

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

bool GetEnvVar( const char* name , int* value ) {
  const char* val;
  if(!GetEnvVar(name,&val))
    return false;
  {
    char* pend = NULL;
    int ival = strtol( name , &pend , 10 );
    if(errno || pend)
      return false;
    *value = ival;
  }
  return true;
}

bool GetEnvVar( const char* name , bool* value ) {
  const char* val;
  if(!GetEnvVar(name,&val))
    return false;
  if(strcmp(val,"true") ==0)
    *value = true;
  else if(strcmp(val,"false") == 0)
    *value = false;
  else
    return false;
  return true;
}

} // namespace core
} // namespace lavascript
