#include "type.h"
#include "src/trace.h"

namespace lavascript {
namespace cbase {

const char* GetTypeKindName( TypeKind kind ) {
  switch(kind) {
#define __(A,B) case TPKIND_##B : return #A;
    LAVASCRIPT_CBASE_TYPE_KIND_LIST(__)
#undef __ // __
    default: lava_die(); return NULL;
  }
}

TypeKind MapValueTypeToTypeKind( ValueType type ) {
  switch(type) {
    case TYPE_REAL:     return TPKIND_FLOAT64;
    case TYPE_BOOLEAN:  return TPKIND_BOOLEAN;
    case TYPE_NULL :    return TPKIND_NIL;
    case TYPE_STRING:   return TPKIND_STRING;
    case TYPE_LIST:     return TPKIND_LIST;
    case TYPE_OBJECT:   return TPKIND_OBJECT;
    case TYPE_ITERATOR: return TPKIND_ITERATOR;
    case TYPE_EXTENSION:return TPKIND_EXTENSION;
    case TYPE_CLOSURE:  return TPKIND_CLOSURE;
    default:            return TPKIND_UNKNOWN;
  }
}

class TPKind::TPKindBuilder{
 public:
  TPKindBuilder();

  void AddChildren( TPKind* p , TPKind* c ) {
    p->children_.push_back(c);
    lava_debug(NORMAL,lava_verify(c->parent_ == NULL););
    c->parent_  = p;
  }

  TPKind* Node( TypeKind tk ) {
    return all_kinds_ + static_cast<int>(tk);
  }

 private:
  TPKind* root_;
  TPKind  all_kinds_[SIZE_OF_TYPE_KIND];
};

TPKind::TPKindBuilder::TPKindBuilder() :
  root_(NULL),
  all_kinds_() {

  root_ = all_kinds_;

  /**
   * Currently we just hard coded the whole relationship for
   * the basica type system.
   * We may want to refactor the code into a more maintable
   * way to populate these data
   */
  {
    // root <- primitive
    AddChildren(root_,Node(TPKIND_PRIMITIVE));
    // primitive <- number
    //           <- boolean
    //           <- nil
    {
      auto primitive = Node(TPKIND_PRIMITIVE);
      AddChildren(primitive,Node(TPKIND_NUMBER));
      // number  <- float64
      //         <- index
      {
        auto number = Node(TPKIND_NUMBER);
        AddChildren(number,Node(TPKIND_FLOAT64));
        AddChildren(number,Node(TPKIND_INDEX));
      }
      AddChildren(primitive,Node(TPKIND_BOOLEAN));
      AddChildren(primitive,Node(TPKIND_NIL));
    }

    // reference <- string
    //           <- object
    //           <- list
    //           <- iterator
    //           <- closure
    //           <- extension
    {
      auto reference = Node(TPKIND_REFERENCE);
      AddChildren(reference,Node(TPKIND_STRING));
      // string  <- small_string
      //         <- long_string
      {
        auto string = Node(TPKIND_STRING);
        AddChildren(string,Node(TPKIND_LONG_STRING));
        AddChildren(string,Node(TPKIND_SMALL_STRING));
      }
      AddChildren(reference,Node(TPKIND_OBJECT));
      AddChildren(reference,Node(TPKIND_LIST));
      AddChildren(reference,Node(TPKIND_ITERATOR));
      AddChildren(reference,Node(TPKIND_CLOSURE));
      AddChildren(reference,Node(TPKIND_EXTENSION));
    }
  }
}



bool TPKind::ToBoolean( TypeKind tp , bool* output ) {
  if(tp == TPKIND_BOOLEAN || tp == TPKIND_UNKNOWN)
    return false;
  else {
    if(tp == TPKIND_NIL)
      *output = false;
    else
      *output = true;
    return true;
  }
}

} // namespace cbase
} // namespace lavascript
