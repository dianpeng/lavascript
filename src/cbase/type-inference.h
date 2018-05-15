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

// Do simple type inference based on the input type
TypeKind GetTypeInference( Expr* );

// evaluate expr node under boolean context
bool GetBooleanValue     ( Expr* , bool* );

} // namespace hir
} // namespace cbase
} // namespace lavascript

#endif // CBASE_TYPE_INFERENCE_H_
