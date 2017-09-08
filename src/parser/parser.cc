#include "parser.h"
#include "ast/ast.h"

#include <src/zone/zone.h>


namespace lavascript {
namespace parser {

using namespace lavascript::zone;

ast::Node* Parser::ParseAtomic() {
  ast::Node* ret;
  switch(lexer_.lexeme().token) {
    case Token::kInteger:
      ret = ast_factory_.NewLiteral( lexer_ , lexer_.lexeme().int_value );
      lexer_.Next();
      return ret;
    case Token::kReal:
      ret = ast_factory_.NewLiteral( lexer_ , lexer_.lexeme().real_value );
      lexer_.Next();
      return ret;
    case Token::kTrue:
      ret = ast_factory_.NewLiteral( lexer_ , true );
      lexer_.Next();
      return ret;
    case Token::kFalse:
      ret = ast_factory_.NewLiteral( lexer_ , false );
      lexer_.Next();
      return ret;
    case Token::kNull:
      ret = ast_factory_.NewLiteral( lexer_ );
      lexer_.Next();
      return ret;
    case Token::kString:
      ret = ast_factory_.NewLiteral( lexer_ , lexer_.lexeme().str_value );
      lexer_.Next();
      return ret;
    case Token::kLPar:
      lexer_.Next();
      if(!(ret = ParseExpression())) return NULL;
      if(lexer_.Expect(Token::kRPar))
        break;
      else {
        Error("Expect an \")\" to close subexpression!");
        return NULL;
      }
    case Token::kLSqr:
      if(!(ret=ParseList())) return NULL;
      break;
    case Token::kLBra:
      if(!(ret=ParseObject())) return NULL;
      break;
    case Token::kIdentifier:
      ret = ast_factory_.NewVariable(lexer_.lexeme().start,
                                     lexer_.lexeme().end,
                                     lexer_.lexeme().str_value);
      lexer_.Next();
      break;
    case Token::kFunction:
      if(!(ret=ParseAnonymousFunction()))
        return NULL;
      break;
    default:
      Error("Unexpected token %s,expect an primary expression",
          lexer_.lexeme().token.token_name());
      return NULL;
  }

  if(lexer_.lexeme().token.IsPrefixOperator()) {
    return ParsePrefix(ret);
  }
  return ret;
}

ast::List* Parser::ParseList() {
  lava_assert( lexer_.lexeme().token == Token::kLSqr , "" );
  if( lexer_.Next().token == Token::kRSqr ) {
    lexer_.Next();
    return ast_factory_.NewList(lexer_);
  } else {
    Vector<ast::Node*>* entry = Vector<ast::Node*>::New(zone_);
    do {
      ast::Node* e = ParseExpression();
      if(!e) return NULL;
      entry->Add(zone_,e);
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
    return ast_factory_.NewList(lexer_,entry);
  }
}

ast::Object* Parser::ParseObject() {
  lava_assert( lexer_.lexeme().token == Token::kLBra , "" );
  if( lexer_.Next().token == Token::kRBra ) {
    lexer_.Next();
    return ast_factory_.NextObject(lexer_);
  } else {
    Vector<ast::Object::Entry>* entry = Vector<ast::Object::Entry>::New(zone_);
    do {
      ast::Node* key;
      switch(lexer_.lexeme().token) {
        case Token::kLSqr: // Subexpression
          lexer_.Next();
          if(!(key = ParseExpression())) return NULL;
          if(!lexer_.Expect(Token::kRSqr)) {
            Error("Expect a \"]\" to close expression in an object");
            return NULL;
          }
          break;
        case Token::kString:
          key = ast_factory_.NewLiteral(lexer_,lexer_.lexeme().str_value);
          lexer_.Next();
          break;
        case Token::kIdentifier:
          key = ast_factory_.NewVariable(l,lexer_.lexeme().str_value);
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
      entry->Add(zone_,ast::Object:Entry(key,val));

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

    return ast_factory_.NewObject(l,entry);
  }
}

ast::FuncCall* Parser::ParseFuncCall() {
  lava_assert( lexer_.lexeme().token == Token::kLPar , "" );

  size_t expr_start = lexer_.lexeme().start;
  size_t expr_end;

  if( lexer_.Next().token == Token::kRPar ) {
    expr_end = lexer_.lexeme().end;
    lexer_.Next();
    return ast_factory_.NewFuncCall(expr_start,expr_end,Vector<Node*>::New(zone_));
  } else {
    Vector<Node*>* arg_list = Vector<Node*>::New(zone_);
    do {
      ast::Node* expr = ParseExpression();
      if(!expr) return NULL;
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

      arg_list->push_back(zone_,expr);
    } while(true);

    return ast_factory_.NewFuncCall(expr_start,expr_end,arg_list);
  }
}

ast::Node* Parser::ParsePrefix( ast::Node* prefix ) {
  lava_assert( lexer_.lexeme().token.IsPrefixOperator() , "");

  /**
   * Parse a prefix expression. A prefix expression starts with a variable/identifier
   * and it can optionally follows
   *  1) dot operator
   *  2) square/index operator
   *  3) function call
   */

  size_t expr_start   = prefix->start;   // Start position of a expression

  Vector<Prefix::Component>* list = Vector<Prefix::Component>::New(zone_);
  do {
    switch(lexer_.lexeme().token) {
      case Token::kDot:
        if(!lexer_.Try(Token::kIdentifier)) {
          Error("Expect an identifier after a \".\" operator");
          return NULL;
        }
        list->Add(zone_,Prefix::Component(ast_factory_.NewVariable(lexer_,
                                                                   lexer_.lexeme().str_value)));
        lexer_.Next();
        break;
      case Token::kLSqr:
        {
          ast::Node* expr = ParseExpression();
          if(!expr) return NULL;
          list->Add(zone_,Prefix::Component(expr));
          break;
        }
      case Token::kLPar:
        {
          ast::FuncCall* call = ParseFuncCall();
          if(!call) return NULL;
          list->Add(zone_,Prefix::Component(call));
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
  if( lexer_.lexeme().IsUnaryOperator() ) {
    /** Yeah, it is an unary */
    Token tk = lexer_.lexeme().token;
    size_t tk_start   = lexer_.lexeme().start;
    size_t tk_end     = lexer_.lexeme().end;
    size_t expr_start = tk_start;

    ast::Node* expr = ParseUnary();
    if(!expr) return NULL;

    return ast_factory_.NewUnary(expr_start,lexer_.lexeme().start,
                                 tk_start,
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
  3, // TK_LT,
  3, // TK_LE,
  3, // TK_GT,
  3, // TK_GE,
  4, // TK_EQ
  4, // TK_NE
  5, // TK_AND,
  6  // TK_OR
};

static const int kMaxPrecedence = 6;

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
  lava_assert( lexer_.lexeme().token == Token::kQuestion , "");
  size_t question_pos = lexer_.lexeme().start;
  lexer_.Next();

  ast::Node* _2nd = ParseExpression();
  if(!_2nd) return NULL;

  size_t colon_pos = lexer_.lexeme().start;
  if(!lexer_.Expect(Token::kColon)) {
    Error("Expect a \":\" in ternary expression");
    return NULL;
  }

  ast::Node* _3rd = ParseExpression();
  if(!_3rd) return NULL;

  return ast_factory_.NewTernary(input->start,lexer_.lexeme().start,
                                 question_pos,
                                 colon_pos,
                                 input, _2nd, _3rd);
}

ast::Node* Parser::ParseExpression() {
  ast::Node* _1st = ParseBinary();
  if(!_1st) return NULL;

  if( lexer_.lexeme().token == Token::kQuestion ) {
    return ParseTernary(_1st);
  }
  return _1st;
}

ast::Var* Parser::ParseVar() {
  lava_assert(lexer_.lexeme().token == Token::kVar , "");

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
  lava_assert(lexer_.lexeme().token == Token::kAssign,"");
  lexer_.Next();
  ast::Node* val = ParseExpression();
  if(!val) return NULL;
  if( v->IsVariable() )
    return ast_factory_.NewAssign(expr_start,
                                  lexer_.lexer().start,
                                  v->AsVariable(),
                                  val);
  else {
    // Check whether v is a valid left hand side value
    if(v->IsPrefix() && !(v->AsPrefix()->list->Last().IsCall()))
      return ast_factory_.NewAssign(expr_start,
                                    lexer_.lexer().start,
                                    v,
                                    val);
    ErrorAt(v->start,"Invalid left hand side for assignment");
    return NULL;
  }
}

ast::Node* Parser::ParsePrefixStatement() {
  ast::Node* expr = ParseExpression();
  if( lexer_.lexeme().token == Token::kAssign ) {
    return ParseAssign(expr);
  } else {
    if(expr->IsPrefix() && expr->AsPrefix()->list->Last().IsCall())
      return expr;
    ErrorAt(expr->start,"Meaningless statement");
    return NULL;
  }
}

ast::If* Parser::ParseIf() {
  lava_assert(lexer_.lexeme().token == Token::kIf,"");
  zone::Vector<Branch>* branch_list = zone::Vector<Branch>::New(zone_);
  If::Branch br;
  size_t expr_start = lexer_.lexeme().start;
  size_t expr_end;

  if(!ParseCondBranch(&br)) return NULL;
  branch_list->Add(zone_,br);

  do {
    switch(lexer_.lexeme().token) {
      case Token::kElif:
        if(!ParseCondBranch(&br))
          return NULL;
        branch_list.Add(zone_,br);
        break;
      case Token::kElse:
        lexer.Next();
        br.cond = NULL;
        if(!(br.body = ParseSingleStatementOrChunk()))
          return NULL;
        branch_list.Add(zone_,br);
        // Fallthru
      default:
        expr_end = lexer_lexeme().start;
        goto done; // We cannot have any other branches here
    }
  } while(true);

done:
  return ast_factory_.NewIf(expr_start,expr_end,branch_list);
}

bool Parser::ParseCondBranch( If::Branch* br ) {
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
  lava_assert( lexer_.lexeme().token == Token::kFor , "");
  size_t expr_start = lexer_.lexeme().start;

  if(!lexer_.Try(Token::kLPar)) {
    Error("Expect a \"(\" to start for statement");
    return NULL;
  }
  ast::Node* var = NULL;

  switch(lexer_.Next().token) {
    case Token::kColon:
      Error("Expect a variable before \":\" in foreach' statement");
      return NULL;
    case Token::kSemicolon:
      return ParseStepFor(expr_start,var);
    default:
      if(!(var = ParseExpression())) return NULL;
  }

  if(lexer_.lexeme().token == Token::kColon) {
    if(var_or_init->IsVariable())
      return ParseForEach(expr_start,var_or_init);
    else {
      Error("Expect a variable before \":\" in foreach's statement");
      return NULL;
    }
  } else if(lexer_.lexeme().token == Token::kSemicolon) {
    return ParseStepFor(expr_start,var);
  } else {
    Error("Expect \":\" or \";\" in for statement");
    return NULL;
  }
}

ast::For* Parser::ParseStepFor( size_t expr_start , ast::Node* expr ) {
  lava_assert( lexer_.lexeme().token == Token::kSemicolon , "");
  ast::Node* cond = NULL;
  ast::Node* step = NULL;
  ast::Node* body = NULL;

  if( lexer_.Next().token != Token::kSemicolon ) {
    if(!(cond = ParseExpression())) return NULL;
    if(!lexer_.Expect(Token::kSemicolon)) {
      Error("Expect a \";\" here");
      return NULL;
    }
  } else {
    lexer_.Next();
  }

  if( lexer_.lexeme().token != Token::kRPar ) {
    if(!(step = ParseExpression())) return NULL;
    if(!lexer.Expect(Token::kRPar)) {
      Error("Expect a \")\" here");
      return NULL;
    }
  } else {
    Lexer_.Next();
  }

  ++nested_loop_;
  if(!(body = ParseSingleStatementOrChunk())) return NULL;
  --nested_loop_;

  return ast_factory_.NewFor( expr_start ,
                              lexer_.lexeme().start,
                              expr,
                              cond,
                              step,
                              body );
}

ast::ForEach* Parser::ParseForEach( size_t expr_start , ast::Node* expr ) {
  lava_assert( lexer_.lexeme().token == Token::kColon , "" );
  ast::Node* itr = NULL;
  ast::Node* body = NULL;

  lexer.Next();

  if(!(itr = ParseExpression())) return NULL;
  if(!lexer_.Expect(Token::kRPar)) {
    Error("Expect a \")\" here");
    return NULL;
  }

  ++nested_loop_;
  if(!(body = ParseSingleStatementOrChunk())) return NULL;
  --nested_loop_;

  return ast_factory_.NewForEach( expr_start ,
                                  lexer_.lexeme().start,
                                  expr,
                                  itr,
                                  body );
}

ast::Break* Parser::ParseBreak() {
  lava_assert( lexer_.lexeme().token == Token::kBreak , "");
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
  lava_assert( lexer_.lexeme().token == Token::kContinue, "");
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
  lava_assert( lexer_.lexeme().token == Token::kReturn , "");
  size_t expr_start = lexer_.lexeme().start;
  size_t expr_end   = lexer_.lexeme().end;
  if(lexer.Next().token == Token::kSemicolon) {
    return ast_factory_.NewReturn(expr_start,expr_end,NULL);
  } else {
    ast::Node* expr = ParseExpression();
    if(!expr) NULL;
    return ast_factory_.NewReturn(expr_start,lexer_.lexeme().start,expr);
  }
}

ast::Node* Parser::ParseStatement() {
  ast::Node* ret;
  switch(lexer_.lexeme().token) {
    case Token::kVar: if(!(ret=ParseVar())) return NULL; break;
    case Token::kIf:  if(!(ret=ParseIf())) return NULL; break;
    case Token::kFor: if(!(ret=ParseFor())) return NULL; break;
    case Token::kReturn: if(!(ret=ParseReturn())) return NULL; break;
    case Token::kBreak: if(!(ret=ParseBreak())) return NULL; break;
    case Token::kContinue: if(!(ret=ParseContinue())) return NULL; break;
    default: if(!(ret=ParsePrefixStatement())) return NULL; break;
  }

  if(lexer_.lexeme().token != Token::kSemicolon) {
    Error("Expect a \";\" here");
    return NULL;
  }

  return ret;
}

ast::Node* Parser::ParseChunk() {
  lava_assert( lexer_.lexeme().token == Token::kLBra , "");
  size_t expr_start = lexer_.lexeme().start;
  size_t expr_end   = lexer_.lexeme().end;
  zone::Vector<ast::Node*>* ck = zone::Vector<ast::Node*>::New(zone_);

  if(lexer_.Next().token == Token::kRBra) {
    return ast_factory_.NewChunk( expr_start , expr_end , ck );
  } else {
    do {
      ast::Node* stmt = ParseStatement();
      if(!stmt) return NULL;
      ck->Add(zone_,stmt);
    } while( lexer_.lexeme().token != Token::kEof &&
             lexer_.lexeme().token != Token::kRBra );

    if( lexer_.lexeme().token == Token::kEof ) {
      Error("Expect a \"}\" to close the lexical scope");
      return NULL;
    }

    expr_end = lexer_.lexeme().end;
    lexer_.Next(); // Skip the last }
    return ast_factory_.NewChunk(expr_start,expr_end,ck);
  }
}

ast::Node* Parser::ParseSingleStatementOrChunk() {
  if(lexer_.lexeme().token == Token::kLBra)
    return ParseChunk();
  else {
    zone::Vector<ast::Node*>* ck = zone::Vector<ast::Node*>::New(zone_);
    ast::Node* stmt = ParseStatement();
    if(!stmt) return NULL;
    ck->Add(zone_,stmt);
    return ast_factory_.NewChunk(stmt->start,stmt->end,ck);
  }
}


} // namespace parser
} // namespace lavascript
