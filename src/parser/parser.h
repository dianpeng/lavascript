#ifndef PARSER_PARSER_H_
#define PARSER_PARSER_H_
#include <string>
#include "lexer.h"

namespace lavascript {
namespace parser {

namespace zone { class Zone; }
namespace ast  { struct Chunk; struct Root }

/* A simple recursive descent parser , nothing special since the grammar of
 * lavascript is really simple and nothing speical **/
class Parser {
 public:
  Parser( const char* source , zone::Zone* zone , std::string* error ) :
   lexer_(source),
   zone_(zone),
   error_(error),
   current_chunk_(NULL),
   nested_loop_(0)
  {}

  /* Parse the source code into AST */
  ast::Root* Parse();

 private:
  /** Expression */
  ast::Node* ParseAtomic();
  ast::Node* ParsePrefix();
  ast::Node* ParseUnary ();
  ast::Node* ParsePrimary( int );
  ast::Node* ParseBinary();
  ast::Node* ParseTernary();
  ast::Node* ParseExpression();
  ast::Node* ParseFuncCall();
  ast::List* ParseList();
  ast::Object* ParseObject();

  /** Statement */

 private:
  Lexer lexer_;
  zone::Zone* zone_;
  std::string* error_;                  // Error buffer if we failed

  /** Tracking status for certain lexical scope */
  ast::Chunk* current_chunk_;           // Current Chunk pointer. Any parsing will always happened
                                        // to belong to a certain chunk
  int nested_loop_;                     // Nested loop number
};


} // namespace parser
} // namespace lavascript

#endif // PARSER_PARSER_H_
