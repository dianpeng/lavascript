#include "infer.h"
#include "src/cbase/fold.h"
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
  TypeKind  type_;
};

// MultiTypeValueRange is an object that is used to track multiple type value's range
// independently. Example like this:
//
// if(a > 1 && a < 2) {
//   if(b) {
//     if(c == 2) {
//       if(d > 1) {
//         if(a > 1.5) {
//           when we reach here, the multiple type value range will contain 4 different
//           independent value range object and the value range of a is already merged.
//
//           Any nested blocks that is domminated by the if(a > 1.5) block will only need
//           to consult the range contained here to know which value needs to infer. It
//           doesn't need to go up the chain. Basically the value collapsing right inside
//           of this dominated block
//         }
//       }
//     }
//   }
// }
//
// This mechanism will not use much memory due to the fact for any block's multiple type
// value range , if this block doesn't contain any enforcement of the value range, it will
// just store a pointer of those block that is owned by its dominator block.

class MultiTypeValueRange : public zone::ZoneObject {
 public:
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
  enum ConditionType {
    BAILOUT,
    DEAD_BRANCH,
    NULL_BRANCH,
    NORMAL
  };

  ConditionGroup( Graph* graph , zone::Zone* zone , ConditionGroup* p ):
    graph_(graph),
    zone_(zone) ,
    prev_(p),
    variable_(NULL),
    type_kind_(TPKIND_UNKNOWN),
    range_(NULL),
    type_(BAILOUT)
  {}

  ConditionType Process ( Expr* );

 public:
  ConditionType type () const { return type_; }

  bool IsBailout() const { return type_ == BAILOUT; }
  bool IsDead   () const { return type_ == DEAD_BRANCH; }
  bool IsNull   () const { return type_ == NULL_BRANCH; }
  bool IsNormal () const { return type_ == NORMAL; }

  Expr* variable() const { lava_debug(NORMAL,lava_verify(!IsBailout());); return variable_; }

  TypeKind type_kind() const { return type_kind_; }

  ConditionGroup* prev() const { return prev_; }

  ValueRange* range() const { return range_; }

 private:
  // Do a recursive inference with all the dominator node's condition group
  int  Infer( Expr* , TypeKind );
  bool InferLocal( Expr* , TypeKind , int* );

 private:
  // create a new boolean node with value specified to *replace* the input node
  Expr* DeduceTo ( Expr* , bool );

  // Simplification phase of expression. This phase will just try to deduce the
  // expression and it will not generate a new value range object
  Expr* Simplify   ( Expr* );
  Expr* DoSimplify ( Expr* );


  void Construct           ( Expr* );
  void ConstructSub        ( ValueRange* , Expr* );
  ValueRange* ConstructSub ( Expr* );

  void ConstructFloat64Expr( ValueRange* , Float64Compare* , bool op = false );

 private:

  Graph*          graph_;
  zone::Zone*     zone_;
  ConditionGroup* prev_;
  Expr*           variable_;
  TypeKind        type_kind_;
  ValueRange*     range_;
  ConditionType   type_;
};


Expr* ConditionGroup::DoSimplify( Expr* node ) {
  switch(node->type()) {
    case IRTYPE_FLOAT64_COMPARE:
      goto do_infer;
    case IRTYPE_BOOLEAN_LOGIC:
      {
        auto n = node->AsBooleanLogic();
        auto l = n->lhs();
        auto r = n->rhs();
        auto op= n->op();

        auto lret = DoSimplify(l);
        auto rret = DoSimplify(r);

        auto nnode = FoldBinary(graph_,n->lhs(),n->rhs(),op,[=]() { return n->ir_info(); });
        if(nnode) {
          node->Replace(nnode);
          return nnode;
        }

        return NULL;
      }
    default:
      {
        // when we reach here it means it must be a boolean type
        lava_debug(NORMAL,lava_verify(type_kind_ == TPKIND_BOOLEAN););
        lava_debug(NORMAL,
            auto v = node;
            if(v->IsBooleanNot()) {
              v = v->AsBooleanNot()->operand();
            }
            lava_verify(v == variable_);
        );
        goto do_infer;
      }
  }

  lava_die();

do_infer:
  /** do single node inference **/
  auto ret = prev_->Infer( node , type_kind_ );
  if(ret == ValueRange::ALWAYS_TRUE) {
    return DeduceTo(node,true);
  } else if(ret == ValueRange::ALWAYS_FALSE) {
    return DeduceTo(node,false);
  }
  return NULL;
}

ConditionGroup::ConditionType ConditionGroup::Simplify( Expr* node ) {
}

void ConditionGroup::ConstructFloat64Expr( ValueRange* output , Float64Compare* comp ,
                                                                bool is_union ) {
  auto var  = comp->lhs()->IsFloat64() ? comp->rhs() : comp->lhs() ;
  auto cst  = comp->lhs()->IsFloat64() ? comp->lhs() : comp->rhs() ;

  lava_debug(NORMAL,lava_verify(var == variable_););
  lava_debug(NORMAL,lava_verify(type_kind_ == TPKIND_FLOAT64););

  if(is_union)
    output->Union(comp->op(),cst);
  else
    output->Intersect(comp->op(),cst);
}

void ConditionGroup::ConstructSub( ValueRange* output , Expr* node ) {
}

void ConditionGroup::Construct( Expr* node ) {

  if(node->type() == IRTYPE_BOOLEAN_LOGIC) {
    auto b = node->AsBooleanLogic();

    // lhs
    if(b->lhs()->IsFloat64Compare()) {
      ConstructFloat64Expr(range_,b->lhs()->AsFloat64Compare(),true);
    } else {
      auto temp = ConstructSub(b->lhs());
      range_->Union(*temp);
    }

    // rhs
    if(b->rhs()->IsFloat64Compare()) {
      ConstructFloat64Expr(range_,b->rhs()->AsFloat64Compare(),(b->op() == Binary::AND));
    } else {
      auto temp = ConstructSub(b->rhs());
      if(b->op() == Binary::AND)
        range_->Intersect(*temp);
      else
        range_->Union(*temp);
    }
  } else if(node->type() == IRTYPE_FLOAT64_COMPARE) {
  }
}

ConditionGroup::ConditionType ConditionGroup::Process( Expr* node ) {
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

  // second pass to do a simplification with inference from its dominator
  // node's ConditionGroup object
  switch(Simplify(node)) {
    case BAILOUT:
    case DEAD_BRANCH:
    case NULL_BRANCH:
      return type_;
    default:
      break;
  }

  // construct the specific type of value range
  if(type_kind_ == TPKIND_FLOAT64) {
    range_ = zone_->New<Float64ValueRange>(zone_);
  } else {
    lava_debug(NORMAL,lava_verify(t == TPKIND_BOOLEAN););
    range_ = zone_->New<BooleanValueRange>(zone_);
  }

  // last pass , which is used to construct the new constraint
  Construct(node);

  return (type_ = NORMAL);

bailout:
  return (type_ = BAILOUT);
}

// ========================================================
//
// SimpleConstraintChecker implementation
//
// ========================================================

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
    if(type_ != TPKIND_BOOLEAN) {
      type_ = TPKIND_FLOAT64;

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
    if(GetTypeInference(expr) == TPKIND_BOOLEAN && type_ != TPKIND_FLOAT64) {
      type_ = TPKIND_BOOLEAN;

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
    auto l = node->AsBooleanLogic();
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
