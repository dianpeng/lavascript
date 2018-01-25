#ifndef CBASE_OPTIMIZATION_LOWER_H_
#define CBASE_OPTIMIZATION_LOWER_H_

#include "src/type-trace.h"
#include "src/cbase/type.h"

/**
 * lower pass for HIR
 *
 * This pass basically does following things :
 *   1) based on static type analyzing and also TypeTrace object
 *      mark certain operation to be specualative executed node.
 *
 *      Rest of the nodes that cannot be speculative executed will
 *      do a full execution based on polymorphic operator
 *
 *   2) add guard for each speculative executed node and also add
 *      index oob check for array/list access
 */

namespace lavascript {
namespace cbase      {
namespace hir        {


} // namespace hir
} // namespace cbase
} // namespace lavascript

#endif // CBASE_OPTIMIZATION_LOWER_H_
