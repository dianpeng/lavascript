#include "type.h"
#include "src/objects.h"
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
    case TYPE_REAL:      return TPKIND_FLOAT64;
    case TYPE_BOOLEAN:   return TPKIND_BOOLEAN;
    case TYPE_NULL :     return TPKIND_NIL;
    case TYPE_STRING:    return TPKIND_STRING;
    case TYPE_LIST:      return TPKIND_LIST;
    case TYPE_OBJECT:    return TPKIND_OBJECT;
    case TYPE_ITERATOR:  return TPKIND_ITERATOR;
    case TYPE_EXTENSION: return TPKIND_EXTENSION;
    case TYPE_CLOSURE:   return TPKIND_CLOSURE;
    default:
      lava_unreachF("we should never see value type %s appear in the value stack",GetValueTypeName(type));
      return TPKIND_UNKNOWN;
  }
}

TypeKind MapValueToTypeKind( const Value& v ) {
  /**
   * The ValueType doesn't have TRUE/FALSE as type, but in our
   * TypeKind system, we list TRUE/FALSE as type to make deeper
   * optimization. Using this function is simply to help us map
   * a Value object's internal tag/type to TypeKind
   */
  return MapValueTypeToTypeKind(v.type());
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

  // initialize all the type_kind_ field inside of the TPKind
  // object
#define __(A,B) Node(TPKIND_##B)->type_kind_ = TPKIND_##B;

  LAVASCRIPT_CBASE_TYPE_KIND_LIST(__)

#undef __ // __

  root_ =  Node(TPKIND_ROOT);

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

TPKind::TPKindBuilder* TPKind::GetTPKindBuilder() {
  static TPKindBuilder kBuilder;
  return &kBuilder;
}

TPKind* TPKind::Node( TypeKind tk ) {
  return TPKind::GetTPKindBuilder()->Node(tk);
}

bool TPKind::Contain( TypeKind parent , TypeKind children , bool* output ) {
  if(parent == TPKIND_UNKNOWN || children == TPKIND_UNKNOWN) {
    return false;
  }

  auto pnode = Node(parent);
  auto cnode = Node(children);
  *output = pnode->IsAncestor(*cnode);
  return true;
}

bool TPKind::Contain( TypeKind parent , ValueType children , bool* output ) {
  return Contain(parent,MapValueTypeToTypeKind(children),output);
}

bool TPKind::Equal( TypeKind lhs , TypeKind rhs , bool* output ) {
  if(lhs == TPKIND_UNKNOWN || rhs == TPKIND_UNKNOWN)
    return false;
  *output = (lhs == rhs);
  return true;
}

bool TPKind::HasChild( const TPKind& kind ) const {
  for( auto & e : children_ ) {
    if( e == &kind ) return true;
  }
  return false;
}

bool TPKind::IsAncestor( const TPKind& kind ) const {
  for( auto & e : children_ ) {
    if( e == &kind )
    return true;
    else {
      if(e->IsAncestor(kind))
        return true;
    }
  }
  return false;
}

bool TPKind::IsDescendent( const TPKind& kind ) const {
  return kind.IsAncestor(*this);
}

} // namespace cbase
} // namespace lavascript
