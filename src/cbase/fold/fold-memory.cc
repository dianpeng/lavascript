#include "fold-memory.h"
#include "folder.h"
#include "src/util.h"

#include <unordered_set>

namespace lavascript {
namespace cbase      {
namespace hir        {
namespace            {

/**
 * Alias Analysis
 *
 * We have a relative simple AA process . In weak type language, do deep
 * AA is extreamly complicated since everything is dynamic. To make AA
 * possible, I have already refactoried many times for the HIR to try to
 * simplify this process and carry out as much information as possible.
 *
 */

class AA {
 public:
  virtual int Query( Expr* , Expr* , EffectBarrier* ) = 0;
};

class ObjectAA : public AA {
 public:
  virtual int Query( Expr* , Expr* , EffectBarrier* );
};

class ListAA   : public AA {
 public:
  virtual int Query( Expr* , Expr* , EffectBarrier* );
};

int ObjectAA::Query( Expr* object , Expr* key , EffectBarrier* effect ) {
  if(effect->Is<ListResize>()) {
    return AA_NOT;
  } else if(effect->Is<EmptyBarrier>()) {
    return AA_NOT;
  } else if(effect->Is<ObjectResize>()) {
    auto resizer = effect->As<ObjectResize>();
    auto obj     = resizer->object();
    auto k       = resizer->key   ();

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


class MemoryFolder : public Folder {
 public:
  MemoryFolder( zone::Zone* zone ):ref_table_() { (void)zone; }

  virtual bool Predicate( const FolderData& ) const;
  virtual Expr* Fold    ( Graph* , const FolderData& );

 private:
  Expr* Fold( Graph* , const ObjectFindFolderData&   );
  Expr* Fold( Graph* , const ObjectRefGetFolderData& );
  Expr* Fold( Graph* , const ObjectRefSetFolderData& );

 private:
  static const char* kObjectRef;
  static const char* kListRef;

  // reference key
  struct RefKey {
    Expr*          object;
    Expr*          key   ;
    EffectBarrier* effect;
    MemoryRef*     ref;
    const char*    ref_type;
    RefKey( MemoryRef* , EffectBarrier* );
    RefKey( Expr* , Expr* , EffectBarrier* , const char* );
  };

  struct RefKeyHasher {
    std::size_t operator() ( const RefKey& rk ) const;
  };

  struct RefKeyEqual {
    bool operator () ( const RefKey& l , const RefKey& r ) const;
  };

  typedef std::unordered_set<RefKey,RefKeyHasher,RefKeyEqual> NumberTable;

  struct RefPos {
    MemoryRef*      ref;
    WriteEffect* effect;
    RefPos(MemoryRef* r, WriteEffect* e):ref(r),effect(e) {}
    RefPos():ref(),effect() {}
    operator bool () const { return ref != NULL; }
  };

  RefPos FindRef( Expr* , Expr* , WriteEffect* , AA* );
 private:
  NumberTable ref_table_;
};

LAVA_REGISTER_FOLDER("memory-folder",MemoryFolderFactory,MemoryFolder);

static const char*          MemoryFolder::kObjectRef = "object-ref";
static const char*          MemoryFolder::kListRef   = "list-ref";
static MemoryFolder::Result MemoryFolder::Result::kDead{DEAD,NULL};
static MemoryFolder::Result MemoryFolder::Result::kFailed{FAILED,NULL};

MemoryFolder::RefKey::RefKey( MemoryRef* r , EffectBarrier* eb ):
  object    (r->object()),
  key       (r->comp()  ),
  effect    (eb),
  ref       (r),
  ref_type  (NULL)
{
  if(ref->Is<ObjectFind>()) {
    ref_type = kObjectRef;
  } else {
    lava_debug(NORMAL,lava_verify(ref->Is<ListInsert>()););
    ref_type = kListRef;
  }
}

MemoryFolder::RefKey::RefKey( Expr* obj , Expr* k , EffectBarrier* e , const char* rt ):
  object  (obj),
  key     (k),
  effect  (e),
  ref     (NULL),
  ref_type(rt)
{}

bool MemoryFolder::RefKeyEqual::operator() ( const RefKey& l , const RefKey& r ) const {
  return l.object->Equal(r.object) && l.key->Equal(r.key)       &&
                                      l.effect->Equal(r.effect) &&
                                      l.ref_type == r.ref_type;
}

std::size_t MemoryFolder::RefKeyHash::operator() ( const RefKey& rk ) const {
  return GVNHash3(rk.ref_type,rk.object->GVNHash(),rk.key->GVNHash(),rk.effect->GVNHash());
}

MemoryFolder::RefPos MemoryFolder::FindRef( Expr* object , Expr* key , WriteEffect* effect ,
                                                                       AA*          aa ) {
  for( auto e = effect->ClosestBarrier(); e && !e->Is<HardBarrier>() ;e = e->NextBarrier() ) {
    if(auto itr = ref_table_.find(RefKey{object,key,effect,kObjectRef});
        itr != ref_table_.end()) {
      lava_debug(NORMAL,lava_verify(itr->ref););
      return RefPos{itr->ref,effect};
    }

    switch(aa->Query(object,key,effect)) {
      case AA::AA_MAY :
      case AA::AA_MUST:
        return RefPos{};
      default:
        break;
    }
  }
  return RefPos{};
}

Expr* MemoryFolder::Fold( Graph* graph , const ObjectFindFolderData& data ) {
  AAObject aa;
  if(auto ref = FindRef(data.object,data.key,data.effect,&aa); ref) {
    return ref.node;
  }
  auto obj = ObjectFind::New(graph,data.object,data.key,data.cp);
  lava_verify(ref_table_.insert(RefKey{obj,data.effect}).second);
  return obj;
}

Expr* MemoryFolder::Fold( Graph* graph , const ObjectRefGetFolderData& data ) {
  auto e = data.effect;
  // perform a store forwarding operation
}

Expr* MemoryFolder::Fold( Graph* graph , const FolderData& data ) {
  if(data.fold_type() == FOLD_OBJECT_FIND) {
    return Fold(graph,static_cast<const ObjectFindFolderData&>(data));
  } else if(data.fold_type() == FOLD_OBJECT_REF_GET) {
    return Fold(graph,static_cast<const ObjectRefGetFolderData&>(data));
  } else if(data.fold_type() == FOLD_OBJECT_REF_SET) {
    return Fold(graph,static_cast<const ObjectRefSetFolderData&>(data));
  }
}

} // namespace
} // namespace hir
} // namespace cbase
} // namespace lavascript
