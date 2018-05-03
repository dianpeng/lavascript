#ifndef CBASE_HIR_GLOBAL_H_
#define CBASE_HIR_GLOBAL_H_
#include "memory.h"

namespace lavascript {
namespace cbase      {
namespace hir        {

// -------------------------------------------------------------------------
// global set/get (side effect)
// -------------------------------------------------------------------------
class GGet : public MemoryRead {
 public:
  inline static GGet* New( Graph* , Expr* );
  Expr* key() const { return operand_list()->First(); }

  GGet( Graph* graph , std::uint32_t id , Expr* name ):
    MemoryRead (HIR_GGET,id,graph)
  {
    AddOperand(name);
  }
 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(GGet)
};

class GSet : public MemoryWrite {
 public:
  inline static GSet* New( Graph* , Expr* key , Expr* value );
  Expr* key () const { return operand_list()->First(); }
  Expr* value()const { return operand_list()->Last() ; }
  GSet( Graph* graph , std::uint32_t id , Expr* key , Expr* value ):
    MemoryWrite (HIR_GSET,id,graph)
  {
    AddOperand(key);
    AddOperand(value);
  }
 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(GSet)
};

} // namespace hir
} // namespace cbase
} // namespace lavascript

#endif // CBASE_HIR_GLOBAL_H_
