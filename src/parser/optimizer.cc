#include "optimizer.h"
#include "ast/ast.h"
#include "ast/ast-factory.h"

#include "src/zone/zone.h"
#include "src/zone/string.h"
#include "src/zone/vector.h"
#include "src/error-report.h"
#include "src/trace.h"
#include "src/util.h"

#include <cmath>
#include <cstdlib>
#include <cerrno>
#include <climits>

namespace lavascript {
namespace parser {
namespace {

using namespace ::lavascript::zone;

enum ExpressionKind {
  EREAL,
  EBOOLEAN,
  ESTRING,
  ENULL,
  ECOMPLEX
};

struct Expression {
  size_t start , end;       // Original AST's start and end , used to construct node
  ExpressionKind ekind;     // What type of expression this Expression object holds
  union {
    double real_value;
    bool   bool_value;
    String* str_value;
    ast::Node* node;
    void* ptr;
  };

 public:
  bool IsString () const { return ekind == ESTRING; }
  bool IsReal   () const { return ekind == EREAL; }
  bool IsNull   () const { return ekind == ENULL; }
  bool IsBoolean() const { return ekind == EBOOLEAN; }
  bool IsComplex() const { return ekind == ECOMPLEX; }

  inline bool   IsLiteral() const;
  inline bool   AsBoolean ( bool* ) const;
  inline bool   AsReal    ( double* ) const;
  inline void   AsString  ( Zone* , String** ) const;
  inline void   AsString  ( std::string* ) const;

  // This function is used inside of boolean context , not for converting
  // the type to boolean
  inline bool   ToBoolean ( bool* ) const;

  // Narrow a real/double value into the integer. If it cannot be narrowed
  // it fails with false
  inline bool   NarrowReal( std::int32_t* ival ) const;

 public:
  Expression():
    start(0),
    end(0),
    ekind(ECOMPLEX)
  { ptr = NULL; }
};

inline bool Expression::IsLiteral() const {
  return IsString() ||
         IsReal()   ||
         IsNull()   ||
         IsBoolean();
}

inline bool Expression::ToBoolean( bool* output ) const {
  if(ekind == EBOOLEAN) {
    *output = bool_value;
    return true;
  } else if(ekind == ECOMPLEX) {
    *output = true;
    return (node->IsList() || node->IsObject());
  } else {
    *output = true;
    return true;
  }
}

inline bool Expression::AsBoolean( bool* output ) const {
  lava_verify( IsLiteral() );
  if(ekind == EBOOLEAN) {
    *output = bool_value;
  } else {
    *output = true;
  }
  return true;
}

inline bool Expression::NarrowReal( std::int32_t* output ) const {
  if(!IsReal()) return false;
  return ::lavascript::NarrowReal<std::int32_t>(real_value,output);
}

inline bool Expression::AsReal( double* output ) const {
  lava_verify( IsLiteral() );
  switch(ekind) {
    case EREAL: *output = real_value; return true;
    case EBOOLEAN: *output = bool_value ? 1.0 : 0.0; return true;
    case ESTRING: return ::lavascript::LexicalCast( str_value->data() , output );
    default: return false;
  }
}

inline void Expression::AsString( Zone* zone , String** output ) const {
  lava_verify( IsLiteral() );
  switch(ekind) {
    case EREAL:
      {
        std::string temp; LexicalCast(real_value,&temp);
        *output = String::New(zone,temp);
      }
      break;
    case EBOOLEAN:
      *output = String::New(zone,bool_value ? "true" : "false"); break;
    case ESTRING:
      *output = str_value; break;
    default:
      *output = String::New(zone,"null"); break;
  }
}

inline void Expression::AsString( std::string* output ) const {
  lava_verify( IsLiteral() );
  switch(ekind) {
    case EREAL: LexicalCast(real_value,output); break;
    case EBOOLEAN: output->assign( bool_value ? "true" : "false" ); break;
    case ESTRING: output->assign( str_value->data() ); break;
    default: output->assign("null"); break;
  }
}

/**
 * Trivial constant fold , strength reduction and boolean expression simplification
 */
class ExpressionOptimizer {
 public:

 public:
  ExpressionOptimizer( Zone* zone , const char* source , std::string* error ):
    ast_factory_(zone),
    zone_(zone),
    source_(source),
    error_(error)
  {}

  ast::Node* Optimize( ast::Node* node ) {
    Expression expr;
    if(!Optimize(node,&expr)) return NULL;
    if(expr.IsLiteral())
      return NewLiteralNode(expr);
    else
      return expr.node;
  }

 private:
  bool Optimize( ast::Literal* , Expression* );
  bool Optimize( ast::Prefix*  , Expression* );
  bool Optimize( ast::List*    , Expression* );
  bool Optimize( ast::Object*  , Expression* );
  bool Optimize( ast::Binary*  , Expression* );
  bool Optimize( ast::Unary*   , Expression* );
  bool Optimize( ast::Ternary* , Expression* );
  bool Optimize( ast::Node*    , Expression* );

 private:
  inline int NumberTypePromotion( int l , int r );
  ast::Literal* NewLiteralNode( const Expression& );
  String* Concat( Zone* , const Expression& , const Expression& );
  void Error( const ast::Node& , const char* , ...);

 private:
  ast::AstFactory ast_factory_;
  Zone* zone_;
  const char* source_;
  std::string* error_;

  LAVA_DISALLOW_COPY_AND_ASSIGN(ExpressionOptimizer);
};

void ExpressionOptimizer::Error( const ast::Node& node , const char* format , ... ) {
  va_list vl;
  va_start(vl,format);
  ReportErrorV(error_,"parser",source_,node.start,node.end,format,vl);
}

inline int ExpressionOptimizer::NumberTypePromotion( int l , int r ) {
  if( l == EREAL || r == EREAL )
    return EREAL;
  else
    lava_die();
  return 0;
}

ast::Literal* ExpressionOptimizer::NewLiteralNode( const Expression& node ) {
  switch(node.ekind) {
    case EREAL:
      return ast_factory_.NewLiteral( node.start , node.end , node.real_value);
    case EBOOLEAN:
      return ast_factory_.NewLiteral( node.start , node.end , node.bool_value);
    case ESTRING:
      return ast_factory_.NewLiteral( node.start , node.end , node.str_value );
    case ENULL:
      return ast_factory_.NewLiteral( node.start , node.end );
    default:
      lava_die();
      return NULL;
  }
}

String* ExpressionOptimizer::Concat( Zone* zone , const Expression& lhs ,
                                                  const Expression& rhs ) {
  lava_verify( lhs.IsLiteral() && rhs.IsLiteral() );
  std::string lhs_str , rhs_str;
  lhs.AsString(&lhs_str);
  rhs.AsString(&rhs_str);
  return String::New(zone,lhs_str + rhs_str);
}

bool ExpressionOptimizer::Optimize( ast::Literal* node , Expression* expr ) {
  expr->start = node->start;
  expr->end   = node->end;
  switch(node->literal_type) {
    case ast::Literal::LIT_REAL:
      expr->real_value = node->real_value; expr->ekind = EREAL;
      break;
    case ast::Literal::LIT_BOOLEAN:
      expr->bool_value = node->bool_value; expr->ekind = EBOOLEAN;
      break;
    case ast::Literal::LIT_STRING:
      expr->str_value = node->str_value; expr->ekind = ESTRING;
      break;
    case ast::Literal::LIT_NULL:
      expr->ekind = ENULL;
      break;
    default:
      lava_die();
      break;
  }
  return true;
}

/** Constant fold for simple function call
 * 1) min
 * 2) max
 * 3) type
 * 4) int
 * 5) real
 * 6) string
 * 7) boolean
 * 8) len
 */
bool ExpressionOptimizer::Optimize( ast::Prefix* node, Expression* expr ) {
  expr->ekind = ECOMPLEX;
  expr->node = node;

  if(node->IsSimpleFuncCall()) {
    const String& name = *(node->var->AsVariable()->name);
    const ast::FuncCall& call = *(node->list->First().fc);

    if((name == "min" && call.args->size() == 2) ||
       (name == "max" && call.args->size() == 2)) {
      Expression a1,a2;
      if(!Optimize(call.args->Index(0),&a1) || !Optimize(call.args->Index(1),&a2))
        return false;

      if(a1.ekind == EREAL && a2.ekind == EREAL) {
        expr->start = node->start; expr->end = node->end;
        expr->ekind = EREAL;

        if(name == "min")
          expr->real_value = std::min(a1.real_value,a2.real_value);
        else
          expr->real_value = std::max(a1.real_value,a2.real_value);
        return true;
      }
    } else if((name == "type" && call.args->size()==1)) {
      String* type = NULL;
      Expression a;
      if(!Optimize(call.args->Index(0),&a)) return false;
      switch(a.ekind) {
        case EREAL:    type = String::New(zone_,"real"   ); break;
        case EBOOLEAN: type = String::New(zone_,"boolean"); break;
        case ENULL:    type = String::New(zone_,"null");    break;
        case ESTRING:  type = String::New(zone_,"string");  break;
        case ECOMPLEX:
          if(a.node->IsList()) { type = String::New(zone_,"list"); break; }
          if(a.node->IsObject()){ type = String::New(zone_,"object"); break;}
          if(a.node->IsFunction()){ type = String::New(zone_,"function"); break; }
          break;
      }
      if(type) {
        expr->ekind = ESTRING;
        expr->start = node->start; expr->end = node->end;
        expr->str_value = type;
        return true;
      }
    } else if((name == "real" && call.args->size() ==1)) {
      Expression a;
      if(!Optimize(call.args->Index(0),&a)) return false;
      if(a.IsLiteral()) {
        expr->start = node->start; expr->end = node->end;
        expr->ekind = EREAL;
        if(!a.AsReal(&(expr->real_value))) {
          Error(*node,"real(): cannot convert argument to real");
          return false;
        }
        return true;
      }
    } else if((name == "boolean" && call.args->size() ==1)) {
      Expression a;
      if(!Optimize(call.args->Index(0),&a)) return false;
      if(a.IsLiteral()) {
        expr->start = node->start; expr->end = node->end;
        expr->ekind = EBOOLEAN;
        if(!a.AsBoolean(&(expr->bool_value))) {
          Error(*node,"boolean(): cannot convert argument to boolean");
          return false;
        }
        return true;
      }
    } else if((name == "string" && call.args->size() == 1)) {
      Expression a;
      if(!Optimize(call.args->Index(0),&a)) return false;
      if(a.IsLiteral()) {
        expr->start = node->start; expr->end = node->end;
        expr->ekind = ESTRING;
        a.AsString(zone_,&(expr->str_value));
        return true;
      }
    } else if((name == "len" && call.args->size() == 1)) {
      Expression a;
      if(!Optimize(call.args->Index(0),&a)) return false;
      switch(a.ekind) {
        case ESTRING:
          expr->start = node->start; expr->end = node->end;
          expr->ekind = EREAL;
          expr->real_value = static_cast<double>(a.str_value->size());
          break;
        case ECOMPLEX:
          if(a.node->IsList() || a.node->IsObject()) {
            expr->start = node->start; expr->end = node->end;
            expr->ekind = EREAL;
            expr->real_value = static_cast<double>(
                a.node->IsList() ? a.node->AsList()->entry->size() :
                                   a.node->AsObject()->entry->size());
          }
          break;
        case EREAL:
        case EBOOLEAN:
          Error(*node,"len(): argument cannot be integer/real/boolean");
          return false;
        default: break;
      }
    }
  }
  return true;
}

bool ExpressionOptimizer::Optimize( ast::List* node , Expression* expr ) {
  const size_t len = node->entry->size();
  for( size_t i = 0 ; i < len ; ++i ) {
    ast::Node* expr = node->entry->Index(i);
    Expression a1;
    if(!Optimize(expr,&a1)) return false;
    if(a1.IsLiteral()) {
      node->entry->Set(i,NewLiteralNode(a1));
    } else {
      node->entry->Set(i,a1.node);
    }
  }
  expr->ekind = ECOMPLEX;
  expr->node = node;
  return true;
}

bool ExpressionOptimizer::Optimize( ast::Object* node , Expression* expr ) {
  const size_t len = node->entry->size();
  for( size_t i = 0 ; i < len ; ++i ) {
    ast::Object::Entry& e = node->entry->Index(i);
    Expression k , v;
    if(!Optimize(e.key,&k) || !Optimize(e.val,&v)) {
      return false;
    }
    if(k.ekind == ESTRING && !((e.key->IsLiteral() && e.key->AsLiteral()->IsString()) ||
                              (e.key->IsVariable()))) {
      e.key = NewLiteralNode(k);
    } else if(k.ekind == ECOMPLEX) {
      e.key = k.node;
    }

    if(v.IsLiteral()) {
      e.val = NewLiteralNode(v);
    } else {
      e.val = v.node;
    }
  }
  expr->ekind = ECOMPLEX;
  expr->node = node;
  return true;
}

bool ExpressionOptimizer::Optimize( ast::Unary* node , Expression* expr ) {
  Expression a;
  expr->ekind = ECOMPLEX;
  expr->node = node;

  if(!Optimize(node->opr,&a)) return false;

  if(a.IsLiteral()) {
    expr->start = node->start; expr->end = node->end;
    switch(a.ekind) {
      case EREAL:
        if(node->op == Token::kSub) {
          expr->ekind = EREAL;
          expr->real_value = -a.real_value;
        } else {
          expr->ekind = EBOOLEAN;
          expr->bool_value= false;
        }
        break;
      case EBOOLEAN:
        if(node->op == Token::kSub) {
          Error(*node,"Cannot apply \"-\" as unary operator in front of boolean");
          return false;
        } else {
          expr->ekind = EBOOLEAN;
          expr->bool_value= !a.bool_value;
        }
        break;
      case ESTRING:
        if(node->op == Token::kSub) {
          Error(*node,"Cannot apply \"-\" as unary operator in front of string");
          return false;
        } else {
          expr->ekind = EBOOLEAN;
          expr->bool_value = false;
        }
        break;
      default:
        if(node->op == Token::kSub) {
          Error(*node,"Cannot apply \"-\" as unary operator in front of null");
          return false;
        } else {
          expr->ekind = EBOOLEAN;
          expr->bool_value = false;
        }
        break;
    }
  } else {
    node->opr = a.node;
  }
  return true;
}

bool ExpressionOptimizer::Optimize( ast::Binary* node , Expression* expr ) {
  expr->ekind = ECOMPLEX;
  expr->node = node;

  if(node->op.IsArithmetic() || node->op.IsComparison()) {
    Expression lhs , rhs;
    if(!Optimize(node->lhs,&lhs) || !Optimize(node->rhs,&rhs)) return false;

    node->lhs = lhs.IsComplex() ? lhs.node : NewLiteralNode(lhs);
    node->rhs = rhs.IsComplex() ? rhs.node : NewLiteralNode(rhs);

    if(lhs.IsLiteral() && rhs.IsLiteral()) {
      // numeric operations , arithmetic operation only appy on numeric
      // operations and we don't do implicit conversion here. So no
      // boolean --> integer/real , just numbers here.
      if(lhs.IsReal()) {
        if(!rhs.IsReal()) {
          Error(*node,"Binary operator \"%s\" can only be used between integer/real type",
                node->op.token_name());
          return false;
        }
        double ld = lhs.real_value;
        double rd = rhs.real_value;

        expr->start = node->start; expr->end = node->end;

        if(node->op.IsArithmetic())
          expr->ekind = EREAL;
        else
          expr->ekind = EBOOLEAN;

        switch(node->op) {
          case Token::TK_ADD: expr->real_value = ld + rd; break;
          case Token::TK_SUB: expr->real_value = ld - rd; break;
          case Token::TK_MUL: expr->real_value = ld * rd; break;
          case Token::TK_DIV: expr->real_value = ld / rd; break;
          case Token::TK_MOD:
            {
              // Should match whatever we did in our assembly interpreter
              std::int64_t lhs = static_cast<std::int64_t>(ld);
              std::int64_t rhs = static_cast<std::int64_t>(rd);
              if(rhs == 0) {
                Error(*node,"Binary operator \"%\" mod by 0");
                return false;
              }
              expr->real_value = static_cast<double>(lhs % rhs);
            }
            break;
          case Token::TK_POW: expr->real_value = std::pow(ld,rd); break;
          case Token::TK_LT: expr->bool_value = (ld < rd); break;
          case Token::TK_LE: expr->bool_value = (ld <=rd); break;
          case Token::TK_GT: expr->bool_value = (ld > rd); break;
          case Token::TK_GE: expr->bool_value = (ld >=rd); break;
          case Token::TK_EQ: expr->bool_value = (ld ==rd); break;
          case Token::TK_NE: expr->bool_value = (ld !=rd); break;
          default: lava_die(); break;
        }
      } else if(lhs.IsString() && rhs.IsString()) {
        if(node->op.IsArithmetic()) {
          Error(*node,"Arithmetic operator cannot be used between string");
          return false;
        }

        /** Only can be applied to comparison operator */
        expr->start = node->start; expr->end = node->end;
        expr->ekind = EBOOLEAN;
        switch(node->op) {
          case Token::TK_LT: expr->bool_value = (*lhs.str_value < *rhs.str_value); break;
          case Token::TK_LE: expr->bool_value = (*lhs.str_value <=*rhs.str_value); break;
          case Token::TK_GT: expr->bool_value = (*lhs.str_value > *rhs.str_value); break;
          case Token::TK_GE: expr->bool_value = (*lhs.str_value >=*rhs.str_value); break;
          case Token::TK_EQ: expr->bool_value = (*lhs.str_value ==*rhs.str_value); break;
          case Token::TK_NE: expr->bool_value = (*lhs.str_value !=*rhs.str_value); break;
          default: lava_die(); break;
        }
      } else if(lhs.IsNull() || rhs.IsNull()) {
        if(node->op.IsArithmetic()) {
          Error(*node,"Arithmetic operator cannot be used with null");
          return false;
        }
        expr->start = node->start; expr->end = node->end;
        expr->ekind = EBOOLEAN;

        switch(node->op) {
          case Token::TK_LT: case Token::TK_LE: case Token::TK_GT: case Token::TK_GE:
            Error(*node,"Comparison operator \"<\",\"<=\",\">\",\">=\" cannot be used with nll");
            return false;
          case Token::TK_EQ:
            expr->bool_value = (lhs.IsNull() && rhs.IsNull()); break;
          default:
            expr->bool_value = (lhs.IsNull() ^ rhs.IsNull()); break;
        }
      }
    }
  } else if( node->op.IsConcat() ) {
    Expression lhs,rhs;
    if(!Optimize(node->lhs,&lhs) || !Optimize(node->rhs,&rhs))
      return false;
    if(lhs.IsLiteral() && rhs.IsLiteral()) {
      expr->start = node->start; expr->end = node->end;
      expr->ekind = ESTRING;
      expr->str_value = Concat(zone_,lhs,rhs);
    } else {
      node->lhs = lhs.IsComplex() ? lhs.node : NewLiteralNode(lhs);
      node->rhs = rhs.IsComplex() ? rhs.node : NewLiteralNode(rhs);
    }
  } else if( node->op == Token::kAnd || node->op == Token::kOr ) {
    Expression lhs,rhs;
    if(!Optimize(node->lhs,&lhs)) return false;
    expr->start = node->start; expr->end = node->end;
    bool bval;
    if(lhs.ToBoolean(&bval)) {
      if(bval && node->op == Token::kOr) {
        *expr = lhs; return true;
      } else if(!bval && node->op == Token::kAnd) {
        *expr = lhs; return true;
      } else {
        if(!Optimize(node->rhs,&rhs)) return false;
        *expr = rhs;
      }
    } else {
      if(!Optimize(node->rhs,&rhs)) return false;
      node->lhs = lhs.IsComplex() ? lhs.node : NewLiteralNode(lhs);
      node->rhs = rhs.IsComplex() ? rhs.node : NewLiteralNode(rhs);
    }
  }

  return true;
}

bool ExpressionOptimizer::Optimize( ast::Ternary* node , Expression* expr ) {
  Expression cond;
  if(!Optimize(node->_1st,&cond)) return false;
  bool bval;
  if(cond.ToBoolean(&bval)) {
    return bval ? Optimize(node->_2nd,expr) :
                  Optimize(node->_3rd,expr);
  } else {
    Expression _2nd , _3rd;

    if(!Optimize(node->_2nd,&_2nd) || !Optimize(node->_3rd,&_3rd))
      return false;

    node->_1st = cond.IsComplex() ? cond.node : NewLiteralNode(cond);
    node->_2nd = _2nd.IsComplex() ? _2nd.node : NewLiteralNode(_2nd);
    node->_3rd = _3rd.IsComplex() ? _3rd.node : NewLiteralNode(_3rd);

    expr->ekind = ECOMPLEX;
    expr->node = node;
  }
  return true;
}

bool ExpressionOptimizer::Optimize( ast::Node* node , Expression* expr ) {
  switch( node->type ) {
    case ast::LITERAL: return Optimize(node->AsLiteral(),expr);
    case ast::PREFIX: return Optimize(node->AsPrefix(),expr);
    case ast::UNARY: return Optimize(node->AsUnary(),expr);
    case ast::BINARY: return Optimize(node->AsBinary(),expr);
    case ast::TERNARY: return Optimize(node->AsTernary(),expr);
    case ast::LIST: return Optimize(node->AsList(),expr);
    case ast::OBJECT: return Optimize(node->AsObject(),expr);
    case ast::VARIABLE:
    case ast::FUNCTION:
      expr->node = node; expr->ekind = ECOMPLEX; return true;
    default: lava_die(); return false;
  }
}

} // namespace

ast::Node* Optimize( Zone* zone , const char* source , ast::Node* node,
                                                       std::string* error ) {
  ExpressionOptimizer optimizer(zone,source,error);
  return optimizer.Optimize(node);
}

} // namespace parser
} // namespace lavascript
