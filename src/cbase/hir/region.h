#ifndef CBASE_HIR_REGION_H_
#define CBASE_HIR_REGION_H_
#include "control-flow.h"

namespace lavascript {
namespace cbase      {
namespace hir        {

class PhiBase;
class EffectMergeBase;

LAVA_CBASE_HIR_DEFINE(HIR_INTERNAL,Merge,public ControlFlow) {
 public:
  inline Merge( IRType , std::uint32_t , Graph* , ControlFlow* region = NULL );
  inline bool ReplacePhi( PhiBase* , PhiBase* );
  inline void AddPhi    ( PhiBase* );
  inline void RemovePhi ( PhiBase* );
  const zone::Vector<PhiBase*>* phi_list() const { return &phi_list_; }
 private:
  zone::Vector<PhiBase*> phi_list_;
};

LAVA_CBASE_HIR_DEFINE(HIR_INTERNAL,EffectMergeRegion,public Merge) {
 public:
  inline EffectMergeRegion( IRType , std::uint32_t , Graph* , ControlFlow* region = NULL );

  inline bool ReplaceEffectMerge( EffectMergeBase* , EffectMergeBase* );
  inline void AddEffectMerge    ( EffectMergeBase* );
  inline void RemoveEffectMerge ( EffectMergeBase* );
  const zone::Vector<EffectMergeBase*>* effect_merge_list() const { return &effect_merge_list_; }
 private:
  zone::Vector<EffectMergeBase*> effect_merge_list_;
};

LAVA_CBASE_HIR_DEFINE(Tag=REGION;Name="region";Leaf=NoLeaf,
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
LAVA_CBASE_HIR_DEFINE(Tag=FAIL;Name="fail";Leaf=NoLeaf,
    Fail,public ControlFlow) {
 public:
  inline static Fail* New( Graph* );
  Fail( Graph* graph , std::uint32_t id ): ControlFlow(HIR_FAIL,id,graph) {}
 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(Fail)
};

LAVA_CBASE_HIR_DEFINE(Tag=SUCCESS;Name="success";Leaf=NoLeaf,
    Success,public Merge) {
 public:
  inline static Success* New( Graph* );
  Expr* return_value() const { return operand_list()->First(); }
  Success( Graph* graph , std::uint32_t id ): Merge(HIR_SUCCESS,id,graph) {}
 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(Success)
};

// Special node of the graph
LAVA_CBASE_HIR_DEFINE(Tag=START;Name="start";Leaf=NoLeaf,
    Start,public ControlFlow) {
 public:
  inline static Start* New( Graph* , Checkpoint* cp , InitBarrier* ib );
  Start( Graph* graph , std::uint32_t id , Checkpoint* cp , InitBarrier* ib ):
    ControlFlow(HIR_START,id,graph) {
    AddOperand(cp);
    AddOperand(ib);
  }
  Checkpoint*    checkpoint() const { return operand_list()->First()->As<Checkpoint>(); }
  InitBarrier* init_barrier() const { return operand_list()->Last()->As<InitBarrier>(); }
 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(Start)
};

LAVA_CBASE_HIR_DEFINE(Tag=END;Name="end";Leaf=NoLeaf,
    End,public ControlFlow) {
 public:
  inline static End* New( Graph* , Success* , Fail* );
  Success* success() const { return backward_edge()->First()->As<Success>(); }
  Fail*    fail()    const { return backward_edge()->Last ()->As<Fail>   (); }
  End( Graph* graph , std::uint32_t id , Success* s , Fail* f ):
    ControlFlow(HIR_END,id,graph)
  {
    AddBackwardEdge(s);
    AddBackwardEdge(f);
  }
 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(End)
};

LAVA_CBASE_HIR_DEFINE(Tag=OSR_START;Name="osr_start";Leaf=NoLeaf,
    OSRStart,public ControlFlow) {
 public:
  inline static OSRStart* New( Graph* , Checkpoint* , InitBarrier* );

  OSRStart( Graph* graph  , std::uint32_t id , Checkpoint* cp , InitBarrier* ib ):
    ControlFlow(HIR_OSR_START,id,graph)
  {
    AddOperand(cp);
    AddOperand(ib);
  }

  Checkpoint*    checkpoint() const { return operand_list()->First()->As<Checkpoint>(); }
  InitBarrier* init_barrier() const { return operand_list()->Last()->As<InitBarrier>(); }
 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(OSRStart)
};

LAVA_CBASE_HIR_DEFINE(Tag=OSR_END;Name="osr_end";Leaf=NoLeaf,
    OSREnd,public ControlFlow) {
 public:
  inline static OSREnd* New( Graph* , Success* succ , Fail* f );

  Success* success() const { return backward_edge()->First()->As<Success>(); }
  Fail*    fail   () const { return backward_edge()->Last()->As<Fail>(); }

  OSREnd( Graph* graph , std::uint32_t id , Success* succ , Fail* f ):
    ControlFlow(HIR_OSR_END,id,graph)
  {
    AddBackwardEdge(succ);
    AddBackwardEdge(f);
  }

 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(OSREnd)
};

LAVA_CBASE_HIR_DEFINE(Tag=INLINE_START;Name="inline_start";Leaf=NoLeaf,
    InlineStart,public ControlFlow) {
 public:
  inline static InlineStart* New( Graph* , ControlFlow* );

  InlineStart( Graph* graph , std::uint32_t id , ControlFlow* region ):
    ControlFlow (HIR_INLINE_START,id,graph,region)
  {}

 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(InlineStart)
};

LAVA_CBASE_HIR_DEFINE(Tag=INLINE_END;Name="inline_end";Leaf=NoLeaf,
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
