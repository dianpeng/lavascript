#ifndef CBASE_HIR_CLS_H_
#define CBASE_HIR_CLS_H_
#include "expr.h"

namespace lavascript {
namespace cbase      {
namespace hir        {

// ----------------------------------------------------------------
// Closure
// ----------------------------------------------------------------
class LoadCls : public Expr {
 public:
  static inline LoadCls* New( Graph* , std::uint32_t ref );
  std::uint32_t ref() const { return ref_; }
  LoadCls( Graph* graph , std::uint32_t id , std::uint32_t ref ):
    Expr (HIR_LOAD_CLS,id,graph),
    ref_ (ref)
  {}
 private:
  std::uint32_t ref_;
  LAVA_DISALLOW_COPY_AND_ASSIGN(LoadCls);
};

class InitCls : public Expr {
 public:
  inline static InitCls* New( Graph* , Expr* );
  InitCls( Graph* graph , std::uint32_t id , Expr* key ):
    Expr (HIR_INIT_CLS,id,graph)
  {
    AddOperand(key);
  }
  Expr* key() const { return operand_list()->First(); }
 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(InitCls)
};

} // namespace hir
} // namespace cbase
} // namespace lavascript

#endif // CBASE_HIR_CLS_H_
