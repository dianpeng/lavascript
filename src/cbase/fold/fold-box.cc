#include "fold-box.h"
#include "src/cbase/hir.h"

namespace lavascript {
namespace cbase      {
namespace hir        {

Expr* FoldBox( Expr* node , TypeKind tk ) {
  if(node->Is<Box>()) {
    lava_debug(NORMAL,lava_verify(tk == node->As<Box>()->type_kind()););
    return NULL;
  } else {
    return node->IsBoxNode() ? node : NULL;
  }
}

Expr* FoldUnbox( Expr* node , TypeKind tk ) {
  if(node->Is<Unbox>()) {
    lava_debug(NORMAL,lava_verify(tk == node->As<Unbox>()->type_kind()););
    return node;
  } else if(node->Is<Box>()) {
    auto b = node->As<Box>();
	  lava_debug(NORMAL,lava_verify(b->type_kind() == tk););
    return b->value();
  } else {
    lava_debug(NORMAL,
			switch(node->type()) {
				case HIR_FLOAT64:
				case HIR_FLOAT64_NEGATE:
				case HIR_FLOAT64_ARITHMETIC:
				case HIR_FLOAT64_BITWISE:
					lava_debug(NORMAL,lava_verify(tk == TPKIND_FLOAT64);); break;
				case HIR_FLOAT64_COMPARE:
				case HIR_STRING_COMPARE:
				case HIR_SSTRING_EQ:
				case HIR_SSTRING_NE:
					lava_debug(NORMAL,lava_verify(tk == TPKIND_BOOLEAN);); break;
        default: break;
			}
    );
    return node->IsUnboxNode() ? node : NULL;
  }
}

Expr* NewBoxNode( Graph* graph , Expr* node , TypeKind tk ) {
  if(auto nnode = FoldBox(node,tk); nnode)
    return nnode;
  return Box::New(graph,node,tk);
}

Expr* NewUnboxNode( Graph* graph , Expr* node , TypeKind tk ) {
  if(auto nnode = FoldUnbox(node,tk); nnode)
    return nnode;
  return Unbox::New(graph,node,tk);
}

} // namespace hir
} // namespace cbase
} // namespace lavascript
