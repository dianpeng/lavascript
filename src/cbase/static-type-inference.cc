#include "static-type-inference.h"
#include "hir.h"

#include "src/interpreter/intrinsic-call.h"

namespace lavascript {
namespace cbase      {
namespace hir        {

using namespace ::lavascript::interpreter;

TypeKind GetStaticTypeInference( Expr* node ) {

  switch(node->type()) {
    // normal high ir node which has implicit type
    case IRTYPE_FLOAT64:                     return TPKIND_FLOAT64;
    case IRTYPE_LONG_STRING:                 return TPKIND_LONG_STRING;
    case IRTYPE_SMALL_STRING:                return TPKIND_SMALL_STRING;
    case IRTYPE_BOOLEAN:                     return TPKIND_BOOLEAN;
    case IRTYPE_NIL:                         return TPKIND_NIL;
    case IRTYPE_LIST:                        return TPKIND_LIST;
    case IRTYPE_OBJECT:                      return TPKIND_OBJECT;
    case IRTYPE_ITR_NEW:                     return TPKIND_ITERATOR;
    case IRTYPE_ITR_TEST:                    return TPKIND_BOOLEAN ;

    // type mark
    case IRTYPE_TYPE_GUARD:                  return node->AsTypeGuard()->type();

    // unbox node
    case IRTPYE_UNBOX:                       return node->AsUnbox()->type();

    // lower HIR type translation
    case IRTYPE_FLOAT64_NEGATE:              return TPKIND_FLOAT64;
    case IRTYPE_FLOAT64_ARITHMETIC:          return TPKIND_FLOAT64;
    case IRTYPE_FLOAT64_COMPARE:             return TPKIND_FLOAT64;
    case IRTYPE_STRING_COMPARE:              return TPKIND_BOOLEAN;
    case IRTYPE_SSTRING_EQ:                  return TPKIND_BOOLEAN;
    case IRTYPE_SSTRING_NE:                  return TPKIND_BOOLEAN;


    case IRTPYE_ICALL:
    {
      auto icall = node->AsICall();
      switch(icall->ic()) {

#define IMPL(NAME,TYPE) case INTRINSIC_CALL_##NAME: return TYPE;

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

        default: return TPKIND_UNKNOWN;
      }
    }
    break;

    // all rest fallback to be unknown type
    default:                                 return TPKIND_UNKNOWN;
  }
}

} // namespace hir
} // namespace cbase
} // namespace lavascript
