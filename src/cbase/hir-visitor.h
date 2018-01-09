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
#define __(A,...) virtual bool Visit##A( A* expr ) { (void)expr; return false; }
  CBASE_IR_EXPRESSION(__)
#undef __ // __
  virtual ~ExprVisitor() = 0;
};

class ControlFlowVisitor {
 public:
#define __(A,...) virtual bool Visit##A( A* node ) { (void)node; return false; }
  CBASE_IR_CONTROL_FLOW(__)
#undef __ // __
  virtual ~ControlFlowVisitor() = 0;
};

class HIRVisitor : public ExprVisitor , ControlFlowVisitor {
 public:
  virtual ~HIRVisitor() = 0;
};


} // namespace hir
} // namespace cbase
} // namespace lavascript

#endif // CBASE_HIR_VISITOR_H_
