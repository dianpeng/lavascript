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
        return ret;
      else {
        Error("Expect an \")\" to close subexpression!");
        return NULL;
      }
    case Token::kLSqr:
      return ParseList();
    case Token::kLBra:
      return ParseObject();
    case Token::kIdentifier:
      return ParsePrefix();
    case Token::kFunction:
      return ParseAnonymousFunction();
    default:
      Error("Expect an primary expression here");
      return NULL;
  }
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

ast::Node* Parser::ParsePrefix() {
  lava_assert( lexer_.lexeme().token == Token::kIndentifier , "");
  /**
   * Parse a prefix expression. A prefix expression starts with a variable/identifier
   * and it can optionally follows
   *  1) dot operator
   *  2) square/index operator
   *  3) function call
   */

  String* prefix = lexer_.lexeme().str_value;
  size_t prefix_start = lexer_.lexeme().start;
  size_t prefix_end   = lexer_.lexeme().end ;
  size_t expr_start   = prefix_start;   // Prefix expression start position

  if(lexer_.Next().token.IsPrefixOperator()) {
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
          return ast_factory_.NewPrefix(expr_start,lexer_.lexeme().end,
                                        list,
                                        ast_factory_.NewVariable(prefix_start,prefix_end,prefix));
      }
    } while(true);

    lava_unreach("");
    return NULL;
  }
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

    return ast_factory_.NewUnary(expr_start,lexer_.lexeme().end,
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
        lhs = ast_factory_.NewBinary(expr_start,lexer_.lexeme().end,op,lhs,rhs);
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
  lava_assert( lexer_.lexeme().token == Token::kQuestion );
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

  return ast_factory_.NewTernary(input->start,lexer_.lexeme().end,
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

} // namespace parser
} // namespace lavascript
