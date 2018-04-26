#ifndef CBASE_HIR_CAST_H_
#define CBASE_HIR_CAST_H_
#include "expr.h"

namespace lavascript {
namespace cbase      {
namespace hir        {

// Cast an expression node into a *boxed* boolean value. This cast will always be
// successful due to our language's semantic. Any types of value has a corresponding
// boolean value.
class CastToBoolean : public Expr {
 public:
  inline static CastToBoolean* New ( Graph* , Expr* );
  // function to create a cast to boolean but negate its end result. this operation
  // basically means negate(unbox(cast_to_boolean(node)))
  inline static Expr* NewNegateCast( Graph* , Expr* );
  Expr* value() const { return operand_list()->First(); }

  CastToBoolean( Graph* graph , std::uint32_t id , Expr* value ):
    Expr( HIR_CAST_TO_BOOLEAN , id , graph )
  { AddOperand(value); }
 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(CastToBoolean)
};

} // namespace hir
} // namespace cbase
} // namespace lavascript

#endif // CBASE_HIR_CAST_H_
