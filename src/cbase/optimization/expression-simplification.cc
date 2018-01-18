#include "expression-simplification.h"
#include "src/cbase/hir-visitor.h"

#include <cmath>

namespace lavascript {
namespace cbase {
namespace hir {

namespace {

Expr* Fold( Graph* graph , Unary::Operator op , Expr* expr ,
                                                const std::function<IRInfo*()>& irinfo ) {
  if(op == Unary::MINUS && expr->IsFloat64()) {
    return Float64::New(graph,-expr->AsFloat64()->value(),irinfo());
  } else if(op == Unary::NOT) {
    switch(expr->type()) {
      case IRTYPE_FLOAT64:
      case IRTYPE_SMALL_STRING:
      case IRTYPE_LONG_STRING:
      case IRTYPE_LIST:
      case IRTYPE_OBJECT:
        return Boolean::New(graph,false,irinfo());
      case IRTYPE_BOOLEAN:
        return Boolean::New(graph,!expr->AsBoolean()->value(),irinfo());
      case IRTYPE_NIL:
        return Boolean::New(graph,true,irinfo());
      default:
        break;
    }
  }
  return NULL;
}

Expr* Fold( Graph* graph , Binary::Operator op , Expr* lhs ,
                                                 Expr* rhs ,
                                                 const std::function<IRInfo* ()>& irinfo ) {
  if(lhs->IsFloat64() && rhs->IsFloat64()) {
    auto lval = lhs->AsFloat64()->value();
    auto rval = rhs->AsFloat64()->value();

    switch(op) {
      case Binary::ADD: return Float64::New(graph,lval+rval,irinfo());
      case Binary::SUB: return Float64::New(graph,lval-rval,irinfo());
      case Binary::MUL: return Float64::New(graph,lval*rval,irinfo());
      case Binary::DIV: return Float64::New(graph,lval/rval,irinfo());
      case Binary::MOD:
        {
          auto lint = static_cast<std::int64_t>(lval);
          auto rint = static_cast<std::int64_t>(rval);
          return rint == 0 ? NULL : Float64::New(graph,lint % rint, irinfo());
        }
      case Binary::POW: return Float64::New(graph,std::pow(lval,rval),irinfo());
      case Binary::LT:  return Boolean::New(graph,lval <  rval,irinfo());
      case Binary::LE:  return Boolean::New(graph,lval <= rval,irinfo());
      case Binary::GT:  return Boolean::New(graph,lval >  rval,irinfo());
      case Binary::GE:  return Boolean::New(graph,lval >= rval,irinfo());
      case Binary::EQ:  return Boolean::New(graph,lval == rval,irinfo());
      case Binary::NE:  return Boolean::New(graph,lval != rval,irinfo());
      case Binary::AND: return Float64::New(graph,rval,irinfo());
      case Binary::OR:  return Float64::New(graph,lval,irinfo());
      default: lava_die(); return NULL;
    }
  } else if(lhs->IsString() && rhs->IsString()) {
    const zone::String* lstr = lhs->IsSString() ? lhs->AsSString()->value() :
                                                  lhs->AsLString()->value() ;

    const zone::String* rstr = rhs->IsSString() ? rhs->AsSString()->value() :
                                                  rhs->AsLString()->value() ;

    switch(op) {
      case Binary::LT: return Boolean::New(graph,*lstr <  *rstr, irinfo());
      case Binary::LE: return Boolean::New(graph,*lstr <= *rstr, irinfo());
      case Binary::GT: return Boolean::New(graph,*lstr >  *rstr, irinfo());
      case Binary::GE: return Boolean::New(graph,*lstr >= *rstr, irinfo());
      case Binary::EQ: return Boolean::New(graph,*lstr == *rstr, irinfo());
      case Binary::NE: return Boolean::New(graph,*lstr != *rstr, irinfo());
      default: return NULL;
    }
  } else if(lhs->IsNil() || rhs->IsNil()) {
    if(op == Binary::NE) {
      return Boolean::New(graph,lhs->IsNil() ^  rhs->IsNil(),irinfo());
    } else if(op == Binary::EQ) {
      return Boolean::New(graph,lhs->IsNil() && rhs->IsNil(),irinfo());
    } else {
      return NULL;
    }
  }

  return NULL;
}

Expr* Fold( Graph* graph , Expr* cond , Expr* lhs , Expr* rhs ,
                                                    const std::function<IRInfo*()>& irinfo) {
  switch(cond->type()) {
    case IRTYPE_FLOAT64:
    case IRTYPE_LONG_STRING:
    case IRTYPE_SMALL_STRING:
    case IRTYPE_LIST:
    case IRTYPE_OBJECT:
      return lhs;
    case IRTYPE_NIL:
      return rhs;
    case IRTYPE_BOOLEAN:
      return (cond->AsBoolean()->value() ? lhs : rhs);
    default:
      break;
  }
  return NULL;
}

// -----------------------------------------------------------------------
// Expression visitor for doing expression simplification
//
class Simplifier : public ExprVisitor {
 public:
  // normal arithmetic operations
  virtual bool VisitUnary ( Unary* );
  virtual bool VisitBinary( Binary* );
  virtual bool VisitTernary( Ternary* );

 private:
  Graph* graph_;
};

bool Simplifier::VisitUnary ( Unary* node ) {
  auto opr = node->operand();
  auto result = Fold(graph_,node->op(),opr,[node](){ return node->ir_info(); });
  if(result) node->Replace(result);
  return true;
}

bool Simplifier::VisitBinary( Binary* node ) {
  auto lhs = node->lhs();
  auto rhs = node->rhs();
  auto result = Fold(graph_,node->op(),lhs,rhs,[node]() { return node->ir_info(); });
  if(result) node->Replace(result);
  return true;
}

bool Simplifier::VisitTernary( Ternary* node ) {
  auto cond = node->condition();
  auto lhs  = node->lhs();
  auto rhs  = node->rhs();
  auto result = Fold(graph_,cond,lhs,rhs,[node]() { return node->ir_info(); });
  if(result) node->Replace(result);
  return true;
}

} // namespace

Expr* SimplifyUnary  ( Graph* graph , Unary::Operator op , Expr* expr ,
                                                           const std::function<IRInfo*()>& irinfo ) {
  return Fold(graph,op,expr,irinfo);
}

Expr* SimplifyBinary ( Graph* graph , Binary::Operator op , Expr* lhs ,
                                                            Expr* rhs ,
                                                            const std::function<IRInfo* ()>& irinfo ) {
  return Fold(graph,op,lhs,rhs,irinfo);
}

Expr* SimplifyTernary( Graph* graph , Expr* cond , Expr* lhs , Expr* rhs ,
                                                               const std::function<IRInfo*()>& irinfo) {
  return Fold(graph,cond,lhs,rhs,irinfo);
}

} // namespace hir
} // namespace cbase
} // namespace lavascript
