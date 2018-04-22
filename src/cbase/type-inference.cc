#include "type-inference.h"
#include "hir.h"

#include "src/interpreter/intrinsic-call.h"

namespace lavascript {
namespace cbase      {
namespace hir        {

using namespace ::lavascript::interpreter;

TypeKind GetTypeInference( Expr* node ) {

  switch(node->type()) {
    // normal high ir node which has implicit type
    case HIR_FLOAT64:            return TPKIND_FLOAT64;
    case HIR_LONG_STRING:        return TPKIND_LONG_STRING;
    case HIR_SMALL_STRING:       return TPKIND_SMALL_STRING;
    case HIR_BOOLEAN:            return TPKIND_BOOLEAN;
    case HIR_NIL:                return TPKIND_NIL;
    case HIR_LIST:               return TPKIND_LIST;
    case HIR_OBJECT:             return TPKIND_OBJECT;
    case HIR_ITR_NEW:            return TPKIND_ITERATOR;
    case HIR_ITR_TEST:           return TPKIND_BOOLEAN ;
    // type mark
    case HIR_TYPE_ANNOTATION:    return node->AsTypeAnnotation()->type_kind();
    // box/unbox node
    case HIR_UNBOX:              return node->AsUnbox()->type_kind();
    case HIR_BOX:                return node->AsBox()->type_kind();
    // lower HIR type translation
    case HIR_FLOAT64_NEGATE:     return TPKIND_FLOAT64;
    case HIR_FLOAT64_ARITHMETIC: return TPKIND_FLOAT64;
    case HIR_FLOAT64_COMPARE:    return TPKIND_FLOAT64;
    case HIR_STRING_COMPARE:     return TPKIND_BOOLEAN;
    case HIR_SSTRING_EQ:         return TPKIND_BOOLEAN;
    case HIR_SSTRING_NE:         return TPKIND_BOOLEAN;
    case HIR_BOOLEAN_LOGIC:      return TPKIND_BOOLEAN;
    case HIR_BOOLEAN_NOT:        return TPKIND_BOOLEAN;
    // intrinsic call's return value mapping
    // TODO:: Add this into a X macro to make our life easier ?
    case HIR_ICALL:
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
