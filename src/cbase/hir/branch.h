#ifndef CBASE_HIR_BRANCH_H_
#define CBASE_HIR_BRANCH_H_
#include "region.h"

namespace lavascript {
namespace cbase      {
namespace hir        {

// -----------------------------------------------------------------------
//
// Branch
//
// Branch node in HIR basically has 2 types :
//
// 1) If related node , which basically represents a normal written if else
//    branch
//
// 2) Unconditional jump
// -----------------------------------------------------------------------

LAVA_CBASE_HIR_DEFINE(Tag=IF;Name="if";Leaf=NoLeaf,
    If,public ControlFlow) {
 public:
  inline static If* New( Graph* , Expr* , ControlFlow* );
  Expr* condition() const { return operand_list()->First(); }

  ControlFlow* merge() const { return merge_; }
  void set_merge( ControlFlow* merge ) { merge_ = merge; }

  If( Graph* graph , std::uint32_t id , Expr* cond , ControlFlow* region ):
    ControlFlow(HIR_IF,id,graph,region),
    merge_(NULL)
  {
    AddOperand(cond);
  }

 private:
  ControlFlow* merge_;
  LAVA_DISALLOW_COPY_AND_ASSIGN(If)
};

LAVA_CBASE_HIR_DEFINE(Tag=IF_TRUE;Name="if_true";Leaf=NoLeaf,
    IfTrue,public ControlFlow) {
 public:
  static const std::size_t kIndex = 1;

  inline static IfTrue* New( Graph* , ControlFlow* );
  inline static IfTrue* New( Graph* );

  void set_branch_start_effect( BranchStartEffect* n ) { AddOperand(n); }
  BranchStartEffect* branch_start_effect() const       {
    return operand_list()->First()->As<BranchStartEffect>();
  }

  IfTrue( Graph* graph , std::uint32_t id , ControlFlow* region ):
    ControlFlow(HIR_IF_TRUE,id,graph,region)
  {}

 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(IfTrue)
};

LAVA_CBASE_HIR_DEFINE(Tag=IF_FALSE;Name="if_false";Leaf=NoLeaf,
    IfFalse,public ControlFlow) {
 public:
  static const std::size_t kIndex = 0;

  inline static IfFalse* New( Graph* , ControlFlow* );
  inline static IfFalse* New( Graph* );

  void set_branch_start_effect( BranchStartEffect* n ) { AddOperand(n); }
  BranchStartEffect* branch_start_effect() const       {
    return operand_list()->First()->As<BranchStartEffect>();
  }

  IfFalse( Graph* graph , std::uint32_t id , ControlFlow* region ):
    ControlFlow(HIR_IF_FALSE,id,graph,region)
  {}

 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(IfFalse)
};

LAVA_CBASE_HIR_DEFINE(Tag=IF_MERGE;Name="if_merge";Leaf=NoLeaf,
    IfMerge,public EffectMergeRegion) {
 public:
  inline static IfMerge* New( Graph* , ControlFlow* );
  inline static IfMerge* New( Graph* );

  IfMerge( Graph* graph , std::uint32_t id , ControlFlow* region ):
    EffectMergeRegion(HIR_IF_MERGE,id,graph,region)
  {}

 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(IfMerge)
};

} // namespace hir
} // namespace cbase
} // namespace lavascript

#endif // CBASE_HIR_BRANCH_H_
