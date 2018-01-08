#include "dot-graph-visualizer.h"
#include "src/util.h"

#include <sstream>

namespace lavascript {
namespace cbase {
namespace ir {

std::string DotGraphVisualizer::Visualize( const Graph& graph ) {
  // 1. prepare all the status variables
  std::stringstream output;
  DynamicBitSet bitset(graph.MaxID());

  graph_   = &graph;
  output_  = &output;
  existed_ = &bitset;

  // 2. edge iterator
  output << "digraph IR {\n";
  for( GraphEdgeIterator itr(graph) ; itr.HasNext() ; itr.Move() ) {
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

void DotGraphVisualizer::RenderControlFlow( const std::string& region_name ,
                                            ControlFlow* region ) {
  Indent(1) << region_name << "[shape=box style=bold color=red label="
                           << "\""
                           << region->type_name()
                           << "\""
                           << "]\n";

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
    case IRTYPE_INT32:
      Indent(1) << name << "[label=\"i32(" << node->AsInt32()->value() << ")\"]\n";
      break;
    case IRTYPE_INT64:
      Indent(1) << name << "[label=\"i64(" << node->AsInt64()->value() << ")\"]\n";
      break;
    case IRTYPE_FLOAT64:
      Indent(1) << name << "[label=\"float(" << node->AsFloat64()->value() << ")\"]\n";
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
        auto list = node->AsIRList();
        for( std::size_t i = 0 ; i < list->array().size() ; ++i ) {
          auto element = list->array().Index(i);
          auto element_name = GetNodeName(element);
          RenderExpr(element_name,element);
          Indent(1) << name << " -> " << element_name << "[label=\"" << i << "\"]\n";
        }
      }
      break;
    case IRTYPE_OBJECT:
      {
        auto obj = node->AsIRObject();
        for( std::size_t i = 0 ; i < obj->array().size() ; ++i ) {
          auto element = obj->array().Index(i);
          auto key_name= GetNodeName(element.key);
          auto val_name= GetNodeName(element.val);
          RenderExpr(key_name,element.key);
          RenderExpr(val_name,element.val);
          Indent(1) << name << " -> " << key_name << "[label=\"key_" << i << "\"]\n";
          Indent(1) << name << " -> " << val_name << "[label=\"val_" << i << "\"]\n";
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
      }
      break;
    case IRTYPE_UNARY:
      {
        auto unary = node->AsUnary();
        Indent(1) << name << "[label=una(" << unary->op_name() << ")\"]\n";
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
    case IRTYPE_UGET:
      Indent(1) << name << "[label=\"index(" << node->AsUGet()->index() << ")\"]\n";
      break;
    case IRTYPE_USET:
      Indent(1) << name << "[label=\"index(" << node->AsUSet()->index() << ")\"]\n";
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
        Indent(1) << name << "[label=\"PHI\" color=blue style=bold]\n";
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

} // namespace ir
} // namespace cbase
} // namespace lavascript
