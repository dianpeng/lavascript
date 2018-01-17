#ifndef CBASE_HIR_VISITOR_H_
#define CBASE_HIR_VISITOR_H_
#include "hir.h"

namespace lavascript {
namespace cbase {
namespace hir {

/** -----------------------------------------------------------------------------
 * Visitors
 *
 * The visitor can be used for different edge iterator to do dispatch. There're
 * dispatch function to help you do the dispatch job
 */

class ExprVisitor {
 public:
#define __(A,...) virtual bool Visit##A( A* expr ) { (void)expr; return true; }
  CBASE_IR_EXPRESSION(__)
#undef __ // __
  virtual ~ExprVisitor() = 0;
};

class ControlFlowVisitor {
 public:
#define __(A,...) virtual bool Visit##A( A* node ) { (void)node; return true; }
  CBASE_IR_CONTROL_FLOW(__)
#undef __ // __
  virtual ~ControlFlowVisitor() = 0;
};

/**
 * Visit a expression based on the input iterator type and invoke the corresponding
 * expression via correct routine in the visitor
 */
template< typename T >
bool VisitExpr( T* itr , ExprVisitor* visitor );


/**
 * Visit a control flow based on the input iterator type and invoke the corresponding
 * control flow via correct routine in the visitor
 */
template< typename T >
bool VisitControlFlow( T* itr , ControlFlowVisitor* visitor );

} // namespace hir
} // namespace cbase
} // namespace lavascript

// the implementation goes to another inline file
#include "hir-visitor-inl.h"

#endif // CBASE_HIR_VISITOR_H_
