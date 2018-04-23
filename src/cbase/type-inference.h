#ifndef CBASE_TYPE_INFERENCE_H_
#define CBASE_TYPE_INFERENCE_H_
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

// Do type inference based on TypeAnnotation node. The graph itself is annotated
// with type already so no external information is needed to do type inference.
TypeKind GetTypeInference( Expr* );

} // namespace hir
} // namespace cbase
} // namespace lavascript

#endif // CBASE_TYPE_INFERENCE_H_
