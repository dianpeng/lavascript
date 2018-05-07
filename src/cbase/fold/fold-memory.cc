#include "fold-memory.h"
#include "src/util.h"

namespace lavascript {
namespace cbase      {
namespace hir        {
namespace {

Expr* TryFoldObjectGet( Graph* graph , Expr* obj , Expr* key ) {
  (void)graph;

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

} // namespace

// Precondition of folding memory node
//
// We use Node::HasRef function to test whether we can fold this object/list node
// the HasRef tests true at least means this node is not referenced by any other node
// which is at least telling us no alias existed and we can just do folding on top of
// it without worrying about any other sort of dependency existed.

Expr* FoldIndexGet( Graph* graph , Expr* obj , Expr* idx ) {
  if(!obj->HasRef()) {
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

Expr* FoldPropGet( Graph* graph , Expr* obj , Expr* prop ) {
  if(!obj->HasRef()) {
    if(auto r = TryFoldObjectGet(graph,obj,prop); r != NULL) return r;
  }
  return NULL;
}

} // namespace hir
} // namespace cbase
} // namespace lavascript
