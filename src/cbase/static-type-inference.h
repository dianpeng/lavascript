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
 * Do a static type inference according to the node type and its implicit
 * indication. This is used to help us do speculative type assertion
 */
TypeKind GetStaticTypeInference( Expr* );


} // namespace hir
} // namespace cbase
} // namespace lavascript

#endif // CBASE_STATIC_TYPE_INFERENCE_H_
