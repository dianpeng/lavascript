#include "hir.h"
#include "type-inference.h"

#include <sstream>

namespace lavascript {
namespace cbase {
namespace hir {

const char* IRTypeGetName( IRType type ) {
#define __(A,B,C,...) case HIR_##B: return C;
  switch(type) {
    CBASE_HIR_LIST(__)
    default: lava_die(); return NULL;
  }
#undef __ // __
}

void Expr::Replace( Expr* another ) {
  // 1. check all the operand_list and patch each operands reference
  //    list to be new one
  lava_foreach( auto &v , ref_list()->GetForwardIterator() ) {
    v.id.set_value(another);
  }
  // 2. modify *this* if the node is a statement
  if(IsStatement()) {
    auto region = stmt_.region;
    region->RemoveStatement(stmt_);
    region->AddStatement(another);
    stmt_.region = NULL;
  }
  // 3. clear all the ref list since it is not referenced by any expression now
  ref_list_.Clear();
  // 4. clear all the operands since this node is dead
  ClearOperand();
}

bool Expr::RemoveRef( const OperandIterator& tar , Node* node ) {
  lava_debug(NORMAL,lava_verify(tar.value() == this););
  for( auto itr = ref_list_.GetForwardIterator(); itr.HasNext(); itr.Move() ) {
    auto &v = itr.value();
    if(v.id == tar && v.node->IsIdentical(node)) {
      ref_list_.Remove(itr);
      return true;
    }
  }
  return false;
}

void Expr::ClearOperand() {
  for( auto itr = operand_list_.GetForwardIterator(); itr.HasNext(); itr.Move() ) {
    auto n = itr.value();
    lava_verify(n->RemoveRef(itr,this));
  }
  operand_list_.Clear();
}

IRList* IRList::Clone( Graph* graph , const IRList& that ) {
  auto ret = IRList::New(graph,that.Size(),that.ir_info());
  lava_foreach( auto &v , that.operand_list()->GetForwardIterator() ) {
    ret->Add(v);
  }
  return ret;
}

IRList* IRList::CloneExceptLastOne( Graph* graph , const IRList& that ) {
  auto ret = IRList::New(graph,that.Size(),that.ir_info());
  if(that.Size() == 0)
    return ret;
  else {
    std::size_t count = 0;
    std::size_t end   = that.Size() - 1;
    lava_foreach( auto &v , that.operand_list()->GetForwardIterator() ) {
      if(count >= end) break;
      ret->Add(v);
    }
    return ret;
  }
}

std::uint64_t ICall::GVNHash() const {
  GVNHashN hasher(type_name());
  hasher.Add(static_cast<std::uint32_t>(ic()));
  lava_foreach( auto &v , operand_list()->GetForwardIterator() ) {
    hasher.Add(v->GVNHash());
  }
  return hasher.value();
}

bool ICall::Equal( const Expr* that ) const {
  if(that->IsICall()) {
    auto tic = that->AsICall();
    if(ic() == tic->ic()) {
      lava_debug(NORMAL, lava_verify(operand_list()->size() == tic->operand_list()->size()););

      auto this_itr(operand_list()->GetForwardIterator());
      auto that_itr(that->operand_list()->GetForwardIterator());
      lava_foreach( auto &v , this_itr ) {
        that_itr.Move();
        if(!v->Equal(that_itr.value())) return false;
      }
      return true;
    }
  }
  return false;
}

void ControlFlow::Replace( ControlFlow* node ) {
  // 1. transfer all *use* node
  lava_foreach( auto &v , ref_list()->GetForwardIterator() ) {
    v.id.set_value(node);
  }
  // 2. transfer all the backward and forward edge to |node|
  node->forward_edge_.Merge(&forward_edge_);
  node->backward_edge_.Merge(&backward_edge_);
  // 3. clear all operand from *this* node
  ClearOperand();
}

void ControlFlow::RemoveBackwardEdge( ControlFlow* node ) {
  auto itr = backward_edge_.Find(node);
  lava_verify(itr.HasNext());
  {
    auto i = node->forward_edge_.Find(this);
    lava_verify(i.HasNext());
    node->forward_edge_.Remove(i);
  }
  backward_edge_.Remove(itr);
}

void ControlFlow::RemoveBackwardEdge( std::size_t index ) {
  return RemoveBackwardEdge(backward_edge()->Index(index));
}

void ControlFlow::RemoveForwardEdge( ControlFlow* node ) {
  auto itr = forward_edge_.Find(node);
  lava_verify(itr.HasNext());
  {
    auto i = node->backward_edge_.Find(this);
    lava_verify(i.HasNext());
    node->backward_edge_.Remove(i);
  }
  forward_edge_.Remove(itr);
}

void ControlFlow::RemoveForwardEdge( std::size_t index ) {
  return RemoveForwardEdge(forward_edge()->Index(index));
}

void ControlFlow::MoveStatement( ControlFlow* cf ) {
  lava_foreach( auto &v , cf->statement_list()->GetForwardIterator() ) {
    AddStatement(v);
  }
}

void ControlFlow::ClearOperand() {
  for( auto itr = operand_list_.GetForwardIterator(); itr.HasNext(); itr.Move() ) {
    auto n = itr.value();
    lava_verify(n->RemoveRef(itr,this));
  }
  operand_list_.Clear();
}

Graph::Graph():
  zone_                 (),
  start_                (NULL),
  end_                  (NULL),
  prototype_info_       (),
  id_                   (),
  no_read_effect_       (),
  no_write_effect_      ()
{
  // initialize placeholders
  no_read_effect_ = NoReadEffect ::New(this);
  no_write_effect_= NoWriteEffect::New(this);
}

Graph::~Graph() {}

void Graph::Initialize( Start* start , End* end ) {
  start_ = start;
  end_   = end;
}

void Graph::Initialize( OSRStart* start , OSREnd* end ) {
  start_ = start;
  end_   = end;
}

void Graph::GetControlFlowNode( std::vector<ControlFlow*>* output ) const {
  output->clear();
  lava_foreach( auto v , ControlFlowBFSIterator(*this) ) {
    output->push_back(v);
  }
}

SetList::SetList( const Graph& graph ):
  existed_(graph.MaxID()),
  array_  ()
{}

bool SetList::Push( Node* node ) {
  if(!existed_[node->id()]) {
    array_.push_back(node);
    existed_[node->id()] = true;
    return true;
  }
  return false;
}

void SetList::Pop() {
  Node* top = Top();
  lava_debug(NORMAL,lava_verify(existed_[top->id()]););
  existed_[top->id()] = false;
  array_.pop_back();
}

OnceList::OnceList( const Graph& graph ):
  existed_(graph.MaxID()),
  array_  ()
{}

bool OnceList::Push( Node* node ) {
  if(!existed_[node->id()]) {
    existed_[node->id()] = true;
    array_.push_back(node);
    return true;
  }
  return false;
}

void OnceList::Pop() {
  array_.pop_back();
}

bool ControlFlowBFSIterator::Move() {
  while(!stack_.empty()) {
    auto top = stack_.Top()->AsControlFlow();
    stack_.Pop();
    lava_foreach( ControlFlow* cf , top->forward_edge()->GetForwardIterator() ) {
      stack_.Push(cf);
    }
    next_ = top;
    return true;
  }
  next_ = NULL;
  return false;
}

namespace {

struct ForwardEdgeGetter {
  const RegionList* Get() const { return node_->forward_edge(); }
  ForwardEdgeGetter( ControlFlow* node ) : node_(node) {}
 private:
  ControlFlow* node_;
};

struct BackwardEdgeGetter {
  const RegionList* Get() const { return node_->backward_edge(); }
  BackwardEdgeGetter( ControlFlow* node ) : node_(node) {}
 private:
  ControlFlow* node_;
};

// Helper function to do DFS iterator move
template< typename GETTER >
ControlFlow* ControlFlowDFSIterMove( OnceList* stack ) {
  while(!stack->empty()) {
recursion:
    ControlFlow* top = stack->Top()->AsControlFlow();
    lava_foreach( auto &v , GETTER(top).Get()->GetForwardIterator() ) {
      if(stack->Push(v)) goto recursion;
    }
    // when we reach here it means we scan through all its predecessor nodes and
    // don't see any one not visited , or maybe this node is a singleton/leaf.
    stack->Pop();
    return top;
  }
  return NULL;
}

} // namespace

bool ControlFlowPOIterator::Move() {
  return (next_ = ControlFlowDFSIterMove<ForwardEdgeGetter>(&stack_));
}

bool ControlFlowRPOIterator::Move() {
  while(!stack_.empty()) {
recursion:
    ControlFlow* top = stack_.Top()->AsControlFlow();
    // 1. check whether all its predecessuor has been visited or not
    lava_foreach( ControlFlow* cf , top->backward_edge()->GetForwardIterator() ) {
      if(!mark_[cf->id()] && stack_.Push(cf)) {
        goto recursion;
      }
    }
    // 2. visit the top node
    lava_debug(NORMAL,lava_verify(!mark_[top->id()]););
    mark_[top->id()] = true;
    stack_.Pop();
    next_ = top;
    return true;
  }
  next_ = NULL;
  return false;
}

bool ControlFlowEdgeIterator::Move() {
  if(!stack_.empty()) {
    ControlFlow* top = stack_.Top()->AsControlFlow();
    stack_.Pop();
    lava_foreach( ControlFlow* cf , top->backward_edge()->GetBackwardIterator() ) {
      stack_.Push(cf);
      results_.push_back(Edge(top,cf));
    }
  }
  if(results_.empty()) {
    next_.Clear();
    return false;
  } else {
    next_ = results_.front();
    results_.pop_front();
    return true;
  }
}

bool ExprDFSIterator::Move() {
  if(!stack_.empty()) {
recursion:
    Expr* top = stack_.Top()->AsExpr();
    lava_foreach( Expr* val , top->operand_list()->GetForwardIterator() ) {
      if(stack_.Push(val)) goto recursion;
    }
    next_ = top;
    stack_.Pop();
    return true;
  }
  next_ = NULL;
  return false;
}

Expr* NewUnboxNode( Graph* graph , Expr* node , TypeKind tk , IRInfo* info ) {
  // we can only unbox a node when we know the type
  lava_debug(NORMAL,lava_verify(tk != TPKIND_UNKNOWN && tk == GetTypeInference(node)););

  // 1. check if the node is already unboxed , if so just return the node itself
  switch(node->type()) {
    case HIR_UNBOX:
      lava_debug(NORMAL,lava_verify(node->AsUnbox()->type_kind() == tk););
      return node;

    case HIR_FLOAT64_NEGATE:
    case HIR_FLOAT64_ARITHMETIC:
    case HIR_FLOAT64_BITWISE:
      lava_debug(NORMAL,lava_verify(tk == TPKIND_FLOAT64););
      return node;

    case HIR_FLOAT64_COMPARE:
    case HIR_STRING_COMPARE:
    case HIR_SSTRING_EQ:
    case HIR_SSTRING_NE:
      lava_debug(NORMAL,lava_verify(tk == TPKIND_BOOLEAN););
      return node;

    case HIR_BOX:
      {
        // if a node is just boxed, then we can just remove the previous box
        auto bvalue = node->AsBox()->value();
        lava_debug(NORMAL,lava_verify(GetTypeInference(bvalue) == tk););
        return bvalue;
      }

    default: break;
  }

  // 2. do a real unbox here
  return Unbox::New(graph,node,tk,info);
}

} // namespace hir
} // namespace cbase
} // namespace lavascript
