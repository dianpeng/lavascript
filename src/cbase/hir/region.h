#ifndef CBASE_HIR_REGION_H_
#define CBASE_HIR_REGION_H_
#include "control-flow.h"

namespace lavascript {
namespace cbase      {
namespace hir        {

// wraper object for phi node to work around the multiple inheritance pitfall
class PhiNode {
 public:
  PhiNode( Expr* phi ): phi_(phi) {
    lava_debug(NORMAL,lava_verify(phi->Is<ValuePhi>() || phi->Is<EffectMergeBase>()););
  }

  inline void set_region ( Merge* );
  inline Merge*    region() const;
  inline void ResetRegion();
  Expr* phi              () const { return phi_;                        }
  bool  IsValuePhi       () const { return phi_->Is<ValuePhi>();        }
  bool  IsEffectMergeBase() const { return phi_->Is<EffectMergeBase>(); }
 private:
  Expr* phi_;
};

LAVA_CBASE_HIR_DEFINE(NO_META,Merge,public ControlFlow) {
 public:
  inline Merge( IRType , std::uint32_t , Graph* , ControlFlow* region = NULL );
  inline void AddPhi   ( PhiNode );
  inline void RemovePhi( PhiNode );
  const zone::Vector<PhiNode>* phi_list() const { return &phi_list_; }
 private:
  zone::Vector<PhiNode> phi_list_;
};

LAVA_CBASE_HIR_DEFINE(Tag=REGION;Name="region";Leaf=NoLeaf;Effect=NoEffect,
    Region,public ControlFlow) {
 public:
  inline static Region* New( Graph* );
  inline static Region* New( Graph* , ControlFlow* );
  Region( Graph* graph , std::uint32_t id ): ControlFlow(HIR_REGION,id,graph) {}
 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(Region)
};

// Fail node represents abnormal way to abort the execution. The most common reason
// is because we failed at type guard or obviouse code bug.
LAVA_CBASE_HIR_DEFINE(Tag=FAIL;Name="fail";Leaf=NoLeaf;Effect=NoEffect,
    Fail,public Merge) {
 public:
  inline static Fail* New( Graph* );
  Fail( Graph* graph , std::uint32_t id ): Merge(HIR_FAIL,id,graph) {}
 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(Fail)
};

LAVA_CBASE_HIR_DEFINE(Tag=SUCCESS;Name="success";Leaf=NoLeaf;Effect=NoEffect,
    Success,public Merge) {
 public:
  inline static Success* New( Graph* );
  Expr* return_value() const { return operand_list()->First(); }
  Success( Graph* graph , std::uint32_t id ): Merge(HIR_SUCCESS,id,graph) {}
 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(Success)
};

// Special node of the graph
LAVA_CBASE_HIR_DEFINE(Tag=START;Name="start";Leaf=NoLeaf;Effect=NoEffect,
    Start,public ControlFlow) {
 public:
  inline static Start* New( Graph* );
  Start( Graph* graph , std::uint32_t id ): ControlFlow(HIR_START,id,graph) {}
 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(Start)
};

LAVA_CBASE_HIR_DEFINE(Tag=END;Name="end";Leaf=NoLeaf;Effect=NoEffect,
    End,public ControlFlow) {
 public:
  inline static End* New( Graph* , Success* , Fail* );
  Success* success() const { return backward_edge()->First()->AsSuccess(); }
  Fail*    fail()    const { return backward_edge()->Last ()->AsFail   (); }
  End( Graph* graph , std::uint32_t id , Success* s , Fail* f ):
    ControlFlow(HIR_END,id,graph)
  {
    AddBackwardEdge(s);
    AddBackwardEdge(f);
  }
 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(End)
};

LAVA_CBASE_HIR_DEFINE(Tag=OSR_START;Name="osr_start";Leaf=NoLeaf;Effect=NoEffect,
    OSRStart,public ControlFlow) {
 public:
  inline static OSRStart* New( Graph* );

  OSRStart( Graph* graph  , std::uint32_t id ):
    ControlFlow(HIR_OSR_START,id,graph)
  {}

 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(OSRStart)
};

LAVA_CBASE_HIR_DEFINE(Tag=OSR_END;Name="osr_end";Leaf=NoLeaf;Effect=NoEffect,
    OSREnd,public ControlFlow) {
 public:
  inline static OSREnd* New( Graph* , Success* succ , Fail* f );

  Success* success() const { return backward_edge()->First()->AsSuccess(); }
  Fail*    fail   () const { return backward_edge()->Last()->AsFail(); }

  OSREnd( Graph* graph , std::uint32_t id , Success* succ , Fail* f ):
    ControlFlow(HIR_OSR_END,id,graph)
  {
    AddBackwardEdge(succ);
    AddBackwardEdge(f);
  }

 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(OSREnd)
};

LAVA_CBASE_HIR_DEFINE(Tag=INLINE_START;Name="inline_start";Leaf=NoLeaf;Effect=NoEffect,
    InlineStart,public ControlFlow) {
 public:
  inline static InlineStart* New( Graph* , ControlFlow* );

  InlineStart( Graph* graph , std::uint32_t id , ControlFlow* region ):
    ControlFlow (HIR_INLINE_START,id,graph,region)
  {}

 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(InlineStart)
};

LAVA_CBASE_HIR_DEFINE(Tag=INLINE_END;Name="inline_end";Leaf=NoLeaf;Effect=NoEffect,
    InlineEnd,public Merge) {
 public:
  inline static InlineEnd* New( Graph* , ControlFlow* );
  inline static InlineEnd* New( Graph* );

  InlineEnd( Graph* graph , std::uint32_t id , ControlFlow* region ):
    Merge(HIR_INLINE_END,id,graph,region)
  {}

  InlineEnd( Graph* graph , std::uint32_t id ):
    Merge(HIR_INLINE_END,id,graph)
  {}
 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(InlineEnd)
};

} // namespace hir
} // namespace cbase
} // namespace lavascript

#endif // CBASE_HIR_REGION_H_
