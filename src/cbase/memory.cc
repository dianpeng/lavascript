#include "memory.h"

namespace lavascript {
namespace cbase      {
namespace hir        {

static const char*       MemoryOpt::kObjectRef = "object-ref";
static const char*       MemoryOpt::kListRef   = "list-ref";
static MemoryOpt::Result MemoryOpt::Result::kDead{DEAD,NULL};
static MemoryOpt::Result MemoryOpt::Result::kFailed{FAILED,NULL};

// interface for performing alias analysis
class MemoryOpt::AA {
 public:
  enum { AA_MUST , AA_MAY , AA_NOT };
  // interface for list and object related alias analyzing
  virtual int Query( Expr* , Expr* , EffectBarrier* ) = 0;
  // do alias analyze against two memory reference
  static  int Query( MemoryRef*    , MemoryRef*   );
};

int MemoryOpt::Query(MemoryRef* lhs, MemoryRef* rhs) {
  if(lhs->type() != rhs->type()) return AA_NOT;
  if(lhs->IsIdentical(rhs))      return AA_MUST;

  auto lref    = lhs->As<MemoryRef>();
  auto rref    = rhs->As<MemoryRef>();
  auto is_list = lhs->Is<ListIndex>();
  if(lref->object()->Equal(rref->object())) {
    if(lref->comp()->Equal(rref->comp())) {
      return AA_MUST;
    } else {
      if(is_list && (lref->comp()->Is<Float64>() && rref->comp()->Is<Float64>())) {

        lava_debug(NORMAL,lava_verify(lref->comp()->As<Float64>()->value() !=
                                      rref->comp()->As<Float64>()->value()););
        return AA_NOT;
      } else if(is_list && (lref->comp()->IsString() && rref->comp()->IsString())) {

        lava_debug(NORMAL,lava_verify(lref->comp()->AsZoneString() !=
                                      rref->comp()->AsZoneString()););
        return AA_NOT;
      }
    }
  } else {
    if(is_list && (lref->object()->IsIRList() && rref->object()->IsIRList())) {
      return AA_NOT;
    } else if(is_list && (lref->object()->IsIRObject() && rref->object()->IsIRObject())) {
      return AA_NOT;
    }
  }
  return AA_MAY;
}

namespace {

class ObjectAA : public MemoryOpt::AA {
 public:
  virtual int Query( Expr* object , Expr* key   , EffectBarrier* effect );
};

class ListAA : public MemoryOpt::AA {
 public:
  virtual int Query( Expr* object , Expr* index , EffectBarrier* effect );
};

int ObjectAA::Query( Expr* object , Expr* key , EffectBarrier* effect ) {
  if(effect->Is<ListResize>()) {
    return AA_NOT;
  } else if(effect->Is<EmptyBarrier>()) {
    return AA_NOT;
  } else if(effect->Is<ObjectResize>()) {
    auto resizer = effect->As<ObjectResize>();
    Expr* obj = resizer->object();
    Expr*   k = resizer->key   ();

    if(obj->Equal(object)) {
      if(k->Equal(key)) {
        return AA_MUST;
      } else if(k->IsString() && key->IsString()) {
        lava_debug(NORMAL,lava_verify(k->AsZoneString() != key->AsZoneString()););
        return AA_NOT;
      }
    } else {
      if(obj->IsIRObject() && object->IsIRObject()) return AA_NOT;
    }
    return AA_MAY;
  } else {
    return AA_MUST;
  }
}

int ListAA::Query( Expr* object , Expr* index , EffectBarrier* effect ) {
  if(effect->Is<ObjectResize>()) {
    return AA_NOT;
  } else if(effet->IsEmptyBarrier>()) {
    return AA_NOT;
  } else if(effect->Is<ListResize>()) {
    auto resizer = effect->As<ListResize>();
    auto obj     = resizer->object();
    auto idx     = resizer->index ();
    if(obj->Equal(object)) {
      if(idx->Equal(index)) {
        return AA_MUST;
      } else if(idx->IsFloat64() && index->IsFloat64()) {
        lava_debug(NORMAL,lava_verify(idx->As<Float64>()->value()!=index->As<Float64>()->value()););
        return AA_NOT;
      }
    } else {
      if(obj->IsIRList() && object->IsIRList())
        return AA_NOT;
    }
    return AA_MAY;
  } else {
    return AA_MUST;
  }
}

} // namespace

MemoryOpt::RefKey::RefKey( MemoryRef* r ):
  object    (r->object()),
  key       (r->comp()  ),
  effect    (r->write_effect()->ClosestBarrier()),
  ref   (r),
  ref_type(NULL)
{
  if(ref->Is<ObjectFind>()) {
    ref_type = kObjectRef;
  } else {
    lava_debug(NORMAL,lava_verify(ref->Is<ListInsert>()););
    ref_type = kListRef;
  }
}

MemoryOpt::RefKey::RefKey( Expr* obj , Expr* k , EffectBarrier* e , const char* rt ):
  object(obj),
  key   (k),
  effect(e),
  ref   (NULL),
  ref_type(rt)
{}

bool RefKeyEqual::operator() ( const RefKey& l , const RefKey& r ) const {
  return l.object->Equal(r.object) && l.key->Equal(r.key)       &&
                                      l.effect->Equal(r.effect) &&
                                      l.ref_type == r.ref_type;
}

std::size_t RefKeyHash::operator() ( const RefKey& rk ) const {
  return GVNHash3(rk.ref_type,rk.object->GVNHash(),rk.key->GVNHash(),rk.effect->GVNHash());
}

MemoryOpt::RefPos MemoryOpt::FindRef( Expr* object , Expr* key , WriteEffect* effect ,
                                                                 Checkpoint*  cp ,
                                                                 AA*          aa ) {
  for( auto e = effect->ClosestBarrier(); e && !e->Is<HardBarrier>() ;e = e->NextBarrier() ) {
    if(auto itr = ref_table_.find(RefKey{object,key,effect}); itr != ref_table_.end())
      return RefPos{*itr,effect};
    switch(aa->Query(object,key,effect)) {
      case AA::AA_MAY :
      case AA::AA_MUST:
        return RefPos{};
      default: break;
    }
  }

  return RefPos{};
}

MemoryOpt::Result MemoryOpt::OptObjectSet( Graph* graph , Expr* object , Expr* key ,
                                                                         Expr* value ,
                                                                         WriteEffect* effect ) {
  AAObject aa;

  if(auto dup_ref = FindRef( object, key , effect , &aa); dup_ref) {

    for( ; effect != dup_ref.effect ; effect = effect->NextEffect() ) {
      // 1. go through all the read effect operation before this write effect node
      lava_foreach( auto k , effect->read_effect()->GetForwardIterator() ) {
        if(k->Is<ObjectRefGet>()) {
          switch(AA::Query(k->As<ObjectRefGet>()->ref(),dup_ref.ref)) {
            case AA::AA_MAY:
            case AA::AA_MUST:
              return Result{FOLD_REF,dup_ref.ref};
            default:
              break;
          }
        }
      }

      // 2. try to do AA against with same type of write , we will never see hardbarrier appear
      if(effect->Is<ObjectRefSet>()) {
        auto ref_set = effect->As<ObjectRefSet>();


      } else {
        lava_debug(NORMAL,lava_verify(!effect->Is<HardBarrier>()););
      }
    }

    return Result{FOLD_REF,dup_ref.ref};
  }

  return Result::kFailed;
}





