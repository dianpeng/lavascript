#include "hir-kit.h"

namespace lavascript {
namespace cbase      {
namespace hir        {
namespace kit        {

void ControlFlowKit::Reset() {
  start_ = NULL;
  end_   = NULL;
  return_list_.clear();
  inline_block_.clear();
  context_.clear();
}

ControlFlowKit& ControlFlowKit::DoStart() {
  lava_debug(NORMAL,lava_verify(!start_););
  start_ = Start::New(graph_,Checkpoint::New(graph_,NULL),InitBarrier::New(graph_));
  context_.push_back(Context(Region::New(graph_,start_)));
  return *this;
}

ControlFlowKit& ControlFlowKit::DoEnd() {
  lava_debug(NORMAL,
    lava_verify(!end_);
    lava_verify(context_.size() == 1);
    lava_verify(inline_block_.size() == 0);
  );

  auto succ = Success::New(graph_);
  auto fail = Fail::New(graph_);

  succ->AddBackwardEdge(region());
  end_ = End::New(graph_,succ,fail);

  auto phi = Phi::New(graph_,succ);
  for( auto k : return_list_ ) {
    succ->AddBackwardEdge(k);
    phi->AddOperand(k->value());
  }

  if(start_->Is<Start>() && end_->Is<End>()) {
    graph_->Initialize(start_->As<Start>(),end_->As<End>());
  } else if(start_->Is<OSRStart>() && end_->Is<OSREnd>()) {
    graph_->Initialize(start_->As<OSRStart>(),end_->As<OSREnd>());
  } else {
    lava_die();
  }

  Reset();
  return *this;
}

ControlFlowKit& ControlFlowKit::DoInlineStart() {
  auto is = InlineStart::New(graph_,region());
  set_region(is);
  inline_block_.push_back(InlineBlock(is));
  return *this;
}

ControlFlowKit& ControlFlowKit::DoInlineEnd() {
  lava_debug(NORMAL,lava_verify(!inline_block_.empty()););
  auto ie = InlineEnd::New(graph_,region());
  if(inline_block_.size() > 1) {
    auto phi= Phi::New(graph_,ie);
    for( auto jv : inline_block().jump_value ) {
      ie->AddBackwardEdge(jv);
      phi->AddOperand(jv->value());
    }
  }
  set_region(ie);
  inline_block_.pop_back();
  return *this;
}

ControlFlowKit& ControlFlowKit::DoReturn( Expr* retval ) {
  if(!retval) retval = Nil::New(graph_);
  auto r = Return::New(graph_,retval,region());
  set_region(r);
  return_list_.push_back(r);
  return *this;
}

ControlFlowKit& ControlFlowKit::DoJumpValue( Expr* retval ) {
  lava_debug(NORMAL,lava_verify(!inline_block_.empty()););
  if(!retval) retval = Nil::New(graph_);
  auto jv = JumpValue::New(graph_,retval,region());
  set_region(jv);
  inline_block().jump_value.push_back(jv);
  return *this;
}

ControlFlowKit& ControlFlowKit::DoIf( Expr* node ) {
  lava_debug(NORMAL,lava_verify(context().IsBB()););

  auto inode = If::New(graph_,node,region());
  // order matters , if_false --> if_true
  auto ifalse= IfFalse::New(graph_,inode);
  auto itrue = IfTrue::New(graph_,inode);

  context().bb = NULL;
  context().br.if_node = inode;
  context().br.if_true = itrue;
  context().br.if_false= ifalse;
  context().type = BR;

  context_.push_back(Context(itrue));
  return *this;
}

ControlFlowKit& ControlFlowKit::DoElse() {
  lava_debug(NORMAL,lava_verify(prev_context().IsBR());lava_verify(context().IsBB()););

  auto pctx = prev_context();
  pctx.br.if_true  = region(); // cached the current updated if_true node
  context().bb  = pctx.br.if_false;
  return *this;
}

ControlFlowKit& ControlFlowKit::DoEndIf( Phi* phi ) {
  lava_debug(NORMAL,lava_verify(prev_context().IsBR());lava_verify(context().IsBB()););

  auto merge = Region::New(graph_);
  (void)phi;

  // order matters , if_false --> if_true
  merge->AddBackwardEdge(region());
  merge->AddBackwardEdge(prev_context().br.if_true);

  context_.pop_back(); context().SetBB(merge);
  return *this;
}

ControlFlowKit& ControlFlowKit::DoRegion() {
  lava_debug(NORMAL,lava_verify(context().IsBB()););

  auto r = Region::New(graph_,region());
  set_region(r);
  return *this;
}

ControlFlowKit::ControlFlowKit( Graph* graph ):
  graph_(graph),
  start_(NULL),
  end_  (NULL),
  return_list_(),
  context_    (),
  inline_block_()
{}

// ---------------------------------------------------------------------
// Expression
// ---------------------------------------------------------------------

E::E( Graph* graph , double value ) : node_(Float64::New(graph,value))        , graph_(graph) {}
E::E( Graph* graph , int    value ) : node_(Float64::New(graph,(double)value)), graph_(graph) {}
E::E( Graph* graph , bool   value ) : node_(Boolean::New(graph,value))        , graph_(graph) {}
E::E( Graph* graph , const char* v) : node_(NewString(graph,v))               , graph_(graph) {}

E::E( Graph* graph , const std::string& v ):
  node_ (NewString(graph,v.c_str())), graph_(graph) {}

E::E( Graph* graph ) : node_(Nil::New(graph)), graph_(graph) {}

E E::Arg( Graph* graph , std::uint32_t index ) {
  E v(graph);
  v.node_ = Arg::New(graph,index);
  return v;
}

E E::GGet( Graph* graph , const char* str ) {
  E v(graph);
  v.node_ = GGet::New(graph,NewString(graph,str));
  return v;
}

E E::UGet  ( Graph* graph , std::uint32_t method , std::uint8_t idx ) {
  E v(graph);
  v.node_ = UGet::New(graph,idx,method);
  return v;
}

E E::Not   ( const E& that ) {
  auto tp = GetTypeInference(that.node_);
  if(tp == TPKIND_BOOLEAN) {
    return E(that.graph_,BooleanNot::New(that.graph_,that.node_));
  }
  return E(that.graph_,Unary::New(that.graph_,that.node_,Unary::NOT));
}

E E::Negate( const E& that ) {
  auto tp = GetTypeInference(that.node_);
  if(tp == TPKIND_FLOAT64) {
    return E(that.graph_,Float64Negate::New(that.graph_,that.node_));
  }
  return E(that.graph_,Unary::New(that.graph_,that.node_,Unary::MINUS));
}

} // namespace kit
} // namespace hir
} // namespace cbase
} // namespace lavascript
