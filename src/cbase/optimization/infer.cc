#include "infer.h"
#include "src/cbase/fold.h"
#include "src/stl-helper.h"
#include "src/cbase/dominators.h"
#include "src/cbase/value-range.h"
#include "src/cbase/ool-vector.h"
#include "src/zone/zone.h"
#include "src/zone/table.h"

namespace lavascript {
namespace cbase      {
namespace hir        {

using namespace ::lavascript;

namespace {

static Boolean kTrueNode(NULL,0,true,NULL);

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
// independently
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
//

class MultiValueRange : public zone::ZoneObject {
 public:
  inline MultiValueRange( zone::Zone* );

  // Inherit this empty object from another object typically comes from its
  // immediate dominator node's value range.
  void Inherit      ( MultiValueRange* );

  // Call this function to setup the node's value range
  void SetCondition ( Expr* , Expr* , TypeKind );

  void Clear() { table_.Clear(); }

 public:
  // Helper to lookup the correct value range for inference
  ValueRange* LookUp( Expr* );

 private:
  ValueRange* NewValueRange     ( ValueRange* that = NULL );
  ValueRange* MaybeCopy         ( Expr* );
  bool        TestTypeCompatible( TypeKind , ValueRange* );

  // Construct a the value range based on the input node
  void Construct  ( ValueRange* , Expr* , Expr* );
  void DoConstruct( ValueRange* , Expr* , Expr* , bool );
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
    lava_debug(NORMAL,lava_verify(type_kind_ == TPKIND_BOOLEAN););
    if(that) {
      lava_debug(NORMAL,lava_verify(that->type() == BOOLEAN_VALUE_RANGE););
      return zone_->New<BooleanValueRange>(*static_cast<BooleanValueRange*>(that));
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
      if(TestTypeCompatible(type_kind_,itr.value().range)) {
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
    lava_verify(table_.Insert(zone_,node->id(),Item(false,range_)).second);
  }

  return range_;
}

void MultiValueRange::DoConstruct  ( ValueRange* range , Expr* node , Expr* v , bool is_union ) {
  if(node->IsFloat64Compare()) {
    auto fcomp = node->AsFloat64Compare();
    auto var   = fcomp->lhs()->IsFloat64() ? fcomp->rhs() : fcomp->lhs() ;
    auto cst   = fcomp->lhs()->IsFloat64() ? fcomp->lhs() : fcomp->rhs() ;
    lava_debug(NORMAL,lava_verify(var == v && type_kind_ == TPKIND_FLOAT64););
    if(is_union)
      range->Union( fcomp->op() , cst );
    else
      range->Intersect( fcomp->op() , cst );
  } else if(node->IsBooleanLogic()) {
    auto bl    = node->AsBooleanLogic();
    Float64ValueRange f64temp(zone_);
    BooleanValueRange btemp;

    ValueRange* temp = (type_kind_ == TPKIND_FLOAT64 ? static_cast<ValueRange*>(&f64temp) :
                                                       static_cast<ValueRange*>(&btemp));
    DoConstruct(temp,bl->lhs(),v,true);
    DoConstruct(temp,bl->rhs(),v,bl->op() == Binary::OR);
    if(is_union)
      range->Union(*temp);
    else
      range->Intersect(*temp);
  } else {
    lava_debug(NORMAL,lava_verify(type_kind_ == TPKIND_BOOLEAN););
    auto is_not = node->IsBooleanNot();

    lava_debug(NORMAL,
        auto n = node;
        if(node->IsBooleanNot()) {
          n = node->AsBooleanNot()->operand();
        }
        lava_verify(n == node);
    );

    if(is_union)
      range->Union    ( is_not ? Binary::NE : Binary::EQ , &kTrueNode );
    else
      range->Intersect( is_not ? Binary::NE : Binary::EQ , &kTrueNode );
  }
}

void MultiValueRange::Construct    ( ValueRange* range , Expr* node , Expr* v ) {
  // When the input range set is empty, use union to initialize empty set;
  // otherwise it must be a intersection since multiple nested if indicates
  // intersection or *and* semantic
  DoConstruct(range,node,v,range->IsEmpty());
}

void MultiValueRange::SetCondition ( Expr* node , Expr* var , TypeKind type ) {
  variable_ = node;
  type_kind_= type;
  auto rng = MaybeCopy(var);
  Construct(rng,node,var);
}

bool MultiValueRange::TestTypeCompatible( TypeKind type , ValueRange* range ) {
  if(type == TPKIND_BOOLEAN)
    return range->type() == BOOLEAN_VALUE_RANGE;
  else if(type == TPKIND_FLOAT64)
    return range->type() == FLOAT64_VALUE_RANGE;
  else
    return false;
}

ValueRange* MultiValueRange::LookUp( Expr* node ) {
  auto itr = table_.Find(node->id());
  return itr.HasNext() ? itr.value().range : NULL;
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
 *
 * The object is designed to be used with monotonic algorithm without worrying about the
 * visit order though we use RPO to visit the graph.
 *
 * To guarantee this we mark all condition group as dead initially, then no prograss can
 * be made unless a node is visited in order basically from the start block. A start block
 * doesn't have a imm dominator node so it is always live.
 *
 * If we visit each node in RPO order then we find that the information is propogated correctly,
 * otherwise if any unexpected visiting order happened, the algorithm will not mark any
 * more information all the condition group is dead and no inference will be made , so at
 * least the algorithm will not make any mistake.
 *
 */

class ConditionGroup {
 public:
  inline ConditionGroup( Graph* , zone::Zone* , ConditionGroup* );

  /**
   * The is_first flag can be decided by doing a dominator lookup, if no
   * dominator node for this condition branch node, then we are the first
   */
  bool Process  ( Expr* , bool is_first = false );
  bool IsDead()            const { return dead_; }
  Expr* variable()         const { lava_debug(NORMAL,lava_verify(!IsDead());); return variable_; }
  TypeKind type_kind()     const { lava_debug(NORMAL,lava_verify(!IsDead());); return type_kind_; }
  ConditionGroup* prev()   const { return prev_; }
  MultiValueRange* range()       { return &range_; }
 private:
  /** ------------------------------------------
   * Simplification
   * ------------------------------------------*/
  Expr* TrueNode   ();
  Expr* DeduceTo   ( Expr* , bool );
  bool  Simplify   ( Expr* , Expr** );
  Expr* DoSimplify ( Expr* );
  void Bailout() { dead_ = true; range_.Clear(); }
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
  range_(zone),
  dead_ (true)
{}

Expr* ConditionGroup::DeduceTo( Expr* node , bool bval ) {
  auto n = bval ? Boolean::New(graph_,true,node->ir_info()) :
                  Boolean::New(graph_,false,node->ir_info());
  node->Replace(n);
  return n;
}

/** -----------------------------------------------------------------------
 *  Expression Simplification
 *  ----------------------------------------------------------------------*/
Expr* ConditionGroup::DoSimplify( Expr* node ) {
  switch(node->type()) {
    case IRTYPE_FLOAT64_COMPARE:
      {
        auto fcomp = node->AsFloat64Compare();
        auto var   = fcomp->lhs()->IsFloat64() ? fcomp->rhs() : fcomp->lhs();
        auto cst   = fcomp->lhs()->IsFloat64() ? fcomp->lhs() : fcomp->rhs();
        lava_debug(NORMAL,lava_verify(var == variable_););
        auto rng   = prev_->range()->LookUp(var); // get the value range
        if(rng) {
          auto infer = rng->Infer(fcomp->op(),cst);
          if(infer == ValueRange::ALWAYS_TRUE) {
            return DeduceTo(node,true);
          } else if(infer == ValueRange::ALWAYS_FALSE) {
            return DeduceTo(node,false);
          }
        }
        return NULL;
      }
    case IRTYPE_BOOLEAN_LOGIC:
      {
        auto n = node->AsBooleanLogic();
        auto l = n->lhs();
        auto r = n->rhs();
        auto op= n->op();

        DoSimplify(l);
        DoSimplify(r);

        auto nnode = FoldBinary(graph_,op,n->lhs(),n->rhs(),[=]() { return n->ir_info(); });
        if(nnode) {
          node->Replace(nnode);
          return nnode;
        }
        return NULL;
      }
    default:
      {
        // when we reach here it means it must be a boolean type
        lava_debug(NORMAL,
          lava_verify(type_kind_ == TPKIND_BOOLEAN);
          auto v = node;
          if(v->IsBooleanNot()) {
            v = v->AsBooleanNot()->operand();
          }
          lava_verify(v == variable_);
        );
        auto n      = node;
        bool is_not = false;
        if(n->IsBooleanNot()) {
          n = n->AsBooleanNot()->operand();
          is_not = true;
        }
        auto rng = prev_->range()->LookUp(n);
        if(rng) {
          auto infer = rng->Infer(is_not ? Binary::NE : Binary::EQ,&kTrueNode);
          if(infer == ValueRange::ALWAYS_TRUE) {
            return DeduceTo(node,true);
          } else if(infer == ValueRange::ALWAYS_FALSE) {
            return DeduceTo(node,false);
          }
        }
        return NULL;
      }
  }

  lava_die();
  return NULL;
}

bool ConditionGroup::Simplify( Expr* node , Expr** nnode ) {
  *nnode = node;
  if(prev_) {
    auto n = DoSimplify(node);
    if(!n) {
      if(n->IsBoolean()) {
        auto bval = n->AsBoolean()->value();
        if(!bval) Bailout();
        return false;
      }
      *nnode = n;
    }
  }

  return true;
}

bool ConditionGroup::Process( Expr* node , bool is_first ) {
  // If we don't have a previous conditional group *or* we have a dead previous
  // condition group then we just bailout directly and mark condition group to
  // be dead group. And we bailout this when is_first is set , basically that the
  // start node will have this set up regardlessly
  if(!is_first && (!prev_ || prev_->IsDead())) {
    return false;
  }

  dead_ = false;

  // Do the inheritance
  if(prev_) range_.Inherit(prev_->range());

  SimpleConstraintChecker checker;
  if(!checker.Check(node,&variable_,&type_kind_)) {
    return !dead_;
  }

  // Do a check whether the dominator condition group has same
  // type with same variable
  if(prev_) {
    if(prev_->variable() == variable_) {
      if(prev_->type_kind() != type_kind_) {
        Bailout();
        return false;
      }
    }
  }

  // Try to simplify the node with the current node
  Expr* nnode;
  if(!Simplify(node,&nnode)) return !dead_;

  // Set up the condition for this ConditionGroup
  range_.SetCondition(nnode,variable_,type_kind_);
  return true;
}

// ========================================================
//
// SimpleConstraintChecker implementation
//
// ========================================================

bool SimpleConstraintChecker::CheckExpr( Expr* expr ) {
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
        return (variable_ == expr);
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


bool Infer::Perform( Graph* graph , HIRPass::Flag flag ) {
  (void)flag;
  // setup temporarily allocation zone
  zone::Zone zone;
  // setup tracking vector
  OOLVector<ConditionGroup*> cg_vec(graph->MaxID());
  // setup dominator information
  Dominators dom; dom.Build(*graph);
  // traversal the control flow graph via RPO order
  for( ControlFlowRPOIterator itr(*graph) ; itr.HasNext() ; itr.Move() ) {
    auto cf   = itr.value();
    if(cf->IsIf() || cf->IsLoopHeader()) {
      auto idom           = dom.GetImmDominator(cf);
      auto cond           = cf->IsIf() ? cf->AsIf()->condition() : cf->AsLoopHeader()->condition();
      ConditionGroup* pcg = NULL;
      bool is_first       = false;
      if(idom) {
        is_first = true;
        pcg = cg_vec[idom->id()];
      }
      auto new_cg         = zone.New<ConditionGroup>(graph,&zone,pcg);
      new_cg->Process(cond,is_first);
      cg_vec[cf->id()] = new_cg;
    }
  }

  return true;
}


} // namespace hir
} // namespace cbase
} // namespace lavascript
