#ifndef CBASE_STATIC_TYPE_INFERENCE_H_
#define CBASE_STATIC_TYPE_INFERENCE_H_
#include "ool-vector.h"

#include "src/cbase/type.h"

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

class Expr;
class ICall;

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
  void AddType( std::uint32_t id , TypeKind tk ) { type_vector_[id] = tk; }

  // Get the type of this node
  TypeKind GetType( Expr* type ) const;

 private:
  typedef OOLVector<TypeKind>   TypeVector;
  mutable TypeVector            type_vector_;
};


} // namespace hir
} // namespace cbase
} // namespace lavascript

#endif // CBASE_STATIC_TYPE_INFERENCE_H_
