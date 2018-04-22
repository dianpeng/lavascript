#include "graph-printer.h"
#include "hir.h"

#include <sstream>

namespace lavascript {
namespace cbase      {
namespace hir        {
namespace            {

class DotPrinter {
 public:
  DotPrinter(): graph_(NULL), existed_(), output_() , opt_() {}
  // Visiualize the graph into DOT representation and return the string
  std::string Visualize( const Graph& , const GraphPrinter::Option& opt );
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
  GraphPrinter::Option opt_;
};

std::string DotPrinter::Visualize( const Graph& graph , const GraphPrinter::Option& opt ) {
  // 1. prepare all the status variables

  graph_   = &graph;
  opt_     = opt;
  existed_.resize(graph.MaxID());

  // 2. edge iterator
  output_ << "digraph IR {\n";
  lava_foreach( auto edge , ControlFlowEdgeIterator(graph) ) {
    RenderEdge(edge.from,edge.to);
  }
  output_ << "}\n";

  return output_.str();
}

std::stringstream& DotPrinter::Indent( int level ) {
  const char* kIndent = "  ";
  for( ; level > 0 ; --level )
    output_ << kIndent;
  return output_;
}

std::string DotPrinter::GetNodeName( Node* node ) {
  return Format("%s_%d",node->type_name(),node->id());
}

void DotPrinter::RenderCheckpoint ( const std::string& cp_name ,
                                            Checkpoint* checkpoint ) {
  if(!checkpoint || !opt_.checkpoint ) return;

  Indent(1) << cp_name << "[shape=diamond style=bold color=pink label=\"" << cp_name <<"\"]\n";

  lava_foreach( auto n , checkpoint->operand_list()->GetForwardIterator() ) {
    lava_debug(NORMAL,lava_verify(n->IsStackSlot()););
    {
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
    }
  }
}

void DotPrinter::RenderControlFlow( const std::string& region_name ,
                                            ControlFlow* region ) {
  Indent(1) << region_name << "[shape=box style=bold color=red label="
                           << "\""
                           << region->type_name()
                           << "\"]\n";

  // for all the operand of each control flow node
  lava_foreach( auto node , region->operand_list()->GetForwardIterator() ) {
    auto name = GetNodeName(node);
    RenderExpr(name,node);
    Indent(1) << region_name << " -> " << name << "[color=blue style=dashed]\n";
  }

  // for all the statment's bounded inside of this control flow node
  lava_foreach( auto expr , region->statement_list()->GetForwardIterator() ) {
    auto name = GetNodeName(expr);
    RenderExpr(name,expr);
    Indent(1) << region_name << " -> " << name << "[color=purple style=dashed label=stmt]\n";
  }
}

void DotPrinter::RenderEdge( ControlFlow* from , ControlFlow* to ) {
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

void DotPrinter::RenderExpr( const std::string& name , Expr* node ) {
  if(existed_[node->id()]) return;
  existed_[node->id()] = true;
  if(node->HasSideEffect()) Indent(1) << name << "[style=bold color=red]\n";

  switch(node->type()) {
    case HIR_FLOAT64:
      Indent(1) << name << "[label=\"f64(" << node->AsFloat64()->value() << ")\"]\n";
      break;
    case HIR_LONG_STRING:
      Indent(1) << name << "[label=\"str(" << node->AsLString()->value()->data() << ")\"]\n";
      break;
    case HIR_SMALL_STRING:
      Indent(1) << name << "[label=\"sso(" << node->AsSString()->value()->data() << ")\"]\n";
      break;
    case HIR_BOOLEAN:
      Indent(1) << name << "[label=\"bool(" << (node->AsBoolean()->value() ? "true" : "false" )
                                            << ")\"]\n";
      break;
    case HIR_NIL:
      Indent(1) << name << "[label=\"nil\"]\n";
      break;
    case HIR_LIST:
      {
        Indent(1) << name << "[label=\"list\"]\n";
        auto list = node->AsIRList();
        std::size_t i = 0 ;
        lava_foreach( auto element , list->operand_list()->GetForwardIterator() ) {
          auto element_name = GetNodeName(element);
          RenderExpr(element_name,element);
          Indent(1) << name << " -> " << element_name << "[label=\"" << i << "\"]\n";
          ++i;
        }
      }
      break;
    case HIR_OBJECT_KV:
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
    case HIR_OBJECT:
      {
        Indent(1) << name << "[label=\"object\"]\n";
        auto obj = node->AsIRObject();
        std::size_t i = 0;
        lava_foreach( auto kv , obj->operand_list()->GetForwardIterator() ) {
          auto kv_name = GetNodeName(kv);
          RenderExpr(kv_name,kv);
          Indent(1) << name << " -> " << kv_name << "[label=\"" << i << "\"]\n";
          ++i;
        }
      }
      break;
    case HIR_LOAD_CLS:
      {
        auto obj = node->AsLoadCls();
        Indent(1) << name << "[label=\"ref(" << obj->ref() << ")\"]\n";
      }
      break;
    case HIR_ARG:
      {
        auto arg = node->AsArg();
        Indent(1) << name << "[label=\"index(" << arg->index() << ")\"]\n";
      }
      break;
    case HIR_BINARY:
    case HIR_FLOAT64_BITWISE:
    case HIR_FLOAT64_ARITHMETIC:
    case HIR_FLOAT64_COMPARE:
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
    case HIR_UNARY:
      {
        auto unary = static_cast<Unary*>(node);
        Indent(1) << name << "[label=\""
                          << unary->type_name()
                          << '('
                          << unary->op_name()
                          << ")\"]\n";

      }
      break;
    case HIR_TERNARY:
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
    case HIR_UGET:
      Indent(1) << name << "[label=\"uget(" << node->AsUGet()->index() << ")\"]\n";
      break;
    case HIR_USET:
      {
        auto uset = node->AsUSet();
        auto opr_name = GetNodeName(uset->value());
        RenderExpr(opr_name,uset->value());
        Indent(1) << name << "[label=\"uset\"]\n";
        Indent(1) << name << opr_name << '\n';
      }
      break;
    case HIR_PGET:
    case HIR_OBJECT_GET:
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
    case HIR_PSET:
    case HIR_OBJECT_SET:
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
    case HIR_IGET: case HIR_LIST_GET:
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
    case HIR_ISET: case HIR_LIST_SET:
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
    case HIR_GGET:
      {
        auto gget = node->AsGGet();
        auto key_name = GetNodeName(gget->key());
        RenderExpr(key_name,gget->key());
        Indent(1) << name << "[label=\"" << gget->type_name() << "\"]\n";
        Indent(1) << name << " -> " << key_name << "[label=\"key\"]\n";

      }
      break;
    case HIR_GSET:
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
    case HIR_ITR_NEW:
      {
        auto itr_new = node->AsItrNew();
        auto opr_name= GetNodeName(itr_new->operand());
        RenderExpr(opr_name,itr_new->operand());
        Indent(1) << name << "[label=\"" << itr_new->type_name() << "\"]\n";
        Indent(1) << name << " -> " << opr_name << '\n';

      }
      break;
    case HIR_ITR_NEXT:
      {
        auto itr_next = node->AsItrNext();
        auto opr_name = GetNodeName(itr_next->operand());
        RenderExpr(opr_name,itr_next->operand());
        Indent(1) << name << "[label=\"" << itr_next->type_name() << "\"]\n";
        Indent(1) << name << " -> " << opr_name << '\n';

      }
      break;
    case HIR_ITR_TEST:
      {
        auto itr_test = node->AsItrTest();
        auto opr_name = GetNodeName(itr_test->operand());
        RenderExpr(opr_name,itr_test->operand());
        Indent(1) << name << "[label=\"" << itr_test->type_name() << "\"]\n";
        Indent(1) << name << " -> " << opr_name << '\n';

      }
      break;
    case HIR_ITR_DEREF:
      {
        auto itr_deref = node->AsItrDeref();
        auto itr_name  = GetNodeName(itr_deref->operand());
        RenderExpr(itr_name,itr_deref->operand());
        Indent(1) << name << "[label=\"" << itr_deref->type_name() << "\"]\n";
        Indent(1) << name << " -> " << itr_name << '\n';
      }
      break;
    case HIR_PROJECTION:
      Indent(1) << name << "[label=\"projection(" << node->AsProjection()->index() <<")]\n";
      break;
    case HIR_INIT_CLS:
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
    case HIR_TEST_LISTOOB:
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

    case HIR_TEST_TYPE:
      {
        auto tt = node->AsTestType();
        Indent(1) << name << "[label=\"test-type(" << tt->type_kind_name() << ")\"]\n";
        auto obj = tt->object();
        auto obj_name = GetNodeName(obj);
        RenderExpr(obj_name,obj);

        Indent(1) << name << " -> " << obj_name << '\n';
      }
      break;

    case HIR_BOX:
      {
        auto box = node->AsBox();
        Indent(1) << name << "[label=\"box(" << GetTypeKindName(box->type_kind()) << ")\"]\n";
        auto obj = box->value();
        auto obj_name = GetNodeName(obj);
        RenderExpr(obj_name,obj);

        Indent(1) << name << " -> " << obj_name << '\n';
      }
      break;
    case HIR_UNBOX:
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
    case HIR_ICALL:
      {
        auto ic = node->AsICall();
        Indent(1) << name << "[label=\"icall(" << (ic->tail_call() ? "tail" : "normal")
                                               <<','
                                               << interpreter::GetIntrinsicCallName(ic->ic())
                                               <<")\"]\n";
        int count = 0;
        lava_foreach( auto arg , ic->operand_list()->GetForwardIterator() ) {
          auto arg_name = GetNodeName(arg);
          RenderExpr(arg_name,arg);
          Indent(1) << name << " -> " << arg_name << "[label=" << count << "]\n";
          ++count;
        }
      }
      break;
    case HIR_OSR_LOAD:
      {
        auto osr_load = node->AsOSRLoad();
        Indent(1) << name << "[label=\"osr_load(" << osr_load->index() << ")\"]\n";
      }
      break;
    case HIR_CHECKPOINT:
      return RenderCheckpoint(name,node->AsCheckpoint());
    default:
      {
        Indent(1) << name << "[label=\"" << node->type_name() << "\"]\n";
        lava_foreach( auto opr , node->operand_list()->GetForwardIterator() ) {
          auto opr_name = GetNodeName(opr);
          RenderExpr(opr_name,opr);
          Indent(1) << name << " -> " << opr_name << '\n';
        }
      }
      break;
  }

  // effect list node
  lava_foreach( auto n , node->effect_list()->GetForwardIterator() ) {
    Indent(1) << name << " -> " << GetNodeName(n) << "[label=\"dep\" style=filled color=blue ]\n";
  }
}
} // namespace

std::string GraphPrinter::Print( const Graph& g , const Option& opt ) {
  return DotPrinter().Visualize(g,opt);
}

} // namespace hir
} // namespace cbase
} // namespave lavascript
