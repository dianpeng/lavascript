#include "ast.h"

namespace lavascript {
namespace parser {
namespace ast {
namespace {

class PrinterVisitor : public AstVisitor<PrinterVisitor> {
 private:
  void Visit( const Literal& node ) {
    switch(node.literal_type) {
      case Literal::kLitInteger: output_ << node.int_value; break;
      case Literal::kLitReal:    output_ << node.real_value;break;
      case Literal::kLitNull:    output_ << "null"; break;
      case Literal::kLitBoolean: output_ << (node.bool_value ? "true" : "false"); break;
      case Literal::kLitString:  output_ << node.str_value->data(); break;
      default: lava_die(); break;
    }
  }

  void Visit( const Variable& node ) {
    output_ << node.name->data();
  }

  void Visit( const Prefix& node ) {
    Visit(*node.var);
    for( size_t i = 0 ; i < node.list->size() ; ++i ) {
      const Prefix::Component& comp = node.list->Index(i);
      switch(comp.t) {
        case Prefix::Component::DOT:
          output_ << '.' ; Visit( *comp.var ); break;
        case Prefix::Component::INDEX:
          output_ << '[' ; VisitNode( *comp.expr ); output_ << ']'; break;
        case Prefix::Component::CALL:
          {
            size_t len = comp.fc->args->size();
            output_ << '(';
            for( size_t i = 0 ; i < len ; ++i ) {
              VisitNode( *comp.fc->args->Index(i) );
              if( i < len - 1 ) output_ << ',';
            }
            output_ << ')';
          }
        default: lava_die(); break;
      }
    }
  }

  void Visit( const Binary& node ) {
    output_ << "( binary " << node.op.token_name() << ' ';
    VisitNode(*node.lhs);
    output_ << ' ';
    VisitNode(*node.rhs);
    output_ << ')';
  }

  void Visit( const Unary& node ) {
    output_ << "( unary " << node.op.token_name() << ' ';
    VisitNode(*node.opr);
    output_ << ')';
  }

  void Visit( const Ternary& node ) {
    output_ << "( ternary ";
    VisitNode(*node._1st);
    output_ << ' ';
    VisitNode(*node._2nd);
    output_ << ' ';
    VisitNode(*node._3rd);
    output_ << ')';
  }

  void Visit( const List& node ) {
    output_ << '[';
    for( size_t i = 0 ; i < node.entry->size() ; ++i ) {
      VisitNode( *node.entry->Index(i) );
      if( i < node.entry->size() - 1 ) output_ << ',';
    }
    output_ << ']';
  }

  void Visit( const Object& node ) {
    output_ << '{';
    for( size_t i = 0 ; i < node.entry->size() ; ++i ) {
      const Object::Entry& e = node.entry->Index(i);
      VisitNode( *e.key ); output_ << ':'; VisitNode(*e.val);
      if( i < node.entry->size() - 1 ) output_ << ',';
    }
    output_ << '}';
  }

  /** Statement **/
  void Visit( const Var& node ) {
    Indent() << "( var ";
    Visit( *node.var );
    output_ << ' ';
    VisitNode( *node.expr );
    output_ << ")\n";
  }

  void Visit( const Assign& node ) {
    Indent() << "( assign ";
    switch( node.lhs_t ) {
      case Assign::LHS_VAR: Visit( *node.lhs_var ); break;
      case Assign::LHS_PREFIX: Visit( *node.lhs_pref ); break;
      default: lava_die(); break;
    }
    output_ << ' ';
    VisitNode(*node.rhs);
    output_ << ")\n";
  }

  void Visit( const Call& node ) {
    Indent() << '('; Visit(*node.call); output_ << ")\n";
  }

  void Visit( const If& node ) {
    Indent() << "( if \n";
    for( size_t i = 0 ; i < node.br_list->size() ; ++i ) {
      Indent(1) << "( branch ";
      const If::Branch& br = node.br_list->Index(i);
      if(br.cond) VisitNode(*br.cond);
      output_ << ' ';

      ++indent_;
      Visit(*br.body);
      --indent_;

      Indent(1) << ")\n";
    }
    Indent() << ")\n";
  }

  void Visit( const For& node ) {
    Indent() << "( for ";
    if(node.has_1st()) {
      output_ << "( _1st ";
      Visit(*node.var);
      output_ << " = ";
      VisitNode(*node._1st);
      output_ << ") ";
    }

    if(node.has_2nd()) {
      output_ << "( _2nd ";
      VisitNode(*node._2nd);
      output_ << ") ";
    }

    if(node.has_3rd()) {
      output_ << "( _3rd ";
      VisitNode(*node._3rd);
      output_ << ") ";
    }
    output_ << ' ';

    ++indent_;
    Visit(*node.body);
    --indent_;

    Indent() << ")\n";
  }

  void Visit( const ForEach& node ) {
    Indent() << "( foreach ";
    Visit(*node.var);
    output_ << ' ';
    VisitNode(*node.iter);
    output_ << ' ';

    ++indent_;
    Visit(*node.body);
    --indent_;

    Indent() << ")\n";
  }

  void Visit( const Break& node ) {
    (void)node;
    Indent() << "( break )\n";
  }

  void Visit( const Continue& node ) {
    (void)node;
    Indent() << "( continue )\n";
  }

  void Visit( const Return& node ) {
    Indent() << "( return ";
    if(node.expr) VisitNode(*node.expr);
    else output_ << " void";
    output_ << ")\n";
  }

  void Visit( const Require& node ) {
    Indent() << "( require ";
    VisitNode(*node.req_expr);
    if(node.has_as()) {
      output_ << " as ";
      Visit(*node.as_var);
    }
    output_ << " )\n";
  }

  void Visit( const Chunk& node ) {
    Indent() << "(\n";
    for( size_t i = 0 ; i < node.body->size() ; ++i ) {
      VisitNode( *node.body->Index(i) );
    }
    Indent() << ")\n";
  }

  void Visit( const Function& node ) {
    Indent() << "( function ";
    if( node.name ) {
      Visit(*node.name);
    } else {
      output_ << "__";
    }
    output_ << ' ';

    output_ << '(';
    for( size_t i = 0 ; i < node.proto->size() ; ++i ) {
      Variable* v = node.proto->Index(i);
      Visit(*v);
      if( i < node.proto->size() - 1 ) output_ << ',';
    }
    output_ << ')';

    ++indent_;
    Visit( *node.body );
    --indent_;

    Indent() << ")\n";
  }

  void Visit( const Root& node ) {
    Visit(*node.body);
  }

  std::ostream& Indent( int off = 0 ) {
    static const char* kIndent = "  ";
    for( int i = 0 ; i < (indent_ + off); ++i ) {
      output_ << kIndent;
    }
    return output_;
  }

 public:
  PrinterVisitor( std::ostream& output ):
    indent_(0),
    output_(output)
  {}

  void Start( const Node& n ) { VisitNode(n); }
 private:
  int indent_;
  std::ostream& output_;

  friend class AstVisitor<PrinterVisitor>;
};

} // namespace

void DumpAst( const Node& n , std::ostream& output ) {
  PrinterVisitor visitor(output);
  visitor.Start(n);
}

} // namespace ast
} // namespace parser
} // namespace lavascript
