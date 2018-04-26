#ifndef CBASE_HIR_UPVALUE_H_
#define CBASE_HIR_UPVALUE_H_
#include "memory.h"

namespace lavascript {
namespace cbase      {
namespace hir        {

class UGet : public MemoryNode {
 public:
  inline static UGet* New( Graph* , std::uint8_t , std::uint32_t );
  std::uint8_t index()   const { return index_;  }
  std::uint32_t method() const { return method_; }

  UGet( Graph* graph , std::uint32_t id , std::uint8_t index , std::uint32_t method ):
    MemoryNode (HIR_UGET,id,graph),
    index_ (index),
    method_(method)
  {}
 public:
  virtual std::uint64_t GVNHash() const {
    return GVNHash1(type_name(),index());
  }
  virtual bool Equal( const Expr* that ) const {
    return that->IsUGet() && that->AsUGet()->index() == index();
  }
 private:
  std::uint8_t index_;
  std::uint32_t method_;
  LAVA_DISALLOW_COPY_AND_ASSIGN(UGet)
};

class USet : public Expr {
 public:
  inline static USet* New( Graph* , std::uint8_t , std::uint32_t , Expr* opr );
  std::uint32_t method() const { return method_; }
  std::uint8_t  index () const { return index_ ; }
  Expr* value() const { return operand_list()->First();  }

  virtual std::uint64_t GVNHash() const {
    return GVNHash2(type_name(),method(),value()->GVNHash());
  }

  virtual bool Equal( const USet* that ) const {
    if(that->IsUSet()) {
      auto that_uset = that->AsUSet();
      return that_uset->method() == method() && that_uset->value()->Equal(value());
    }
    return false;
  }

  USet( Graph* graph , std::uint8_t id , std::uint8_t index , std::uint32_t method , Expr* value ):
    Expr    (HIR_USET,id,graph),
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
