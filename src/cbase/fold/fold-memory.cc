#include "fold-memory.h"
#include "folder.h"
#include "src/cbase/type.h"
#include "src/cbase/aa.h"
#include "src/util.h"

#include <unordered_set>

namespace lavascript {
namespace cbase      {
namespace hir        {
namespace            {


class MemoryFolder : public Folder {
 public:
  MemoryFolder( zone::Zone* zone ):ref_table_() { (void)zone; }

  virtual bool CanFold( const FolderData& ) const;
  virtual Expr* Fold    ( Graph* , const FolderData& );

 private:
  // object
  Expr* Fold( Graph* , const ObjectFindFolderData&   );
  Expr* Fold( Graph* , const ObjectRefGetFolderData& );
  Expr* Fold( Graph* , const ObjectRefSetFolderData& );
  // list
  Expr* Fold( Graph* , const ListIndexFolderData&    );
  Expr* Fold( Graph* , const ListRefGetFolderData&   );
  Expr* Fold( Graph* , const ListRefSetFolderData&   );
  Expr* Fold( Graph* , const ExprFolderData& );

  template< typename Set , typename Get > Expr* LoadCollapse( Expr* , Expr* , WriteEffect* );
  template< typename Set >                Expr* StoreForward( Expr* , WriteEffect* );
 private:
  static const char* kObjectRef;
  static const char* kListRef;

  // For iterative value numbering of all the memory reference node. Memory
  // reference nodes will not participate GVN operations due to the side
  // effect it generates , ie any nodes inside of the effect chain will not
  // be part of GVN. But they can be numbering. Here the folder function will
  // do a special numbering of memory reference node and combine it with AA
  // to optimize out those redundant memory reference node.
  struct RefKey {
    Expr*           object;
    Expr*           key   ;
    EffectBarrier*  effect;
    StaticRef*      ref;
    const char*     ref_type;
    RefKey( StaticRef* , EffectBarrier* );
    RefKey( Expr* , Expr* , EffectBarrier* , const char* );
  };

  struct RefKeyHash {
    std::size_t operator() ( const RefKey& rk ) const;
  };

  struct RefKeyEqual {
    bool operator () ( const RefKey& l , const RefKey& r ) const;
  };

  typedef std::unordered_set<RefKey,RefKeyHash,RefKeyEqual> NumberTable;

  StaticRef* FindRef( Expr* , Expr* , WriteEffect* , TypeKind );
 private:
  NumberTable ref_table_;
};

LAVA_REGISTER_FOLDER("memory-folder",MemoryFolderFactory,MemoryFolder);

const char* MemoryFolder::kObjectRef = "object-ref";
const char* MemoryFolder::kListRef   = "list-ref"  ;

MemoryFolder::RefKey::RefKey( StaticRef* r , EffectBarrier* eb ):
  object    (NULL),
  key       (NULL),
  effect    (eb),
  ref       (r),
  ref_type  (NULL)
{
  if(ref->Is<ObjectFind>()) {
    auto of  = ref->As<ObjectFind>();
    ref_type = kObjectRef;
    object   = of->object();
    key      = of->key   ();
  } else {
    auto li  = ref->As<ListIndex>();
    ref_type = kListRef;
    object   = li->object();
    key      = li->index ();
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

StaticRef* MemoryFolder::FindRef( Expr* object , Expr* key , WriteEffect* effect , TypeKind hint ) {
  // This function essentially is a value numbering process for dedup of memory ref node
  // It starts to compute number of the memory ref node indicated by {object,key} with
  // a specific needed effect node until we hit one
  for( auto e = effect->FirstBarrier(); !e->Is<HardBarrier>() ;e = e->NextBarrier() ) {
    if(auto itr = ref_table_.find(RefKey{object,key,e->As<EffectBarrier>(),kObjectRef});
        itr != ref_table_.end()) {
      lava_debug(NORMAL,lava_verify(itr->ref););
      return itr->ref;
    }
    int ret = (hint == TPKIND_OBJECT ? AA::QueryObject(object,e->As<EffectBarrier>()) :
                                       AA::QueryList  (object,e->As<EffectBarrier>()));
    switch(ret) {
      case AA::AA_MAY :
      case AA::AA_MUST:
        return NULL;
      default:
        break;
    }
  }
  return NULL;
}

template< typename Set, typename Get >
Expr* MemoryFolder::LoadCollapse( Expr* ref , Expr* value , WriteEffect* effect ) {
  // store collapsing. eg :
  // a[1] = 20;
  // a[1] = 30;
  // dedup the second write operation
  for( auto e = effect; !e->Is<HardBarrier>() ; e = e->NextWrite() ) {
    // 1. go through all the read happened before this write opereations
    lava_foreach( auto rd , e->read_effect()->GetForwardIterator() ) {
      if(rd->Is<Get>()) {
        switch(AA::Query(FieldRefNode{ref},FieldRefNode{rd->As<Get>()->ref()})) {
          case AA::AA_MUST:
          case AA::AA_MAY : return NULL; // there's a read
          default: break;
        }
      }
    }

    // 2. check this write operation
    if(e->Is<Set>()) {
      if(AA::Query(FieldRefNode{ref},FieldRefNode{e->As<Set>()->ref()}) == AA::AA_MUST) {
        // collapsing
        e->ReplaceOperand(1,value);
        return e;
      }
    }
  }
}

template< typename Set >
Expr* MemoryFolder::StoreForward( Expr* ref , WriteEffect* effect ) {
  if(!ref->Is<StaticRef>()) return NULL; // none static ref cannot do forwarding
                                         // but I don't know whether we have it or not
  for( auto e = effect; !e->Is<HardBarrier>() ; e = e->NextWrite() ) {
    // walk through all the write happened before this Load operation and
    // try to find one write that writes to exactly same position this load
    // operates on and then try to do a forwarding.
    if(e->Is<Set>()) {
      switch(AA::Query(FieldRefNode{ref},FieldRefNode{e->As<Set>()->ref()})) {
        case AA::AA_MAY : return NULL;
        case AA::AA_MUST: return e->As<Set>()->value(); // forwarding
        default:          break;
      }
    }
  }
  return NULL;
}

Expr* MemoryFolder::Fold( Graph* graph , const ExprFolderData& data ) {
  (void)graph;
  if(data.node->Is<StaticRef>()) {
    auto sr = data.node->As<StaticRef>();
    ref_table_.insert(RefKey{data.node->As<StaticRef>(),sr->write_effect()->FirstBarrier()});
    return sr;
  }
  return NULL;
}

Expr* MemoryFolder::Fold( Graph* graph , const ObjectFindFolderData& data ) {
  (void)graph;
  return FindRef(data.object,data.key,data.effect,TPKIND_OBJECT);
}

Expr* MemoryFolder::Fold( Graph* graph , const ObjectRefGetFolderData& data ) {
  (void)graph;
  return StoreForward<ObjectRefSet>(data.ref,data.effect);
}

Expr* MemoryFolder::Fold( Graph* graph , const ObjectRefSetFolderData& data ) {
  (void)graph;
  return LoadCollapse<ObjectRefSet,ObjectRefGet>(data.ref,data.value,data.effect);
}

Expr* MemoryFolder::Fold( Graph* graph , const ListIndexFolderData& data ) {
  (void)graph;
  return FindRef(data.object,data.index,data.effect,TPKIND_LIST);
}

Expr* MemoryFolder::Fold( Graph* graph , const ListRefGetFolderData& data ) {
  (void)graph;
  return StoreForward<ListRefSet>(data.ref,data.effect);
}

Expr* MemoryFolder::Fold( Graph* graph , const ListRefSetFolderData& data ) {
  (void)graph;
  return LoadCollapse<ListRefSet,ListRefGet>(data.ref,data.value,data.effect);
}

bool MemoryFolder::CanFold( const FolderData& data ) const {
  switch(data.fold_type()) {
    case FOLD_OBJECT_FIND:
    case FOLD_OBJECT_REF_GET:
    case FOLD_OBJECT_REF_SET:
      return true;
    case FOLD_EXPR:
      if(auto d = static_cast<const ExprFolderData&>(data); d.node->Is<StaticRef>())
        return true;
      break;
    default:
      break;
  }
  return false;
}

Expr* MemoryFolder::Fold( Graph* graph , const FolderData& data ) {
  switch(data.fold_type()) {
    case FOLD_OBJECT_FIND:
      return Fold(graph,static_cast<const ObjectFindFolderData&>(data));
    case FOLD_OBJECT_REF_GET:
      return Fold(graph,static_cast<const ObjectRefGetFolderData&>(data));
    case FOLD_OBJECT_REF_SET:
      return Fold(graph,static_cast<const ObjectRefSetFolderData&>(data));
    case FOLD_LIST_INDEX:
      return Fold(graph,static_cast<const ListIndexFolderData&>(data));
    case FOLD_LIST_REF_GET:
      return Fold(graph,static_cast<const ListRefGetFolderData&>(data));
    case FOLD_LIST_REF_SET:
      return Fold(graph,static_cast<const ListRefSetFolderData&>(data));
    case FOLD_EXPR:
      return Fold(graph,static_cast<const ExprFolderData&>(data));
    default: lava_die(); return NULL;
  }
}

} // namespace
} // namespace hir
} // namespace cbase
} // namespace lavascript
