#ifndef CBASE_HIR_GUARD_H_
#define CBASE_HIR_GUARD_H_
#include "expr.h"

namespace lavascript {
namespace cbase      {
namespace hir        {

/* -------------------------------------------------------
 * Testing node , used with Guard node or If node
 * ------------------------------------------------------*/
LAVA_CBASE_HIR_DEFINE(NO_META,Test,public Expr) {
 public:
  // return the object this test node test against with
  virtual Expr* object() const = 0;
 protected:
  Test( IRType type , std::uint32_t id , Graph* g ): Expr(type,id,g) {}
};

LAVA_CBASE_HIR_DEFINE(Tag=TEST_TYPE;Name="test_type";Leaf=NoLeaf;Effect=NoEffect,
    TestType,public Test) {
 public:
  inline static TestType* New( Graph* , TypeKind , Expr* );
  TypeKind type_kind() const { return type_kind_; }
  const char* type_kind_name() const { return GetTypeKindName(type_kind_); }
  virtual Expr* object() const { return operand_list()->First(); }

  TestType( Graph* graph , std::uint32_t id , TypeKind tc , Expr* obj ):
    Test(HIR_TEST_TYPE,id,graph),
    type_kind_(tc)
  {
    AddOperand(obj);
  }
 public:
  virtual std::uint64_t GVNHash() const {
    return GVNHash2(type_name(),type_kind(),object()->GVNHash());
  }
  virtual bool Equal( const Expr* that ) const {
    if(that->IsTestType()) {
      auto n = that->AsTestType();
      return type_kind() == n->type_kind() && object()->Equal(n->object());
    }
    return false;
  }
 private:
  TypeKind type_kind_;
  LAVA_DISALLOW_COPY_AND_ASSIGN(TestType)
};

// -----------------------------------------------------
// Guard
// -----------------------------------------------------
LAVA_CBASE_HIR_DEFINE(Tag=GUARD;Name="guard";Leaf=NoLeaf;Effect=NoEffect,
    Guard,public Expr) {
 public:
  inline static Guard* New( Graph* , Test* , Checkpoint* );
  Test*             test() const { return operand_list()->First()->As<Test>(); }
  Expr*           object() const { return test()->object(); }
  Checkpoint* checkpoint() const { return operand_list()->Last()->AsCheckpoint(); }

  Guard( Graph* graph , std::uint32_t id , Test* test , Checkpoint* cp ):
    Expr(HIR_GUARD,id,graph)
  {
    AddOperand(test);
    AddOperand(cp);
  }
 public:
  // Checkpoint doesn't participate in GVN hash function. Checkpoint doesn't mean
  // anything since it is not neither a operand used by node , ie data dependent ,
  // nor a side effect can be observed , ie effect dependent. It just means when
  // this node bailout, it uses this Checkpoint as a way to reconstruct interpreter's
  // state.
  virtual std::uint64_t GVNHash() const {
    return GVNHash1(type_name(),test()->GVNHash());
  }
  virtual bool Equal( const Expr* that ) const {
    if(that->IsGuard()) {
      auto n = that->AsGuard();
      return test()->Equal(n->test());
    }
    return false;
  }
 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(Guard)
};

} // namespace hir
} // namespace cbase
} // namespace lavascript

#endif // CBASE_HIR_GUARD_H_
