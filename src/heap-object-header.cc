#include "heap-object-header.h"

namespace lavascript {

const char* GetGCStateName( GCState state ) {
  switch(state) {
    case GC_BLACK : return "black";
    case GC_WHITE:  return "white";
    case GC_GRAY:   return "gray";
    default:        return "reserved";
  }
}

} // namespace lavascript
