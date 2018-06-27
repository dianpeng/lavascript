#ifndef CBASE_TYPE_H_
#define CBASE_TYPE_H_
#include "src/macro.h"
#include "src/object-type.h"
#include <vector>

namespace lavascript {
class Value;
namespace cbase {

/**
 * Type flag definitions
 *
 * The indent here indicate the parental relationship between each type
 * NOTES: String are conisdered to be primitive due to its immutable
 */
#define LAVASCRIPT_CBASE_TYPE_KIND_LIST(__)             \
  __(unknown,UNKNOWN)                                   \
  __(root,ROOT)                                         \
  __(primitive,PRIMITIVE)                               \
  __(number,NUMBER)                                     \
  __(float64,FLOAT64)                                   \
  __(int64  ,INT64)                                     \
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
  static bool Contain( TypeKind , TypeKind , bool* );
  // check whether the *second* valuetype is included by *first* typekind
  static bool Contain( TypeKind , ValueType, bool* );
  // check whether we can compare both types
  static bool Equal  ( TypeKind , TypeKind , bool* );
 public:
  // try to convert type kind to a boolean value if we can
  inline static bool ToBoolean( TypeKind , bool* );
  // check whether this TypeKind is a string type or not
  inline static bool IsString( TypeKind tp );
  // check whether this TypeKind is a number type
  inline static bool IsNumber( TypeKind tp );
  // type is primitive , primitive type doesn't have side effect they
  // are immutable essentially. String is Primitive type since it doesn't
  // have side effect
  inline static bool IsLiteral( TypeKind tp );
  // heap type has mutability and can cause side effect
  inline static bool IsMutable     ( TypeKind tp );
  // unknown means the type is a mixed , cannot tell what it actually is
  inline static bool IsUnknown  ( TypeKind tp ) {
    return !IsLiteral(tp) && !IsMutable(tp);
  }
  inline static bool IsLeaf( TypeKind tk ) {
    return Node(tk)->IsLeaf();
  }
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
  switch(tp) {
    case TPKIND_STRING:
    case TPKIND_LONG_STRING:
    case TPKIND_SMALL_STRING:
      return true;
    default:
      return false;
  }
}

inline bool TPKind::IsNumber( TypeKind tp ) {
  switch(tp) {
    case TPKIND_NUMBER:
    case TPKIND_INT64:
    case TPKIND_FLOAT64:
      return true;
    default:
      return false;
  }
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

inline bool TPKind::IsLiteral( TypeKind tp ) {
  switch(tp) {
    case TPKIND_PRIMITIVE:
    case TPKIND_NUMBER:
    case TPKIND_FLOAT64:
    case TPKIND_INT64:
    case TPKIND_BOOLEAN:
    case TPKIND_NIL:
    case TPKIND_STRING:
    case TPKIND_LONG_STRING:
    case TPKIND_SMALL_STRING:
      return true;
    default:
      return false;
  }
}

inline bool TPKind::IsMutable( TypeKind tp ) {
  switch(tp) {
    case TPKIND_REFERENCE:
    case TPKIND_OBJECT:
    case TPKIND_LIST:
    case TPKIND_ITERATOR:
    case TPKIND_CLOSURE:
    case TPKIND_EXTENSION:
      return true;
    default:
      return false;
  }
}

} // namespace cbase
} // namespace lavascript

#endif // CBASE_TYPE_H_
