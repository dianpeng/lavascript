#include "fold-box.h"
#include "src/cbase/hir.h"

namespace lavascript {
namespace cbase      {
namespace hir        {

Expr* FoldBoxNode( Expr* node, TypeKind tk ) {
  // You can only box a node when the node is already in Unbox version
}

} // namespace hir
} // namespace cbase
} // namespace lavascript
