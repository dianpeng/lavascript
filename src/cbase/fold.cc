#include "fold.h"
#include "static-type-inference.h"

#include "src/bits.h"

#include <cmath>

namespace lavascript {
namespace cbase {
namespace hir {

namespace {

using namespace ::lavascript::interpreter;

inline bool IsUnaryMinus( Expr* node ) {
  return node->IsUnary() && node->AsUnary()->op() == Unary::MINUS;
}

inline bool IsUnaryNot  ( Expr* node ) {
  return node->IsUnary() && node->AsUnary()->op() == Unary::NOT;
}

inline bool IsTrue( Expr* node ) {
  return node->IsBoolean() && node->AsBoolean()->value();
}

inline bool IsFalse( Expr* node ) {
  return node->IsBoolean() && !node->AsBoolean()->value();
}

inline bool AsUInt8( Expr* node , std::uint8_t* value ) {
  if(node->IsFloat64()) {
    // we don't care about the shifting overflow, the underly ISA
    // only allows a 8bit register serve as how many bits shifted.
    *value = static_cast<std::uint8_t>(node->AsFloat64()->value());
    return true;
  }
  return false;
}

inline bool AsUInt32( Expr* node , std::uint32_t* value ) {
  if(node->IsFloat64()) {
    *value = static_cast<std::uint32_t>(node->AsFloat64()->value());
    return true;
  }
  return false;
}

inline bool AsReal  ( Expr* node , double* real ) {
  if(node->IsFloat64()) {
    *real = node->AsFloat64()->value();
    return true;
  }
  return false;
}

template< typename T >
inline bool IsNumber( Expr* node , T value ) {
  return node->IsFloat64() ? (static_cast<double>(value) == node->AsFloat64()->value()) : false;
}

Expr* Fold( Graph* graph , Unary::Operator op , Expr* expr ,
                                                const std::function<IRInfo*()>& irinfo ) {
  if(op == Unary::MINUS) {
    if(expr->IsFloat64()) {
      return Float64::New(graph,-expr->AsFloat64()->value(),irinfo());
    } else {
      // Handle cases that we have multiple nested negate operator,
      // example like:
      //  --a   ==> a
      //  ---a  ==> -a
      Expr* output = NULL;
      auto temp = expr;
      while(IsUnaryMinus(temp)) {
        output = temp->AsUnary()->operand();
        temp   = output;

        // check whether temp is yet another unary operation with - operator
        // every iteration we solve 2 leve of nested unary operation
        if(IsUnaryMinus(temp)) {
          temp = temp->AsUnary()->operand();
        } else {
          break;
        }
      }

      return output;
    }

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
        {
          // fallback to use static type inference to do folding
          auto t = graph->static_type_inference()->GetType(expr);
          bool bv;
          if(TPKind::ToBoolean(t,&bv)) {
            return Boolean::New(graph,!bv,irinfo());
          }
        }
        break;
    }
  }

  return NULL;
}

// Doing simple algebra reassocication to enable better value inference during GVN and DCE phase
// We can *only* do algebra reassociation when lhs and rhs are both float64 type otherwise we cannot
// do anything due to the fact that the extension type bailout every types of optimization
Expr* Float64Reassociate( Graph* graph , Binary::Operator op , Expr* lhs ,
                                                               Expr* rhs ,
                                                               const std::function<IRInfo* ()>& irinfo ) {
  /**
   * Due to the fact that our operands are both floating point number, not too much
   * operation can be safely done.
   *
   * The following are safe :
   *
   * 1. -a + b    => b - a
   * 2.  a + (-b) => a - b
   * 3. -a - b    => -b- a
   * 4.  a - (-b) => a + b
   * 5.  a / 1    => a
   * 6.  a / -1   => -a
   * 7. -a * -b   => a * b
   */

  if(IsUnaryMinus(lhs) && op == Binary::ADD) {
    // 1. (-a) + b  => b - a
    auto info = irinfo();
    return NewBoxedNodeWithTypeFeedback<Float64Arithmetic>( graph , TPKIND_FLOAT64 ,
                                                                    info,
                                                                    rhs,
                                                                    lhs->AsUnary()->operand(),
                                                                    Binary::SUB,
                                                                    info);
  } else if(IsUnaryMinus(rhs) && op == Binary::ADD) {
    // 2. a + (-b) => a - b
    auto info = irinfo();
    return NewBoxedNodeWithTypeFeedback<Float64Arithmetic>( graph , TPKIND_FLOAT64 ,
                                                                    info,
                                                                    lhs ,
                                                                    rhs->AsUnary()->operand(),
                                                                    Binary::SUB,
                                                                    info);
  } else if(IsUnaryMinus(lhs) && op == Binary::SUB) {
    // 3. -a - b => -b - a
    auto info = irinfo();
    auto new_lhs = NewNodeWithTypeFeedback<Float64Negate>( graph , TPKIND_FLOAT64 , rhs, info);

    return NewBoxedNodeWithTypeFeedback<Float64Arithmetic>( graph , TPKIND_FLOAT64 ,
                                                                    info,
                                                                    new_lhs ,
                                                                    lhs->AsUnary()->operand(),
                                                                    Binary::SUB,
                                                                    info);
  } else if(IsUnaryMinus(rhs) && op == Binary::SUB) {
    // 4. a - (-b) => a + b
    auto info = irinfo();
    return NewBoxedNodeWithTypeFeedback<Float64Arithmetic>( graph , TPKIND_FLOAT64 ,
                                                                    info,
                                                                    lhs,
                                                                    rhs->AsUnary()->operand(),
                                                                    Binary::ADD,
                                                                    info);
  } else if(op == Binary::DIV && IsNumber(rhs,1)) {
    // 5. a / 1 => a
    return lhs;
  } else if(op == Binary::DIV && IsNumber(rhs,-1)) {
    // 6. a / -1 => -a
    auto info = irinfo();
    return NewBoxedNodeWithTypeFeedback<Float64Negate>( graph , TPKIND_FLOAT64, info ,lhs , info );
  } else if(IsUnaryMinus(lhs) && IsUnaryMinus(rhs) && op == Binary::MUL) {
    // 7. -a * -b => a * b
    auto info = irinfo();
    return NewBoxedNodeWithTypeFeedback<Float64Arithmetic>( graph , TPKIND_FLOAT64 ,
                                                                    info,
                                                                    lhs->AsUnary()->operand(),
                                                                    rhs->AsUnary()->operand(),
                                                                    Binary::MUL,
                                                                    info);
  } else {
    return NULL;
  }
}

Expr* SimplifyLogicAnd( Graph* graph , TypeKind lhs_type , TypeKind rhs_type ,
                                                           Expr* lhs ,
                                                           Expr* rhs ,
                                                           const std::function<IRInfo* ()>& irinfo ) {
  (void)lhs_type;
  (void)rhs_type;

  if(IsFalse(lhs)) { return Boolean::New(graph,false,irinfo()); }  // false && any ==> false
  if(IsTrue(lhs))  { return rhs; }                                 // true  && any ==> any

  if(lhs == rhs) return lhs; // a && a ==> a

  if(IsUnaryNot(lhs) && lhs->AsUnary()->operand() == rhs) {
    // !a && a ==> false
    return Boolean::New(graph,false,irinfo());
  }

  if(IsUnaryNot(rhs) && rhs->AsUnary()->operand() == lhs) {
    // a && !a ==> false
    return Boolean::New(graph,false,irinfo());
  }

  return NULL;
}

Expr* SimplifyLogicOr ( Graph* graph , TypeKind lhs_type , TypeKind rhs_type ,
                                                           Expr* lhs,
                                                           Expr* rhs,
                                                           const std::function<IRInfo* ()>& irinfo ) {
  if(IsTrue(lhs))  { return Boolean::New(graph,true,irinfo()); }  // true || any ==> true
  if(IsFalse(lhs)) { return rhs; }                                // false|| any ==> any

  if(lhs == rhs) return lhs; // a || a ==> a

  if(IsUnaryNot(lhs) && lhs->AsUnary()->operand() == rhs) {
    // !a || a ==> true
    return Boolean::New(graph,true,irinfo());
  }

  if(IsUnaryNot(rhs) && rhs->AsUnary()->operand() == lhs) {
    // a || !a ==> true
    return Boolean::New(graph,true,irinfo());
  }

  return NULL;
}

Expr* SimplifyBinary( Graph* graph , Binary::Operator op , Expr* lhs ,
                                                           Expr* rhs ,
                                                           const std::function<IRInfo* ()>& irinfo ) {
  auto typer = graph->static_type_inference();
  auto lhs_type = typer->GetType(lhs);
  auto rhs_type = typer->GetType(rhs);

  if(lhs_type == TPKIND_FLOAT64 && rhs_type == TPKIND_FLOAT64) {
    return Float64Reassociate(graph,op,lhs,rhs,irinfo);
  } else if(op == Binary::AND) {
    return SimplifyLogicAnd(graph,lhs_type,rhs_type,lhs,rhs,irinfo);
  } else if(op == Binary::OR) {
    return SimplifyLogicOr (graph,lhs_type,rhs_type,lhs,rhs,irinfo);
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

  return SimplifyBinary(graph,op,lhs,rhs,irinfo);
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
      {
        // do a static type inference to check which value should return
        bool bv;
        auto t = graph->static_type_inference()->GetType(cond);
        if(TPKind::ToBoolean(t,&bv)) {
          return bv ? lhs : rhs;
        }
      }
      break;
  }
  return NULL;
}

Expr* FoldICall( Graph* graph , ICall* node ) {
  switch(node->ic()) {
    case INTRINSIC_CALL_MAX:
      {
        double a1 ,a2;
        if(AsReal(node->operand_list()->Index(0),&a1) &&
           AsReal(node->operand_list()->Index(1),&a2)) {
          return (Float64::New(graph,std::max(a1,a2),node->ir_info()));
        }
      }
      break;

    case INTRINSIC_CALL_MIN:
      {
        double a1,a2;
        if(AsReal(node->operand_list()->Index(0),&a1) &&
           AsReal(node->operand_list()->Index(1),&a2)) {
          return (Float64::New(graph,std::min(a1,a2),node->ir_info()));
        }
      }
      break;

    case INTRINSIC_CALL_SQRT:
      {
        double a1;
        if(AsReal(node->operand_list()->Index(0),&a1)) {
          return (Float64::New(graph,std::sqrt(a1),node->ir_info()));
        }
      }
      break;

    case INTRINSIC_CALL_SIN:
      {
        double a1;
        if(AsReal(node->operand_list()->Index(0),&a1)) {
          return (Float64::New(graph,std::sin(a1),node->ir_info()));
        }
      }
      break;

    case INTRINSIC_CALL_COS:
      {
        double a1;
        if(AsReal(node->operand_list()->Index(0),&a1)) {
          return (Float64::New(graph,std::cos(a1),node->ir_info()));
        }
      }
      break;

    case INTRINSIC_CALL_TAN:
      {
        double a1;
        if(AsReal(node->operand_list()->Index(0),&a1)) {
          return (Float64::New(graph,std::tan(a1),node->ir_info()));
        }
      }
      break;

    case INTRINSIC_CALL_ABS:
      {
        double a1;
        if(AsReal(node->operand_list()->Index(0),&a1)) {
          return (Float64::New(graph,std::abs(a1),node->ir_info()));
        }
      }
      break;

    case INTRINSIC_CALL_CEIL:
      {
        double a1;
        if(AsReal(node->operand_list()->Index(0),&a1)) {
          return (Float64::New(graph,std::ceil(a1),node->ir_info()));
        }
      }
      break;

    case INTRINSIC_CALL_FLOOR:
      {
        double a1;
        if(AsReal(node->operand_list()->Index(0),&a1)) {
          return (Float64::New(graph,std::floor(a1),node->ir_info()));
        }
      }
      break;

    case INTRINSIC_CALL_LSHIFT:
      {
        std::uint32_t a1;
        std::uint8_t  a2;
        if(AsUInt32(node->operand_list()->Index(0),&a1) &&
           AsUInt8(node->operand_list()->Index(1),&a2)) {
          return (Float64::New(graph,static_cast<double>(a1 << a2),node->ir_info()));
        }
      }
      break;

    case INTRINSIC_CALL_RSHIFT:
      {
        std::uint32_t a1;
        std::uint8_t  a2;
        if(AsUInt32(node->operand_list()->Index(0),&a1) &&
           AsUInt8 (node->operand_list()->Index(1),&a2)) {
          return (Float64::New(graph,static_cast<double>(a1 >> a2), node->ir_info()));
        }
      }
      break;

    case INTRINSIC_CALL_LRO:
      {
        std::uint32_t a1;
        std::uint8_t  a2;
        if(AsUInt32(node->operand_list()->Index(0),&a1) &&
           AsUInt8 (node->operand_list()->Index(1),&a2)) {
          return (Float64::New(graph,static_cast<double>(bits::BRol(a1,a2)),node->ir_info()));
        }
      }
      break;

    case INTRINSIC_CALL_RRO:
      {
        std::uint32_t a1;
        std::uint8_t  a2;
        if(AsUInt32(node->operand_list()->Index(0),&a1) &&
           AsUInt8 (node->operand_list()->Index(1),&a2)) {
          return (Float64::New(graph,static_cast<double>(bits::BRor(a1,a2)),node->ir_info()));
        }
      }
      break;
    case INTRINSIC_CALL_BAND:
      {
        std::uint32_t a1;
        std::uint32_t a2;
        if(AsUInt32(node->operand_list()->Index(0),&a1) &&
           AsUInt32(node->operand_list()->Index(1),&a2)) {
          return (Float64::New(graph,static_cast<double>((a1 & a2)),node->ir_info()));
        }
      }
      break;

    case INTRINSIC_CALL_BOR:
      {
        std::uint32_t a1;
        std::uint32_t a2;
        if(AsUInt32(node->operand_list()->Index(0),&a1) &&
           AsUInt32(node->operand_list()->Index(1),&a2)) {
          return (Float64::New(graph,static_cast<double>(a1 | a2),node->ir_info()));
        }
      }
      break;
    case INTRINSIC_CALL_BXOR:
      {
        std::uint32_t a1;
        std::uint32_t a2;
        if(AsUInt32(node->operand_list()->Index(0),&a1) &&
           AsUInt32(node->operand_list()->Index(1),&a2)) {
          return (Float64::New(graph,static_cast<double>(a1 ^ a2),node->ir_info()));
        }
      }
      break;

    case INTRINSIC_CALL_INT:
      {
        auto n1 = node->operand_list()->Index(0);
        switch(n1->type()) {
          case IRTYPE_FLOAT64:
            return Float64::New(graph,
                                CastRealAndStoreAsReal<std::int32_t>(n1->AsFloat64()->value()),
                                n1->ir_info());

          case IRTYPE_LONG_STRING: case IRTYPE_SMALL_STRING:
            {
              double dv;
              if(LexicalCast(n1->AsZoneString().data(),&dv)) {
                return Float64::New(graph,
                                    CastRealAndStoreAsReal<std::int32_t>(dv),
                                    n1->ir_info());
              }
            }
            break;
          case IRTYPE_BOOLEAN:
            return Float64::New(graph,n1->AsBoolean()->value() ? 1.0 : 0.0,n1->ir_info());

          default:
            break;
        }
      }
      break;
    case INTRINSIC_CALL_REAL:
      {
        auto n1 = node->operand_list()->Index(0);
        switch(n1->type()) {
          case IRTYPE_FLOAT64:
            return Float64::New(graph,n1->AsFloat64()->value(),n1->ir_info());
          case IRTYPE_LONG_STRING: case IRTYPE_SMALL_STRING:
            {
              double val;
              if(LexicalCast(n1->AsZoneString().data(),&val)) {
                return Float64::New(graph,val,n1->ir_info());
              }
            }
            break;
          case IRTYPE_BOOLEAN:
            return Float64::New(graph,n1->AsBoolean()->value() ? 1.0 : 0.0 , n1->ir_info());
          default:
            break;
        }
      }
      break;
    case INTRINSIC_CALL_STRING:
      {
        auto n1 = node->operand_list()->Index(0);
        switch(n1->type()) {
          case IRTYPE_FLOAT64:
            return NewStringFromReal( graph , n1->AsFloat64()->value() , n1->ir_info());
          case IRTYPE_LONG_STRING:
            return LString::New(graph,n1->AsLString()->value(),n1->ir_info());
          case IRTYPE_SMALL_STRING:
            return SString::New(graph,n1->AsSString()->value(),n1->ir_info());
          case IRTYPE_BOOLEAN:
            return NewStringFromBoolean(graph,n1->AsBoolean()->value(),n1->ir_info());
          default:
            break;
        }
      }
      break;

    case INTRINSIC_CALL_PUSH:
      {
        auto n1 = node->operand_list()->Index(0);
        if(n1->IsIRList()) {
          auto new_list = IRList::Clone(graph,*n1->AsIRList());
          new_list->Add( node->operand_list()->Index(1) );
          return new_list;
        }
      }
      break;

    case INTRINSIC_CALL_POP:
      {
        auto n1 = node->operand_list()->Index(0);
        if(n1->IsIRList()) {
          return IRList::CloneExceptLastOne(graph,*n1->AsIRList());
        }
      }
      break;
    default:
      break;
  }

  return NULL; // nothing can be done
}

} // namespace

Expr* FoldUnary  ( Graph* graph , Unary::Operator op , Expr* expr ,
                                                       const std::function<IRInfo*()>& irinfo ) {
  return Fold(graph,op,expr,irinfo);
}

Expr* FoldBinary ( Graph* graph , Binary::Operator op , Expr* lhs ,
                                                        Expr* rhs ,
                                                        const std::function<IRInfo* ()>& irinfo ) {
  return Fold(graph,op,lhs,rhs,irinfo);
}

Expr* FoldTernary( Graph* graph , Expr* cond , Expr* lhs , Expr* rhs ,
                                                           const std::function<IRInfo*()>& irinfo) {
  return Fold(graph,cond,lhs,rhs,irinfo);
}

Expr* FoldIntrinsicCall( Graph* graph , ICall* icall ) {
  return FoldICall(graph,icall);
}

} // namespace hir
} // namespace cbase
} // namespace lavascript
