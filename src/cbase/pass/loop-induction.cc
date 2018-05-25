#include "loop-induction.h"
#include "src/cbase/hir.h"
#include "src/zone/stl.h"
#include "src/zone/zone.h"


namespace lavascript {
namespace cbase      {
namespace hir        {
namespace            {

class LoopInductionOptimization {
 public:
  LoopInductionOptimization( zone::Zone* temp );

  // function to perform loop induction variable narrowing
  bool TryLoopInductionOptimization( LoopIV* iv );

 private:
  // If the expression tree is nested too deep we don't narrow it
  // as well since it requires too much work to do propogation
  static const int kDeepLevel = 8;

  struct LoopIVComponent {
    Expr* init;
    Expr* step;
    Expr* expr;
  };
  // decompose a loopiv into a LoopIVComponent object
  bool DecomposeLoopIV( LoopIV* , LoopIVComponent* );

  // do a backwards analyze to see whether it is profitable
  bool ShouldLoopInductionOptimization   ( LoopIV* , const LoopIVComponent& ) ;
  void PushUse        ( Expr* , zone::stl::ZoneQueue<Expr*>* );
  void ShouldLoopInductionOptimizationComponent( Expr* );

  // try to do narrowing and do backwards type propogation
  void DoLoopInductionOptimization         ( LoopIV* , const LoopIVComponent& );
  Expr* AddGuardIfNeeded( Expr* );

 private:
  zone::Zone* temp_zone_;
  Graph*      graph_;
};

bool LoopInductionOptimization::DecomposeLoopIV( LoopIV* iv , LoopIVComponent* output ) {
  lava_debug(NORMAL,lava_verify(iv->operand_list()->size() == 2 && iv->IsUsed()););
  Expr* init = iv->Operand(0);
  Expr* step = NULL;
  Expr* expr = NULL;

  // check step expression is a recognized binary or not
  if(auto step_expr = iv->Operand(1); step_expr->Is<Arithmetic>()) {
    auto bin = step_expr->As<Arithmetic>();
    if(bin->lhs()->IsIdentical(iv)) {
      step = bin->rhs();
    }
    expr = bin;
  }
  if(!step) return false;

  output->init = init;
  output->step = step;
  output->expr = expr;
  return true;
}

void LoopInductionOptimization::PushUse( Expr* node , zone::stl::ZoneQueue<Expr*>* output ) {
  lava_foreach( auto &k , node->ref_list()->GetForwardIterator() ) {
    output->push(k.node);
  }
}

void LoopInductionOptimization::ShouldLoopInductionOptimizationComponent( Expr* comp ) {
  if(comp->Is<Float64>() && !CanLoopInductionOptimizationReal(comp->As<Float64>()->value()))
    return false;
  else if(!comp->Is<Float64Arithmetic>() && !comp->Is<Arg>()) {
    if(GetTypeInference(comp) != TPKIND_INT32)
      return false;
  }
  return true;
}

bool LoopInductionOptimization::ShouldLoopInductionOptimization( LoopIV* iv , const LoopIVComponent& comp ) {
  if(!ShouldLoopInductionOptimizationComponent(comp.init) || !ShouldLoopInductionOptimizationComponent(comp.step))
    return false;

  int level = 0;
  zone::stl::ZoneQueue<Expr*> precedence(temp_zone_);
  precedence.reserve(16);
  precedence.push(iv);

  do {
    auto top = precedence.top();
    precedence.pop();
    // go through its use chain
    lava_foreach(auto &k, top->ref_list()->GetForwardIterator()) {
      auto node = k.node;
      if(node->Is<Int32Arithmetic>() ||
         node->Is<Float64Bitwise> ()) {
        PushUse(node,&precedence);
      } else if(node->Is<Float64Arithmetic>() ||
                node->Is<Float64Compare>   ()) {
        auto sb  = node->As<SpecializeBinary>();
        auto rest= sb->lhs() == top ? sb->rhs() : sb->lhs();
        if(rest->Is<Float64>()) {
          if(!CanLoopInductionOptimizationReal(rest->As<Float64>()->value())) return false;
        } else {
          return false;
        }
        if(node->Is<Float64Arithmetic>()) PushUse(node,&precedence);
      } else if(node->Is<Unbox>()) {
        // Search for ListIndex explicitly, the ListIndex has following
        // types of graph and also each node will be sololy used because
        // GVN will not be runned before this pass
        //
        // -------------
        // | ListIndex |
        // -------------
        //       |
        //       |
        // -------------
        // |  F64ToI32 |
        // -------------
        //       |
        //       |
        // -------------
        // |  Unbox    |
        // -------------
        auto unbox = node->As<Unbox>();
        if(unbox->As<Unbox>()->type_kind() != TPKIND_FLOAT64)
          return false;
        if(unbox->ref_list()->empty() || !unbox->Ref(0).node->Is<Float64ToInt32>())
          return false;
        auto f64_to_i32 = unbox->Ref(0).node->As<Float64ToInt32>();
        if(f64_to_i32->ref_list()->empty() || !f64_to_i32->Ref(0).node->As<ListIndex>())
          return false;
      }
    } else if(node->Is<StackSlot> () ||
              node->Is<IRObjectKV>()) {
      // no need to worry since these places doesn't really require a guard
      // if we want do type narrowing
    } else {
      return false;
    }
  } while(level < kDeepLevel && !precedence.empty());

  return level < kDeepLevel;
}

Expr* LoopInductionOptimization::AddGuardIfNeeded( Expr* node ) {
  auto tp = GetTypeInference(node);
  if(tp == TPKIND_INT32) {
    return node;
  } else if(tp == TPKIND_FLOAT64) {
    std::int32_t i32;
    lava_verify(LoopInductionOptimizationReal(node->As<Float64>()->value(),&i32));
    return Int32::New(graph_,i32);
  } else {
    lava_die();
    return NULL;
  }
}

void LoopInductionOptimization::DoLoopInductionOptimization( LoopIV* iv , const LoopIVComponent& comp ) {
  // 1. guard the loopiv node
  auto init = AddGuardIfNeeded(comp.init);
  auto step = AddGuardIfNeeded(comp.step);
  comp.expr->ReplaceOperand(1,step);
  auto iiv  = LoopIVInt32::New(graph_,init,comp.expr,iv->region());
  iv->Replace(iiv);
}

bool LoopInductionOptimization::TryLoopInductionOptimization( LoopIV* iv ) {
  LoopIVComponent comp;
  if(!DecomposeLoopIV(iv,&comp)) return false;
  if(!ShouldLoopInductionOptimization    (iv,comp)) return false;
  DoLoopInductionOptimization(iv);
  return true;
}

