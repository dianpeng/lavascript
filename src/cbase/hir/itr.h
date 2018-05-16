#ifndef CBASE_HIR_ITR_H_
#define CBASE_HIR_ITR_H_
#include "expr.h"

namespace lavascript {
namespace cbase      {
namespace hir        {

// -------------------------------------------------------------------------
// Iterator node (side effect)
// -------------------------------------------------------------------------
LAVA_CBASE_HIR_DEFINE(ItrNew,public Expr) {
 public:
  inline static ItrNew* New( Graph* , Expr* );
  Expr* operand() const { return operand_list()->First(); }
  ItrNew( Graph* graph , std::uint32_t id , Expr* operand ):
    Expr  (HIR_ITR_NEW,id,graph)
  {
    AddOperand(operand);
  }
 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(ItrNew)
};

LAVA_CBASE_HIR_DEFINE(ItrNext,public Expr) {
 public:
  inline static ItrNext* New( Graph* , Expr* );
  Expr* operand() const { return operand_list()->First(); }
  ItrNext( Graph* graph , std::uint32_t id , Expr* operand ):
    Expr  (HIR_ITR_NEXT,id,graph)
  {
    AddOperand(operand);
  }
 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(ItrNext)
};

LAVA_CBASE_HIR_DEFINE(ItrTest,public Expr) {
 public:
  inline static ItrTest* New( Graph* , Expr* );
  Expr* operand() const { return operand_list()->First(); }
  ItrTest( Graph* graph , std::uint32_t id , Expr* operand ):
    Expr  (HIR_ITR_TEST,id,graph)
  {
    AddOperand(operand);
  }
 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(ItrTest)
};

LAVA_CBASE_HIR_DEFINE(ItrDeref,public Expr) {
 public:
  enum {
    PROJECTION_KEY = 0,
    PROJECTION_VAL
  };
  inline static ItrDeref* New( Graph* , Expr* );
  Expr* operand() const { return operand_list()->First(); }
  ItrDeref( Graph* graph , std::uint32_t id , Expr* operand ):
    Expr   (HIR_ITR_DEREF,id,graph)
  {
    AddOperand(operand);
  }
 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(ItrDeref)
};

} // namespace hir
} // namespace cbase
} // namespace lavascript

#endif // CBASE_HIR_ITR_H_
