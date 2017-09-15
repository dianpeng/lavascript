#include "heap-object-header.h"

namespace lavascript {

const char* GetValueTypeName( ValueType vt ) {
#define __(A,B,C) case A: return C;
  switch(vt) {
    LAVASCRIPT_VALUE_TYPE_LIST(__)
    default: return NULL;
  }
#undef __ // __
}

const char* GetGCStateName( GCState state ) {
  switch(state) {
    case GC_BLACK : return "black";
    case GC_WHITE:  return "white";
    case GC_GRAY:   return "gray";
    default: return "reserved";
  }
}

} // namespace lavascript
