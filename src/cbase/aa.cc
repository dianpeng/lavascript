#include "aa.h"

namespace lavascript {
namespace cbase      {
namespace hir        {

int AA::Query( const FieldRefNode& lnode , const FieldRefNode& rnode ) {
  if(lnode.node()->IsIdentical(rnode.node())) return AA_MUST;
  {
    if((lnode.IsListRef  () && !rnode.IsListRef()) ||
       (lnode.IsObjectRef() && !rnode.IsObjectRef()))
      return AA_NOT; // not same reference

    if(lnode.object()->Equal(rnode.object())) {
      if(lnode.comp()->Equal(rnode.comp())) return AA_MUST;

      if((lnode.comp()->IsFloat64() && rnode.comp()->IsFloat64()) ||
         (lnode.comp()->IsString () && rnode.comp()->IsString ())) {
        return AA_NOT;
      }
    } else {
      auto lobj = lnode.object();
      auto robj = rnode.object();

      // 1. literal doesn't alias with each other
      // 2. literal doesn't alias with UGet/Arg node

      if(lobj->Is<IRList>() || lobj->Is<IRObject>()) {
        if(robj->Is<Arg>() || robj->Is<UGet>())
          return AA_NOT;
        else if(robj->Is<IRList>() || robj->Is<IRObject>())
          return AA_NOT;
      }

      if(robj->Is<IRList>() || robj->Is<IRObject>()) {
        if(lobj->Is<Arg>() || lobj->Is<UGet>())
          return AA_NOT;
        else if(lobj->Is<IRList>() || lobj->Is<IRObject>())
          return AA_NOT;
      }
    }
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
