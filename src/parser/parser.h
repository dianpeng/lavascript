#ifndef PARSER_PARSER_H_
#define PARSER_PARSER_H_
#include <string>

#include "lexer.h"
#include "ast/ast.h"
#include "ast/ast-factory.h"

namespace lavascript {
namespace zone { class Zone; }
namespace parser {

class LocVarContextAdder;

/* A simple recursive descent parser , nothing special since the grammar of
 * lavascript is really simple and nothing speical **/
class Parser {
 public:
  Parser( const char* source , ::lavascript::zone::Zone* zone , std::string* error ) :
   lexer_(zone,source),
   zone_(zone),
   error_(error),
   nested_loop_(0),
   lctx_(NULL),
   ast_factory_(zone)
  { lexer_.Next(); /* initialize the lexer */ }

  /* Parse the source code into AST */
  ast::Root* Parse();

 private:
  /** Expression */
  ast::Node* ParseAtomic();
  ast::Node* ParsePrefix( ast::Node* );
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
  ast::For* ParseStepFor( size_t , ast::Var* );
  ast::ForEach* ParseForEach( size_t , ast::Variable* );
  ast::Break* ParseBreak();
  ast::Continue* ParseContinue();
  ast::Return* ParseReturn();
  ast::Node* ParseStatement();

  /** Chunk and Statement **/
  ast::Chunk* ParseSingleStatementOrChunk();
  ast::Chunk* ParseChunk();
  std::size_t AddChunkStmt( ast::Node* , ::lavascript::zone::Vector<ast::Variable*>* );

  /** Function definition */
  ast::Function* ParseFunction();
  ast::Function* ParseAnonymousFunction();
  ::lavascript::zone::Vector<ast::Variable*>* ParseFunctionPrototype();
  bool CheckArgumentExisted( const ::lavascript::zone::Vector<ast::Variable*>& ,
                             const ::lavascript::zone::String& ) const;
  // helper function for mutating current loc var context object
  void AddLocVarContextVar ( ast::Variable* );
  void AddLocVarContextIter( std::size_t cnt );

 private:
  void Error(const char* , ...);
  void ErrorAt( size_t start , size_t end , const char* , ... );
  void ErrorAtV( size_t start , size_t end , const char* , va_list );

 private:
  Lexer lexer_;
  ::lavascript::zone::Zone* zone_;
  std::string* error_;                  // Error buffer if we failed

  /** Tracking status for certain lexical scope */
  int nested_loop_;                     // Nested loop number

  /** Tracking current LocVarContext object */
  ast::LocVarContext* lctx_;

  ast::AstFactory ast_factory_;         // AST nodes factory for creating different AST nodes

  friend class LocVarContextAdder;      // RAII to change LocVarContext during parsing
};


} // namespace parser
} // namespace lavascript

#endif // PARSER_PARSER_H_
