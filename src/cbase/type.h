#ifndef CBASE_TYPE_H_
#define CBASE_TYPE_H_
#include "src/common.h"
#include "src/object-type.h"
#include <vector>

namespace lavascript {
class Value;
namespace cbase {

/**
 * Type flag definitions
 *
 * The indent here indicate the parental relationship between each type
 */
#define LAVASCRIPT_CBASE_TYPE_KIND_LIST(__) \
  __(unknown,UNKNOWN)                                   \
  __(root,ROOT)                                         \
  __(primitive,PRIMITIVE)                               \
  __(number,NUMBER)                                     \
  __(float64,FLOAT64)                                   \
  __(index  ,INDEX  )                                   \
  __(boolean,BOOLEAN)                                   \
  __(nil,NIL)                                           \
  __(reference,REFERENCE)                               \
  __(string ,STRING )                                   \
  __(long_string,LONG_STRING)                           \
  __(small_string,SMALL_STRING)                         \
  __(object,OBJECT)                                     \
  __(list,LIST)                                         \
  __(iterator,ITERATOR)                                 \
  __(closure,CLOSURE)                                   \
  __(extension,EXTENSION)


enum TypeKind {

#define __(A,B) TPKIND_##B,

  LAVASCRIPT_CBASE_TYPE_KIND_LIST(__)

#undef __ // __

  SIZE_OF_TYPE_KIND
};

const char* GetTypeKindName    ( TypeKind     );
TypeKind MapValueTypeToTypeKind( ValueType    );
TypeKind MapValueToTypeKind    ( const Value& );

/**
 * A type descriptor used throughout the backend optimization
 *
 * It conatins all the parental type system information
 */
class TPKind {
 public:
  // convert a TypeKind into a TPKind node object
  static TPKind* Node( TypeKind tk );

  // check whether the *second* typekind is included by *first* typekind
  static bool Contain( TypeKind , TypeKind );

  // check whether the *second* valuetype is included by *first* typekind
  static bool Contain( TypeKind , ValueType );

 public:
  // try to convert type kind to a boolean value if we can
  inline static bool ToBoolean( TypeKind , bool* );

  // check whether this TypeKind is a string type or not
  inline static bool IsString( TypeKind tp );

 public:
  TypeKind type_kind() const { return type_kind_; }

  const char* type_kind_name() const {
    return GetTypeKindName(type_kind());
  }

  // Get the parent node of this TypeKind, if it returns NULL,
  // it means it is the root of the type system, ie TPKIND_UNKNOWN
  TPKind*  parent() const    { return parent_; }

  bool IsParent( const TPKind& kind ) const {
    return parent_ == &kind;
  }

  // Check whether the input |kind| is the one of the children of
  // this PTKind object
  bool HasChild( const TPKind& kind ) const;

  bool HasChild( TypeKind kind ) const {
    return HasChild(*Node(kind));
  }

  // Check whether this type node is a *LEAF* node
  bool IsLeaf() const { return children_.empty(); }

  // Check whether |this| is the ancestor of the input kind
  bool IsAncestor( const TPKind& kind ) const;

  bool IsAncestor( TypeKind kind ) const {
    return IsAncestor( *Node(kind) );
  }

  // Check whether |this| is the descendent of the input *kind*
  bool IsDescendent( const TPKind& kind ) const;

  bool IsDescendent( TypeKind kind ) const {
    return IsDescendent( *Node(kind) );
  }

 private:
  TPKind(): type_kind_(), parent_(NULL),children_() {}

  class TPKindBuilder;

  // Get TPKindBuilder object. This object is a static variable
  // and it is global
  static TPKindBuilder* GetTPKindBuilder();


  TypeKind type_kind_;
  TPKind* parent_;
  std::vector<TPKind*> children_;

  friend class TPKindBuilder;

  LAVA_DISALLOW_COPY_AND_ASSIGN(TPKind);
};

inline bool TPKind::IsString( TypeKind tp ) {
  return tp == TPKIND_STRING      || tp == TPKIND_LONG_STRING || tp == TPKIND_SMALL_STRING;
}

inline bool TPKind::ToBoolean( TypeKind tp , bool* output ) {
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

#endif // CBASE_TYPE_H_
