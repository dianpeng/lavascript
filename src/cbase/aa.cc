#include "aa.h"

namespace lavascript {
namespace cbase      {
namespace hir        {

int AA::Query( Expr* lhs , Expr* rhs ) {
  if(lhs->IsIdentical(rhs)) return AA_MUST;
  {
    FieldRefNode lnode(lhs) , rnode(rhs);
    if((lnode.IsListRef  () &&  rnode.IsListRef()) ||
        (lnode.IsObjectRef() && !rnode.IsObjectRef()))
      return AA_NOT; // not same reference

    if(lnode.object()->Equal(rnode.object()) &&
        lnode.comp  ()->Equal(rnode.comp  ()))
      return AA_MUST;
  }
  return AA_MAY;
}

int AA::Query( Expr* object , EffectBarrier* effect , TypeKind type_hint ) {
  if(effect->Is<ListResize>()) {
    if(type_hint == TPKIND_OBJECT) return AA_NOT; // type mismatched
    if(type_hint == TPKIND_LIST  ) {
      auto resizer = effect->As<ListResize>();
      auto obj     = resizer->object();
      if(obj->Equal(object)) {
        return AA_MUST;
      }
    }
  } else if(effect->Is<ObjectResize>()) {
    if(type_hint == TPKIND_LIST  ) return AA_NOT;
    if(type_hint == TPKIND_OBJECT) {
      auto resizer = effect->As<ObjectResize>();
      auto obj     = resizer->object();
      if(obj->Equal(object)) {
        return AA_MUST;
      }
    }
  }

  return AA_MAY;
}

int AA::QueryObject( Expr* object , EffectBarrier* effect ) {
  return Query(object,effect,TPKIND_OBJECT);
}

int AA::QueryList  ( Expr* list   , EffectBarrier* effect ) {
  return Query(list,effect,TPKIND_LIST);
}

} // namespace hir
} // namespace cbase
} // namespace lavascript
