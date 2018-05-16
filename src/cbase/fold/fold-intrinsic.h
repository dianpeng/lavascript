#ifndef CBASE_FOLD_INTRINSIC_H_
#define CBASE_FOLD_INTRINSIC_H_
#include <memory>

namespace lavascript {
namespace cbase      {
namespace hir        {

class Folder;
class Graph;
class Expr;
class ICall;

Expr* FoldIntrinsicCall( Graph* , ICall* );

} // namespace lavascript
} // namespace cbase
} // namespace lavascript

#endif // CBASE_FOLD_INTRINSIC_H_
