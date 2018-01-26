#include "static-type-inference.h"
#include "src/interpreter/intrinsic-call.h"

namespace lavascript {
namespace cbase      {
namespace hir        {

using namespace ::lavascript::interpreter;

// Implicit type inference
TypeKind StaticTypeInference::GetImplicitType( Expr* node ) const {
  switch(node->type()) {
    case IRTYPE_FLOAT64:   return TPKIND_FLOAT64;
    case IRTYPE_LONG_STRING:    return TPKIND_LONG_STRING;
    case IRTYPE_SMALL_STRING:   return TPKIND_SMALL_STRING;
    case IRTYPE_BOOLEAN:   return TPKIND_BOOLEAN;
    case IRTYPE_NIL:       return TPKIND_NIL;
    case IRTYPE_LIST:      return TPKIND_LIST;
    case IRTYPE_OBJECT:    return TPKIND_OBJECT;
    case IRTYPE_ITR_NEW:   return TPKIND_ITERATOR;
    case IRTYPE_ITR_TEST:  return TPKIND_BOOLEAN ;
    default:               return TPKIND_UNKNOWN;
  }
}

TypeKind StaticTypeInference::GetType( Expr* node ) const {
  if(node->id() < type_vector_.size()) {
    auto n = type_vector_[node->id()];
    if(n != TPKIND_UNKNOWN) return n;
  }

  auto t = GetImplicitType(node);
  if(node->id() >= type_vector_.size())
    type_vector_.resize(node->id()+1);
  type_vector_[node->id()] = t;

  return t;
}

void StaticTypeInference::AddIntrinsicCallType( ICall* node ) {
  /**
   * the following type inference is based on the predefined knowledge of
   * our intrinsic call and its return value. If intrinsic call changed,
   * these code may not be valid anymore.
   *
   * One option is to put its return value along with the intrinsic call
   * function's definition to make it more readable
   */

  switch(node->ic()) {

#define IMPL(NAME,TYPE) \
  case INTRINSIC_CALL_##NAME: AddType(node->id(),TYPE); break;

    IMPL(MIN ,TPKIND_FLOAT64);
    IMPL(MAX ,TPKIND_FLOAT64);
    IMPL(SQRT,TPKIND_FLOAT64);
    IMPL(SIN ,TPKIND_FLOAT64);
    IMPL(COS ,TPKIND_FLOAT64);
    IMPL(TAN ,TPKIND_FLOAT64);
    IMPL(ABS ,TPKIND_FLOAT64);
    IMPL(CEIL,TPKIND_FLOAT64);
    IMPL(FLOOR,TPKIND_FLOAT64);
    IMPL(LSHIFT,TPKIND_FLOAT64);
    IMPL(RSHIFT,TPKIND_FLOAT64);
    IMPL(LRO ,TPKIND_FLOAT64);
    IMPL(RRO ,TPKIND_FLOAT64);
    IMPL(BAND,TPKIND_FLOAT64);
    IMPL(BOR ,TPKIND_FLOAT64);
    IMPL(BXOR,TPKIND_FLOAT64);
    IMPL(INT ,TPKIND_FLOAT64);
    IMPL(REAL,TPKIND_FLOAT64);
    IMPL(STRING,TPKIND_STRING);
    IMPL(BOOLEAN,TPKIND_BOOLEAN);

    IMPL(POP ,TPKIND_BOOLEAN);
    IMPL(PUSH,TPKIND_BOOLEAN);
    IMPL(SET ,TPKIND_BOOLEAN);
    IMPL(HAS ,TPKIND_BOOLEAN);
    IMPL(UPDATE,TPKIND_BOOLEAN);
    IMPL(PUT ,TPKIND_BOOLEAN);
    IMPL(DELETE,TPKIND_BOOLEAN);

    IMPL(CLEAR, TPKIND_BOOLEAN);
    IMPL(TYPE , TPKIND_STRING );
    IMPL(LEN  , TPKIND_FLOAT64);
    IMPL(EMPTY, TPKIND_BOOLEAN);
    IMPL(ITER , TPKIND_ITERATOR);

#undef IMPL // IMPL

    // We don't need to go deeper for each function to do
    // static type inference since the constant folding
    // happened before we do type inference. All the possible
    // situation has already been foleded correctly
    default: break;
  }
}

TypeKind StaticTypeInference::ResolveUnaryOperatorType ( Expr* node , Unary::Operator op ) {
  (void)op;

  return GetType(node);
}

TypeKind StaticTypeInference::ResolveBinaryOperatorType( Expr* lhs , Expr* rhs , Binary::Operator op ) {
  auto ltype = GetType(lhs);
  auto rtype = GetType(rhs);
  if( ltype == TPKIND_UNKNOWN || rtype == TPKIND_UNKNOWN )
    return TPKIND_UNKNOWN;

  switch(op) {
    case Binary::ADD: case Binary::SUB: case Binary::MUL:
    case Binary::DIV: case Binary::MOD: case Binary::POW:
      if(ltype == TPKIND_FLOAT64 && rtype == TPKIND_FLOAT64)
        return TPKIND_FLOAT64;
      else
        return TPKIND_UNKNOWN;

    case Binary::LT: case Binary::LE: case Binary::GT:
    case Binary::GE: case Binary::EQ: case Binary::NE:
      if((ltype == TPKIND_FLOAT64 && rtype == TPKIND_FLOAT64) ||
         (TPKind::IsString(ltype) && TPKind::IsString(rtype)))
        return TPKIND_BOOLEAN;
      else
        return TPKIND_UNKNOWN;

    case Binary::AND: case Binary::OR:
      return TPKIND_BOOLEAN;
    default:
      lava_die();
      return TPKIND_UNKNOWN;
  }
}

TypeKind StaticTypeInference::ResolveTernaryOperatorType( Expr* cond , Expr* lhs ,
                                                                       Expr* rhs ) {
  auto ltype = GetType(lhs);
  auto rtype = GetType(rhs);
  if(ltype == TPKIND_UNKNOWN || rtype == TPKIND_UNKNOWN || (ltype != rtype))
    return TPKIND_UNKNOWN;

  lava_debug(NORMAL,lava_verify(ltype == rtype););
  return ltype;
}

} // namespace hir
} // namespace cbase
} // namespace lavascript
