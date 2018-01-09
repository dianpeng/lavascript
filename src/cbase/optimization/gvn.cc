#include "gvn.h"

namespace lavascript {
namespace cbase {
namespace hir {

namespace {

/**
 * GVN hash table.
 *
 * A gvn hash table is just a hash table wrapper around std::unoredered_map
 * plus specific function to determine whether we can add it into the hash
 * table and then do the correct job.
 *
 * A hir::Expr* can return 0 as its hash value which basically *disable* GVN
 * pass on this node.
 *
 * One thing to note , though GVN is disabled but reduction is still left as
 * it is. The GVN pass also perform expression level reduction.
 */

class GVNImpl {
};

} // namespace
} // namespace hir
} // namespace cbase
} // namespace lavascript
