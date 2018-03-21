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

// MultiValueRange is an object that is used to track multiple type value's range
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
//
//
// This object should be used in 2 steps :
// 1) Get a clone from its domminator node if it has one
//
// 2) Mutate it directly , since essentially this object is copy on write style, so internally
//    it will duplicate the value range if you modify one

class MultiValueRange : public zone::ZoneObject {
 public:
  inline MultiValueRange( zone::Zone* );

  // Inherit this empty object from another object typically comes from its
  // immediate dominator node's value range.
  void Inherit      ( MultiValueRange* );

  // Call this function to setup the node's value range if needed; this is
  // used for set up new constraint when enter into the current branch node
  void SetCondition ( Expr* , TypeKind , ValueRange* );

  void Clear() { table_.Clear(); }

 public:
  // inference
  int  Infer     ( Binary::Operator , Expr* );
  Expr* Collapse ( Graph* , IRInfo* ) const;

 private:
  ValueRange* NewValueRange     ( ValueRange* that = NULL );
  ValueRange* MaybeCopy         ( Expr* );
  bool        TestTypeCompatible( TypeKind , ValueRange* );


 private:
  zone::Zone* zone_;

  // the object that will be stored inside of the table internally held by this object.
  // it basically records whether the range stored here is a reference or not; since we
  // support copy on write semantic, we will duplicate the range object on the fly if we
  // try to modify it
  struct Item : public zone::ZoneObject {
    bool ref;             // whether this object is a reference
    ValueRange* range;    // the corresponding range object

    Item() : ref() , range(NULL) {}
    Item( ValueRange* r ) : ref(true) , range(r) {}
    Item( bool r , ValueRange* rng ) : ref(r) , range(rng) {}
  };

  Expr* variable_;
  ValueRange* range_;
  zone::Table<std::uint32_t,Item> table_;
  TypeKind type_kind_;
};

inline MultiValueRange::MultiValueRange( zone::Zone* zone ):
  zone_     (zone),
  variable_ (NULL),
  range_    (NULL),
  table_    (zone),
  type_kind_(TPKIND_UNKNOWN)
{}

void MultiValueRange::Inherit( MultiValueRange* another ) {
  lava_debug(NORMAL,lava_verify(table_.empty()););

  // do a copy and mark everything to be reference
  for( auto itr(another->table_.GetIterator()); itr.HasNext() ; itr.Move() ) {
    lava_verify(
        table_.Insert(zone_,itr.key(),Item(itr.value().range)).second
    );
  }
}

ValueRange* MultiValueRange::NewValueRange( ValueRange* that ) {
  if(type_kind_ == TPKIND_FLOAT64) {
    if(that) {
      lava_debug(NORMAL,lava_verify(that->type() == FLOAT64_VALUE_RANGE););
      return zone_->New<Float64ValueRange>(*static_cast<Float64ValueRange*>(that));
    }
    return zone_->New<Float64ValueRange>(zone_);
  } else {
    lava_debug(NORMAL,lava_verify(type_kind_ == TPKIND_BOOLENA););
    if(that) {
      lava_debug(NORMAL,lava_verify(that->type() == BOOLEAN_VALUE_RANGE););
      return zone_->New<BooleanValueRange>(*static_cast<Float64ValueRange*>(that));
    }
    return zone_->New<BooleanValueRange>(zone_);
  }
}

ValueRange* MultiValueRange::MaybeCopy( Expr* node ) {
  auto itr = table_.Find(node->id());
  if(itr.HasNext()) {
    if(itr.value().ref) {
      /**
       * if the existed node is a UnknownValueRange then the type test will not
       * pass so still we will mark this node as a UnknownValueRange
       */
      if(TypeTestCompatible(type_kind_,itr.value().range)) {
        range_ = NewValueRange(itr.value().range);
      } else {
        range_ = UnknownValueRange::Get(); // mark it as unknown range since the previous dominator
                                           // has a different type of the same expression node , mostly
                                           // a bug in code or a dead branch
      }
      itr.set_value(Item(false,range_));

    } else {
      lava_debug(NORMAL,lava_verify(range_ == itr.value().range););
    }
  } else {
    range_ = NewValueRange();
    lava_verify(table_.Insert(&zone_,node->id(),Item(false,range_)).second);
  }

  return range_;
}

void MultiValueRange::SetCondition ( Expr* node , TypeKind type , ValueRange* range ) {
  variable_ = node;
  type_kind_= type;
  MaybeCopy(node)->Intersect(*range);
}

bool MultiValueRange::TestTypeCompatible( TypeKind type , ValueRange* range ) {
  if(type == TPKIND_BOOLEAN)
    return range->type() == BOOLEAN_VALUE_RANGE;
  else if(type == TPKIND_FLOAT64)
    return range->type() == FLOAT64_VALUE_RANGE;
  else
    return false;
}

int  MultiValueRange::Infer( Binary::Operator op , Expr* node ) {
  auto itr = table_.Find(node->id());
  if(itr.HasNext()) {
    return itr.value().range->Infer(op,node);
  }
  return ValueRange::UNKNOWN;
}

int  MultiValueRange::Collapse( Graph* graph , IRInfo* node ) {
  auto itr = table_.Find(node->id());
  if(itr.HasNext()) {
    return itr.value().range->Collapse(graph,node);
  }
  return NULL;
}

/**
 * Condition group object. Used to represent a simple constraint whenever a branch node
 * is encountered.
 *
 * We only support simple constraint, a simple constraint means a condition for a branch
 * node that *only* has one variables and also it must be typed with certain types.
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
  inline ConditionGroup( Graph* , zone::Zone* , ConditionGroup* );

  void Process ( Expr* );

 public:
  Expr* variable() const { lava_debug(NORMAL,lava_verify(!IsBailout());); return variable_; }

  bool IsDead() const { return dead_; }

  TypeKind type_kind() const { return type_kind_; }

  ConditionGroup* prev() const { return prev_; }

  MultiValueRange* range() const { return &range_; }

 private:
  // Do a recursive inference with all the dominator node's condition group
  int  Infer( Expr* , TypeKind );
  bool InferLocal( Expr* , TypeKind , int* );

 private:
  /** ------------------------------------------
   * Simplification
   * ------------------------------------------*/
  Expr* DeduceTo   ( Expr* , bool );
  bool  Simplify   ( Expr* , Expr** );
  Expr* DoSimplify ( Expr* );


  /** ------------------------------------------
   * Construction
   * ------------------------------------------*/
  void Construct           ( Expr* );
  void ConstructSub        ( ValueRange* , Expr* );
  ValueRange* ConstructSub ( Expr* );
  void ConstructFloat64Expr( ValueRange* , Float64Compare* , bool op = false );

 private:

  Graph*              graph_;
  zone::Zone*         zone_;
  ConditionGroup*     prev_;
  Expr*               variable_;
  TypeKind            type_kind_;
  MultiValueRange     range_;
  bool                dead_;
};

inline ConditionGroup::ConditionGroup( Graph* graph , zone::Zone* zone , ConditionGroup* p ):
  graph_(graph),
  zone_(zone) ,
  prev_(p),
  variable_(NULL),
  type_kind_(TPKIND_UNKNOWN),
  range_(),
  dead_ (false)
{}

int ConditionGroup::Infer( Expr* node , TypeKind type_kind ) {
}

/** -----------------------------------------------------------------------
 *  Expression Simplification
 *  ----------------------------------------------------------------------*/
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

bool ConditionGroup::Simplify( Expr* node , Expr** nnode ) {
  *nnode = DoSimplify(node);

  if((*nnode)->IsBoolean()) {
    auto bval = (*nnode)->AsBoolean()->value();
    if(!bval) { // just mark
      range_.Clear();
      dead_ = true;
    }
    return false;
  }

  return true;
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

void ConditionGroup::Process( Expr* node ) {
  // Check if we are dominated by a dead branch or not
  if(prev_ && prev_->IsDead()) {
    dead_ = true;
    return;
  }

  // Do the inheritance
  if(prev_) range_.Inherit(prev_->range());


  SimpleConstraintChecker checker;
  if(!checker.Check(node,&variable_,&type_kind_)) {
    return;
  }

  // Do a check whether the dominator condition group has same
  // type with same variable
  if(prev_) {
    if(prev_->variable() == variable_) {
      if(prev_->type_kind() != type_kind_) {
        return;
      }
    }
  }

  // Try to simplify the node with the current node
  Expr* nnode;
  if(!Simplify(node,&nnode)) return;

  // Construct the simplified node and then merge it back to range
  Construct(nnode);
  return;
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
