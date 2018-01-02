#include "ir.h"

namespace lavascript {
namespace cbase {
namespace ir {

const char* IRTypeGetName( IRType type ) {
#define __(A,B,C) case IRTYPE_##B: return C;
  switch(type) {
    CBASE_IR_LIST(__)
    default: lava_die(); return NULL;
  }
#undef __ // __
}

} // namespace ir
} // namespace cbase
} // namespace lavascript
