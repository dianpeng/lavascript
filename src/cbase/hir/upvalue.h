#ifndef CBASE_HIR_UPVALUE_H_
#define CBASE_HIR_UPVALUE_H_
#include "memory.h"

namespace lavascript {
namespace cbase      {
namespace hir        {

LAVA_CBASE_HIR_DEFINE(Tag=UGET;Name="uget";Leaf=Leaf;Effect=Effect,
    UGet,public ReadEffect) {
 public:
  inline static UGet* New( Graph* , std::uint8_t , std::uint32_t );
  std::uint8_t  index () const { return index_;  }
  std::uint32_t method() const { return method_; }

  UGet( Graph* graph , std::uint32_t id , std::uint8_t index , std::uint32_t method ):
    ReadEffect (HIR_UGET,id,graph),
    index_ (index),
    method_(method)
  {}
 private:
  std::uint8_t index_;
  std::uint32_t method_;
  LAVA_DISALLOW_COPY_AND_ASSIGN(UGet)
};

LAVA_CBASE_HIR_DEFINE(Tag=USET;Name="uset";Leaf=NoLeaf;Effect=Effect,
    USet,public WriteEffect) {
 public:
  inline static USet* New( Graph* , std::uint8_t , std::uint32_t , Expr* opr );
  std::uint8_t  method() const { return method_; }
  std::uint32_t index () const { return index_ ; }
  Expr* value() const { return operand_list()->First();  }

  USet( Graph* graph , std::uint8_t id , std::uint8_t index , std::uint32_t method , Expr* value ):
    WriteEffect (HIR_USET,id,graph),
    index_  (index),
    method_ (method)
  {
    AddOperand(value);
  }
 private:
  std::uint8_t  index_;
  std::uint32_t method_;
  LAVA_DISALLOW_COPY_AND_ASSIGN(USet)
};

} // namespace hir
} // namespace cbase
} // namespace lavascript

#endif // CBASE_HIR_UPVALUE_H_
