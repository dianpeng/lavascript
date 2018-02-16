#ifndef CBASE_STATIC_TYPE_INFERENCE_H_
#define CBASE_STATIC_TYPE_INFERENCE_H_

#include "src/cbase/hir.h"
#include "src/cbase/type.h"

#include <vector>

/**
 * Based on static type analyzing and also TypeTrace object
 * mark certain operation to be specualative executed node.
 *
 * Rest of the nodes that cannot be speculative executed will
 * do a full execution based on polymorphic operator
 *
 */

namespace lavascript {
namespace cbase      {
namespace hir        {

/**
 * A helper class/object to lower the type and record its current type
 * internally.
 *
 * This class is used to do static type inference. If it failed, the
 * IR graph builder will emit speculative execution with guard.
 */
class StaticTypeInference {
 public:
  static const std::size_t kInitSize = 256;

  StaticTypeInference() : type_vector_(kInitSize) {}

 public:
  // Try to get the implicit type of this expression node. The
  // implicit type is marked by the node type , like float64 node
  // has type TPKIND_FLOAT64.
  //
  // This function should be called after we check we cannot get
  // type inside of the type_vector_
  static TypeKind GetImplicitType( Expr* );

 public:
  // Add intrinsic function's type
  void AddIntrinsicCallType( ICall* );

  // Add a type with corresponding node
  inline void AddType( std::uint32_t id , TypeKind );

  // Get the type of this node
  TypeKind GetType( Expr* type ) const;

 private:
  typedef std::vector<TypeKind> TypeVector;
  mutable TypeVector            type_vector_;
};

inline void StaticTypeInference::AddType( std::uint32_t id , TypeKind type ) {
  if(type_vector_.size() <= id)
    type_vector_.resize(id+1);
  type_vector_[id] = type;
}

} // namespace hir
} // namespace cbase
} // namespace lavascript

#endif // CBASE_STATIC_TYPE_INFERENCE_H_
