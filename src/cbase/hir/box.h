#ifndef CBASE_HIR_BOX_H_
#define CBASE_HIR_BOX_H_
#include "expr.h"

namespace lavascript {
namespace cbase      {
namespace hir        {

// -------------------------------------------------------------------------
//  Box/Unbox
// -------------------------------------------------------------------------
LAVA_CBASE_HIR_DEFINE(Tag=BOX;Name="box";Leaf=NoLeaf,
    Box,public Expr) {
 public:
  inline static Box* New( Graph* , Expr* , TypeKind );
  Expr* value() const { return operand_list()->First(); }
  TypeKind type_kind() const { return type_kind_; }
  Box( Graph* graph , std::uint32_t id , Expr* object , TypeKind tk ):
    Expr(HIR_BOX,id,graph),
    type_kind_(tk)
  {
    AddOperand(object);
  }

 public:
  virtual std::uint64_t GVNHash() const {
    return GVNHash1(type_name(),value()->GVNHash());
  }

  virtual bool Equal( const Expr* that ) const {
    if(that->Is<Box>()) {
      auto that_box = that->As<Box>();
      return value()->Equal(that_box->value());
    }
    return false;
  }

 private:
  TypeKind type_kind_;
  LAVA_DISALLOW_COPY_AND_ASSIGN(Box)
};

LAVA_CBASE_HIR_DEFINE(Tag=UNBOX;Name="unbox";Leaf=NoLeaf;Box=Unbox,
    Unbox,public Expr) {
 public:
  inline static Unbox* New( Graph* , Expr* , TypeKind );
  Expr* value() const { return operand_list()->First(); }
  TypeKind type_kind() const { return type_kind_; }
  Unbox( Graph* graph , std::uint32_t id , Expr* object , TypeKind tk ):
    Expr(HIR_UNBOX,id,graph),
    type_kind_(tk)
  {
    AddOperand(object);
  }
 public:
  virtual std::uint64_t GVNHash() const {
    return GVNHash1(type_name(),value()->GVNHash());
  }
  virtual bool Equal( const Expr* that ) const {
    if(that->Is<Unbox>()) {
      auto that_unbox = that->As<Unbox>();
      return value()->Equal(that_unbox->value());
    }
    return false;
  }
 private:
  TypeKind type_kind_;
  LAVA_DISALLOW_COPY_AND_ASSIGN(Unbox)
};

} // namespace hir
} // namespace cbase
} // namespace lavascript

#endif // CBASE_HIR_BOX_H_
