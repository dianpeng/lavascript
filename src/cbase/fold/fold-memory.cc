#include "fold-memory.h"

#include <utility>

namespace lavascript {
namespace cbase      {
namespace hir        {

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

template< typename Set, typename Get, typename T >
MemoryFolder::BranchAA MemoryFolder::StoreCollapseSingleBranchAA( const FieldRefNode& ref ,
                                                                  WriteEffect*        e  ) {
  do {
    // 1. check all the read happened before the *NextWrite*
    lava_foreach(auto rd, e->read_effect()->GetForwardIterator()) {
      if(rd->Is<Get>()) {
        switch(AA::Query(ref,FieldRefNode{rd->As<Get>()->ref()})) {
          case AA::AA_MUST: return BranchAA{Must{}};
          case AA::AA_MAY : return BranchAA{}  ; // may
          case AA::AA_NOT : break;               // continue
          default: break;
        }
      }
    }

    // 2. now checkcurrent effect node to see whether we need to go on
    if(e->Is<HardBarrier>()) {
      if(e->Is<EffectMerge>()) {
        // nested branches
        return StoreCollapseBranchAA<Set,Get,T>(ref,e->As<EffectMerge>());
      } else if(e->Is<BranchStartEffect>()) {
        return BranchAA{e->As<BranchStartEffect>()};
      }
      // we don't know what happened here since we stop at a HardBarrier
      return BranchAA{};
    }

    // 3. check the write
    if(e->Is<Set>()) {
      switch(AA::Query(ref,FieldRefNode{e->As<Set>()->ref()})) {
        case AA::AA_MUST: return BranchAA{Must{}};
        case AA::AA_MAY : return BranchAA{};
        case AA::AA_NOT : break;
        default: break;
      }
    } else if(e->Is<T>()) {
      if(ref.object()->Equal(e->As<T>())) {
        return BranchAA{Must{}};
      }
      // else not aliased with each other
    }

    // go to next write
    e = e->NextWrite();
  } while(true);

  lava_die(); return BranchAA{};
}

template< typename Set, typename Get , typename T >
MemoryFolder::BranchAA MemoryFolder::StoreCollapseBranchAA( const FieldRefNode& ref ,
                                                            EffectMerge*          phi ) {
  if(phi->operand_list()->size() < 2) return BranchAA{}; // FIXME:: assert ?
  auto first = phi->Operand(0)->As<WriteEffect>();
  auto aa = StoreCollapseSingleBranchAA<Set,Get,T>(ref,first);
  if(aa.result == AA::AA_MAY)
    return BranchAA{};

  // handle rest of the branches
  for( std::size_t i = 1 ; i < phi->operand_list()->size() ; ++i ) {
    auto temp = StoreCollapseSingleBranchAA<Set,Get,T>(ref,phi->Operand(i)->As<WriteEffect>());
    if((temp.result == AA::AA_MAY) || (aa.result != temp.result)) {
      return BranchAA{};
    }
    lava_debug(NORMAL,
      if(aa.result == AA::AA_NOT )
        lava_verify(aa.end->NextWrite()->IsIdentical(temp.end->NextWrite()));
    );
  }
  return aa;
}

template< typename Set, typename Get , typename T >
Expr* MemoryFolder::StoreCollapse( Expr* ref , Expr* value , WriteEffect* e ) {
  // store collapsing. eg :
  // a[1] = 20;
  // a[1] = 30;
  // dedup the second write operation
  do {
    // 1. go through all the read happened before this write opereations
    lava_foreach( auto rd , e->read_effect()->GetForwardIterator() ) {
      if(rd->Is<Get>()) {
        switch(AA::Query(FieldRefNode{ref},FieldRefNode{rd->As<Get>()->ref()})) {
          case AA::AA_MUST:
          case AA::AA_MAY :
            return NULL; // there's a read
          default:
            break;
        }
      }
    }

    // 2. check the effect node's type , break when the effect is a hard barrier
    if(e->Is<HardBarrier>()) {
      if(e->Is<EffectMerge>()) {
        // do a branch alias analyzing
        auto res = StoreCollapseBranchAA<Set,Get,T>(FieldRefNode{ref},e->As<EffectMerge>());
        switch(res.result) {
          case AA::AA_MAY :
          case AA::AA_MUST:
            break;
          default:
            e = res.end->NextWrite();
            continue; // continue the loop again
        }
      }

      // not able to optimize
      return NULL;
    }

    // 3. check this write operation
    if(e->Is<Set>()) {
      if(AA::Query(FieldRefNode{ref},FieldRefNode{e->As<Set>()->ref()}) == AA::AA_MUST) {
        // collapsing
        e->ReplaceOperand(1,value);
        return e;
      }
    } else if(e->Is<T>()) {
      FieldRefNode n{ref};
      if(n.object()->Equal(e->As<T>())) {
        // collapsing store like this:
        // a = { "a" : 1 }; a.a = 2; ==> a = { "a" : 2 };
        if(auto i = static_cast<ComponentBase*>(e->As<T>());i->Store(n.comp(),value))
          return e->As<T>();
        else
          return NULL;
      }
    }

    // 4. move to next write effect
    e = e->NextWrite();
  } while(true);

  return NULL;
}

template< typename Set , typename T >
MemoryFolder::BranchAA MemoryFolder::StoreForwardSingleBranchAA( const FieldRefNode& ref ,
                                                                 WriteEffect*          e ) {
  do {
    // only need to care about write effect node , no need to do read
    if(e->Is<HardBarrier>()) {
      if(e->Is<EffectMerge>()) {
        return StoreForwardBranchAA<Set,T>(ref,e->As<EffectMerge>());
      } else if(e->Is<BranchStartEffect>()) {
        return BranchAA{e->As<BranchStartEffect>()}; // not aliase with each other
      }
      return BranchAA{}; // may be aliased
    }

    if(e->Is<Set>()) {
      switch(AA::Query(ref,FieldRefNode{e->As<Set>()->ref()})) {
        case AA::AA_MAY : return BranchAA{};
        case AA::AA_MUST: return BranchAA{Must{}};
        default:          break;
      }
    } else if(e->Is<T>()) {
      if(ref.object()->Equal(e->As<T>())) {
        return BranchAA{Must{}};
      }
    }

    e = e->NextWrite();
  } while(true);

  lava_die();
  return BranchAA{};
}

template< typename Set , typename T >
MemoryFolder::BranchAA MemoryFolder::StoreForwardBranchAA( const FieldRefNode& ref ,
                                                           EffectMerge*          phi ) {
  if(phi->operand_list()->size() < 2) return BranchAA{}; // FIXME:: assert ?
  auto first = phi->Operand(0)->As<WriteEffect>();
  auto aa    = StoreForwardSingleBranchAA<Set,T>(ref,first);
  if(aa.result == AA::AA_MAY) return BranchAA{};

  for( std::size_t i = 1 ; i < phi->operand_list()->size() ; ++i ) {
    auto temp = StoreForwardSingleBranchAA<Set,T>(ref,phi->Operand(i)->As<WriteEffect>());
    if(temp.result == AA::AA_MAY || (temp.result != aa.result))
      return BranchAA{};
    lava_debug(NORMAL,
      if(temp.result == AA::AA_NOT )
        lava_verify(temp.end->NextWrite()->IsIdentical(aa.end->NextWrite()));
    );
  }

  return aa;
}

template< typename Set , typename T >
Expr* MemoryFolder::StoreForward( Expr* ref , WriteEffect* e ) {
  do {
    if(e->Is<HardBarrier>()) {
      if(e->Is<EffectMerge>()) {
        auto res = StoreForwardBranchAA<Set,T>(FieldRefNode{ref},e->As<EffectMerge>());
        switch(res.result) {
          case AA::AA_MUST:
          case AA::AA_MAY:
            break;
          default:
            e = res.end->NextWrite();
            continue;
        }
      }
      return NULL;
    }

    // walk through all the write happened before this Load operation and
    // try to find one write that writes to exactly same position this load
    // operates on and then try to do a forwarding.
    if(e->Is<Set>()) {
      switch(AA::Query(FieldRefNode{ref},FieldRefNode{e->As<Set>()->ref()})) {
        case AA::AA_MAY : return NULL;
        case AA::AA_MUST: return e->As<Set>()->value(); // forwarding
        default:          break;
      }
    } else if(e->Is<T>()) {
      FieldRefNode n{ref};
      if(n.object()->Equal(e->As<T>())) {
        // forward store like this:
        // a = { "a" : 1 }; return a.a == > return 1;
        auto i = static_cast<ComponentBase*>(e->As<T>());
        if(auto result = i->Load(n.comp()); result) return result;
      }
    }
    // go to next write
    e = e->NextWrite();
  } while(true);
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
  return StoreForward<ObjectRefSet,IRObject>(data.ref,data.effect);
}

Expr* MemoryFolder::Fold( Graph* graph , const ObjectRefSetFolderData& data ) {
  (void)graph;
  return StoreCollapse<ObjectRefSet,ObjectRefGet,IRObject>(data.ref,data.value,data.effect);
}

Expr* MemoryFolder::Fold( Graph* graph , const ListIndexFolderData& data ) {
  (void)graph;
  return FindRef(data.object,data.index,data.effect,TPKIND_LIST);
}

Expr* MemoryFolder::Fold( Graph* graph , const ListRefGetFolderData& data ) {
  (void)graph;
  return StoreForward<ListRefSet,IRList>(data.ref,data.effect);
}

Expr* MemoryFolder::Fold( Graph* graph , const ListRefSetFolderData& data ) {
  (void)graph;
  return StoreCollapse<ListRefSet,ListRefGet,IRList>(data.ref,data.value,data.effect);
}

bool MemoryFolder::CanFold( const FolderData& data ) const {
  switch(data.fold_type()) {
    case FOLD_OBJECT_FIND   :
    case FOLD_OBJECT_REF_GET:
    case FOLD_OBJECT_REF_SET:
    case FOLD_LIST_INDEX    :
    case FOLD_LIST_REF_SET  :
    case FOLD_LIST_REF_GET  :
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

} // namespace hir
} // namespace cbase
} // namespace lavascript
