#include "fold-memory.h"
#include "src/util.h"

namespace lavascript {
namespace cbase      {
namespace hir        {
namespace {

Expr* TryFoldObjectGet( Graph* graph , Expr* obj , Expr* key ) {
  if(obj->IsIRObject() && key->IsString()) {
    auto irobj = obj->AsIRObject();
    auto zstr  = key->AsZoneString();
    lava_foreach( auto e , irobj->operand_list()->GetForwardIterator() ) {
      auto kv = e->AsIRObjectKV();
      auto rk = kv->key()->AsZoneString();
      if(rk == zstr) return kv->value();
    }
  }
  return NULL;
}

bool TryFoldObjectSet( Graph* graph , Expr* obj, Expr* key, Expr* value ) {
  if(obj->IsIRObject() && key->IsString()) {
    auto irobj = obj->AsIRObject();
    auto zstr  = key->AsZoneString();
    lava_foreach( auto e , irobj->operand_list()->GetForwardIterator() ) {
      auto kv = e->AsIRObjectKV();
      auto rk = kv->key()->AsZoneString();
      if(rk == zstr) {
        // the value are stored in the index 1 position , 0 is the key
        kv->set_value(value);
        return true;
      }
    }
  }
  return false;
}

} // namespace

// fold of iget/pget is kind of hard due to the fact that we don't reflect the
// mutation of field back to the list/object node itself but rely on the statement
// list and dependency. so not too much can be done. but still we can fold iget
// one it is side effect free.
Expr* FoldIndexGet( Graph* graph , IGet* node ) {
  if(!node->HasSideEffect()) {
    auto obj = node->object();
    auto idx = node->index ();
    // 1. try to dereference as a list literal index
    if(obj->IsIRList() && idx->IsFloat64()) {
      // a constant index into a list literal and also this node doesn't have any
      // side effect dependency currently
      std::uint32_t iidx;
      if(!CastToIndex(idx->AsFloat64()->value(),&iidx)) goto bailout;
      auto list = obj->AsIRList();
      // TODO:: instead of bailout, returning a trap node to force it fail here
      //        since we are sure this is a bug
      if(iidx >= list->Size()) goto bailout;
      return list->operand_list()->Index(iidx);
    }
    // 2. try to dereference as a object literal index like a["xx"]
    if(auto r = TryFoldObjectGet(graph,obj,idx); r != NULL) return r;
  }

bailout:
  return NULL;
}

Expr* FoldPropGet( Graph* graph , PGet* node ) {
  if(!node->HasSideEffect()) {
    auto obj = node->object();
    auto prop= node->key   ();
    if(auto r = TryFoldObjectGet(graph,obj,prop); r != NULL) return r;
  }
  return NULL;
}

bool FoldIndexSet( Graph* graph , ISet* node ) {
  if(!node->HasSideEffect()) {
    auto obj = node->object();
    auto idx = node->index ();
    auto val = node->value ();
    // 1. try list literal
    if(obj->IsIRList() && idx->IsFloat64()) {
      std::uint32_t iidx;
      if(!CastToIndex(idx->AsFloat64()->value(),&iidx)) goto bailout;
      auto list = obj->AsIRList();
      if(iidx >= list->Size()) goto bailout;
      list->SetOperand( iidx , val ); // replace the old value with new value
      return list;
    }
    // 2. try object literal
    if(TryFoldObjectSet(graph,obj,idx,val)) return true;
  }

bailout:
  return false;
}

bool FoldPropSet( Graph* graph , PSet* node ) {
  if(!node->HasSideEffect()) {
    auto obj = node->object();
    auto idx = node->key   ();
    auto val = node->value ();
    if(TryFoldObjectSet(graph,obj,idx,val)) return true;
  }
  return false;
}

} // namespace hir
} // namespace cbase
} // namespace lavascript
