#include "expression-simplification.h"
#include "src/bits.h"
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

  // iget/iset && pget/pset
  virtual bool VisitISet( ISet* );
  virtual bool VisitPSet( PSet* );

  // intrinsic call function folding
  virtual bool VisitICall ( ICall* );

 private:
  Expr* FoldPSet( PSet* , const zone::ZoneString& );

  bool AsUInt32( Expr* , std::uint32_t* );
  bool AsReal  ( Expr* , double* );

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

Expr* Simplifier::FoldPSet( PSet* pset , const zone::ZoneString& key ) {
  auto key = pset->key();
  if(key->IsString() && (key->AsZoneString() == key))
    return pset->value();
  return NULL;
}

bool Simplifier::VisitIGet( IGet* node ) {
  /**
   * it tries to fold situations of following combination
   *
   * a[0]  = 10;  // a is a unknown type , not a literal
   * return a[0]; // 10 can be forwarded, though we don't know a's type
   */
  auto obj = node->object();
  auto idx = node->index ();

  if(obj->IsISet() && idx->IsFloat64()) {
    auto iidx = static_cast<std::int32_t>(idx->AsFloat64()->value());
    auto iset = obj->AsISet();
    if(iset->index()->IsFloat64()) {
      auto iset_idx = iset->index()->AsFloat64();
      auto iset_iidx= static_cast<std::int32_t>(iset_idx->value());

      if(iset_idx = iset_iidx) {
        node->Replace(iset->value());
      }
    }
  } else if(obj->IsPSet() && idx->IsString()) {
    auto r = FoldPSet(obj->AsPSet(),idx->AsZoneString());
    if(r) node->Replace(r);
  }
  return true;
}

bool Simplifier::VisitPGet( PGet* node ) {
  auto obj = node->object();
  auto idx = node->key();

  if(obj->IsPSet() && idx->IsString()) {
    auto r = FoldPSet(obj->AsPSet(),idx->AsZoneString());
    if(r) node->Replace(r);
  }
  return true;
}

bool Simplifier::VisitICall( ICall* node ) {
  switch(node->ic()) {
    case INTRINSIC_CALL_MAX:
      {
        double a1 ,a2;
        if(AsReal(node->operand_list()->Index(0),&a1) &&
           AsReal(node->opreand_list()->Index(1),&a2)) {
          node->Replace(Float64::New(graph_,std::max(a1,a2),node->ir_info()));
        }
      }
      break;

    case INTRINSIC_CALL_MIN:
      {
        double a1,a2;
        if(AsReal(node->operand_list()->Index(0),&a1) &&
           AsReal(node->operand_list()->Index(1),&a2)) {
          node->Replace(Float64::New(graph_,std::min(a1,a2),node->ir_info()));
        }
      }
      break;

    case INTRINSIC_CALL_SQRT:
      {
        double a1;
        if(AsReal(node->operand_list()->Index(0),&a1)) {
          node->Replace(Float64::New(graph_,std::sqrt(a1),node->ir_info()));
        }
      }
      break;

    case INTRINSIC_CALL_SIN:
      {
        double a1;
        if(AsReal(node->operand_list()->Index(0),&a1)) {
          node->Replace(Float64::New(graph_,std::sin(a1),node->ir_info()));
        }
      }
      break;

    case INTRINSIC_CALL_COS:
      {
        double a1;
        if(AsReal(node->operand_list()->Index(0),&a1)) {
          node->Replace(Float64::New(graph_,std::cos(a1),node->ir_info()));
        }
      }
      break;

    case INTRINSIC_CALL_TAN:
      {
        double a1;
        if(AsReal(node->operand_list()->Index(0),&a1)) {
          node->Replace(Float64::New(graph_,std::tan(a1),node->ir_info()));
        }
      }
      break;

    case INTRINSIC_CALL_ABS:
      {
        double a1;
        if(AsReal(node->operand_list()->Index(0),&a1)) {
          node->Replace(Float64::New(graph_,std::abs(a1),node->ir_info()));
        }
      }
      break;

    case INTRINSIC_CALL_CEIL:
      {
        double a1;
        if(AsReal(node->operand_list()->Index(0),&a1)) {
          node->Replace(Float64::New(graph_,std::ceil(a1),node->ir_info()));
        }
      }
      break;

    case INTRINSIC_CALL_FLOOR:
      {
        double a1;
        if(AsReal(node->operand_list()->Index(0),&a1)) {
          node->Replace(Float64::New(graph_,std::floor(a1),node->ir_info()));
        }
      }
      break;

    case INTRINSIC_CALL_LSHIFT:
      {
        std::uint32_t a1;
        std::uint8_t  a2;
        if(AsUInt32(node->operand_list()->Index(0),&a1) &&
           AsUInt8(node->operand_list()->Index(1),&a2)) {
          node->Replace(Float64::New(graph_,static_cast<double>(a1 << a2),node->ir_info()));
        }
      }
      break;

    case INTRINSIC_CALL_RSHIFT:
      {
        std::uint32_t a1;
        std::uint8_t  a2;
        if(AsUInt32(node->operand_list()->Index(0),&a1) &&
           AsUInt8 (node->operand_list()->Index(1),&a2)) {
          node->Replace(Float64::New(graph_,static_cast<double>(a1 >> a2), node->ir_info()));
        }
      }
      break;

    case INTRINSIC_CALL_LRO:
      {
        std::uint32_t a1;
        std::uint8_t  a2;
        if(AsUInt32(node->operand_list()->Index(0),&a1) &&
           AsUInt8 (node->operand_list()->Index(1),&a2)) {
          node->Replace(Float64::New(graph_,static_cast<double>(bits::BRol(a1,a2)),node->ir_info()));
        }
      }
      break;



  }
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
