#ifndef CBASE_HIR_ARG_H_
#define CBASE_HIR_ARG_H_
#include "expr.h"

namespace lavascript {
namespace cbase      {
namespace hir        {

// Argument
LAVA_CBASE_HIR_DEFINE(Tag=ARG;Name="arg";Leaf=Leaf;Effect=Effect,
    Arg,public Expr) {
 public:
  inline static Arg* New( Graph* , std::uint32_t );
  std::uint32_t index() const { return index_; }
  Arg( Graph* graph , std::uint32_t id , std::uint32_t index ):
    Expr(HIR_ARG,id,graph),
    index_(index)
  {}
 public:
  virtual std::uint64_t GVNHash() const {
    return GVNHash1(type_name(),index());
  }
  virtual bool Equal( const Expr* that ) const {
    return that->IsArg() && (that->AsArg()->index() == index());
  }
 private:
  std::uint32_t index_;
  LAVA_DISALLOW_COPY_AND_ASSIGN(Arg)
};

} // namespace hir
} // namespace cbase
} // namespace lavascript

#endif // CBASE_HIR_ARG_H_
