#ifndef CBASE_HIR_ITR_H_
#define CBASE_HIR_ITR_H_
#include "effect.h"

namespace lavascript {
namespace cbase      {
namespace hir        {

// -------------------------------------------------------------------------
// Iterator node (side effect)
// -------------------------------------------------------------------------
LAVA_CBASE_HIR_DEFINE(Tag=ITR_NEW;Name="itr_new";Leaf=NoLeaf;Effect=Effect,
    ItrNew,public HardBarrier) {
 public:
  inline static ItrNew* New( Graph* , Expr* );
  Expr* operand() const { return operand_list()->First(); }

  ItrNew( Graph* graph , std::uint32_t id , Expr* operand ):
    HardBarrier(HIR_ITR_NEW,id,graph)
  {
    AddOperand(operand);
  }
 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(ItrNew)
};

LAVA_CBASE_HIR_DEFINE(Tag=ITR_NEXT;Name="itr_next";Leaf=NoLeaf;Effect=Effect,
    ItrNext,public HardBarrier) {
 public:
  inline static ItrNext* New( Graph* , Expr* );
  Expr* operand() const { return operand_list()->First(); }

  ItrNext( Graph* graph , std::uint32_t id , Expr* operand ):
    HardBarrier(HIR_ITR_NEXT,id,graph)
  {
    AddOperand(operand);
  }
 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(ItrNext)
};

LAVA_CBASE_HIR_DEFINE(Tag=ITR_TEST;Name="itr_test";Leaf=NoLeaf;Effect=Effect,
    ItrTest,public HardBarrier) {
 public:
  inline static ItrTest* New( Graph* , Expr* );
  Expr* operand() const { return operand_list()->First(); }

  ItrTest( Graph* graph , std::uint32_t id , Expr* operand ):
    HardBarrier(HIR_ITR_TEST,id,graph)
  {
    AddOperand(operand);
  }
 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(ItrTest)
};

LAVA_CBASE_HIR_DEFINE(Tag=ITR_DEREF;Name="itr_deref";Leaf=NoLeaf;Effect=Effect,
    ItrDeref,public HardBarrier) {
 public:
  enum {
    PROJECTION_KEY = 0,
    PROJECTION_VAL
  };
  inline static ItrDeref* New( Graph* , Expr* );
  Expr* operand() const { return operand_list()->First(); }

  ItrDeref( Graph* graph , std::uint32_t id , Expr* operand ):
    HardBarrier(HIR_ITR_DEREF,id,graph)
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
