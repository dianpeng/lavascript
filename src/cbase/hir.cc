#include "hir.h"
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

std::uint64_t IRList::GVNHash() const {
  GVNHashN hasher(type_name());
  for( auto itr(operand_list()->GetForwardIterator()) ;
       itr.HasNext(); itr.Move() ) {
    auto v = itr.value();
    auto h = v->GVNHash();
    if(!h) return 0;
    hasher.Add(h);
  }
  return hasher.value();
}

bool IRList::Equal( const Expr* that ) const {
  if(that->IsIRList()) {
    auto irlist = that->AsIRList();
    if(irlist->operand_list()->size() == operand_list()->size()) {
      auto that_itr(that->operand_list()->GetForwardIterator());

      for( auto itr(operand_list()->GetForwardIterator()) ;
           itr.HasNext(); itr.Move() , that_itr.Move() ) {
        auto v = itr.value();
        if(!v->Equal(that_itr.value()))
          return false;
      }

      return true;
    }
  }
  return false;
}

std::uint64_t IRObject::GVNHash() const {
  GVNHashN hasher(type_name());

  for( auto itr(operand_list()->GetForwardIterator()) ;
       itr.HasNext() ; itr.Move() ) {
    auto v = itr.value();
    auto h = v->GVNHash();
    if(!h) return 0;
    hasher.Add(h);
  }

  return hasher.value();
}

bool IRObject::Equal( const Expr* that ) const {
  if(that->IsIRObject()) {
    auto irobj = that->AsIRObject();
    if(irobj->operand_list()->size() == operand_list()->size()) {
      auto that_itr(that->operand_list()->GetForwardIterator());

      for( auto itr(operand_list()->GetForwardIterator()) ;
           itr.HasNext(); itr.Move() , that_itr.Move()) {
        auto v = itr.value();
        if(!v->Equal(that_itr.value()))
          return false;
      }
      return true;
    }
  }
  return false;
}

std::uint64_t Phi::GVNHash() const {
  auto opr_list = operand_list();
  auto len = opr_list->size();
  GVNHashN hasher(type_name());
  for( std::size_t i = 0 ; i < len ; ++i ) {
    auto val = opr_list->Index(i)->GVNHash();
    hasher.Add(val);
  }
  return hasher.value();
}

bool Phi::Equal( const Expr* that ) const {
  if(that->IsPhi()) {
    auto phi = that->AsPhi();
    if(operand_list()->size() == phi->operand_list()->size()) {
      auto len = operand_list()->size();
      for( std::size_t i = 0 ; i < len ; ++i ) {
        if(!operand_list()->Index(i)->Equal(phi->operand_list()->Index(i)))
          return false;
      }
      return true;
    }
  }
  return false;
}

void Expr::Replace( Expr* another ) {
  // 1. check all the operand_list and patch each operands reference
  //    list to be new one
  for( auto itr = ref_list_.GetForwardIterator(); itr.HasNext() ; itr.Move() ) {
    itr.value().id.set_value( another );
  }

  // 2. check whether this operation has side effect or not and update the
  //    side effect accordingly
  if(HasEffect()) {
    EffectEdge ee (effect());
    ee.iterator.set_value(another);
    another->set_effect(ee);
  }
}

void Graph::Initialize( Start* start , End* end ) {
  start_ = start;
  end_   = end;
}

void Graph::Initialize( OSRStart* start , OSREnd* end ) {
  start_ = start;
  end_   = end;
}

namespace {

class DotGraphVisualizer {
 public:
  DotGraphVisualizer(): graph_(NULL), existed_(NULL), output_(NULL) , opt_() {}

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
  DynamicBitSet* existed_;
  std::stringstream* output_;
  Graph::DotFormatOption opt_;
};

std::string DotGraphVisualizer::Visualize( const Graph& graph , const Graph::DotFormatOption& opt ) {
  // 1. prepare all the status variables
  std::stringstream output;
  DynamicBitSet bitset(graph.MaxID());

  graph_   = &graph;
  output_  = &output;
  existed_ = &bitset;
  opt_ = opt;

  // 2. edge iterator
  output << "digraph IR {\n";
  for( ControlFlowEdgeIterator itr(graph) ; itr.HasNext() ; itr.Move() ) {
    auto edge = itr.value();
    RenderEdge(edge.from,edge.to);
  }
  output << "}\n";

  return output_->str();
}

std::stringstream& DotGraphVisualizer::Indent( int level ) {
  const char* kIndent = "  ";
  for( ; level > 0 ; --level )
    (*output_) << kIndent;
  return (*output_);
}

std::string DotGraphVisualizer::GetNodeName( Node* node ) {
  return Format("%s_%d",node->type_name(),node->id());
}

void DotGraphVisualizer::RenderCheckpoint ( const std::string& operation,
                                            Checkpoint* checkpoint ) {
  if(!checkpoint || !opt_.checkpoint ) return;

  auto cp_name = GetNodeName(checkpoint);
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

  // link the checkpoint name back to the attached operation node
  Indent(1) << operation << " -> " << cp_name << '\n';
}

void DotGraphVisualizer::RenderControlFlow( const std::string& region_name ,
                                            ControlFlow* region ) {
  Indent(1) << region_name << "[shape=box style=bold color=red label="
                           << "\""
                           << region->type_name()
                           << "\"]\n";

  switch(region->type()) {
    case IRTYPE_LOOP_HEADER:
      {
        auto node = region->AsLoopHeader();
        auto name = GetNodeName(node->condition());
        RenderExpr(name,node->condition());
        Indent(1) << region_name << " -> " << name << '\n';
      }
      break;
    case IRTYPE_LOOP_EXIT:
      {
        auto node = region->AsLoopExit();
        auto name = GetNodeName(node->condition());
        RenderExpr(name,node->condition());
        Indent(1) << region_name << " -> " << name << '\n';
      }
      break;
    case IRTYPE_IF:
      {
        auto node = region->AsIf();
        auto name = GetNodeName(node->condition());
        RenderExpr(name,node->condition());
        Indent(1) << region_name << " -> " << name << '\n';
      }
      break;
    case IRTYPE_RETURN:
      {
        auto node = region->AsReturn();
        auto name = GetNodeName(node->value());
        RenderExpr(name,node->value());
        Indent(1) << region_name << " -> " << name << '\n';
      }
      break;
    case IRTYPE_END:
      {
        auto node = region->AsEnd();
        auto name = GetNodeName(node->return_value());
        RenderExpr(name,node->return_value());
        Indent(1) << region_name << " -> " << name << '\n';
      }
      break;
    default:
      break;
  }

  for( auto itr = region->effect_expr()->GetForwardIterator() ;
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

  if(!(*existed_)[from->id()]) {
    (*existed_)[from->id()] = true;
    RenderControlFlow(from_name,from);
  }

  if(!(*existed_)[to->id()]) {
    (*existed_)[to->id()] = true;
    RenderControlFlow(to_name,to);
  }

  Indent(1) << from_name << " -> " << to_name << "[color=black style=bold]\n";
}

void DotGraphVisualizer::RenderExpr( const std::string& name , Expr* node ) {
  if((*existed_)[node->id()])
    return;

  (*existed_)[node->id()] = true;

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
      {
        auto binary = node->AsBinary();
        auto lhs_name = GetNodeName(binary->lhs());
        auto rhs_name = GetNodeName(binary->rhs());
        RenderExpr(lhs_name,binary->lhs());
        RenderExpr(rhs_name,binary->rhs());
        Indent(1) << name << "[label=\"bin(" << binary->op_name() << ")\"]\n";
        Indent(1) << name << " -> " << lhs_name << '\n';
        Indent(1) << name << " -> " << rhs_name << '\n';

        RenderCheckpoint(name,binary->checkpoint());
      }
      break;
    case IRTYPE_UNARY:
      {
        auto unary = node->AsUnary();
        Indent(1) << name << "[label=una(" << unary->op_name() << ")\"]\n";

        RenderCheckpoint(name,unary->checkpoint());
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

        RenderCheckpoint(name,tern->checkpoint());
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
      {
        auto pget = node->AsPGet();
        auto obj_name = GetNodeName(pget->object());
        auto key_name = GetNodeName(pget->key());
        RenderExpr(obj_name,pget->object());
        RenderExpr(key_name,pget->key());
        Indent(1) << name << "[label=\"" << pget->type_name() << "\"]\n";
        Indent(1) << name << " -> " << obj_name << "[label=\"object\"]\n";
        Indent(1) << name << " -> " << key_name << "[label=\"key\"]\n";

        RenderCheckpoint(name,pget->checkpoint());
      }
      break;
    case IRTYPE_PSET:
      {
        auto pset = node->AsPSet();
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

        RenderCheckpoint(name,pset->checkpoint());
      }
      break;
    case IRTYPE_IGET:
      {
        auto iget = node->AsIGet();
        auto obj_name = GetNodeName(iget->object());
        auto idx_name = GetNodeName(iget->index());
        RenderExpr(obj_name,iget->object());
        RenderExpr(idx_name,iget->index());
        Indent(1) << name << "[label=\"" << iget->type_name() << "\"]\n";
        Indent(1) << name << " -> " << obj_name << "[label=\"object\"]\n";
        Indent(1) << name << " -> " << idx_name << "[label=\"index\"]\n";

        RenderCheckpoint(name,iget->checkpoint());
      }
      break;
    case IRTYPE_ISET:
      {
        auto iset = node->AsISet();
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

        RenderCheckpoint(name,iset->checkpoint());
      }
      break;
    case IRTYPE_GGET:
      {
        auto gget = node->AsGGet();
        auto key_name = GetNodeName(gget->key());
        RenderExpr(key_name,gget->key());
        Indent(1) << name << "[label=\"" << gget->type_name() << "\"]\n";
        Indent(1) << name << " -> " << key_name << "[label=\"key\"]\n";

        RenderCheckpoint(name,gget->checkpoint());
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

        RenderCheckpoint(name,gset->checkpoint());
      }
      break;
    case IRTYPE_ITR_NEW:
      {
        auto itr_new = node->AsItrNew();
        auto opr_name= GetNodeName(itr_new->operand());
        RenderExpr(opr_name,itr_new->operand());
        Indent(1) << name << "[label=\"" << itr_new->type_name() << "\"]\n";
        Indent(1) << name << " -> " << opr_name << '\n';

        RenderCheckpoint(name,itr_new->checkpoint());
      }
      break;
    case IRTYPE_ITR_NEXT:
      {
        auto itr_next = node->AsItrNext();
        auto opr_name = GetNodeName(itr_next->operand());
        RenderExpr(opr_name,itr_next->operand());
        Indent(1) << name << "[label=\"" << itr_next->type_name() << "\"]\n";
        Indent(1) << name << " -> " << opr_name << '\n';

        RenderCheckpoint(name,itr_next->checkpoint());
      }
      break;
    case IRTYPE_ITR_TEST:
      {
        auto itr_test = node->AsItrTest();
        auto opr_name = GetNodeName(itr_test->operand());
        RenderExpr(opr_name,itr_test->operand());
        Indent(1) << name << "[label=\"" << itr_test->type_name() << "\"]\n";
        Indent(1) << name << " -> " << opr_name << '\n';

        RenderCheckpoint(name,itr_test->checkpoint());
      }
      break;
    case IRTYPE_ITR_DEREF:
      {
        auto itr_deref = node->AsItrDeref();
        auto itr_name  = GetNodeName(itr_deref->operand());
        RenderExpr(itr_name,itr_deref->operand());
        Indent(1) << name << "[label=\"" << itr_deref->type_name() << "\"]\n";
        Indent(1) << name << " -> " << itr_name << '\n';

        RenderCheckpoint(name,itr_deref->checkpoint());
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

        auto bounded_region = phi->region();
        auto bounded_region_name = GetNodeName(bounded_region);

        Indent(1) << bounded_region_name << " -> " << name << "[color=gray style=bold]\n";
      }
      break;

    case IRTYPE_PROJECTION:
      Indent(1) << name << "[label=\"projection(" << node->AsProjection()->index()
                                                  <<")]\n";
      break;
    case IRTYPE_INIT_CLS:
      {
        auto icls = node->AsInitCls();
        auto key  = icls->key();
        auto key_name = GetNodeName(key);
        RenderExpr(key_name,key);
        Indent(1) << name << " -> " << key_name << "[label=\"init_cls\"]\n";
      }
      break;
    case IRTYPE_OSR_LOAD:
      {
        auto osr_load = node->AsOSRLoad();
        Indent(1) << name << "[label=\"osr_load(" << osr_load->index() << ")\"]\n";
      }
      break;
    default:
      lava_die();
  }
}

} // namespace

std::string Graph::PrintToDotFormat( const Graph& graph , const Graph::DotFormatOption& opt ) {
  return DotGraphVisualizer().Visualize(graph,opt);
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

bool ControlFlowDFSIterator::Move() {
  while(!stack_.empty()) {
recursion:
    ControlFlow* top = stack_.Top()->AsControlFlow();

    // iterate through all its predecessor / backward-edge
    for( std::size_t i = 0 ; i < top->backward_edge()->size() ; ++i ) {
      // check all its predecessor to see whether there're some not visited
      // and then do it recursively
      ControlFlow* pre = top->backward_edge()->Index(i);

      if(stack_.Push(pre)) goto recursion;
    }

    // when we reach here it means we scan through all its predecessor nodes and
    // don't see any one not visited , or maybe this node is a singleton/leaf.
    next_ = top;
    stack_.Pop();
    return true;
  }

  next_ = NULL;
  return false;
}

bool ControlFlowBFSIterator::Move() {
  if(!stack_.empty()) {
    ControlFlow* top = stack_.Top()->AsControlFlow();
    stack_.Pop(); // pop the top element

    for( auto itr = top->backward_edge()->GetBackwardIterator() ;
              itr.HasNext() ; itr.Move() ) {
      ControlFlow* pre = itr.value();
      stack_.Push(pre);
    }

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

} // namespace hir
} // namespace cbase
} // namespace lavascript
