#include "graph-printer.h"
#include "hir.h"
#include "src/zone/zone.h"

#include <sstream>

namespace lavascript {
namespace cbase      {
namespace hir        {
namespace            {

class DotPrinter {
 public:
  DotPrinter(): zone_ () , graph_(NULL), existed_(&zone_), output_() , opt_() {}
  // Visiualize the graph into DOT representation and return the string
  std::string Visualize ( const Graph& , const GraphPrinter::Option& opt );
 private:
  void RenderControlFlow( const std::string& , ControlFlow* );
  void RenderExpr       ( const std::string& , Expr* );
  void RenderExprOperand( const std::string& , Expr* );
  void RenderExprBrief  ( const std::string& , Expr* );
  void RenderExprEffect ( const std::string& , Expr* );
  void RenderEdge       ( ControlFlow* , ControlFlow*);
  void RenderCheckpoint ( const std::string& , Checkpoint* );

  std::stringstream& Indent( int level );
  std::string GetNodeName( Node* );

  zone::Zone* zone() { return &zone_; }
 private:
  zone::SmallZone      zone_;
  const Graph*         graph_;
  zone::stl::BitSet    existed_;
  std::stringstream    output_;
  GraphPrinter::Option opt_;
};

std::string DotPrinter::Visualize( const Graph& graph , const GraphPrinter::Option& opt ) {
  // 1. prepare all the status variables

  graph_   = &graph;
  opt_     = opt;
  existed_.resize(graph.MaxID());

  // 2. edge iterator
  output_ << "digraph IR {\n";
  lava_foreach( auto &k , ControlFlowEdgeIterator(zone(),graph) ) {
    RenderEdge(k.from,k.to);
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

void DotPrinter::RenderControlFlow( const std::string& region_name , ControlFlow* region ) {
  Indent(1) << region_name << "[shape=box style=bold color=red label="
                           << "\""
                           << region->type_name()
                           << "\"]\n";

  // for all the operand of each control flow node
  {
    auto count = 0;
    lava_foreach( auto node , region->operand_list()->GetForwardIterator() ) {
      auto name = GetNodeName(node);
      RenderExpr(name,node);
      Indent(1) << region_name << " -> " << name << "[color=black style=bold label=" << count << "]\n";
      ++count;
    }
  }

  // for all statement node
  {
    auto count = 0;
    lava_foreach( auto expr , region->stmt_list()->GetForwardIterator() ) {
      auto name = GetNodeName(expr);
      RenderExpr(name,expr);
      Indent(1) << region_name << " -> " << name << "[color=purple style=dashed label=" << count << "]\n";
      ++count;
    }
  }
}

void DotPrinter::RenderEdge( ControlFlow* from , ControlFlow* to ) {
  auto from_name = GetNodeName(from);
  auto to_name   = GetNodeName(to);
  if(!existed_[from->id()]) {
    existed_[from->id()] = true;
    RenderControlFlow(from_name,from);
  }
  if(!existed_[to->id()]) {
    existed_[to->id()] = true;
    RenderControlFlow(to_name  ,to  );
  }
  Indent(1) << from_name << " -> " << to_name << "[color=blue style=bold]\n";
}

void DotPrinter::RenderExprOperand( const std::string& name , Expr* node ) {
  switch(node->type()) {
    case HIR_INT32:
      Indent(1) << name << "[label=\"i32(" << node->As<Int32>  ()->value() << ")\"]\n";
      break;
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
    // node that implements BinaryNode interface
    case HIR_FLOAT64_BITWISE:
    case HIR_FLOAT64_ARITHMETIC:
    case HIR_FLOAT64_COMPARE:
    case HIR_BOOLEAN_LOGIC:
    case HIR_STRING_COMPARE:
    case HIR_SSTRING_EQ:
    case HIR_SSTRING_NE:
    case HIR_ARITHMETIC:
    case HIR_COMPARE:
    case HIR_LOGICAL:
      {
        auto binary   = dynamic_cast<BinaryNode*>(node);
        auto lhs_name = GetNodeName(binary->lhs());
        auto rhs_name = GetNodeName(binary->rhs());
        RenderExpr(lhs_name,binary->lhs());
        RenderExpr(rhs_name,binary->rhs());
        Indent(1) << name << "[label=\"" << node->type_name()
                          << '(' << binary->op_name() << ")\"]\n";
        Indent(1) << name << " -> " << lhs_name << "[label=L]\n";
        Indent(1) << name << " -> " << rhs_name << "[label=R]\n";
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
      Indent(1) << name << "[label=\"uget(" << (std::uint32_t)node->AsUGet()->index() << ")\"]\n";
      break;
    case HIR_USET:
      {
        auto uset = node->AsUSet();
        auto opr_name = GetNodeName(uset->value());
        RenderExpr(opr_name,uset->value());
        Indent(1) << name << "[label=\"uset(" << (std::uint32_t)uset->index() << ")\"]\n";
        Indent(1) << name << opr_name << '\n';
      }
      break;
    case HIR_PROJECTION:
      Indent(1) << name << "[label=\"projection(" << node->AsProjection()->index() <<")]\n";
      break;
    /** test **/
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
        auto count = 0;
        lava_foreach( auto opr , node->operand_list()->GetForwardIterator() ) {
          auto opr_name = GetNodeName(opr);
          RenderExpr(opr_name,opr);
          Indent(1) << name << " -> " << opr_name << "[label=" << count << "]\n";
          ++count;
        }
      }
      break;
  }
}

void DotPrinter::RenderExprEffect( const std::string& name , Expr* node ) {
  // effect list node
  lava_foreach( auto n , node->GetDependencyIterator() ) {
    Indent(1) << name << " -> " << GetNodeName(n) << "[ style=bold color=green ]\n";
  }
}

void DotPrinter::RenderExprBrief( const std::string& name , Expr* node ) {
  switch(node->type()) {
    case HIR_INT32:
      Indent(1) << name << "[label=\"i32(" << node->As<Int32>  ()->value() << ")\"]\n";
      break;
    case HIR_FLOAT64:
      Indent(1) << name << "[label=\"f64(" << node->As<Float64>()->value() << ")\"]\n";
      break;
    case HIR_LONG_STRING:
      Indent(1) << name << "[label=\"str(" << node->As<LString>()->value()->data() << ")\"]\n";
      break;
    case HIR_SMALL_STRING:
      Indent(1) << name << "[label=\"sso(" << node->As<SString>()->value()->data() << ")\"]\n";
      break;
    case HIR_BOOLEAN:
      Indent(1) << name << "[label=\"bool(" <<(node->As<Boolean>()->value() ? "true":"false") << ")\"]\n";
      break;
    case HIR_NIL:
      Indent(1) << name << "[label=\"nil\"]\n";
      break;
    default:
      Indent(1) << name << "[label=\"" << node->type_name() << "\"]\n";
      break;
  }
}

void DotPrinter::RenderExpr( const std::string& name , Expr* node ) {
  if(existed_[node->id()]) return;
  existed_[node->id()] = true;
  if(node->Is<EffectNode>()) {
    Indent(1) << name << "[style=bold color=purple]\n";
  }

  if(opt_.ShouldRenderOperand()) RenderExprOperand(name,node);
  else                           RenderExprBrief  (name,node);
  if(opt_.ShouldRenderEffect ()) RenderExprEffect (name,node);
}

} // namespace

std::string GraphPrinter::Print( const Graph& g , const Option& opt ) {
  return DotPrinter().Visualize(g,opt);
}

} // namespace hir
} // namespace cbase
} // namespave lavascript
