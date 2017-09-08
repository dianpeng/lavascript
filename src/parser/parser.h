#ifndef PARSER_PARSER_H_
#define PARSER_PARSER_H_
#include <string>
#include "lexer.h"
#include "ast/ast.h"
#include "ast/ast-factory.h"

namespace lavascript {
namespace zone { class Zone; }
namespace parser {

/* A simple recursive descent parser , nothing special since the grammar of
 * lavascript is really simple and nothing speical **/
class Parser {
 public:
  Parser( const char* source , ::lavascript::zone::Zone* zone , std::string* error ) :
   lexer_(zone,source),
   zone_(zone),
   error_(error),
   current_chunk_(NULL),
   nested_loop_(0),
   ast_factory_(zone)
  {}

  /* Parse the source code into AST */
  ast::Root* Parse();

 private:
  /** Expression */
  ast::Node* ParseAtomic();
  ast::Node* ParsePrefix( ast::Variable* );
  ast::Node* ParseUnary ();
  ast::Node* ParsePrimary( int );
  ast::Node* ParseBinary();
  ast::Node* ParseTernary( ast::Node* );
  ast::Node* ParseExpression();
  ast::FuncCall* ParseFuncCall();
  ast::List* ParseList();
  ast::Object* ParseObject();

  /** Statement */
  ast::Var* ParseVar();
  ast::Node* ParsePrefixStatement();
  ast::Assign* ParseAssign( ast::Node* );
  ast::If* ParseIf();
  bool ParseCondBranch( ast::If::Branch* );
  ast::Node* ParseFor();
  ast::For* ParseStepFor( size_t , ast::Node* );
  ast::ForEach* ParseForEach( size_t , ast::Node* );
  ast::Break* ParseBreak();
  ast::Continue* ParseContinue();
  ast::Return* ParseReturn();
  ast::Node* ParseStatement();

  /** Chunk and Statement **/
  ast::Node* ParseSingleStatementOrChunk();
  ast::Node* ParseChunk();

  /** Function definition */
  ast::Function* ParseFunction();
  ast::Function* ParseAnonymousFunction();
  bool ParseFunctionPrototype( Function* );

 private:
  void Error(const char* , ...);
  void ErrorAt( size_t start , const char* , ... );

 private:
  Lexer lexer_;
  ::lavascript::zone::Zone* zone_;
  std::string* error_;                  // Error buffer if we failed

  /** Tracking status for certain lexical scope */
  ast::Chunk* current_chunk_;           // Current Chunk pointer. Any parsing will always happened
                                        // to belong to a certain chunk
  int nested_loop_;                     // Nested loop number

  ast::AstFactory ast_factory_;         // AST nodes factory for creating different AST nodes
};


} // namespace parser
} // namespace lavascript

#endif // PARSER_PARSER_H_
