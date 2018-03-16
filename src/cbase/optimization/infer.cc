#include "infer.h"
#include "src/stl-helper.h"
#include "src/cbase/dominators.h"
#include "src/cbase/value-range.h"
#include "src/zone/zone.h"

namespace lavascript {
namespace cbase      {
namespace hir        {

using namespace ::lavascript;

namespace {

// Check if a condition expression is suitable for doing inference or not. If not then
// just needs to mark this condition as bailout condition group object.
//
// This checker doesn't use normal constraint visitor but manually visit the expression
//
// The algorithm does a *pre-order* visiting and selectively visit its children. The basic
// idea is that the top most of a expression or sub-expression must be logic operator;
// otherwise bailout. For qualified sub-expression, we just need to check whether it is a
// form that we can accept and also check the participate in variable is 1) typped 2) is
// only variable.

class SimpleConstraintChecker {
 public:
  // Main function to check the expression whether it is a simple constraint.
  // The output is the pointer points to the main variable and the type for
  // this main variable. Otherwise bailout.
  bool Check( Expr* , Expr** , TypeKind* );

 private:
  // do the actual check for a logic combined operation
  bool DoCheck  ( Expr* lhs , Expr* rhs ) { return CheckExpr(lhs) && CheckExpr(rhs); }
  bool CheckExpr( Expr* );

  /**
   * The only important HIR node is BooleanLogic node. Since all the required node must
   * be typped and the possible type are 1) float64 2) boolean ; these two types comparison
   * operation will result in boolean value so the logic node to combine them must be
   * the typped hir which is BooleanLogic node. And if we see any other types of none primitive
   * node, we could just bailout directly
   */
  Expr*     expr_;
  Expr*     variable_;
  TypeKind* type_;
};


/**
 * Condition group object. Used to represent a simple constraint whenever a branch node
 * is encountered.
 *
 * We only support simple constraint, a simple constraint means a condition for a branch
 * node that *only* has one variables and also it must be typpped with certain types.
 *
 * The current support types are 1) float64 2) boolean. And the form must be as following:
 *
 * 1) for float64
 *    between compare operator, one side must be that variable the other side must be the
 *    constant value.
 *
 * 2) for boolean
 *    the variable itself or !variable. The situation that `bval == true or bval == false`
 *    has been transformmed into standard form during the fold phase
 *
 * Any other forms condition will bailout to not performing inference in its nested blocks
 */

class ConditionGroup {
 public:
  ConditionGroup( zone::Zone* zone , ConditionGroup* p ):
    zone_(zone) , prev_(p), variable_(NULL), type_kind_(TPKIND_UNKNOWN), range_(NULL)
  {}

  // Call this function to construct a condition group
  bool Build ( Expr* );

 public:
  bool IsBailout() const { return variable_ == NULL; }

  Expr* variable() const { lava_debug(NORMAL,lava_verify(!IsBailout());); return variable_; }

  TypeKind type_kind() const { return type_kind_; }

  ConditionGroup* prev() const { return prev_; }

  ValueRange* range() const { return range_; }

 public: // apis
  int   Infer   ( Binary::Operator , Expr* ) const;
  Expr* Collapse( Graph* , IRInfo* )         const;

 private:
  void DoSimplify ( Expr* );
  void DoConstruct( Expr* );

 private:
  zone::Zone*     zone_;
  ConditionGroup* prev_;     // previous condition group if it has a immediate dominator
  Expr*           variable_; // if this variable is NULL, then it means it bailout
  TypeKind        type_kind_;
  ValueRange*     range_;
};

bool ConditionGroup::Build( Expr* node ) {
  SimpleConstraintChecker checker;
  if(!checker.Check(node,&variable_,&type_kind_)) {
    goto bailout;
  }

  // do a check whether the dominator condition group has same
  // type with same variable
  if(prev_) {
    if(prev_->variable() == variable_) {
      if(prev_->type_kind() != type_kind_) {
        goto bailout;
      }
    }
  }

  // construct the specific type of value range
  if(t == TPKIND_FLOAT64) {
    range_ = zone_->New<Float64ValueRange>(zone_);
  } else {
    lava_debug(NORMAL,lava_verify(t == TPKIND_BOOLEAN););
    range_ = zone_->New<BooleanValueRange>(zone_);
  }

  // second pass of the expression to do simple fold if we can do inference
  // against our dominator condition group
  DoSimplify(node);

  // last pass , which is used to construct the new constraint
  DoConstruct(node);

  return true;

bailout:
  variable_ = NULL;
  return false;
}

bool SimpleConstraintChecker::CheckExpr( Expr* expr ) {
  /**
   * For Float64 type, we only need to check whether expression is
   *  1) Float64Compare
   *  2) BooleanLogic
   *
   * For Boolean type, we only need to check whether expression is
   *  1) None constant node
   *  2) BooleanLogic
   */

  if(expr->IsFloat64Compare()) {
    if(*type_ != TPKIND_BOOLEAN) {
      *type_ = TPKIND_FLOAT64;

      auto fcomp = expr->AsFloat64Compare();

      // check the comparison format to be correct or not
      auto lhs = fcomp->lhs();
      auto rhs = fcomp->rhs();

      if(lhs->IsFloat64()) {
        // constant < variable style
        if(variable_)
          return rhs == variable_;
        else {
          variable_ = rhs;
          return true;
        }
      } else if(rhs->IsFloat64()) {
        if(variable_)
          return lhs == variable_;
        else {
          variable_ = lhs;
          return true;
        }
      }
    }

  } else if(expr->IsBooleanLogic()) {
    auto l = expr->AsBooleanLogic();
    return DoCheck(l->lhs(),l->rhs());
  } else {
    if(GetTypeInference(expr) == TPKIND_BOOLEAN && *type_ != TPKIND_FLOAT64) {
      *type_ = TPKIND_BOOLEAN;

      // do a dereference if the node is a ! operation
      expr = expr->IsBooleanNot() ? expr->AsBooleanNot()->operand() : expr;

      if(variable_) {
        return variable_ = expr;
      } else {
        variable_ = expr;
        return true;
      }
    }
  }

  // bailout
  return false;
}

bool SimpleConstraintChecker::Check( Expr* node , Expr** variable , TypeKind* tp ) {
  expr_      = node;
  variable_  = NULL;
  type_      = TPKIND_UNKNOWN;

  auto ret   = false;

  if(node->IsBooleanLogic()) {
    auto l = expr->AsBooleanLogic();
    ret = DoCheck(l->lhs(),l->rhs());
  } else {
    ret = CheckExpr(node);
  }

  if(ret) {
    lava_debug(NORMAL,lava_verify(variable_);
                      lava_verify(type_ == TPKIND_BOOLEAN || type_ == TPKIND_FLOAT64););
    *tp = type_;
    *variable = variable_;
  }

  return ret;
}

} // namespace


} // namespace hir
} // namespace cbase
} // namespace lavascript
