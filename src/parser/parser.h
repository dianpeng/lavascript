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
class LexicalScopeAdder;

/* A simple recursive descent parser , nothing special since the grammar of
 * lavascript is really simple and nothing speical **/
class Parser {
 public:
  Parser( const char* source , ::lavascript::zone::Zone* zone , std::string* error ) :
   lexer_(zone,source),
   zone_(zone),
   error_(error),
   nested_loop_(0),
   function_scope_info_(NULL),
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
  ast::ForEach* ParseForEach( size_t , ast::Variable* ,
                                       ast::Variable* );
  ast::Break* ParseBreak();
  ast::Continue* ParseContinue();
  ast::Return* ParseReturn();
  ast::Node* ParseStatement();

  /** Chunk and Statement **/
  ast::Chunk* ParseSingleStatementOrChunk();
  ast::Chunk* ParseChunk();

  enum ChunkStmtAddResult {
    VARIABLE_EXISTED = -1 , // already have such variable in current scope
    VARIABLE_OKAY         , // variable has been added and its counter has been bumped
    ITERATOR_NEED1        , // need one iterator
    ITERATOR_NEED2        , // need two iterator
    ITERATOR_NEED3          // need three iterator
  };

  ChunkStmtAddResult AddChunkStmt( ast::Node* , ::lavascript::zone::Vector<ast::Variable*>* );

  /** Function definition */
  ast::Function* ParseFunction();
  ast::Function* ParseAnonymousFunction();
  ::lavascript::zone::Vector<ast::Variable*>* ParseFunctionPrototype();
  bool CheckArgumentExisted( const ::lavascript::zone::Vector<ast::Variable*>& ,
                             const ::lavascript::zone::String& ) const;
 private:
  void Error(const char* , ...);
  void ErrorAt( size_t start , size_t end , const char* , ... );
  void ErrorAtV( size_t start , size_t end , const char* , va_list );

 private:
  /** Tracking current lexical scope , mainly used for
   *  tracking how many variables are defined here */
  struct LexicalScopeInfo {
    std::size_t var_count;
    LexicalScopeInfo():var_count(0) {}
  };

  struct FunctionScopeInfo {
    ast::LocVarContext* var_context; // variable context for this function scope
    std::vector<LexicalScopeInfo> lexical_scope_info;
    int current_scope;

    FunctionScopeInfo( ast::LocVarContext* ctx ):
      var_context(ctx),
      lexical_scope_info(),
      current_scope(-1)
    {}

    // promote all local variable ahead of the function scope and
    // figure out the maximum alive variable/register needed
    void CalculateFunctionScopeInfo() {
      std::size_t total_count = 0;
      for( auto & e : lexical_scope_info ) {
        total_count += e.var_count;
      }
      var_context->var_count = total_count;
    }

    LexicalScopeInfo* top_scope() { return &(lexical_scope_info[0]); }
  };

  LexicalScopeInfo* lexical_scope_info() {
    return &(function_scope_info_->lexical_scope_info[
      function_scope_info_->current_scope]);
  }

  FunctionScopeInfo* function_scope_info() {
    return function_scope_info_;
  }

  ast::LocVarContext* local_variable_context() {
    return function_scope_info()->var_context;
  }

  void CalculateLexcialScopeInfo( std::size_t var_count  ,
                                  std::size_t iter_count ) {
    const std::size_t count_in_chunk = iter_count + var_count;
    if(count_in_chunk > lexical_scope_info()->var_count) {
      lexical_scope_info()->var_count = count_in_chunk;
    }
  }

 private:
  Lexer lexer_;
  ::lavascript::zone::Zone* zone_;
  std::string* error_;                  // Error buffer if we failed

  /** Tracking status for certain lexical scope */
  int nested_loop_;

  /** Tracking current LocVarContext object */
  FunctionScopeInfo* function_scope_info_;

  ast::AstFactory ast_factory_;         // AST nodes factory for creating different AST nodes

  friend class LocVarContextAdder;      // RAII to change LocVarContext during parsing
  friend class LexicalScopeAdder ;
};


} // namespace parser
} // namespace lavascript

#endif // PARSER_PARSER_H_
