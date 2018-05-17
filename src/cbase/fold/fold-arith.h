#ifndef CBASE_FOLD_ARITH_H_
#define CBASE_FOLD_ARITH_H_
#include "src/cbase/hir.h"

#include <functional>
#include <memory>

namespace lavascript {
namespace cbase {
namespace hir {

class Folder;

// Exposed internal fold function for certain specific use case , genernally
// user should use FoldChain as interface to use fully fold optimization pipeline
Expr* FoldBinary   ( Graph* , Binary::Operator, Expr* , Expr* );
Expr* FoldTernary  ( Graph* , Expr* , Expr* , Expr* );

} // namespace hir
} // namespace cbase
} // namespace lavascript

#endif // CBASE_FOLD_ARITH_H_
