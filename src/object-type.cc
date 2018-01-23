#include "object-type.h"
#include <cstddef>

namespace lavascript {

const char* GetValueTypeName( ValueType vt ) {
#define __(A,B,C) case A: return C;
  switch(vt) {
    LAVASCRIPT_VALUE_TYPE_LIST(__)
    default: return NULL;
  }
#undef __ // __
}

} // namespace lavascript
