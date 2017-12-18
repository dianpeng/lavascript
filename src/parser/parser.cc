#include "parser.h"
#include "optimizer.h"
#include "ast/ast.h"

#include "src/zone/zone.h"
#include "src/error-report.h"
#include "src/util.h"

#include <vector>


namespace lavascript {
namespace parser {

using namespace lavascript::zone;

// Help to modify lctx_ field in Parser whennever a new function is setup
class LocVarContextAdder {
 public:
  LocVarContextAdder( Parser* p ):
    old_var_(p->function_scope_info_),
    temp_   (p->ast_factory_.NewLocVarContext()),
    p_      (p)
  {
    p->function_scope_info_ = &temp_;
  }

  ~LocVarContextAdder() {
    p_->function_scope_info_->CalculateFunctionScopeInfo();
    p_->function_scope_info_ = old_var_;
  }
 private:
  Parser::FunctionScopeInfo* old_var_;
  Parser::FunctionScopeInfo temp_;
  Parser* p_;

  LAVA_DISALLOW_COPY_AND_ASSIGN(LocVarContextAdder);
};

class LexicalScopeAdder {
 public:
  LexicalScopeAdder( Parser* p ):
    parser_(p)
  {
    int idx = ++(p->function_scope_info()->current_scope);
    if(static_cast<std::size_t>(idx) ==
        p->function_scope_info()->lexical_scope_info.size()) {
      p->function_scope_info()->lexical_scope_info.push_back(Parser::LexicalScopeInfo());
    }
  }

  ~LexicalScopeAdder() {
    --parser_->function_scope_info()->current_scope;
  }
 private:
  Parser* parser_;
};


void Parser::ErrorAtV( size_t start , size_t end , const char* format , va_list vl ) {
  if(lexer_.lexeme().token == Token::kError) {
    *error_ = lexer_.lexeme().error_description;
  } else {
    ReportError(error_,"parser",lexer_.source(),start,end,format,vl);
  }
}

void Parser::Error( const char* format , ... ) {
  va_list vl;
  va_start(vl,format);
  ErrorAtV(lexer_.lexeme().start,lexer_.lexeme().end,format,vl);
}

void Parser::ErrorAt( size_t start , size_t end , const char* format , ... ) {
  va_list vl;
  va_start(vl,format);
  ErrorAtV(start,end,format,vl);
}

ast::Node* Parser::ParseAtomic() {
  ast::Node* ret;
  switch(lexer_.lexeme().token) {
    case Token::TK_REAL:
      ret = ast_factory_.NewLiteral( lexer_ , lexer_.lexeme().real_value );
      lexer_.Next();
      return ret;
    case Token::TK_TRUE:
      ret = ast_factory_.NewLiteral( lexer_ , true );
      lexer_.Next();
      return ret;
    case Token::TK_FALSE:
      ret = ast_factory_.NewLiteral( lexer_ , false );
      lexer_.Next();
      return ret;
    case Token::TK_NULL:
      ret = ast_factory_.NewLiteral( lexer_ );
      lexer_.Next();
      return ret;
    case Token::TK_STRING:
      ret = ast_factory_.NewLiteral( lexer_ , lexer_.lexeme().str_value );
      lexer_.Next();
      return ret;
    case Token::TK_LPAR:
      lexer_.Next();
      if(!(ret = ParseExpression())) return NULL;
      if(lexer_.Expect(Token::kRPar))
        break;
      else {
        Error("Expect a \")\" to close sub expression");
        return NULL;
      }
    case Token::TK_LSQR:
      if(!(ret=ParseList())) return NULL;
      break;
    case Token::TK_LBRA:
      if(!(ret=ParseObject())) return NULL;
      break;
    case Token::TK_IDENTIFIER:
      ret = ast_factory_.NewVariable(lexer_.lexeme().start,
                                     lexer_.lexeme().end,
                                     lexer_.lexeme().str_value);
      lexer_.Next();
      break;
    case Token::TK_FUNCTION:
      if(!(ret=ParseAnonymousFunction()))
        return NULL;
      break;
    default:
      Error("Expect an primary expression here,but got token %s",
            lexer_.lexeme().token.token_name());
      return NULL;
  }

  if(lexer_.lexeme().token.IsPrefixOperator()) {
    return ParsePrefix(ret);
  }
  return ret;
}

ast::List* Parser::ParseList() {
  lava_verify( lexer_.lexeme().token == Token::kLSqr );
  size_t start = lexer_.lexeme().start;
  if( lexer_.Next().token == Token::kRSqr ) {
    size_t end = lexer_.lexeme().end + 1;
    lexer_.Next();
    return ast_factory_.NewList(start,end,Vector<ast::Node*>::New(zone_));
  } else {
    Vector<ast::Node*>* entry = Vector<ast::Node*>::New(zone_);
    do {
      ast::Node* e = ParseExpression();
      if(!e) return NULL;
      entry->Add(zone_,e);
      if(entry->size() > kMaxListEntryCount) {
        Error("Too many list literal's entry, at most %zu is allowed",
              kMaxListEntryCount);
        return NULL;
      }
      if( lexer_.lexeme().token == Token::kComma ) {
        lexer_.Next();
      } else if( lexer_.lexeme().token == Token::kRSqr ) {
        lexer_.Next();
        break;
      } else {
        Error("Expect a \",\" or \"]\" in a list literal");
        return NULL;
      }
    } while(true);
    return ast_factory_.NewList(start,lexer_.lexeme().start,entry);
  }
}

ast::Object* Parser::ParseObject() {
  lava_verify( lexer_.lexeme().token == Token::kLBra );
  size_t start = lexer_.lexeme().start;
  if( lexer_.Next().token == Token::kRBra ) {
    size_t end = lexer_.lexeme().end + 1;
    lexer_.Next();
    return ast_factory_.NewObject(start,end,Vector<ast::Object::Entry>::New(zone_));
  } else {
    Vector<ast::Object::Entry>* entry = Vector<ast::Object::Entry>::New(zone_);
    do {
      ast::Node* key;
      switch(lexer_.lexeme().token) {
        case Token::TK_LSQR:
          lexer_.Next();
          if(!(key = ParseExpression())) return NULL;
          if(!lexer_.Expect(Token::kRSqr)) {
            Error("Expect a \"]\" to close expression in an object");
            return NULL;
          }
          break;
        case Token::TK_STRING:
          key = ast_factory_.NewLiteral(lexer_,lexer_.lexeme().str_value);
          lexer_.Next();
          break;
        case Token::TK_IDENTIFIER:
          key = ast_factory_.NewVariable(lexer_,lexer_.lexeme().str_value);
          lexer_.Next();
          break;
        default:
          Error("Expect a expression serve as dictionary's key");
          return NULL;
      }

      if(!lexer_.Expect(Token::kColon)) {
        Error("Expect a \":\"");
        return NULL;
      }

      ast::Node* val;

      if(!(val = ParseExpression())) return NULL;
      entry->Add(zone_,ast::Object::Entry(key,val));
      if(entry->size() > kMaxObjectEntryCount) {
        Error("Too many object literal's entry, at most %zu is allowed",
              kMaxObjectEntryCount);
        return NULL;
      }

      if(lexer_.lexeme().token == Token::kComma) {
        lexer_.Next();
      } else if(lexer_.lexeme().token == Token::kRBra) {
        lexer_.Next();
        break;
      } else {
        Error("Expect a \",\" or \"}\" in a object literal");
        return NULL;
      }

    } while(true);

    return ast_factory_.NewObject(start,lexer_.lexeme().start,entry);
  }
}

ast::FuncCall* Parser::ParseFuncCall() {
  lava_verify( lexer_.lexeme().token == Token::kLPar );

  size_t expr_start = lexer_.lexeme().start;
  size_t expr_end;

  if( lexer_.Next().token == Token::kRPar ) {
    expr_end = lexer_.lexeme().end;
    lexer_.Next();
    return ast_factory_.NewFuncCall(expr_start,expr_end,Vector<ast::Node*>::New(zone_));
  } else {
    Vector<ast::Node*>* arg_list = Vector<ast::Node*>::New(zone_);
    do {
      ast::Node* expr = ParseExpression();
      if(!expr) return NULL;

      arg_list->Add(zone_,expr);
      if(arg_list->size() > interpreter::kMaxFunctionArgumentCount) {
        Error("Too many function argument, at most %zu is allowed",
              interpreter::kMaxFunctionArgumentCount);
        return NULL;
      }

      if(lexer_.lexeme().token == Token::kComma) {
        lexer_.Next();
      } else if(lexer_.lexeme().token == Token::kRPar) {
        expr_end = lexer_.lexeme().end;
        lexer_.Next();
        break;
      } else {
        Error("Expect a \",\" or \")\" in function call argument list");
        return NULL;
      }
    } while(true);

    return ast_factory_.NewFuncCall(expr_start,expr_end,arg_list);
  }
}

ast::Node* Parser::ParsePrefix( ast::Node* prefix ) {
  lava_verify( lexer_.lexeme().token.IsPrefixOperator() );

  /**
   * Parse a prefix expression. A prefix expression starts with a variable/identifier
   * and it can optionally follows
   *  1) dot operator
   *  2) square/index operator
   *  3) function call
   */

  size_t expr_start   = prefix->start;   // Start position of a expression

  Vector<ast::Prefix::Component>* list = Vector<ast::Prefix::Component>::New(zone_);

  do {
    switch(lexer_.lexeme().token) {
      case Token::TK_DOT:
        if(!lexer_.Try(Token::kIdentifier)) {
          Error("Expect an identifier after a \".\" operator");
          return NULL;
        }
        list->Add(zone_,ast::Prefix::Component(ast_factory_.NewVariable(lexer_,
                                               lexer_.lexeme().str_value)));
        lexer_.Next();
        break;
      case Token::TK_LSQR:
        {
          lexer_.Next();
          ast::Node* expr = ParseExpression();
          if(!expr) return NULL;
          if(!lexer_.Expect(Token::kRSqr)) {
            Error("Expect an \"]\" to close the index operator");
            return NULL;
          }
          list->Add(zone_,ast::Prefix::Component(expr));
          break;
        }
      case Token::TK_LPAR:
        {
          ast::FuncCall* call = ParseFuncCall();
          if(!call) return NULL;
          list->Add(zone_,ast::Prefix::Component(call));
          break;
        }
      default:
        return ast_factory_.NewPrefix(expr_start,lexer_.lexeme().start,
                                      list,
                                      prefix);
    }
  } while(true);

  lava_unreach("");
  return NULL;
}

ast::Node* Parser::ParseUnary() {
  if( lexer_.lexeme().token.IsUnaryOperator() ) {
    /** Yeah, it is an unary */
    Token tk = lexer_.lexeme().token;
    size_t expr_start = lexer_.lexeme().start;
    lexer_.Next();

    ast::Node* expr = ParseUnary();
    if(!expr) return NULL;

    return ast_factory_.NewUnary(expr_start,
                                 lexer_.lexeme().start,
                                 tk,
                                 expr);
  } else {
    return ParseAtomic();
  }
}

namespace {

/** Expression precedence **/
static int kPrecendence[] = {
  2, // TK_ADD
  2, // TK_SUB
  1, // TK_MUL
  1, // TK_DIV
  1, // TK_MOD
  1, // TK_DIV
  3, // TK_CONCAT,
  4, // TK_LT,
  4, // TK_LE,
  4, // TK_GT,
  4, // TK_GE,
  5, // TK_EQ
  5, // TK_NE
  6, // TK_AND,
  7  // TK_OR
};

static const int kMaxPrecedence = 7;

} // namespace

ast::Node* Parser::ParsePrimary( int precedence ) {
  if(precedence == 0)
    return ParseUnary();
  else {
    size_t expr_start = lexer_.lexeme().start; // Starting position of expression

    ast::Node* lhs = ParsePrimary(precedence-1);
    if(!lhs) return NULL;

    while(lexer_.lexeme().token.IsBinaryOperator()) {
      ast::Node* rhs;
      int p = kPrecendence[lexer_.lexeme().token];

      lava_assert( p >= precedence , "Current precedence must be larger or "
                                     "equal than the input precedence");

      if(p == precedence) {
        /** We could consume such token since the precedence matches */
        Token op = lexer_.lexeme().token;
        lexer_.Next();
        if(!(rhs = ParsePrimary(precedence-1))) return NULL;
        lhs = ast_factory_.NewBinary(expr_start,lexer_.lexeme().start,op,lhs,rhs);
      } else {
        /**
         * Here the current precedence is larger than the input prcedence
         * so we cannot consume this token here and we need to break, the
         * caller that resides below us on callstack should be able to consume
         * this token
         */
        break;
      }
    }

    return lhs;
  }
}

ast::Node* Parser::ParseBinary() {
  return ParsePrimary(kMaxPrecedence);
}

ast::Node* Parser::ParseTernary( ast::Node* input ) {
  lava_verify( lexer_.lexeme().token == Token::kQuestion );
  lexer_.Next();

  ast::Node* _2nd = ParseExpression();
  if(!_2nd) return NULL;

  if(!lexer_.Expect(Token::kColon)) {
    Error("Expect a \":\" in ternary expression");
    return NULL;
  }

  ast::Node* _3rd = ParseExpression();
  if(!_3rd) return NULL;

  return ast_factory_.NewTernary(input->start,lexer_.lexeme().start,
                                 input, _2nd, _3rd);
}

ast::Node* Parser::ParseExpression() {
  ast::Node* _1st = ParseBinary();
  if(!_1st) return NULL;

  if( lexer_.lexeme().token == Token::kQuestion ) {
    ast::Node* node = ParseTernary(_1st);
    return node ? Optimize(zone_,lexer_.source(),node,error_) : node;
  }
  return _1st ? Optimize(zone_,lexer_.source(),_1st,error_) : NULL;
}

ast::Var* Parser::ParseVar() {
  lava_verify(lexer_.lexeme().token == Token::kVar);

  size_t stmt_start = lexer_.lexeme().start;

  if(!lexer_.Try(Token::kIdentifier)) {
    Error("Expect an identifier after keyword \"var\" in var statement");
    return NULL;
  }

  ast::Variable* name = ast_factory_.NewVariable(lexer_.lexeme().start,
                                                 lexer_.lexeme().end,
                                                 lexer_.lexeme().str_value);
  ast::Node* val = NULL;
  if( lexer_.Next().token == Token::kAssign ) {
    lexer_.Next();
    if(!(val = ParseExpression())) return NULL;
  }

  return ast_factory_.NewVar(stmt_start, lexer_.lexeme().start, name, val);
}

ast::Assign* Parser::ParseAssign( ast::Node* v ) {
  size_t expr_start = v->start;
  lava_verify(lexer_.lexeme().token == Token::kAssign);
  lexer_.Next();
  ast::Node* val = ParseExpression();
  if(!val) return NULL;
  if( v->IsVariable() )
    return ast_factory_.NewAssign(expr_start,
                                  lexer_.lexeme().start,
                                  v->AsVariable(),
                                  val);
  else {
    // Check whether v is a valid left hand side value
    if(v->IsPrefix() && !(v->AsPrefix()->list->Last().IsCall()))
      return ast_factory_.NewAssign(expr_start,
                                    lexer_.lexeme().start,
                                    v->AsPrefix(),
                                    val);
    ErrorAt(v->start,v->end,"Invalid left hand side for assignment");
    return NULL;
  }
}

ast::Node* Parser::ParsePrefixStatement() {
  ast::Node* expr = ParseExpression();
  if(!expr) return NULL;

  if( lexer_.lexeme().token == Token::kAssign ) {
    return ParseAssign(expr);
  } else {
    if(expr->IsPrefix() && expr->AsPrefix()->list->Last().IsCall())
      return ast_factory_.NewCall(expr->start,expr->end,expr->AsPrefix());
    ErrorAt(expr->start,expr->end,"Meaningless statement");
    return NULL;
  }
}

ast::If* Parser::ParseIf() {
  lava_verify(lexer_.lexeme().token == Token::kIf);
  Vector<ast::If::Branch>* branch_list = Vector<ast::If::Branch>::New(zone_);
  ast::If::Branch br;
  size_t expr_start = lexer_.lexeme().start;
  size_t expr_end;

  if(!ParseCondBranch(&br)) return NULL;
  branch_list->Add(zone_,br);

  do {
    switch(lexer_.lexeme().token) {
      case Token::TK_ELIF:
        if(!ParseCondBranch(&br))
          return NULL;
        branch_list->Add(zone_,br);
        break;
      case Token::TK_ELSE:
        lexer_.Next();
        br.cond = NULL;
        if(!(br.body = ParseSingleStatementOrChunk()))
          return NULL;
        branch_list->Add(zone_,br);
        // Fallthru
      default:
        expr_end = lexer_.lexeme().start;
        goto done; // We cannot have any other branches here
    }
  } while(true);

done:
  return ast_factory_.NewIf(expr_start,expr_end,branch_list);
}

bool Parser::ParseCondBranch( ast::If::Branch* br ) {
  if(!lexer_.Try(Token::kLPar)) {
    Error("Expect a \"(\" to start a if/elif condition");
    return false;
  }

  lexer_.Next();

  if(!(br->cond = ParseExpression())) return false;

  if(!lexer_.Expect(Token::kRPar)) {
    Error("Expect a \")\" to end a if/elif condition");
    return false;
  }

  return (br->body = ParseSingleStatementOrChunk());
}

ast::Node* Parser::ParseFor() {
  lava_verify( lexer_.lexeme().token == Token::kFor );
  size_t expr_start = lexer_.lexeme().start;

  if(!lexer_.Try(Token::kLPar)) {
    Error("Expect a \"(\" to start for statement");
    return NULL;
  }

  if(lexer_.Next().token == Token::kVar) {
    ast::Var* short_assign = ParseVar();
    if(!short_assign) return NULL;

    switch(lexer_.lexeme().token) {
      case Token::TK_COMMA:
        {
          // This must be a *foreach* statement , it must have style like :
          // for( var idx , key in array ) { ... }
          if(lexer_.Next().token != Token::kIdentifier) {
            ErrorAt(expr_start,short_assign->end, "foreach statement's expect a identifier to indicate "
                                                  "value in the foreach, if no need to have the value "
                                                  "please specify a _ to denote placeholder");
            return NULL;
          }

          // Create the value
          ast::Variable* val = ast_factory_.NewVariable(lexer_.lexeme().start,
                                                        lexer_.lexeme().end,
                                                        lexer_.lexeme().str_value);

          if(!lexer_.Try(Token::kIn)) {
            ErrorAt(expr_start,lexer_.lexeme().end, "foreach statement expect a \"in\" "
                                                    "after variable definition");
            return NULL;
          }

          if(short_assign->expr) {
            ErrorAt(expr_start,short_assign->end, "foreach statement's variable "
                                                  "expects a \"in\" after variable "
                                                  "not an normal assignment");
            return NULL;
          }
          return ParseForEach(expr_start,short_assign->var,val);
        }
      case Token::TK_SEMICOLON:
        return ParseStepFor(expr_start,short_assign);
      default:
        Error("Expect a \"in\" or \";\" in for statement");
        return NULL;
    }
  } else {
    if(lexer_.lexeme().token == Token::kSemicolon) {
      // Can be null for for( _ ; _ ; _ ) style for statement
      return ParseStepFor(expr_start,NULL);
    } else {
      Error("Unexpected statement in for/foreach.Requires a "
            "short declaration or leave it just empty!");
      return NULL;
    }
  }
}

ast::For* Parser::ParseStepFor( size_t expr_start , ast::Var* expr ) {
  lava_verify( lexer_.lexeme().token == Token::kSemicolon );
  ast::Node* cond = NULL;
  ast::Node* step = NULL;
  ast::Chunk* body = NULL;

  if( lexer_.Next().token != Token::kSemicolon ) {
    if(!(cond = ParseExpression())) return NULL;
    if(!lexer_.Expect(Token::kSemicolon)) {
      Error("Expect a \";\" here");
      return NULL;
    }
    if(!expr) {
      Error("You specify a condition for the loop,however you do not "
            "specify the short assignment to initalize loop induction "
            "variable!");
      return NULL;
    }
  } else {
    lexer_.Next();
  }

  if( lexer_.lexeme().token != Token::kRPar ) {
    if(!(step = ParseExpression())) return NULL;
    if(!lexer_.Expect(Token::kRPar)) {
      Error("Expect a \")\" here");
      return NULL;
    }
    if(!expr) {
      Error("You specify a step variable,but you do not specify "
            "loop induction variable");
      return NULL;
    }
  } else {
    lexer_.Next();
  }

  ++nested_loop_;
  if(!(body = ParseSingleStatementOrChunk())) return NULL;
  --nested_loop_;

  return ast_factory_.NewFor( expr_start ,
                              lexer_.lexeme().start ,
                              expr,
                              cond,
                              step,
                              body );
}

ast::ForEach* Parser::ParseForEach( size_t expr_start , ast::Variable* key ,
                                                        ast::Variable* val ) {
  lava_verify( lexer_.lexeme().token == Token::kIn );
  ast::Node* itr = NULL;
  ast::Chunk* body = NULL;

  lexer_.Next();

  if(!(itr = ParseExpression())) return NULL;
  if(!lexer_.Expect(Token::kRPar)) {
    Error("Expect a \")\" here");
    return NULL;
  }

  ++nested_loop_;
  if(!(body = ParseSingleStatementOrChunk())) return NULL;
  --nested_loop_;

  return ast_factory_.NewForEach( expr_start ,
                                  lexer_.lexeme().start ,
                                  key,
                                  val,
                                  itr,
                                  body );
}

ast::Break* Parser::ParseBreak() {
  lava_verify( lexer_.lexeme().token == Token::kBreak );
  if(nested_loop_ == 0) {
    Error("break/continue must be in a loop body");
    return NULL;
  }

  ast::Break* ret = ast_factory_.NewBreak( lexer_.lexeme().start,
                                           lexer_.lexeme().end );

  lexer_.Next();
  return ret;
}

ast::Continue* Parser::ParseContinue() {
  lava_verify( lexer_.lexeme().token == Token::kContinue );
  if(nested_loop_ == 0) {
    Error("break/continue must be in a loop body");
    return NULL;
  }

  ast::Continue* ret = ast_factory_.NewContinue( lexer_.lexeme().start ,
                                                 lexer_.lexeme().end );

  lexer_.Next();
  return ret;
}

ast::Return* Parser::ParseReturn() {
  lava_verify( lexer_.lexeme().token == Token::kReturn );
  size_t expr_start = lexer_.lexeme().start;
  size_t expr_end   = lexer_.lexeme().end;
  if(lexer_.Next().token == Token::kSemicolon) {
    return ast_factory_.NewReturn(expr_start,expr_end,NULL);
  } else {
    ast::Node* expr = ParseExpression();
    if(!expr) return NULL;
    return ast_factory_.NewReturn(expr_start,lexer_.lexeme().start,expr);
  }
}

ast::Node* Parser::ParseStatement() {
  ast::Node* ret;
  switch(lexer_.lexeme().token) {
    case Token::TK_VAR:
      if(!(ret=ParseVar())) return NULL;
      break;
    case Token::TK_IF:  return ParseIf();
    case Token::TK_FOR: return ParseFor();
    case Token::TK_RETURN:
      if(!(ret=ParseReturn())) return NULL;
      break;
    case Token::TK_BREAK:
      if(!(ret=ParseBreak())) return NULL;
      break;
    case Token::TK_CONTINUE:
      if(!(ret=ParseContinue())) return NULL;
      break;
    case Token::TK_FUNCTION: return ParseFunction();
    default:
      if(!(ret=ParsePrefixStatement())) return NULL;
      break;
  }

  if(lexer_.lexeme().token != Token::kSemicolon) {
    ErrorAt(ret->start,ret->end,"Expect a \";\" after this statement");
    return NULL;
  }

  lexer_.Next();
  return ret;
}

Parser::ChunkStmtAddResult
Parser::AddChunkStmt( ast::Node* stmt , Vector<ast::Variable*>* lv ) {
  /**
   * Sort out all the local variable declaration and put them
   * into the local_vars list. The code generator will first
   * reserve the needed register for those local variable to
   * maintain register allocation in order
   */
  ChunkStmtAddResult ret = VARIABLE_OKAY; // nothing would happen

  if(stmt->IsVar()) {
    /* check if this variable existed or not */
    ast::Variable* v = stmt->AsVar()->var;
    if(!CheckArgumentExisted(*lv,*v->name)) {
      return VARIABLE_EXISTED;
    }
    lv->Add(zone_,v);
    ret = VARIABLE_OKAY;

  } else if(stmt->IsFor()) {
    ast::Var* v = stmt->AsFor()->_1st;
    if(v) {
      ast::Variable* var = v->var;
      /** for pratical reason, we silently ignore variable has duplicated
       *  definition here. so yes you could redefine a variable inside of
       *  a for range loop */
      if(CheckArgumentExisted(*lv,*var->name)) {
        lv->Add(zone_,var);
      }
    }

    // Figure out how much reserved iterator needs to be in this chunk
    // for this loop. We reserve loop condition and step variable to
    // have very simple loop bytecode.
    {
      int temp = 0;
      if(stmt->AsFor()->_2nd) temp++;
      if(stmt->AsFor()->_3rd) temp++;
      ret = static_cast<ChunkStmtAddResult>(temp);
    }
  } else if(stmt->IsForEach()) {
    /**
     * Same as for range loop , we don't complain about redefinition of variable
     * for key and value for pratical reason they literally just over-shadow the
     * same name variable in same lexical scope
     */
    {
      ast::Variable* key = stmt->AsForEach()->key;
      if(CheckArgumentExisted(*lv,*key->name)) {
        lv->Add(zone_,key);
      }
    }

    {
      ast::Variable* val = stmt->AsForEach()->val;
      if(CheckArgumentExisted(*lv,*val->name)) {
        lv->Add(zone_,val);
      }
    }

    ret = ITERATOR_NEED1;
  }

  return ret;
}

ast::Chunk* Parser::ParseChunk() {
  lava_verify( lexer_.lexeme().token == Token::kLBra );
  LexicalScopeAdder lscope(this); // enter lexical scope

  size_t expr_start = lexer_.lexeme().start;
  size_t expr_end   = lexer_.lexeme().end;

  Vector<ast::Node*>* ck = Vector<ast::Node*>::New(zone_);
  Vector<ast::Variable*>* lv = Vector<ast::Variable*>::New(zone_);

  if(lexer_.Next().token == Token::kRBra) {
    lexer_.Next(); // Eat '}'
    return ast_factory_.NewChunk( expr_start , lexer_.lexeme().end ,
                                                               ck ,
                                                               lv ,
                                                               0 );
  } else {
    std::size_t iter_cnt = 0;
    do {
      ast::Node* stmt = ParseStatement();
      if(!stmt) return NULL;
      ChunkStmtAddResult result = AddChunkStmt(stmt,lv);

      if(result == VARIABLE_EXISTED) {
        Error("variable %s already defined",stmt->AsVar()->var->name->data());
        return NULL;
      }
      std::size_t temp = static_cast<std::size_t>(result);
      if(iter_cnt < temp) iter_cnt = temp;

      ck->Add(zone_,stmt);
    } while( lexer_.lexeme().token != Token::kEof &&
             lexer_.lexeme().token != Token::kRBra );

    if( lexer_.lexeme().token == Token::kEof ) {
      Error("Expect a \"}\" to close the lexical scope");
      return NULL;
    }

    expr_end = lexer_.lexeme().end;
    lexer_.Next(); // Skip the last }

    // update local variable count information
    CalculateLexcialScopeInfo(lv->size(),iter_cnt);

    return ast_factory_.NewChunk(expr_start,expr_end,ck,lv,iter_cnt);
  }
}

ast::Chunk* Parser::ParseSingleStatementOrChunk() {
  if(lexer_.lexeme().token == Token::kLBra)
    return ParseChunk();
  else {
    LexicalScopeAdder lscope(this);

    Vector<ast::Node*>* ck = Vector<ast::Node*>::New(zone_);
    ast::Node* stmt = ParseStatement();
    if(!stmt) return NULL;

    Vector<ast::Variable*>* lv = Vector<ast::Variable*>::New(zone_);

    ChunkStmtAddResult result = AddChunkStmt(stmt,lv);
    if(result == VARIABLE_EXISTED) {
      Error("variable %s already defined",stmt->AsVar()->var->name->data());
      return NULL;
    }

    ck->Add(zone_,stmt);

    std::size_t iter_cnt = static_cast<std::size_t>(result);

    CalculateLexcialScopeInfo(lv->size(),iter_cnt);

    return ast_factory_.NewChunk(stmt->start,stmt->end,ck,lv,iter_cnt);
  }
}

bool Parser::CheckArgumentExisted( const Vector<ast::Variable*>& arg_list,
                                   const String& arg ) const {

  for( size_t i = 0 ; i < arg_list.size() ; ++i ) {
    const ast::Variable* v = arg_list.Index(i);
    if(*(v->name) == arg) return false;
  }
  return true;
}

Vector<ast::Variable*>* Parser::ParseFunctionPrototype() {
  lava_verify( lexer_.lexeme().token == Token::kLPar );
  if( lexer_.Next().token == Token::kRPar ) {
    lexer_.Next();
    return Vector<ast::Variable*>::New(zone_);
  } else {
    Vector<ast::Variable*>* arg_list = Vector<ast::Variable*>::New(zone_);

    do {
      if( lexer_.lexeme().token == Token::kIdentifier ) {

        ast::Variable* v = ast_factory_.NewVariable(lexer_.lexeme().start,
                                                    lexer_.lexeme().end,
                                                    lexer_.lexeme().str_value);
        // just use add chunkstmt to add argument and then check whether an
        // argument is existed or not
        if(AddChunkStmt(v,arg_list) == VARIABLE_EXISTED) {
          Error("argument %s already exists",v->name->data());
          return NULL;
        }

        // add the count back to the top level lexical scope's variable counter
        function_scope_info()->top_scope()->var_count++;

        if(arg_list->size() == interpreter::kMaxFunctionArgumentCount) {
          Error("Too many function argument, at most %zu is allowed",
                interpreter::kMaxFunctionArgumentCount);
          return NULL;
        }

        arg_list->Add(zone_,v);
        lexer_.Next();
      } else {
        Error("Expect a identifier to represent function argument");
        return NULL;
      }

      if(lexer_.lexeme().token == Token::kComma) {
        lexer_.Next();
      } else if(lexer_.lexeme().token == Token::kRPar) {
        lexer_.Next();
        break;
      } else {
        Error("Expect a \",\" or \")\" here in function's argument list");
        return NULL;
      }
    } while(true);

    return arg_list;
  }
}

ast::Function* Parser::ParseFunction() {
  lava_verify( lexer_.lexeme().token == Token::kFunction );
  size_t expr_start = lexer_.lexeme().start;
  if(!lexer_.Try(Token::kIdentifier)) {
    Error("Expect a identifier followed by \"function\" in function definition");
    return NULL;
  }

  LocVarContextAdder lctx_adder(this);
  LexicalScopeAdder  lscope    (this);

  ast::Variable* fname = ast_factory_.NewVariable(lexer_.lexeme().start,
                                                  lexer_.lexeme().end,
                                                  lexer_.lexeme().str_value);

  Vector<ast::Variable*>* arg_list;

  if(!lexer_.Try(Token::kLPar)) {
    Error("Expect a \"(\" to start the function prototype");
    return NULL;
  }
  if(!(arg_list = ParseFunctionPrototype())) return NULL;
  if(lexer_.lexeme().token != Token::kLBra) {
    Error("Expect a \"{\" to start the function body");
    return NULL;
  }

  /** this will actually open a new lexical scope but nothing hurt */
  ast::Chunk* body = ParseChunk();
  if(!body) return NULL;

  return ast_factory_.NewFunction(expr_start,
                                  lexer_.lexeme().start ,
                                  fname,
                                  arg_list,
                                  body,
                                  function_scope_info()->var_context);
}

ast::Function* Parser::ParseAnonymousFunction() {
  lava_verify( lexer_.lexeme().token == Token::kFunction );
  size_t expr_start = lexer_.lexeme().start;
  if(!lexer_.Try(Token::kLPar)) {
    Error("Expect a \"(\" to start the function prototype");
    return NULL;
  }

  LocVarContextAdder lctx_adder(this);
  LexicalScopeAdder  lscope    (this);

  Vector<ast::Variable*>* arg_list = ParseFunctionPrototype();
  if(!arg_list) return NULL;
  if(lexer_.lexeme().token != Token::kLBra) {
    Error("Exepct a \"{\" to start the function body");
    return NULL;
  }
  ast::Chunk* body = ParseChunk();
  if(!body) return NULL;

  return ast_factory_.NewFunction(expr_start,
                                  lexer_.lexeme().start,
                                  NULL,
                                  arg_list,
                                  body,
                                  function_scope_info()->var_context);
}

ast::Root* Parser::Parse() {
  size_t expr_start = lexer_.lexeme().start;
  Vector<ast::Node*>* main_body = Vector<ast::Node*>::New(zone_);
  Vector<ast::Variable*>* lv = Vector<ast::Variable*>::New(zone_);
  std::size_t iter_cnt = 0;

  LocVarContextAdder lctx_adder(this);
  LexicalScopeAdder lscope(this);

  while( lexer_.lexeme().token != Token::kEof ) {
    ast::Node* stmt = ParseStatement();
    if(!stmt) return NULL;
    main_body->Add(zone_,stmt);

    ChunkStmtAddResult result = AddChunkStmt(stmt,lv);
    if(result == VARIABLE_EXISTED) {
      Error("variable %s already defined",stmt->AsVar()->var->name->data());
      return NULL;
    }
    std::size_t temp = static_cast<std::size_t>(result);
    if(iter_cnt < temp) iter_cnt = temp;
  }

  // Add iterator to loc var context if we have iterator
  CalculateLexcialScopeInfo(lv->size(),iter_cnt);

  return ast_factory_.NewRoot(expr_start, lexer_.lexeme().start,
      ast_factory_.NewChunk(expr_start, lexer_.lexeme().start,main_body,
                                                              lv,
                                                              iter_cnt),
      function_scope_info()->var_context);
}

} // namespace parser
} // namespace lavascript
