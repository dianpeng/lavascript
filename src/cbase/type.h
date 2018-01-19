#ifndef CBASE_TYPE_H_
#define CBASE_TYPE_H_

#include "src/common.h"
#include "src/trace.h"
#include "src/util.h"

#include <memory>
#include <vector>

/**
 * Type
 *
 * The type system here will be used throughout the optimization phase. Each HIR node will
 * have a type field used to indicate which type it has. The type is generated during the
 * lowering phas after the GVN phase.
 *
 * The type system is a tree structure. And each type field has a TypeDescriptor which is
 * a singleton used to indicate the general type. Also a range is attached for each node,
 * the range based type check will help us to deep branch elimination and other optimization
 */

namespace lavascript {
namespace cbase {

class TypeDescriptor;
class TypeDescriptorBuilder;

// Type forms a type graph, to make life easier we just use shared_ptr and stl.
// Performance will not be a thing since this graph is a singleton and needs no
// modification
typedef std::shared_ptr<TypeDescriptor> TypeDescriptorPtr;

/**
 * Type flag definitions
 *
 * The indent here indicate the parental relationship between each type
 */
enum TypeKind {
  TPKIND_UNKNOWN,
    TPKIND_PRIMITIVE,
      TPKIND_NUMBER ,
        TPKIND_FLOAT64,
        TPKIND_INT32,
        TPKIND_INT64,
        TPKIND_UINT32,
        TPKIND_UINT64,
        TPKIND_INDEX,

      TPKIND_STRING,
        TPKIND_LONG_STRING,
        TPKIND_SMALL_STRING,

      TPKIND_BOOLEAN,
      TPKIND_NIL,

    TPKIND_OBJECT,
      TPKIND_INDEXABLE,
        TPKIND_LIST,
        TPYE_OBJECT,
        /* TPKIND_EXTENSION */

      TPKIND_PROPTIABLE,
        /* TPKIND_OBJECT */
        /* TPKIND_EXTENSION */

      TPKIND_CALLABLE,
        TPKIND_CLOSURE,
        TPKIND_EXTENSION,

  SIZE_OF_TYPES
};

const char* GetTypeKindName( TypeKind );

// TypeOperation
//
// A type operation interface define the algebra that can be used with type
// operation.
//
// Several pass of optimization basically forms a propositional logical system
// and its solo goal is to answer type question
class TypeOperation {
};

// Type descriptor is a concret class used to represent a certain general type
class TypeDescriptor {
 public:
  // Name of the type
  const char* type_name() const { return GetTypeKindName(type_); }

  // Tag of the TypeDescriptor object
  TypeKind type() const { return type_; }

  // List of parent
  const std::vector<TypeDescriptorPtr>& parent() const { return parent_; }

  // Check whether a node is |this| node's predecessor
  bool IsPredecessor( TypeDescriptor* ) const;

  // Check whether a node is |this| node's parent
  bool IsParent     ( TypeDescriptor* ) const;

 private:
  void AddParent( const TypeDescriptorPtr& parent ) {
    parent_.push_back(parent);
  }

 private:
  TypeKind type_;

  std::vector<TypeDescriptorPtr> parent_;

  friend class TypeDescriptorBuilder;

  LAVA_DISALLOW_COPY_AND_ASSIGN(TypeDescriptor)
};





} // namespace cbase
} // namespace lavascript

#endif // CBASE_TYPE_H_
