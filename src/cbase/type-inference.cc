#include "type-inference.h"
#include "hir.h"

#include "src/interpreter/intrinsic-call.h"

namespace lavascript {
namespace cbase      {
namespace hir        {
using namespace ::lavascript::interpreter;

namespace {

TypeKind GetICallType( ICall* icall ) {
  /**
   * TODO:: change these type mappings as part of builtins x macro
   */
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

// TODO:: This is buggy since Phi node can have cycle due to loop induction variable
TypeKind GetPhiType( Phi* node ) {
  if(node->operand_list()->size() == 0)
    return TPKIND_UNKNOWN;
  else {
    auto tk = GetTypeInference(node->operand_list()->Index(0));
    if(tk == TPKIND_UNKNOWN)
      return TPKIND_UNKNOWN;
    auto itr = node->operand_list()->GetForwardIterator();
    // skip the first element since we peel it out
    itr.Move();

    lava_foreach( auto &k , itr ) {
      // avoid cycle since loop induction variable has cycle
      if(k->IsIdentical(node)) return TPKIND_UNKNOWN;
      auto t = GetTypeInference(k);
      if(t != tk) return TPKIND_UNKNOWN;
    }
    return tk;
  }
}

} // namespace


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
    // phi
    case HIR_PHI:                return GetPhiType(node->AsPhi());
    // guard
    case HIR_GUARD:
      {
        auto guard = node->AsGuard();
        auto test  = guard->test  ();
        return test->IsTestType() ? test->AsTestType()->type_kind() : TPKIND_UNKNOWN;
      }
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
    case HIR_ICALL:              return GetICallType(node->AsICall());
    // closure
    case HIR_CLOSURE:            return TPKIND_CLOSURE;
    default:                     return TPKIND_UNKNOWN;
  }
}

} // namespace hir
} // namespace cbase
} // namespace lavascript
