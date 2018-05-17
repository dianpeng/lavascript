#ifndef CBASE_AA_H_
#define CBASE_AA_H_

#include "hir.h"
#include "src/all-static.h"

namespace lavascript {
namespace cbase      {
namespace hir        {

/**
 * Alias Analysis
 *
 * We have a relative simple AA process . In weak type language, do deep
 * AA is extreamly complicated since everything is dynamic. To make AA
 * possible, I have already refactoried many times for the HIR to try to
 * simplify this process and carry out as much information as possible.
 *
 */
class AA : public AllStatic {
 public:
  enum { AA_NOT , AA_MAY , AA_MUST };

  // Query alias information against 2 field reference node. Assuming
  static int Query( const FieldRefNode& , const FieldRefNode& );

  // Query a memory with type Object represented by Expr* node against
  // an Operation to see whether the operation node EffectBarrier uses
  // the input memory node or not
  static int QueryObject( Expr* , EffectBarrier* );

  // Query a memory with type List represented by Expr* node against
  // an Operation to see whether the operation node EffectBarrier uses
  // the input memory node or not
  static int QueryList  ( Expr* , EffectBarrier* );

 private:
  // Query the alias information against 2 nodes. 1st argument represents
  // the first memory , 2nd argument represents an operation wrt a certain
  // memory.
  static int Query( Expr* , EffectBarrier* , TypeKind );
};

} // namespace hir
} // namespace cbase
} // namespace lavascript

#endif // CBASE_AA_H_
