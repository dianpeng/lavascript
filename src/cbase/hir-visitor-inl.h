#ifndef CBASE_HIR_VISITOR_INL_H_
#define CBASE_HIR_VISITOR_INL_H_

namespace lavascript {
namespace cbase {
namespace hir {

namespace detail {

inline bool VisitExprNode( Expr* node , ExprVisitor* visitor ) {
  lava_debug(NORMAL,lava_verify(node->IsExpr()););

#define __(A,B,C,D) case IRTYPE_##B: return visitor->Visit##A(node->As##A());
  switch(node->type()) {
    CBASE_IR_EXPRESSION(__)
    default: lava_die(); return false;
  }
#undef __ // __
}

inline bool VisitControlFlowNode( ControlFlow* node , ControlFlowVisitor* visitor ) {
  lava_debug(NORMAL,lava_verify(node->IsControlFlow()););

#define __(A,B,C,D) case IRTYPE_##B: return visitor->Visit##A(node->As##A());
  switch(node->type()) {
    CBASE_IR_CONTROL_FLOW(__)
    default: lava_die(); return false;
  }
#undef __ // __
}

} // namespace detail

template< typename T >
bool VisitExpr( T* itr , ExprVisitor* visitor ) {
  static_assert( IsExprIterator<T>() );
  for( ; itr->HasNext(); itr->Move() ) {
    if(!detail::VisitExprNode(itr->value(),visitor)) return false;
  }
  return true;
}

template< typename T >
bool VisitControlFlow( T* itr , ControlFlowVisitor* visitor ) {
  static_assert( IsControlFlowIterator<T>() );
  for( ; itr->HasNext() ; itr->Move() ) {
    if(!detail::VisitControlFlowNode(itr->value(),visitor)) return false;
  }
  return true;
}

} // namespace hir
} // namespace cbase
} // namespace lavascript

#endif // CBASE_HIR_VISITOR_INL_H_
