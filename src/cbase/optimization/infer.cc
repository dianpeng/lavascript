#include "infer.h"

#include "src/cbase/fold-arith.h"
#include "src/cbase/dominators.h"
#include "src/cbase/predicate.h"
#include "src/zone/vector.h"
#include "src/zone/zone.h"
#include "src/zone/table.h"
#include "src/stl-helper.h"

namespace lavascript {
namespace cbase      {
namespace hir        {
using namespace ::lavascript;
namespace {

static Boolean kTrueNode(NULL,0,true,NULL);

// MultiPredicate is an object that is used to track multiple type value's range
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
class MultiPredicate : public zone::ZoneObject {
 public:
  inline MultiPredicate( zone::Zone* );
  // Inherit this empty object from another object typically comes from its
  // immediate dominator node's value range.
  void Inherit      ( MultiPredicate* );
  // Call this function to setup the node's value range
  void SetCondition ( Expr* , Expr* , PredicateType );
  void Clear()      { table_.Clear(); }
 public:
  // Helper to lookup the correct value range for inference
  Predicate* LookUp( Expr* );
 private:
  Predicate* NewPredicate ( PredicateType , Predicate* that = NULL );
  Predicate* MaybeCopy    ( PredicateType , Expr* );
  // Construct a the value range based on the input node
  void Construct  ( Predicate* , Expr* , Expr* );
  void DoConstruct( Predicate* , Expr* , Expr* , bool );
 private:
  zone::Zone* zone_;
  // the object that will be stored inside of the table internally held by this object.
  // it basically records whether the range stored here is a reference or not; since we
  // support copy on write semantic, we will duplicate the range object on the fly if we
  // try to modify it
  struct Item : public zone::ZoneObject {
    bool ref;           // whether this object is a reference
    Predicate* range;   // the corresponding range object
    Item() : ref() , range(NULL) {}
    Item( Predicate* r ) : ref(true) , range(r) {}
    Item( bool r , Predicate* rng ) : ref(r) , range(rng) {}
  };
  Expr* variable_;
  Predicate* range_;
  zone::Table<std::uint32_t,Item> table_;
  PredicateType type_;
};

inline MultiPredicate::MultiPredicate( zone::Zone* zone ):
  zone_     (zone),
  variable_ (NULL),
  range_    (NULL),
  table_    (zone),
  type_     (UNKNOWN_PREDICATE)
{}

void MultiPredicate::Inherit( MultiPredicate* another ) {
  lava_debug(NORMAL,lava_verify(table_.empty()););
  // do a copy and mark everything to be reference
  another->table_.Copy(zone_,&table_);
}

Predicate* MultiPredicate::NewPredicate( PredicateType t , Predicate* that ) {
  lava_debug(NORMAL,if(that) lava_verify(that->type() == t););
  switch(t) {
    case FLOAT64_PREDICATE:
      return that ? zone_->New<Float64Predicate>(*static_cast<Float64Predicate*>(that)) :
                    zone_->New<Float64Predicate>(zone_);
    case BOOLEAN_PREDICATE:
      return that ? zone_->New<BooleanPredicate>(*static_cast<BooleanPredicate*>(that)) :
                    zone_->New<BooleanPredicate>(zone_);
    case TYPE_PREDICATE:
      return that ? zone_->New<TypePredicate>(*static_cast<TypePredicate*>(that)) :
                    zone_->New<TypePredicate>(zone_);

    default: lava_die(); return NULL;
  }
}

Predicate* MultiPredicate::MaybeCopy( PredicateType type , Expr* node ) {
  auto itr = table_.Find(node->id());
  Predicate* pred = NULL;
  if(itr.HasNext()) {
    if(itr.value().ref) {
      if(type == itr.value().range->type()) {
        // type matched , so get correct predicate
        pred = NewPredicate(type,itr.value().range);
      } else {
        // type mismatched, so put an unknown predicate and forbid all the following inference
        pred = UnknownPredicate::Get();
      }
      itr.set_value(Item(false,pred));
    } else {
      pred = itr.value().range;
    }
  } else {
    pred = NewPredicate(type);
    lava_verify(table_.Insert(zone_,node->id(),Item(false,pred)).second);
  }
  return pred;
}

void MultiPredicate::DoConstruct  ( Predicate* range , Expr* node , Expr* v , bool is_union ) {
  lava_debug(NORMAL,lava_verify(range->type() == type_););
  switch(node->type()) {
    case HIR_FLOAT64_COMPARE:
      {
        auto fcomp = node->AsFloat64Compare();
        auto var   = fcomp->lhs()->IsFloat64() ? fcomp->rhs() : fcomp->lhs() ;
        auto cst   = fcomp->lhs()->IsFloat64() ? fcomp->lhs() : fcomp->rhs() ;
        lava_debug(NORMAL,lava_verify(var == v);lava_verify(range->type() == FLOAT64_PREDICATE););
        if(is_union) range->Union    ( fcomp->op() , cst );
        else         range->Intersect( fcomp->op() , cst );
      }
      break;
    case HIR_BOOLEAN_LOGIC:
      {
        auto bl    = node->AsBooleanLogic();
        auto temp  = NewPredicate(type_,NULL);
        DoConstruct(temp,bl->lhs(),v,true);
        DoConstruct(temp,bl->rhs(),v,bl->op() == Binary::OR);
        if(is_union) range->Union    (*temp);
        else         range->Intersect(*temp);
      }
      break;
    case HIR_TEST_TYPE:
      if(is_union) range->Union    (Binary::EQ,node);
      else         range->Intersect(Binary::EQ,node);
      break;
    default:
      {
        lava_debug(NORMAL,
            lava_verify(range->type() == BOOLEAN_PREDICATE);
            auto n = node;
            if(node->IsBooleanNot()) {
              n = node->AsBooleanNot()->operand();
            }
            lava_verify(n == node);
        );

        auto is_not = node->IsBooleanNot();
        if(is_union) range->Union    ( is_not ? Binary::NE : Binary::EQ , &kTrueNode );
        else         range->Intersect( is_not ? Binary::NE : Binary::EQ , &kTrueNode );
      }
      break;
    }
}

void MultiPredicate::Construct    ( Predicate* range , Expr* node , Expr* v ) {
  // When the input range set is empty, use union to initialize empty set;
  // otherwise it must be a intersection since multiple nested if indicates
  // intersection or *and* semantic
  DoConstruct(range,node,v,range->IsEmpty());
}

void MultiPredicate::SetCondition ( Expr* node , Expr* var , PredicateType type ) {
  lava_debug(NORMAL, lava_verify(!variable_);
                     lava_verify(type_ == UNKNOWN_PREDICATE);
                     lava_verify(!range_););
  variable_ = node;
  type_     = type;

  auto rng = MaybeCopy(type,var);
  if(!rng->IsUnknownPredicate()) Construct(rng,node,var);
}

Predicate* MultiPredicate::LookUp( Expr* node ) {
  auto itr = table_.Find(node->id());
  return itr.HasNext() ? itr.value().range : NULL;
}

// Object used to track all the predicate related to all the dominators that
// dominate this node. So all the dominated block by this conditional block
// will just need to do one lookup then it could do inference
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
  PredicateType type()     const { lava_debug(NORMAL,lava_verify(!IsDead());); return type_;     }
  ConditionGroup* prev()   const { return prev_; }
  MultiPredicate* range()        { return &range_; }
 private:
  bool Validate    ( Expr* );
  bool SetCondition( Expr* , Expr* , PredicateType );
  bool CheckIfConstantBooleanCondition( Expr* );
  bool IsBooleanTrue ( Expr* n ) { return n->IsBoolean() ? n->AsBoolean()->value() : false; }
  bool IsBooleanFalse( Expr* n ) { return n->IsBoolean() ? !n->AsBoolean()->value() : false; }
  /** ------------------------------------------
   * Simplification
   * ------------------------------------------*/
  Expr* TrueNode   ();
  Expr* DeduceTo   ( Expr* , int    );
  bool  Simplify   ( Expr* , Expr** );
  Expr* Simplify   ( Expr* );
  Expr* SimplifyF64Compare  ( Float64Compare* );
  Expr* SimplifyBooleanLogic( BooleanLogic* );
  Expr* SimplifyTestType    ( TestType* );
  Expr* SimplifyBoolean     ( Expr* );
  bool  Bailout    () { dead_ = true; range_.Clear(); return false; }
 private:
  Graph*            graph_;
  zone::Zone*       zone_;
  ConditionGroup*   prev_;
  Expr*             variable_;
  PredicateType     type_;
  MultiPredicate    range_;
  bool              dead_;
};

inline ConditionGroup::ConditionGroup( Graph* graph , zone::Zone* zone , ConditionGroup* p ):
  graph_(graph),
  zone_(zone) ,
  prev_(p),
  variable_(NULL),
  type_ (UNKNOWN_PREDICATE),
  range_(zone),
  dead_ (true)
{}

Expr* ConditionGroup::DeduceTo( Expr* node , int result ) {
  Expr* n = NULL;
  if(result == Predicate::ALWAYS_TRUE) {
    n = Boolean::New(graph_,true,node->ir_info());
  } else if(result == Predicate::ALWAYS_FALSE) {
    n = Boolean::New(graph_,false,node->ir_info());
  }
  if(n) node->Replace(n);
  return n;
}

bool ConditionGroup::CheckIfConstantBooleanCondition( Expr* node ) {
  // if condition is a boolean false, then just mark this condition group dead
  if(IsBooleanFalse(node)) return Bailout();
  return true;
}

bool ConditionGroup::Validate( Expr* node ) {
  if(!CheckIfConstantBooleanCondition(node)) return false;
  auto t = ClassifyPredicate(node);
  if(t.type == UNKNOWN_PREDICATE) return Bailout();
  type_     = t.type;
  variable_ = t.main_variable;
  return true;
}

bool ConditionGroup::SetCondition( Expr* cond , Expr* var , PredicateType t ) {
  if(IsBooleanFalse(cond)) return Bailout(); // simplify to boolean false
  if(IsBooleanTrue (cond)) return true;      // no need to set any condition for this group
  range_.SetCondition(cond,var,t);
  return true;
}

/** -----------------------------------------------------------------------
 *  Expression Simplification
 *  ----------------------------------------------------------------------*/
Expr* ConditionGroup::SimplifyF64Compare( Float64Compare* fcomp ) {
  auto var   = fcomp->lhs()->IsFloat64() ? fcomp->rhs() : fcomp->lhs();
  auto cst   = fcomp->lhs()->IsFloat64() ? fcomp->lhs() : fcomp->rhs();
  lava_debug(NORMAL,lava_verify(var == variable_););
  auto rng   = prev_->range()->LookUp(var); // get the value range
  return rng ? DeduceTo(fcomp,rng->Infer(fcomp->op(),cst)) : NULL;
}

Expr* ConditionGroup::SimplifyTestType( TestType* tt ) {
  lava_debug(NORMAL,lava_verify( tt->object() == variable_ ););
  auto rng = prev_->range()->LookUp(tt);
  return rng ? DeduceTo(tt,rng->Infer(Binary::EQ,tt)) : NULL;
}

Expr* ConditionGroup::SimplifyBooleanLogic( BooleanLogic* n ) {
  auto l = n->lhs();
  auto r = n->rhs();
  auto op= n->op();
  if( Simplify(l) || Simplify(r) ) {
    auto nnode = FoldBinary(graph_,op,n->lhs(),n->rhs(),[=]() { return n->ir_info(); });
    if(nnode) {
      n->Replace(nnode);
      return nnode;
    } else {
      return n;
    }
  }
  return NULL;
}

Expr* ConditionGroup::SimplifyBoolean( Expr* node ) {
  // when we reach here it means it must be a boolean type
  lava_debug(NORMAL,
    lava_verify(type_ == BOOLEAN_PREDICATE);
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
  return rng ? DeduceTo(node,rng->Infer(is_not ? Binary::NE : Binary::EQ,&kTrueNode)) : NULL;
}

Expr* ConditionGroup::Simplify( Expr* node ) {
  switch(node->type()) {
    case HIR_FLOAT64_COMPARE:
      return SimplifyF64Compare(node->AsFloat64Compare());
    case HIR_BOOLEAN_LOGIC:
      return SimplifyBooleanLogic(node->AsBooleanLogic());
    case HIR_TEST_TYPE:
      return SimplifyTestType(node->AsTestType());
    default:
      return SimplifyBoolean(node);
  }
  lava_die(); return NULL;
}

bool ConditionGroup::Simplify( Expr* node , Expr** nnode ) {
  *nnode = node;
  // if the condition node is a boolean true, then no need to simplify it,
  // it cannot be boolean false due to the Validate should already rule it out
  if(prev_ && !IsBooleanTrue(node)) {
    auto n = Simplify(node);
    if(!n) {
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
  // Collapsing all the constraint into the current one
  if(prev_) range_.Inherit(prev_->range());
  // Validate the expression to check whether we can categorize this node
  if(!Validate(node)) return false;
  // Try to simplify the node with the current node
  {
    Expr* nnode;
    if(!Simplify(node,&nnode)) return !dead_;
    node = nnode;
  }
  // Set up the condition for this ConditionGroup
  SetCondition(node,variable_,type_);
  return true;
}

} // namespace


bool Infer::Perform( Graph* graph , HIRPass::Flag flag ) {
  (void)flag;
  // temporary zone object, use normal zone since it will cost some memory
  zone::Zone zone;
  // setup tracking vector
  zone::OOLVector<ConditionGroup*> cg_vec(&zone,graph->MaxID());
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
