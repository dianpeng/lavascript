#include "fold-arith.h"
#include "type-inference.h"
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
inline bool IsTrue( Expr* node , TypeKind tp ) {
  if(node->IsBoolean() && node->AsBoolean()->value())
    return true;
  else {
    bool bval;
    if(TPKind::ToBoolean(tp,&bval)) {
      return bval;
    }
    return false;
  }
}
inline bool IsFalse( Expr* node , TypeKind tp ) {
  if(node->IsBoolean() && !node->AsBoolean()->value())
    return true;
  else {
    bool bval;
    if(TPKind::ToBoolean(tp,&bval))
      return !bval;
  }
  return false;
}
template< typename T >
inline bool IsNumber( Expr* node , T value ) {
  return node->IsFloat64() ?
         (static_cast<double>(value) == node->AsFloat64()->value()) : false;
}

// Fold the unary operations
Expr* Fold( Graph* graph , Unary::Operator op , Expr* expr , const IRInfoProvider& irinfo ) {
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
      case HIR_FLOAT64:
      case HIR_SMALL_STRING:
      case HIR_LONG_STRING:
      case HIR_LIST:
      case HIR_OBJECT:
        return Boolean::New(graph,false,irinfo());
      case HIR_BOOLEAN:
        return Boolean::New(graph,!expr->AsBoolean()->value(),irinfo());
      case HIR_NIL:
        return Boolean::New(graph,true,irinfo());
      default:
        {
          // fallback to use static type inference to do folding
          auto t = GetTypeInference(expr);
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
                                                               const IRInfoProvider& irinfo ) {
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
    return NewBoxNode<Float64Arithmetic>( graph , TPKIND_FLOAT64 , info,
                                          NewUnboxNode(graph,rhs,TPKIND_FLOAT64,info),
                                          NewUnboxNode(graph,lhs->AsUnary()->operand(),TPKIND_FLOAT64,info),
                                          Binary::SUB, info );
  } else if(IsUnaryMinus(rhs) && op == Binary::ADD) {
    // 2. a + (-b) => a - b
    auto info = irinfo();
    return NewBoxNode<Float64Arithmetic>( graph , TPKIND_FLOAT64 , info,
                                          NewUnboxNode(graph,lhs,TPKIND_FLOAT64,info),
                                          NewUnboxNode(graph,rhs->AsUnary()->operand(),TPKIND_FLOAT64,info),
                                          Binary::SUB, info );
  } else if(IsUnaryMinus(lhs) && op == Binary::SUB) {
    // 3. -a - b => -b - a
    auto info = irinfo();
    auto new_lhs = Float64Negate::New( graph , NewUnboxNode(graph,rhs,TPKIND_FLOAT64,info) , info );
    return NewBoxNode<Float64Arithmetic>( graph , TPKIND_FLOAT64 , info,
                                          NewUnboxNode(graph,new_lhs,TPKIND_FLOAT64,info),
                                          NewUnboxNode(graph,lhs->AsUnary()->operand(),TPKIND_FLOAT64,info),
                                          Binary::SUB, info );
  } else if(IsUnaryMinus(rhs) && op == Binary::SUB) {
    // 4. a - (-b) => a + b
    auto info = irinfo();
    return NewBoxNode<Float64Arithmetic>( graph , TPKIND_FLOAT64 , info,
                                          NewUnboxNode(graph,lhs,TPKIND_FLOAT64,info),
                                          NewUnboxNode(graph,rhs->AsUnary()->operand(),TPKIND_FLOAT64,info),
                                          Binary::ADD, info );
  } else if(op == Binary::DIV && IsNumber(rhs,1)) {
    // 5. a / 1 => a
    return lhs;
  } else if(op == Binary::DIV && IsNumber(rhs,-1)) {
    // 6. a / -1 => -a
    auto info = irinfo();
    return NewBoxNode<Float64Negate>( graph , TPKIND_FLOAT64, info ,
                                      NewUnboxNode(graph,lhs,TPKIND_FLOAT64,info) ,
                                      info );
  } else if(IsUnaryMinus(lhs) && IsUnaryMinus(rhs) && op == Binary::MUL) {
    // 7. -a * -b => a * b
    auto info = irinfo();
    return NewBoxNode<Float64Arithmetic>( graph , TPKIND_FLOAT64 , info,
                                          NewUnboxNode(graph,lhs->AsUnary()->operand(),TPKIND_FLOAT64,info),
                                          NewUnboxNode(graph,rhs->AsUnary()->operand(),TPKIND_FLOAT64,info),
                                          Binary::MUL, info );
  } else {
    return NULL;
  }
}
Expr* SimplifyLogicAnd( Graph* graph , TypeKind lhs_type , TypeKind rhs_type , Expr* lhs ,
                                                                               Expr* rhs ,
                                                                               const IRInfoProvider& irinfo ) {
  (void)lhs_type;
  (void)rhs_type;
  if(IsFalse(lhs,lhs_type)) { return Boolean::New(graph,false,irinfo()); }  // false && any ==> false
  if(IsTrue (lhs,lhs_type)) { return rhs; }                                 // true  && any ==> any
  if(lhs->IsReplaceable(rhs)) return lhs; // a && a ==> a
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
Expr* SimplifyLogicOr ( Graph* graph , TypeKind lhs_type , TypeKind rhs_type , Expr* lhs,
                                                                               Expr* rhs,
                                                                               const IRInfoProvider& irinfo ) {
  if(IsTrue (lhs,lhs_type)) { return Boolean::New(graph,true,irinfo()); }  // true || any ==> true
  if(IsFalse(lhs,lhs_type)) { return rhs; }                                // false|| any ==> any
  if(lhs->IsReplaceable(rhs)) return lhs; // a || a ==> a
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
Expr* SimplifyBooleanCompare( Graph* graph , Binary::Operator op, TypeKind lhs_type ,
                                                                  TypeKind rhs_type ,
                                                                  Expr* lhs,
                                                                  Expr* rhs,
                                                                  const IRInfoProvider& irinfo ) {
  if(lhs_type == TPKIND_BOOLEAN && rhs->IsBoolean()) {
    auto info = irinfo();
    return rhs->AsBoolean()->value() ?  lhs :
      NewBoxNode<BooleanNot>(graph,TPKIND_BOOLEAN,info,NewUnboxNode(graph,lhs,TPKIND_FLOAT64,info),info);
  } else if(rhs_type == TPKIND_BOOLEAN && lhs->IsBoolean()) {
    auto info = irinfo();
    return lhs->AsBoolean()->value() ? rhs :
      NewBoxNode<BooleanNot>(graph,TPKIND_BOOLEAN,info,NewUnboxNode(graph,rhs,TPKIND_FLOAT64,info),info);
  }
  return NULL;
}
Expr* SimplifyBinary( Graph* graph , Binary::Operator op , Expr* lhs , Expr* rhs , const IRInfoProvider& irinfo ) {
  auto lhs_type = GetTypeInference(lhs);
  auto rhs_type = GetTypeInference(rhs);
  if(lhs_type == TPKIND_FLOAT64 && rhs_type == TPKIND_FLOAT64) {
    return Float64Reassociate(graph,op,lhs,rhs,irinfo);
  } else if(op == Binary::AND) {
    return SimplifyLogicAnd(graph,lhs_type,rhs_type,lhs,rhs,irinfo);
  } else if(op == Binary::OR) {
    return SimplifyLogicOr (graph,lhs_type,rhs_type,lhs,rhs,irinfo);
  } else if(((lhs_type == TPKIND_BOOLEAN && rhs->IsBoolean()) ||
             (rhs_type == TPKIND_BOOLEAN && lhs->IsBoolean()))&&
            (op == Binary::EQ || op == Binary::NE)) {
    /**
     * This situation is that the left hand side is a speculative boolean
     * and the right hand side is a bollean literal or vise versa , example :
     *
     * if(a == true) or if(false == b)
     *
     * The above format can be simplifed to be if(a) or if(!b) which genreates
     * same boolean result.
     *
     * One thing to note is this rule doesn't apply on other types because in
     * lavascript `a` in a boolean context evaluate to a itself but `a == true`
     * evaluate to a boolean result.
     *
     * This also helps infer phase since infer looks for something like (a) or
     * (!a) in boolean context and doesn't expect something like (a == true)or
     * (a == false)
     *
     */
    return SimplifyBooleanCompare(graph,op,lhs_type,rhs_type,lhs,rhs,irinfo);
  }
  return NULL;
}

Expr* Fold( Graph* graph , Binary::Operator op , Expr* lhs , Expr* rhs , const IRInfoProvider& irinfo ) {
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
    const zone::String* lstr = lhs->IsSString() ? lhs->AsSString()->value() : lhs->AsLString()->value() ;
    const zone::String* rstr = rhs->IsSString() ? rhs->AsSString()->value() : rhs->AsLString()->value() ;
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

Expr* Fold( Graph* graph , Expr* cond , Expr* lhs , Expr* rhs , const IRInfoProvider& irinfo) {
  switch(cond->type()) {
    case HIR_FLOAT64:
    case HIR_LONG_STRING:
    case HIR_SMALL_STRING:
    case HIR_LIST:
    case HIR_OBJECT:
      return lhs;
    case HIR_NIL:
      return rhs;
    case HIR_BOOLEAN:
      return (cond->AsBoolean()->value() ? lhs : rhs);
    default:
      {
        // do a static type inference to check which value should return
        bool bv;
        auto t = GetTypeInference(cond);
        if(TPKind::ToBoolean(t,&bv)) {
          return bv ? lhs : rhs;
        }
      }
      break;
  }
  // 1. check if lhs and rhs are the same if so check if cond is side effect free
  //    if it is side effect free then just return lhs/rhs
  if(lhs->IsReplaceable(rhs) && !cond->HasSideEffect()) return lhs;
  // 2. check following cases
  // 1) value = cond ? true : false ==> value = cast_to_boolean(cond)
  // 2) value = cond ? false: true  ==> value = cast_to_boolean(cond,negate)
  if( lhs->IsBoolean() && rhs->IsBoolean() ) {
    auto lb = lhs->AsBoolean()->value();
    auto rb = rhs->AsBoolean()->value();
    if(lb) {
      lava_debug(NORMAL,lava_verify(!rb););
      return CastToBoolean::New(graph,cond,irinfo());
    } else {
      lava_debug(NORMAL,lava_verify(rb););
      return CastToBoolean::NewNegateCast(graph,cond,irinfo());
    }
  }

  return NULL;
}

} // namespace

Expr* FoldUnary  ( Graph* graph , Unary::Operator op , Expr* expr , const IRInfoProvider& irinfo ) {
  return Fold(graph,op,expr,irinfo);
}
Expr* FoldBinary ( Graph* graph , Binary::Operator op , Expr* lhs , Expr* rhs ,
                                                                    const IRInfoProvider& irinfo ) {
  return Fold(graph,op,lhs,rhs,irinfo);
}
Expr* FoldTernary( Graph* graph , Expr* cond , Expr* lhs , Expr* rhs , const IRInfoProvider& irinfo) {
  return Fold(graph,cond,lhs,rhs,irinfo);
}

Expr* SimplifyLogic ( Graph* graph , Expr* lhs , Expr* rhs , Binary::Operator op,
                                                             const IRInfoProvider& irinfo ) {
  if(op == Binary::AND) {
    return SimplifyLogicAnd(graph,GetTypeInference(lhs), GetTypeInference(rhs), lhs, rhs, irinfo);
  } else {
    lava_debug(NORMAL,lava_verify(op == Binary::OR););
    return SimplifyLogicOr (graph,GetTypeInference(lhs), GetTypeInference(rhs), lhs, rhs, irinfo);
  }
}

} // namespace hir
} // namespace cbase
} // namespace lavascript
