#include "type.h"
#include "src/trace.h"

namespace lavascript {
namespace cbase {

const char* GetTypeKindName( TypeKind kind ) {
  switch(kind) {
#define __(A,B) case TPKIND_##B : return #A;
    LAVASCRIPT_CBASE_TYPE_KIND_LIST(__)
#undef __ // __
    default: lava_die(); return NULL;
  }
}

} // namespace cbase
} // namespace lavascript
