#include "hir.h"
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

bool Expr::IsUnboxNode() const {
#define __(A,B,C,D,E) case HIR_##B: \
  return HIR_BOX_##E == HIR_BOX_Unbox || HIR_BOX_##E == HIR_BOX_Both;

  switch(type()) {
    CBASE_HIR_LIST(__)
    default: lava_die(); return false;
  }
#undef __ // __
}

bool Expr::IsBoxNode() const {
#define __(A,B,C,D,E) case HIR_##B: \
  return HIR_BOX_##E == HIR_BOX_Box || HIR_BOX_##E == HIR_BOX_Both;
  switch(type()) {
    CBASE_HIR_LIST(__)
    default: lava_die(); return false;
  }
#undef __ // __
}

void Expr::Replace( Expr* another ) {
  if(IsIdentical(another)) return;
  lava_foreach( auto &v , ref_list()->GetForwardIterator() ) {
    v.id.set_value(another);
  }
  another->ref_list_.Merge(&ref_list_);
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

// Replace operations for EffectRead/EffectWrite --------------------------
void ReadEffect::Replace( Expr* node ) {
  // replace a node with this |read effect| node. It depends on the type of
  // the replacement. You will not replace a read with higher order node, so
  // the basic idead is always lowering. A read that depends on effect can only
  // be replaced by a node that is 1) no effect or 2) read effect node. Other
  // nodes are not allowed to be replaced or should not be generated in general.
  lava_debug(NORMAL,lava_verify(node->Is<ReadEffect>() || !node->Is<EffectNode>()););

  // 1. remove itself from the dependency chain
  if(effect_edge_.node) {
    effect_edge_.node->RemoveReadEffect(&effect_edge_);
  }

  // 2. just do the replacement as with normal replace
  Expr::Replace(node);
}

void WriteEffect::Replace( Expr* node ) {
  // to replace a write effect node, one can replace it with lower none side
  // effect node.
  lava_debug(NORMAL,lava_verify(!node->Is<EffectNode>()););

  // 1. put all the read happened after |this| node to the node
  //    that's Next to it.
  auto next_write = NextWrite();
  {
    lava_foreach( auto &k , read_effect_.GetForwardIterator() ) {
      auto itr = next_write->read_effect_.PushBack(zone(),k);
      k->set_effect_edge( itr , next_write );
    }

    // now remove  |this| from the dependency chain
    RemoveLink();
  }

  // 2. Do the normal replacement of the expression node
  {
    // 2.1 iterate against all the *ref_list* and patch each ref pointed to
    //     the new replacement. There's one exception, that is if the ref
    //     node is a EffectMergeBase node, then you have to write it to next
    //     node of |this|
    lava_foreach( auto &k , ref_list()->GetForwardIterator() ) {
      if(k.node->Is<EffectMergeBase>()) {
        next_write->AddRef(k.node,k.id);
        k.id.set_value(next_write);
      } else {
        node->AddRef(k.node,k.id);
        k.id.set_value(node);
      }
    }

    // 2.2 clear all the existed operands
    ClearOperand();
  }
}

std::uint64_t SpecializeBinary::GVNHash () const {
  return GVNHash3(type_name(),op(),lhs()->GVNHash(),rhs()->GVNHash());
}

bool SpecializeBinary::Equal( const Expr* that ) const {
  if(that->type() == type()) {
    auto n = dynamic_cast<const BinaryNode*>(that);
    return op() == n->op() && lhs()->Equal(n->lhs()) && rhs()->Equal(n->rhs());
  }
  return false;
}

class ReadEffect::ReadEffectDependencyIterator {
 public:
  ReadEffectDependencyIterator( const ReadEffect* node ):
    node_(node) ,
    has_(!node->effect_edge_.IsEmpty())
  {}

  bool HasNext() const { return has_; }
  bool Move   () const { has_ = false; return false; }
  Expr* value () const { return node_->effect_edge_.node; }
  Expr* value ()       { return node_->effect_edge_.node; }

 private:
  const ReadEffect* node_;
  mutable bool has_;
};

Expr::DependencyIterator ReadEffect::GetDependencyIterator() const {
  return DependencyIterator(ReadEffectDependencyIterator(this));
}

class WriteEffect::WriteEffectDependencyIterator {
 public:
  WriteEffectDependencyIterator( const WriteEffect* node ):
    next_( node->NextLink()->read_effect_.empty () ? node->NextLink() : NULL ),
    itr_ ( node->NextLink()->read_effect_.GetForwardIterator() )
  {}

  bool HasNext() const { return next_ || itr_.HasNext(); }
  bool Move   () const {
    lava_debug(NORMAL,HasNext(););
    if(next_) { next_ = NULL; return false; }
    else      return itr_.Move();
  }

  Expr* value () const {
    lava_debug(NORMAL,lava_verify(HasNext()););
    if(next_) return next_;
    else      return itr_.value();
  }
  Expr* value () {
    lava_debug(NORMAL,lava_verify(HasNext()););
    if(next_) return next_;
    else      return itr_.value();
  }

 private:
  mutable WriteEffect* next_;
  const ReadEffectListIterator itr_;
};

Expr::DependencyIterator WriteEffect::GetDependencyIterator() const {
  if(NextLink())
    return DependencyIterator(WriteEffectDependencyIterator(this));
  else
    return DependencyIterator();
}

void WriteEffect::HappenAfter( WriteEffect* input ) {
  AddLink(input);
}

ReadEffectListIterator WriteEffect::AddReadEffect( ReadEffect* effect ) {
  auto itr = read_effect_.Find(effect);
  if(itr.HasNext()) return itr;
  return read_effect_.PushBack(zone(),effect);
}

EffectBarrier* WriteEffect::FirstBarrier() const {
  if(Is<EffectBarrier>())
    return const_cast<EffectBarrier*>(As<EffectBarrier>());
  else
    return NextBarrier();
}

EffectBarrier* WriteEffect::NextBarrier() const {
  auto e = NextLink();
  while(!e->Is<EffectBarrier>() && !e->Is<InitBarrier>()) {
    e = e->NextLink();
  }
  return e->As<EffectBarrier>();
}

class EffectMergeBase::EffectMergeBaseDependencyIterator {
 public:
  EffectMergeBaseDependencyIterator( const EffectMergeBase* node ):
    state_(),
    merge_(node),
    itr_  ()
  {
    auto w = node->lhs_effect();
    if(w->read_effect()->empty()) {
      state_ = LHSW;
    } else {
      state_ = LHS;
      itr_   = w->read_effect()->GetForwardIterator();
    }
  }
  bool HasNext() const { return state_ != DONE; }

  bool Move   () const {
    lava_debug(NORMAL,lava_verify(HasNext()););
    switch(state_) {
      case LHS :
        if(itr_.Move()) return true;
        // fallthrough
      case LHSW:
        {
          auto w = merge_->rhs_effect();
          if(w->read_effect()->empty()) {
            state_ = RHSW;
          } else {
            state_ = RHS;
            itr_   = w->read_effect()->GetForwardIterator();
          }
        }
        break;
      case RHS :
        if(itr_.Move()) return true;
        // fallthrough
      case RHSW:
        state_ = DONE;
        break;
      default:
        break;
    }

    return false;
  }

  Expr* value() {
    switch(state_) {
      case RHS :
      case LHS : return itr_.value();
      case LHSW: return merge_->lhs_effect();
      case RHSW: return merge_->rhs_effect();
      default:   break;
    }

    lava_die();
    return NULL;
  }

  Expr* value() const {
    return const_cast<EffectMergeBaseDependencyIterator*>(this)->value();
  }
 private:
  enum { LHSW , LHS, RHSW, RHS , DONE };
  mutable int state_;
  const EffectMergeBase* merge_;
  mutable ReadEffectListIterator itr_;
};

Expr::DependencyIterator EffectMergeBase::GetDependencyIterator() const {
	return DependencyIterator(EffectMergeBaseDependencyIterator(this));
}

std::size_t EffectMergeBase::dependency_size() const {
  auto l = lhs_effect();
  auto lsz = l->read_effect()->empty() ? 1 : l->read_effect()->size();

  auto r = rhs_effect();
  auto rsz = r->read_effect()->empty() ? 1 : r->read_effect()->size();

  return lsz + rsz;
}

Expr* IRList::Load( Expr* index ) const {
  if(index->Is<Float64>()) {
    if(std::uint32_t idx = 0; CastToIndex(index->As<Float64>()->value(),&idx)) {
      if(idx < operand_list()->size()) {
        return Operand(idx);
      }
    }
  }
  return NULL;
}

bool IRList::Store( Expr* index , Expr* value ) {
  if(index->Is<Float64>()) {
    if(std::uint32_t idx = 0; CastToIndex(index->As<Float64>()->value(),&idx)) {
      ReplaceOperand(idx,value);
      return true;
    }
  }
  return false;
}

Expr* IRObject::Load( Expr* key ) const {
  if(key->Is<StringNode>()) {
    auto &str = key->AsZoneString();
    lava_foreach( auto &k , operand_list()->GetForwardIterator() ) {
      auto kv = k->As<IRObjectKV>();
      if(kv->key()->Is<StringNode>() && kv->key()->AsZoneString() == str) {
        return kv->value();
      }
    }
  }
  return NULL;
}

bool IRObject::Store( Expr* key , Expr* value ) {
  if(key->Is<StringNode>()) {
    auto &str = key->AsZoneString();
    lava_foreach( auto &k , operand_list()->GetForwardIterator() ) {
      auto kv = k->As<IRObjectKV>();
      if(kv->key()->Is<StringNode>() && kv->key()->AsZoneString() == str) {
        kv->ReplaceOperand(1,value);
        return true;
      }
    }
  }
  return false;
}

void ControlFlow::Replace( ControlFlow* node ) {
  if(IsIdentical(node)) return;
  // 1. transfer all *use* node
  lava_foreach( auto &v , ref_list()->GetForwardIterator() ) {
    v.id.set_value(node);
  }
  node->ref_list_.Merge(&ref_list_);
  // 2. transfer all the backward and forward edge to |node|
  node->forward_edge_.Merge (&forward_edge_ );
  node->backward_edge_.Merge(&backward_edge_);
  // 3. clear all operand from *this* node
  ClearOperand();
}

void ControlFlow::RemoveBackwardEdgeOnly( ControlFlow* node ) {
  auto itr = backward_edge_.Find(node);
  lava_verify(itr.HasNext());
  backward_edge_.Remove(itr);
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

void ControlFlow::ClearBackwardEdge() {
  for( auto itr = backward_edge_.GetForwardIterator(); itr.HasNext(); itr.Move() ) {
    // remove it from its linked node's forward edge
    itr.value()->RemoveForwardEdgeOnly(this);
  }
  backward_edge_.Clear();
}

void ControlFlow::RemoveForwardEdgeOnly( ControlFlow* node ) {
  auto itr = forward_edge_.Find(node);
  lava_verify(itr.HasNext());
  forward_edge_.Remove(itr);
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

void ControlFlow::ClearForwardEdge() {
  for( auto itr = forward_edge_.GetForwardIterator(); itr.HasNext(); itr.Move() ) {
    itr.value()->RemoveBackwardEdgeOnly(this);
  }
  forward_edge_.Clear();
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
  id_                   ()
{}

Graph::~Graph() {}

void Graph::Initialize( Start* start , End* end ) {
  start_ = start;
  end_   = end;
}

void Graph::Initialize( OSRStart* start , OSREnd* end ) {
  start_ = start;
  end_   = end;
}

SetList::SetList( zone::Zone* zone , const Graph& graph ):
  zone_   (zone),
  existed_(zone,false,graph.MaxID()),
  array_  (zone)
{}

SetList::SetList( zone::Zone* zone , std::size_t size ):
  zone_   (zone),
  existed_(zone,false,size),
  array_  (zone)
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

OnceList::OnceList( zone::Zone* zone , const Graph& graph ):
  zone_   (zone),
  existed_(zone,false,graph.MaxID()),
  array_  (zone)
{}

OnceList::OnceList( zone::Zone* zone , std::size_t size ):
  zone_   (zone),
  existed_(zone,false,size),
  array_  (zone)
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
    auto top = stack_.Top()->As<ControlFlow>();
    stack_.Pop();
    lava_foreach( auto cf , top->forward_edge()->GetForwardIterator() ) {
      stack_.Push(cf);
    }
    next_ = top;
    return true;
  }
  next_ = NULL;
  return false;
}

bool ControlFlowEdgeIterator::Move() {
  if(!stack_.empty()) {
    ControlFlow* top = stack_.Top()->As<ControlFlow>();
    stack_.Pop();
    lava_foreach( auto cf , top->backward_edge()->GetBackwardIterator() ) {
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

} // namespace hir
} // namespace cbase
} // namespace lavascript
