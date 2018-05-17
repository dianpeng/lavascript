#include "dce.h"
#include "src/cbase/type.h"
#include "src/cbase/dominators.h"
#include "src/cbase/hir-visitor.h"

#include <vector>

namespace lavascript {
namespace cbase      {
namespace hir        {
namespace {

// Used to do inference for predicate expression for boolean value.
// If we can infer the value this function returns true ; otherwise
// returns false.
bool InferPredicate( Expr* predicate , bool* output ) {
  return GetBooleanValue(predicate,output);
}

class DCEImpl {
 public:
  void Visit  ( Graph* );
 private:
  bool VisitIf( ControlFlow* );
  struct DCEBlock : public ::lavascript::zone::ZoneObject {
    ControlFlow* block;
    bool cond;
    DCEBlock( ControlFlow* b , bool c ) : block(b) , cond(c) {}
  };
  ::lavascript::zone::Zone zone_;                // temporary memory zone
  ::lavascript::zone::Vector<DCEBlock> blocks_;  // record all blocks that needs to be removed
};

bool DCEImpl::VisitIf( ControlFlow* node ) {
  bool bval;
  auto cond = node->IsIf() ? node->AsIf()->condition() :
                             node->AsLoopHeader()->condition();

  if(!InferPredicate(cond,&bval)) return false;
  blocks_.Add(&zone_,DCEBlock(node,bval));
  return true;
}

void DCEImpl::Visit( Graph* graph ) {

  // mark all blocks that needs to be DCEed
  lava_foreach( auto cf , ControlFlowRPOIterator(&zone_,*graph) ) {
    if(cf->IsIf())              VisitIf(cf);
    else if(cf->IsLoopHeader()) VisitIf(cf);
  }

  // patch all blocks that needs to be removed
  lava_foreach( auto &e, blocks_.GetForwardIterator() ) {
    auto node = e.block;
    auto bval = e.cond;
    auto if_parent = node->parent();
    // link the merged region back to the if_parent node
    auto merge = node->IsIf() ? node->AsIf()->merge() : node->AsLoopHeader()->merge();
    // remove the PHI node
    lava_foreach( auto n , merge->operand_list()->GetForwardIterator() ) {
      if(n->IsPhi()) {
        auto phi = n->AsPhi();
        lava_debug(NORMAL,lava_verify(phi->operand_list()->size() == 2););
        auto v = bval ? phi->Operand(IfTrue::kIndex) :  // true
                        phi->Operand(IfFalse::kIndex) ; // false

        // use v to replace all the phi uses
        phi->Replace(v);
      }
    }
    // do the block deletion here
    auto true_block = merge->backward_edge()->Index(IfTrue::kIndex);
    auto false_block= merge->backward_edge()->Index(IfFalse::kIndex);

    if(bval) {
      merge->RemoveBackwardEdge(false_block);
      if(false_block != node) {
        node->RemoveForwardEdge(IfFalse::kIndex);
      }
    } else {
      merge->RemoveBackwardEdge(true_block);
      if(true_block != node) {
        node->RemoveForwardEdge(IfTrue::kIndex);
      }
    }
    // get all the statement from if_parent node
    node->MoveStmt(if_parent);
    // remove all its backwards edge since it will be replaced
    // by its parental node
    node->ClearBackwardEdge();
    // replace if_parent with node itself
    node->Replace(if_parent);
  }
}

} // namespace

bool DCE::Perform( Graph* graph , HIRPass::Flag flag ) {
  (void)flag;
  DCEImpl impl;
  impl.Visit(graph);
  return true;
}

} // namespace hir
} // namespace cbase
} // namespace lavascript
