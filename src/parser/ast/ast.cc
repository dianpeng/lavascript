#include "ast.h"
#include <src/parser/lexer.h>

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
      case Literal::kLitString:  output_ << '"' <<
                                            Lexer::EscapeStringLiteral(*node.str_value)
                                         << '"'; break;
      default: lava_die(); break;
    }
  }

  void Visit( const Variable& node ) {
    output_ << node.name->data();
  }

  void Visit( const Prefix& node ) {
    VisitNode(*node.var);
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
            output_ << " )";
          }
          break;
        default: lava_die(); break;
      }
    }
  }

  void Visit( const Binary& node ) {
    output_ << "( binary " << node.op.token_name() << ' ';
    VisitNode(*node.lhs);
    output_ << ' ';
    VisitNode(*node.rhs);
    output_ << " )";
  }

  void Visit( const Unary& node ) {
    output_ << "( unary " << node.op.token_name() << ' ';
    VisitNode(*node.opr);
    output_ << " )";
  }

  void Visit( const Ternary& node ) {
    output_ << "( ternary ";
    VisitNode(*node._1st);
    output_ << ' ';
    VisitNode(*node._2nd);
    output_ << ' ';
    VisitNode(*node._3rd);
    output_ << " )";
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
    if(node.has_initialization()) VisitNode( *node.expr );
    output_ << " )\n";
  }

  void Visit( const Assign& node ) {
    Indent() << "( assign ";
    switch( node.lhs_type() ) {
      case Assign::LHS_VAR: Visit( *node.lhs_var ); break;
      case Assign::LHS_PREFIX: Visit( *node.lhs_pref ); break;
      default: lava_die(); break;
    }
    output_ << ' ';
    VisitNode(*node.rhs);
    output_ << ")\n";
  }

  void Visit( const Call& node ) {
    Indent() << '('; Visit(*node.call); output_ << " )\n";
  }

  void Visit( const If& node ) {
    Indent() << "( if \n";
    ++indent_;
    for( size_t i = 0 ; i < node.br_list->size() ; ++i ) {
      Indent() << "( branch ";
      const If::Branch& br = node.br_list->Index(i);
      if(br.cond) VisitNode(*br.cond);
      output_ << '\n';

      ++indent_;
      Visit(*br.body);
      --indent_;

      Indent() << ")\n";
    }
    --indent_;
    Indent() << ")\n";
  }

  void Visit( const For& node ) {
    Indent() << "( for\n";
    ++indent_;

    if(node.has_1st()) {
      Indent()<< "( _1st\n";

      ++indent_;
      VisitNode(*node._1st);
      --indent_;

      Indent()<< ")\n";
    }

    if(node.has_2nd()) {
      Indent()<< "( _2nd ";
      VisitNode(*node._2nd);
      output_ << ")\n";
    }

    if(node.has_3rd()) {
      Indent()<< "( _3rd ";
      VisitNode(*node._3rd);
      output_ << ")\n";
    }

    Visit(*node.body);

    --indent_;
    Indent() << ")\n";
  }

  void Visit( const ForEach& node ) {
    Indent() << "( foreach\n";
    ++indent_;

    VisitNode(*node.var);
    Indent()<< " in ";

    VisitNode(*node.iter);
    output_ << '\n';

    Visit(*node.body);

    --indent_;
    Indent() << " )\n";
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
    output_ << " )\n";
  }

  void Visit( const Chunk& node ) {
    Indent() << "(\n";
    ++indent_;
    for( size_t i = 0 ; i < node.body->size() ; ++i ) {
      VisitNode( *node.body->Index(i) );
    }
    --indent_;
    Indent() << ")\n";
  }

  void Visit( const Function& node ) {
    if(!node.name) {
      output_ << '\n';
      ++indent_;
    }

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
    output_ << " )\n";

    indent_++;
    Visit( *node.body );
    indent_--;

    Indent() << ")\n";
    if(!node.name) {
      --indent_;
      Indent(); // Resume the indention
    }
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
