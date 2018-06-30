#ifndef CBASE_FOLD_BOX_H_
#define CBASE_FOLD_BOX_H_
#include "src/cbase/type.h"

namespace lavascript {
namespace cbase      {
namespace hir        {

class Graph;
class Expr;
class Box;
class Unbox;

// Fold w.r.t Box/Unbox node
Expr* FoldBoxNode  ( Expr* , TypeKind );
Expr* FoldUnboxNode( Expr* , TypeKind );

// Create a box node based on the input node. This function will take care of the
// folding process. Basically it only creates the box node when it needs to; otherwise
// it just returns the old node since it is already in boxed status
Expr* NewBoxNode  ( Graph* , Expr* , TypeKind );


// Create a unbox node based on the input node. This function will take care of the
// fold process. Basically it only creates the unbox node when it needs to; otherwise
// it just returns a the old node since it is in the unboxed status.
Expr* NewUnboxNode( Graph* , Expr* , TypeKind );


// This template function create a node and then box it based on the type.
template< typename T , typename ...ARGS >
inline Box* NewBoxNode( Graph* graph , TypeKind tk , ARGS ...args ) {
  auto n = T::New(graph,args...);
  return NewBoxNode(graph,n,tk);
}

} // namespace lavascript
} // namespace cbase
} // namespace hir

#endif // CBASE_FOLD_BOX_H_
