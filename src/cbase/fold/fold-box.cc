#include "fold-box.h"
#include "src/cbase/hir.h"

namespace lavascript {
namespace cbase      {
namespace hir        {

Expr* FoldBoxNode( Expr* node , TypeKind tk ) {
  if(node->Is<Box>()) {
    lava_debug(NORMAL,lava_verify(tk == node->As<Box>()->type_kind()););
    return NULL;
  } else {
    return node->IsBoxNode() ? node : NULL;
  }
}

Expr* FoldUnboxNode( Expr* node , TypeKind tk ) {
  if(node->IsUnboxNode()) {
  } else {
  }
}


} // namespace hir
} // namespace cbase
} // namespace lavascript
