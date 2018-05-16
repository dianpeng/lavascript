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

  virtual int Query( Expr* , Expr* , WriteEffect* ) = 0;
  virtual int Query( MemoryRef*    , MemoryRef*   ) = 0;
};

namespace {

class ObjectAA : public MemoryOpt::AA {
 public:
  virtual int Query( Expr* object , Expr* key , WriteEffect* effect );
};

class ListAA : public MemoryOpt::AA {
 public:
  virtual int Query( Expr* object , Expr* index , WriteEffect* effect );
};

int ObjectAA::Query( Expr* object , Expr* key , WriteEffect* effect ) {
  if(effect->Is<ListResize>()) {
    return AA_NOT; // cannot alias , type mismatch
  } else if(effect->Is<EmptyBarrier>() ||
            effect->Is<PSet>()         ||
            effect->Is<USet>()) {
    // obviously no alias possibility
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
      // not aliased with each other , both are literals
      if(obj->IsIRObject() && object->IsIRObject())
        return AA_NOT;
    }
    // may be aliased with each other
    return AA_MAY;
  } else {
    // must alias, prevent from moving backward more
    return AA_MUST;
  }
}

int ListAA::Query( Expr* object , Expr* index , WriteEffect* effect ) {
  if(effect->Is<ObjectResize>()) {
    return AA_NOT;
  } else if(effet->IsEmptyBarrier>() ||
            effect->Is<PSet>      () ||
            effect->Is<USet>      ()) {
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
  object(r->object()),
  key   (r->comp()  ),
  effect(r->write_effect()),
  checkpoint(r->checkpoint()),
  ref   (r),
  ref_type(NULL)
{
  if(ref->Is<ObjectFind>())
    ref_type = kObjectRef;
  else {
    lava_debug(NORMAL,lava_verify(ref->Is<ListInsert>()););
    ref_type = kListRef;
  }
}

MemoryOpt::RefKey::RefKey( Expr* obj , Expr* k , WriteEffect* e , Checkpoint* cp ,
                                                                  const char* rt ):
  object(obj),
  key   (k),
  effect(e),
  checkpoint(cp),
  ref   (NULL),
  ref_type(rt)
{}

bool RefKeyEqual::operator() ( const RefKey& l , const RefKey& r ) const {
  return l.object->Equal(r.object) &&
    l.key->Equal   (r.key)    &&
    l.effect->Equal(r.effect) &&
    l.checkpoint->Equal(r.checkpoint) &&
    l.ref_type == r.ref_type;
}

std::size_t RefKeyHash::operator() ( const RefKey& rk ) const {
  return GVNHash4(rk.ref_type,rk.object->GVNHash(),rk.key->GVNHash(),
      rk.effect->GVNHash(),
      rk.checkpoint->GVNHash());
}

MemoryOpt::RefPos MemoryOpt::FindRef( Expr* object , Expr* key , WriteEffect* effect , Checkpoint* cp ,
                                                                                AA* aa ) {
  do {
    if(auto itr = ref_table_.find(RefKey{object,key,effect,cp}); itr != ref_table_.end())
      return RefPos{*itr,effect};
    switch( aa->Query( object , key , effect ) ) {
      case AA::AA_MAY :
      case AA::AA_MUST:
        return RefPos{};
      default:
        effect = effect->NextLink();
        break;
    }
  } while(true);

  lava_die(); return RefPos{};
}

MemoryOpt::Result MemoryOpt::OptObjectSet( Graph* graph , Expr* object , Expr* key , Expr* value ,
                                                                                     WriteEffect* effect ) {
  AAObject aa;
  // 1. Find a duplicate object reference until we cannot forward
  if(auto dup_ref = FindRef( object, key , effect , &aa); dup_ref.ref ) {
    // 2. Try to dedup/remove the stupid store store.
    //
    // Code like this :
    //
    // a[1] = 10;
    // a[1] = 20;
    //
    // We forward the a[1] = 20 --> a[1] = 20;
  }
  return Result::kDead;
}





