#include "hir.h"
#include "value-range.h"
#include <sstream>

namespace lavascript {
namespace cbase {
namespace hir {

const char* IRTypeGetName( IRType type ) {
#define __(A,B,C,...) case IRTYPE_##B: return C;
  switch(type) {
    CBASE_IR_LIST(__)
    default: lava_die(); return NULL;
  }
#undef __ // __
}

void Expr::Replace( Expr* another ) {
  // 1. check all the operand_list and patch each operands reference
  //    list to be new one
  for( auto itr = ref_list_.GetForwardIterator(); itr.HasNext() ; itr.Move() ) {
    itr.value().id.set_value( another );
  }

  // 2. modify *this* if the node is a statement
  if(IsStatement()) {
    auto region = statement_edge().region;

    region->RemoveStatement(statement_edge());

    region->AddStatement(another);
  }
}

IRList* IRList::Clone( Graph* graph , const IRList& that ) {
  auto ret = IRList::New(graph,that.Size(),that.ir_info());
  for( auto itr(that.operand_list()->GetForwardIterator());
       itr.HasNext(); itr.Move() ) {
    ret->Add(itr.value());
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
    for( auto itr(that.operand_list()->GetForwardIterator());
        itr.HasNext() && (count < end); itr.Move() ) {
      ret->Add(itr.value());
    }
    return ret;
  }
}

std::uint64_t ICall::GVNHash() const {
  GVNHashN hasher(type_name());
  hasher.Add(static_cast<std::uint32_t>(ic()));
  for( auto itr(operand_list()->GetForwardIterator()); itr.HasNext(); itr.Move() ) {
    hasher.Add(itr.value()->GVNHash());
  }
  return hasher.value();
}

bool ICall::Equal( const Expr* that ) const {
  if(that->IsICall()) {
    auto tic = that->AsICall();
    if(ic() == tic->ic()) {
      lava_debug(NORMAL,
        lava_verify(operand_list()->size() == tic->operand_list()->size()););
      auto this_itr(operand_list()->GetForwardIterator());
      auto that_itr(that->operand_list()->GetForwardIterator());
      for( ; this_itr.HasNext() ; this_itr.Move() , that_itr.Move() ) {
        if(!this_itr.value()->Equal(that_itr.value())) {
          return false;
        }
      }
      return true;
    }
  }
  return false;
}

void ControlFlow::Replace( ControlFlow* node ) {
  // 1. transfer all *use* node
  for( auto itr = ref_list_.GetForwardIterator(); itr.HasNext() ; itr.Move() ) {
    itr.value().id.set_value(node);
  }

  // 2. transfer all the backward and forward edge to |node|
  node->forward_edge ()->Append(forward_edge());
  node->backward_edge()->Append(backward_edge());
}

void ControlFlow::RemoveBackwardEdge( ControlFlow* node ) {
  auto itr = backward_edge()->Find(node);
  lava_verify(itr.HasNext());

  {
    auto i = node->forward_edge()->Find(this);
    lava_verify(i.HasNext());
    node->forward_edge()->Remove(i);
  }

  backward_edge()->Remove(itr);
}

void ControlFlow::RemoveBackwardEdge( std::size_t index ) {
  return RemoveBackwardEdge(backward_edge()->Index(index));
}

void ControlFlow::RemoveForwardEdge( ControlFlow* node ) {
  auto itr = forward_edge()->Find(node);
  lava_verify(itr.HasNext());

  {
    auto i = node->backward_edge()->Find(this);
    lava_verify(i.HasNext());
    node->backward_edge()->Remove(i);
  }

  forward_edge()->Remove(itr);
}

void ControlFlow::RemoveForwardEdge( std::size_t index ) {
  return RemoveForwardEdge(forward_edge()->Index(index));
}

void ControlFlow::MoveStatement( ControlFlow* cf ) {
  for( auto itr(cf->statement_list()->GetForwardIterator());
       itr.HasNext(); itr.Move() ) {
    auto n = itr.value();
    AddStatement(n);
  }
}

Graph::Graph():
  zone_                 (),
  start_                (NULL),
  end_                  (NULL),
  prototype_info_       (),
  id_                   (),
  static_type_inference_(),
  value_range_          ()
{}

Graph::~Graph() {}

void Graph::Initialize( Start* start , End* end ) {
  start_ = start;
  end_   = end;

  value_range_.resize(MaxID());
}

void Graph::Initialize( OSRStart* start , OSREnd* end ) {
  start_ = start;
  end_   = end;

  value_range_.resize(MaxID());
}

void Graph::SetValueRange( std::uint32_t id ,
                           std::unique_ptr<ValueRange>&& ptr ) {
  value_range_[id] = std::move(ptr);
}

void Graph::GetControlFlowNode( std::vector<ControlFlow*>* output ) const {
  output->clear();
  for( ControlFlowBFSIterator itr(*this) ; itr.HasNext() ; itr.Move() ) {
    output->push_back(itr.value());
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

    for( auto itr(top->forward_edge()->GetForwardIterator());
         itr.HasNext() ; itr.Move() ) {
      stack_.Push(itr.value());
    }
    next_ = top;
    return true;
  }
  next_ = NULL;
  return false;
}

namespace {

struct ForwardEdgeGetter {
  RegionList* Get() const { return node_->forward_edge(); }
  ForwardEdgeGetter( ControlFlow* node ) : node_(node) {}
 private:
  ControlFlow* node_;
};

struct BackwardEdgeGetter {
  RegionList* Get() const { return node_->backward_edge(); }
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

    for( auto itr(GETTER(top).Get()->GetForwardIterator()) ;
         itr.HasNext() ; itr.Move() ) {

      ControlFlow* pre = itr.value();

      if(stack->Push(pre)) goto recursion;
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
    for( auto itr(top->backward_edge()->GetForwardIterator());
         itr.HasNext() ; itr.Move() ) {
      auto value = itr.value();
      if(!mark_[value->id()] && stack_.Push(value)) {
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

    for( auto itr = top->backward_edge()->GetBackwardIterator();
         itr.HasNext(); itr.Move() ) {
      ControlFlow* pre = itr.value();
      stack_.Push(pre);
      results_.push_back(Edge(top,pre));
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
    for( std::size_t i = 0 ; i < top->operand_list()->size() ; ++i ) {
      Expr* val = top->operand_list()->Index(i);
      if(stack_.Push(val)) goto recursion;
    }

    next_ = top;
    stack_.Pop();
    return true;
  }
  next_ = NULL;
  return false;
}

namespace {

class DotGraphVisualizer {
 public:
  DotGraphVisualizer(): graph_(NULL), existed_(), output_() , opt_() {}

  // Visiualize the graph into DOT representation and return the string
  std::string Visualize( const Graph& , const Graph::DotFormatOption& opt );
 private:
  void RenderControlFlow( const std::string& , ControlFlow* );
  void RenderExpr       ( const std::string& , Expr* );
  void RenderEdge       ( ControlFlow* , ControlFlow* );
  void RenderCheckpoint ( const std::string& , Checkpoint* );

  std::stringstream& Indent( int level );
  std::string GetNodeName( Node* );
 private:
  const Graph* graph_;
  DynamicBitSet existed_;
  std::stringstream output_;
  Graph::DotFormatOption opt_;
};

std::string DotGraphVisualizer::Visualize( const Graph& graph , const Graph::DotFormatOption& opt ) {
  // 1. prepare all the status variables

  graph_   = &graph;
  opt_     = opt;
  existed_.resize(graph.MaxID());

  // 2. edge iterator
  output_ << "digraph IR {\n";
  for( ControlFlowEdgeIterator itr(graph) ; itr.HasNext() ; itr.Move() ) {
    auto edge = itr.value();
    RenderEdge(edge.from,edge.to);
  }
  output_ << "}\n";

  return output_.str();
}

std::stringstream& DotGraphVisualizer::Indent( int level ) {
  const char* kIndent = "  ";
  for( ; level > 0 ; --level )
    output_ << kIndent;
  return output_;
}

std::string DotGraphVisualizer::GetNodeName( Node* node ) {
  return Format("%s_%d",node->type_name(),node->id());
}

void DotGraphVisualizer::RenderCheckpoint ( const std::string& cp_name ,
                                            Checkpoint* checkpoint ) {
  if(!checkpoint || !opt_.checkpoint ) return;

  Indent(1) << cp_name << "[shape=diamond style=bold color=pink label=\"" << cp_name <<"\"]\n";

  const std::size_t len = checkpoint->operand_list()->size();

  for( std::size_t i = 0 ; i < len ; ++i ) {
    auto n = checkpoint->operand_list()->Index(i);
    if(n->IsStackSlot()) {
      auto ss        = n->AsStackSlot();
      auto ss_name   = GetNodeName(ss);

      Indent(1) << ss_name << "[shape=doublecircle style=bold color=cyan label=\"stack_slot("
                            << ss->index()
                            << ")\"]\n";

      // render the expression
      auto expr      = ss->expr();
      auto expr_name = GetNodeName(expr);
      RenderExpr(expr_name,expr);
      Indent(1) << ss_name << " -> " << expr_name <<'\n';
      Indent(1) << cp_name << " -> " << ss_name   <<'\n';
    } else {
      lava_debug(NORMAL,lava_verify(n->IsUValSlot()););

      auto us      = n->AsUValSlot();
      auto us_name = GetNodeName(us);

      Indent(1) << us_name << "[shape=doublecircle style=bold color=cyan label=\"[uval_slot("
                           << us->index()
                           << ")\"]\n";

      // render the expression
      auto expr      = us->expr();
      auto expr_name = GetNodeName(expr);
      RenderExpr(expr_name,expr);
      Indent(1) << us_name << " -> " << expr_name <<'\n';
      Indent(1) << cp_name << " -> " << us_name   <<'\n';
    }
  }
}

void DotGraphVisualizer::RenderControlFlow( const std::string& region_name ,
                                            ControlFlow* region ) {
  Indent(1) << region_name << "[shape=box style=bold color=red label="
                           << "\""
                           << region->type_name()
                           << "\"]\n";

  // for all the operand of each control flow node
  for( auto itr(region->operand_list()->GetForwardIterator()) ;
       itr.HasNext() ; itr.Move() ) {
    auto node = itr.value();
    auto name = GetNodeName(node);
    RenderExpr(name,node);
    Indent(1) << region_name << " -> " << name << '\n';
  }

  // for all the statment's bounded inside of this control flow node
  for( auto itr = region->statement_list()->GetForwardIterator() ;
            itr.HasNext() ; itr.Move() ) {
    auto expr = itr.value();
    auto name = GetNodeName(expr);
    RenderExpr(name,expr);
    Indent(1) << region_name << " -> " << name << "[color=grey style=dashed]\n";
  }
}

void DotGraphVisualizer::RenderEdge( ControlFlow* from , ControlFlow* to ) {
  std::string from_name = GetNodeName(from);
  std::string to_name   = GetNodeName(to);

  if(!existed_[from->id()]) {
    existed_[from->id()] = true;
    RenderControlFlow(from_name,from);
  }

  if(!existed_[to->id()]) {
    existed_[to->id()] = true;
    RenderControlFlow(to_name,to);
  }

  Indent(1) << from_name << " -> " << to_name << "[color=black style=bold]\n";
}

void DotGraphVisualizer::RenderExpr( const std::string& name , Expr* node ) {
  if(existed_[node->id()])
    return;

  existed_[node->id()] = true;

  switch(node->type()) {
    case IRTYPE_FLOAT64:
      Indent(1) << name << "[label=\"f64(" << node->AsFloat64()->value() << ")\"]\n";
      break;
    case IRTYPE_LONG_STRING:
      Indent(1) << name << "[label=\"str(" << node->AsLString()->value()->data() << ")\"]\n";
      break;
    case IRTYPE_SMALL_STRING:
      Indent(1) << name << "[label=\"sso(" << node->AsSString()->value()->data() << ")\"]\n";
      break;
    case IRTYPE_BOOLEAN:
      Indent(1) << name << "[label=\"bool(" << (node->AsBoolean()->value() ? "true" : "false" )
                                            << ")\"]\n";
      break;
    case IRTYPE_NIL:
      Indent(1) << name << "[label=\"nil\"]\n";
      break;
    case IRTYPE_LIST:
      {
        Indent(1) << name << "[label=\"list\"]\n";
        auto list = node->AsIRList();
        std::size_t i = 0 ;

        for( auto itr(list->operand_list()->GetForwardIterator()) ;
             itr.HasNext() ; itr.Move() ) {
          auto element = itr.value();
          auto element_name = GetNodeName(element);
          RenderExpr(element_name,element);
          Indent(1) << name << " -> " << element_name << "[label=\"" << i << "\"]\n";
          ++i;
        }
      }
      break;
    case IRTYPE_OBJECT_KV:
      {
        Indent(1) << name << "[label=\"object_kv\"]\n";
        auto kv = node->AsIRObjectKV();
        auto key= kv->key();
        auto key_name = GetNodeName(key);
        RenderExpr(key_name,key);
        Indent(1) << name << " -> " << key_name << "[label=\"key\"]\n";

        auto val = kv->value();
        auto val_name = GetNodeName(val);
        RenderExpr(val_name,val);
        Indent(1) << name << " -> " << val_name << "[label=\"val\"]\n";
      }
      break;
    case IRTYPE_OBJECT:
      {
        Indent(1) << name << "[label=\"object\"]\n";
        auto obj = node->AsIRObject();
        std::size_t i = 0;
        for( auto itr(obj->operand_list()->GetForwardIterator()) ;
             itr.HasNext(); itr.Move() ) {
          auto kv = itr.value();
          auto kv_name = GetNodeName(kv);
          RenderExpr(kv_name,kv);
          Indent(1) << name << " -> " << kv_name << "[label=\"" << i << "\"]\n";
          ++i;
        }
      }
      break;
    case IRTYPE_LOAD_CLS:
      {
        auto obj = node->AsLoadCls();
        Indent(1) << name << "[label=\"ref(" << obj->ref() << ")\"]\n";
      }
      break;
    case IRTYPE_ARG:
      {
        auto arg = node->AsArg();
        Indent(1) << name << "[label=\"index(" << arg->index() << ")\"]\n";
      }
      break;
    case IRTYPE_BINARY:
    case IRTYPE_FLOAT64_BITWISE:
    case IRTYPE_FLOAT64_ARITHMETIC:
    case IRTYPE_FLOAT64_COMPARE:
      {
        auto binary = static_cast<Binary*>(node);

        auto lhs_name = GetNodeName(binary->lhs());
        auto rhs_name = GetNodeName(binary->rhs());
        RenderExpr(lhs_name,binary->lhs());
        RenderExpr(rhs_name,binary->rhs());

        Indent(1) << name << "[label=\""
                          << binary->type_name()
                          << '('
                          << binary->op_name()
                          << ")\"]\n";

        Indent(1) << name << " -> " << lhs_name << '\n';
        Indent(1) << name << " -> " << rhs_name << '\n';
      }
      break;
    case IRTYPE_UNARY:
      {
        auto unary = static_cast<Unary*>(node);
        Indent(1) << name << "[label=\""
                          << unary->type_name()
                          << '('
                          << unary->op_name()
                          << ")\"]\n";

      }
      break;
    case IRTYPE_TERNARY:
      {
        auto tern = node->AsTernary();
        auto cond_name = GetNodeName(tern->condition());
        auto lhs_name  = GetNodeName(tern->lhs());
        auto rhs_name  = GetNodeName(tern->rhs());
        RenderExpr(cond_name,tern->condition());
        RenderExpr(lhs_name ,tern->lhs());
        RenderExpr(rhs_name ,tern->rhs());
        Indent(1) << name << "[label=\"" << tern->type_name() << "\"]\n";
        Indent(1) << name << " -> " << cond_name << "[label=\"condition\"]\n";
        Indent(1) << name << " -> " << lhs_name  << "[label=\"lhs\"]\n";
        Indent(1) << name << " -> " << rhs_name  << "[label=\"rhs\"]\n";

      }
      break;
    case IRTYPE_UVAL:
      Indent(1) << name << "[label=\"uval(" << node->AsUVal()->index() << ")\"]\n";
      break;
    case IRTYPE_USET:
      {
        auto uset = node->AsUSet();
        auto opr_name = GetNodeName(uset->value());
        RenderExpr(opr_name,uset->value());
        Indent(1) << name << "[label=\"uset\"]\n";
        Indent(1) << name << opr_name << '\n';
      }
      break;
    case IRTYPE_PGET:
    case IRTYPE_OBJECT_GET:
      {
        auto pget = static_cast<PGet*>(node);

        auto obj_name = GetNodeName(pget->object());
        auto key_name = GetNodeName(pget->key());
        RenderExpr(obj_name,pget->object());
        RenderExpr(key_name,pget->key());
        Indent(1) << name << "[label=\"" << pget->type_name() << "\"]\n";
        Indent(1) << name << " -> " << obj_name << "[label=\"object\"]\n";
        Indent(1) << name << " -> " << key_name << "[label=\"key\"]\n";

      }
      break;
    case IRTYPE_PSET:
    case IRTYPE_OBJECT_SET:
      {
        auto pset = static_cast<PSet*>(node);

        auto obj_name = GetNodeName(pset->object());
        auto key_name = GetNodeName(pset->key());
        auto val_name = GetNodeName(pset->value());
        RenderExpr(obj_name,pset->object());
        RenderExpr(key_name,pset->key());
        RenderExpr(val_name,pset->value());
        Indent(1) << name << "[label=\"" << pset->type_name() << "\"]\n";
        Indent(1) << name << " -> " << obj_name << "[label=\"object\"]\n";
        Indent(1) << name << " -> " << key_name << "[label=\"key\"]\n";
        Indent(1) << name << " -> " << val_name << "[label=\"value\"]\n";

      }
      break;
    case IRTYPE_IGET:
    case IRTYPE_LIST_GET:
    case IRTYPE_EXTENSION_GET:
      {
        auto iget = static_cast<IGet*>(node);

        auto obj_name = GetNodeName(iget->object());
        auto idx_name = GetNodeName(iget->index());
        RenderExpr(obj_name,iget->object());
        RenderExpr(idx_name,iget->index());
        Indent(1) << name << "[label=\"" << iget->type_name() << "\"]\n";
        Indent(1) << name << " -> " << obj_name << "[label=\"object\"]\n";
        Indent(1) << name << " -> " << idx_name << "[label=\"index\"]\n";

      }
      break;
    case IRTYPE_ISET:
    case IRTYPE_LIST_SET:
    case IRTYPE_EXTENSION_SET:
      {
        auto iset = static_cast<ISet*>(node);

        auto obj_name = GetNodeName(iset->object());
        auto idx_name = GetNodeName(iset->index() );
        auto val_name = GetNodeName(iset->value() );
        RenderExpr(obj_name,iset->object());
        RenderExpr(idx_name,iset->index());
        RenderExpr(val_name,iset->value());
        Indent(1) << name << "[label=\"" << iset->type_name() << "\"]\n";
        Indent(1) << name << " -> " << obj_name << "[label=\"object\"]\n";
        Indent(1) << name << " -> " << idx_name << "[label=\"index\"]\n";
        Indent(1) << name << " -> " << val_name << "[label=\"value\"]\n";

      }
      break;
    case IRTYPE_GGET:
      {
        auto gget = node->AsGGet();
        auto key_name = GetNodeName(gget->key());
        RenderExpr(key_name,gget->key());
        Indent(1) << name << "[label=\"" << gget->type_name() << "\"]\n";
        Indent(1) << name << " -> " << key_name << "[label=\"key\"]\n";

      }
      break;
    case IRTYPE_GSET:
      {
        auto gset = node->AsGSet();
        auto key_name = GetNodeName(gset->key());
        auto val_name = GetNodeName(gset->value());
        RenderExpr(key_name,gset->key());
        RenderExpr(val_name,gset->value());
        Indent(1) << name << "[label=\"" << gset->type_name() << "\"]\n";
        Indent(1) << name << " -> " << key_name << "[label=\"key\"]\n";
        Indent(1) << name << " -> " << val_name << "[label=\"val\"]\n";

      }
      break;
    case IRTYPE_ITR_NEW:
      {
        auto itr_new = node->AsItrNew();
        auto opr_name= GetNodeName(itr_new->operand());
        RenderExpr(opr_name,itr_new->operand());
        Indent(1) << name << "[label=\"" << itr_new->type_name() << "\"]\n";
        Indent(1) << name << " -> " << opr_name << '\n';

      }
      break;
    case IRTYPE_ITR_NEXT:
      {
        auto itr_next = node->AsItrNext();
        auto opr_name = GetNodeName(itr_next->operand());
        RenderExpr(opr_name,itr_next->operand());
        Indent(1) << name << "[label=\"" << itr_next->type_name() << "\"]\n";
        Indent(1) << name << " -> " << opr_name << '\n';

      }
      break;
    case IRTYPE_ITR_TEST:
      {
        auto itr_test = node->AsItrTest();
        auto opr_name = GetNodeName(itr_test->operand());
        RenderExpr(opr_name,itr_test->operand());
        Indent(1) << name << "[label=\"" << itr_test->type_name() << "\"]\n";
        Indent(1) << name << " -> " << opr_name << '\n';

      }
      break;
    case IRTYPE_ITR_DEREF:
      {
        auto itr_deref = node->AsItrDeref();
        auto itr_name  = GetNodeName(itr_deref->operand());
        RenderExpr(itr_name,itr_deref->operand());
        Indent(1) << name << "[label=\"" << itr_deref->type_name() << "\"]\n";
        Indent(1) << name << " -> " << itr_name << '\n';

      }
      break;
    case IRTYPE_PHI:
      {
        auto phi = node->AsPhi();
        Indent(1) << name << "[label=\"PHI\" color=gray style=filled]\n";
        std::size_t count = 0;
        for( auto itr = phi->operand_list()->GetForwardIterator() ; itr.HasNext() ; itr.Move() , ++count ) {
          auto node = itr.value();
          auto node_name = GetNodeName(node);
          RenderExpr(node_name,node);
          Indent(1) << name << " -> " << node_name << "[label=\"" << count
                                                                  << "\" color=pink style=bold]\n";
        }
      }
      break;

    case IRTYPE_PROJECTION:
      Indent(1) << name << "[label=\"projection(" << node->AsProjection()->index()
                                                  <<")]\n";
      break;
    case IRTYPE_INIT_CLS:
      {
        Indent(1) << name << "[label=\"init_cls\"]\n";
        auto icls = node->AsInitCls();
        auto key  = icls->key();
        auto key_name = GetNodeName(key);
        RenderExpr(key_name,key);
        Indent(1) << name << " -> " << key_name << '\n';
      }
      break;

    /** test **/
    case IRTYPE_TEST_LISTOOB:
      {
        Indent(1) << name << "[label=\"list-oob\"]\n";
        auto oob = node->AsTestListOOB();

        auto obj = oob->object();
        auto obj_name = GetNodeName(obj);
        RenderExpr(obj_name,obj);
        Indent(1) << name << " -> " << obj_name << '\n';

        auto idx = oob->index();
        auto idx_name = GetNodeName(idx);
        Indent(1) << name << " -> " << idx_name << '\n';
      }
      break;

    case IRTYPE_TEST_TYPE:
      {
        auto tt = node->AsTestType();
        Indent(1) << name << "[label=\"test-type(" << tt->type_kind_name() << ")\"]\n";
        auto obj = tt->object();
        auto obj_name = GetNodeName(obj);
        RenderExpr(obj_name,obj);

        Indent(1) << name << " -> " << obj_name << '\n';
      }
      break;

    case IRTYPE_BOX:
      {
        auto box = node->AsBox();
        Indent(1) << name << "[label=\"box(" << GetTypeKindName(box->type_kind()) << ")\"]\n";
        auto obj = box->value();
        auto obj_name = GetNodeName(obj);
        RenderExpr(obj_name,obj);

        Indent(1) << name << " -> " << obj_name << '\n';
      }
      break;
    case IRTYPE_UNBOX:
      {
        auto unbox = node->AsUnbox();
        Indent(1) << name << "[label=\"unbox(" << GetTypeKindName(unbox->type_kind()) << ")\"]\n";
        auto obj = unbox->value();
        auto obj_name = GetNodeName(obj);
        RenderExpr(obj_name,obj);

        Indent(1) << name << " -> " << obj_name << '\n';
      }
      break;

    /** function call and icall **/
    case IRTYPE_ICALL:
      {
        auto ic = node->AsICall();

        Indent(1) << name << "[label=\"icall(" << (ic->tail_call() ? "tail" : "normal")
                                               <<','
                                               << interpreter::GetIntrinsicCallName(ic->ic())
                                               <<")\"]\n";

        int count = 0;
        for( auto itr(ic->operand_list()->GetForwardIterator()) ;
             itr.HasNext() ; itr.Move() ) {
          auto arg      = itr.value();
          auto arg_name = GetNodeName(arg);
          RenderExpr(arg_name,arg);
          Indent(1) << name << " -> " << arg_name << "[label=" << count << "]\n";
          ++count;
        }
      }
      break;
    case IRTYPE_OSR_LOAD:
      {
        auto osr_load = node->AsOSRLoad();
        Indent(1) << name << "[label=\"osr_load(" << osr_load->index() << ")\"]\n";
      }
      break;
    case IRTYPE_CHECKPOINT:
      return RenderCheckpoint(name,node->AsCheckpoint());
    default:
      {
        Indent(1) << name << "[label=\"" << node->type_name() << "\"]\n";
        for( auto itr(node->operand_list()->GetForwardIterator()) ;
             itr.HasNext() ; itr.Move() ) {
          auto opr = itr.value();
          auto opr_name = GetNodeName(opr);
          RenderExpr(opr_name,opr);
          Indent(1) << name << " -> " << opr_name << '\n';
        }
      }
      break;
  }

  // effect list node
  for( auto itr(node->effect_list()->GetForwardIterator()) ;
       itr.HasNext() ; itr.Move() ) {
    Indent(1) << name << " -> " << GetNodeName(itr.value())
                                << "[label=\"depend\" style=filled color=blue ]\n";
  }
}

} // namespace

std::string Graph::PrintToDotFormat( const Graph& graph , const Graph::DotFormatOption& opt ) {
  return DotGraphVisualizer().Visualize(graph,opt);
}

} // namespace hir
} // namespace cbase
} // namespace lavascript
