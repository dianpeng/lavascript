#ifndef CBASE_HIR_LOOP_H_
#define CBASE_HIR_LOOP_H_
#include "expr.h"
#include "region.h"

namespace lavascript {
namespace cbase      {
namespace hir        {

// --------------------------------------------------------------------------
//
// Loop related blocks
//
// --------------------------------------------------------------------------
LAVA_CBASE_HIR_DEFINE(Tag=LOOP_HEADER;Name="loop_header";Leaf=NoLeaf,
    LoopHeader,public ControlFlow) {
 public:
  inline static LoopHeader* New( Graph* , ControlFlow* );

  Expr* condition() const { return operand_list()->First(); }
  void set_condition( Expr* condition ) {
    lava_debug(NORMAL,lava_verify(operand_list()->empty()););
    AddOperand(condition);
  }

  ControlFlow* merge() const { return merge_; }
  void set_merge( ControlFlow* merge ) { merge_ = merge; }

  LoopHeader( Graph* graph , std::uint32_t id , ControlFlow* region ):
    ControlFlow(HIR_LOOP_HEADER,id,graph,region)
  {}
 private:
  ControlFlow* merge_;
  LAVA_DISALLOW_COPY_AND_ASSIGN(LoopHeader);
};

LAVA_CBASE_HIR_DEFINE(Tag=LOOP;Name="loop";Leaf=NoLeaf,
    Loop,public EffectMergeRegion) {
 public:
  inline static Loop* New( Graph* );

  Loop( Graph* graph , std::uint32_t id ):
    EffectMergeRegion(HIR_LOOP,id,graph),loop_exit_(NULL) {}

  void set_loop_effect_start( LoopEffectStart* n ) { AddEffectMerge(n); }
  LoopEffectStart* set_loop_effect_start() const   { return effect_merge_list()->First()->As<LoopEffectStart>(); }

  void      set_loop_exit( LoopExit* loop_exit )   { loop_exit_ = loop_exit; }
  LoopExit* loop_exit    () const                  { return loop_exit_; }
 private:
  LoopExit* loop_exit_;
  LAVA_DISALLOW_COPY_AND_ASSIGN(Loop)
};

LAVA_CBASE_HIR_DEFINE(Tag=LOOP_EXIT;Name="loop_exit";Leaf=NoLeaf,
    LoopExit,public EffectMergeRegion) {
 public:
  inline static LoopExit* New( Graph* , Expr* );
  Expr* condition() const { return operand_list()->First(); }
  LoopExit( Graph* graph , std::uint32_t id , Expr* cond ):
    EffectMergeRegion(HIR_LOOP_EXIT,id,graph)
  {
    AddOperand(cond);
  }
 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(LoopExit)
};

LAVA_CBASE_HIR_DEFINE(Tag=LOOP_MERGE;Name="loop_merge";Leaf=NoLeaf,
    LoopMerge,public EffectMergeRegion) {
 public:
  inline static LoopMerge* New( Graph* );
  LoopMerge( Graph* graph , std::uint32_t id ):
    EffectMergeRegion(HIR_LOOP_MERGE,id,graph) {}
 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(LoopMerge)
};

} // namespace hir
} // namespace cbase
} // namespace lavascript
#endif // CBASE_HIR_LOOP_H_
