#include "dce.h"

#include "src/cbase/dominators.h"
#include "src/cbase/hir-helper.h"
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
  switch(predicate->type()) {
    case IRTYPE_BOOLEAN:
      *output = predicate->AsBoolean()->value();
      return true;
    case IRTYPE_NIL:
      *output = false;
      return true;
    case IRTYPE_FLOAT64:
    case IRTYPE_LONG_STRING :
    case IRTYPE_SMALL_STRING:
    case IRTYPE_LIST:
    case IRTYPE_OBJECT:
    case IRTYPE_LOAD_CLS:
    case IRTYPE_ITR_NEW:
    case IRTYPE_FLOAT64_NEGATE:
    case IRTYPE_FLOAT64_ARITHMETIC:
    case IRTYPE_FLOAT64_BITWISE:
      *output = true;
      return true;
    default:
      return false;
  }
}

class DCEImpl {
 public:
  void Visit  ( Graph* );

 private:
  bool VisitIf( ControlFlow* );

  struct DCEBlock {
    ControlFlow* block;
    bool cond;
    DCEBlock( ControlFlow* b , bool c ) : block(b) , cond(c) {}
  };

  std::vector<DCEBlock> blocks_; // record all blocks that needs to be removed
};

bool DCEImpl::VisitIf( ControlFlow* node ) {
  bool bval;
  auto cond = node->IsIf() ? node->AsIf()->condition() :
                             node->AsLoopHeader()->condition();

  if(!InferPredicate(cond,&bval)) return false;
  blocks_.push_back(DCEBlock(node,bval));
  return true;
}

void DCEImpl::Visit( Graph* graph ) {

  // mark all blocks that needs to be DCEed
  for( ControlFlowRPOIterator itr(*graph) ; itr.HasNext() ; itr.Move() ) {
    auto cf = itr.value();
    // all the operands node
    if(cf->IsIf())              VisitIf(cf);
    else if(cf->IsLoopHeader()) VisitIf(cf);
  }

  // path all blocks that needs to be removed
  for( auto &e : blocks_ ) {
    auto node = e.block;
    auto bval = e.cond;
    auto if_parent = node->parent();

    // need to add all statement with side effect to its parental node
    // since the *if* node will be removed entirely
    if_parent->MoveStatement(node);
    // 2. link the merged region back to the if_parent node
    auto merge = node->IsIf() ? node->AsIf()->merge() :
                                node->AsLoopHeader()->merge();

    // remove the PHI node
    for( auto itr(merge->operand_list()->GetForwardIterator());
         itr.HasNext() ; itr.Move() ) {
      auto n = itr.value();
      if(n->IsPhi()) {
        auto phi = n->AsPhi();
        lava_debug(NORMAL,lava_verify(phi->operand_list()->size() == 2););
        auto v = bval ? phi->operand_list()->Index(1) : // true
                        phi->operand_list()->Index(0) ; // false
        // use v to replace all the phi uses
        phi->Replace(v);

        // we should not/never expect another PHI node
        break;
      }
    }
    // remove node from if_parent node
    if_parent->RemoveForwardEdge(node);

    // clear all merge's backwards edge
    merge->ClearBackwardEdge();

    // get all statement from merge nod
    if_parent->MoveStatement(merge);

    // replace merge to be if_parent
    merge->Replace(if_parent);
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