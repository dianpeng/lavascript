#ifndef CBASE_HIR_PHI_H_
#define CBASE_HIR_PHI_H_
#include "expr.h"

namespace lavascript {
namespace cbase      {
namespace hir        {

// Phi node
LAVA_CBASE_HIR_DEFINE(HIR_INTERNAL,PhiBase,public Expr) {
 public:
  // Set the bounded region, only applicable when the region is not set
  inline void set_region( Merge* );
  // Get the boundede region
  Merge* region() const { return region_; }
  // Reset the region to be NULL
  void ResetRegion() { region_ = NULL; }
  // Check if this Phi node is not used. We cannot use HasRef function since
  // a Phi node may added to a region during setup time and there will be one
  // ref inside of the RefList. We just need to check that
  inline bool IsUsed() const;
 public:
  // Remove the phi node from its belonged region. The reason this one is just
  // a static function is because this function *doesn't* touch its ref_list,
  // so its ref_list will still have its belonged region's reference there and
  // it is invalid. This function should be used under strict condition.
  static inline void RemovePhiFromRegion( PhiBase* );
  // Bounded control flow region node.
  // Each phi node is bounded to a control flow regional node
  // and by this we can easily decide which region contributs
  // to a certain input node of Phi node
  PhiBase( IRType type , std::uint32_t id , Graph* graph ):
    Expr(type,id,graph) ,
    region_(NULL)
  {}
 private:
  Merge* region_;
};

// Normal value phi node. Used in the merged region for join value produced by
// different branch in control flow graph.
LAVA_CBASE_HIR_DEFINE(Tag=PHI;Name="phi";Leaf=NoLeaf,
    Phi,public PhiBase) {
 public:
  inline static Phi* New( Graph* );
  inline static Phi* New( Graph* , Expr* , Expr* );
  inline static Phi* New( Graph* , Merge*  );
  inline static Phi* New( Graph* , Expr* , Expr* , Merge* );
  // Bounded control flow region node.
  // Each phi node is bounded to a control flow regional node
  // and by this we can easily decide which region contributs
  // to a certain input node of Phi node
  Phi( Graph* graph , std::uint32_t id ): PhiBase( HIR_PHI , id , graph ) {}

 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(Phi)
};

// Loop induction variables. This node is specifically used for representing
// a loop induction variable which is essentially just a Phi node.
//
// This is a normal loopiv node. We also have specialized loop iv node which
// implicitly have type tagged to simplify type inference
LAVA_CBASE_HIR_DEFINE(Tag=LOOP_IV;Name="loop_iv";Leaf=NoLeaf,
    LoopIV, public PhiBase ) {
 public:
  inline static LoopIV* New( Graph* );
  inline static LoopIV* New( Graph* , Expr* , Expr* );
  inline static LoopIV* New( Graph* , Loop* );
  inline static LoopIV* New( Graph* , Expr* , Expr* , Loop* );

  LoopIV( Graph* graph , std::uint32_t id ): PhiBase( HIR_LOOP_IV , id, graph ) {}
 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(LoopIV)
};

// LoopIVInt node is a specialized loop induction variable node and it is in *UNBOXED*
// type. This node will only be generated during the loop induction phase. And again,
// this node is *UNBOXED*. The boxed value simply cannot have a integer type internally
LAVA_CBASE_HIR_DEFINE(Tag=LOOP_IV_INT64;Name="loop_iv_int64";Leaf=NoLeaf;Box=Unbox,
    LoopIVInt64, public PhiBase ) {
 public:
  inline static LoopIVInt64* New( Graph* );
  inline static LoopIVInt64* New( Graph* , Expr* , Expr* );
  inline static LoopIVInt64* New( Graph* , Loop* );
  inline static LoopIVInt64* New( Graph* , Expr* , Expr* , Loop* );
  LoopIVInt64( Graph* graph , std::uint32_t id ): PhiBase( HIR_LOOP_IV_INT64 , id, graph ) {}
  private:
   LAVA_DISALLOW_COPY_AND_ASSIGN(LoopIVInt64)
};

// This is the typped the LoopIV node. It is specialized with float64 type and it is in
// *BOXED* type.
LAVA_CBASE_HIR_DEFINE(Tag=LOOP_IV_FLOAT64;Name="loop_iv_float64";Leaf=NoLeaf;Box=Both,
    LoopIVFloat64, public PhiBase ) {
  public:
   inline static LoopIVFloat64* New( Graph* );
   inline static LoopIVFloat64* New( Graph* , Expr* , Expr* );
   inline static LoopIVFloat64* New( Graph* , Loop* );
   inline static LoopIVFloat64* New( Graph* , Expr* , Expr* , Loop* );
   LoopIVFloat64( Graph* graph , std::uint32_t id ) : PhiBase(HIR_LOOP_IV_FLOAT64, id, graph ) {}
  private:
   LAVA_DISALLOW_COPY_AND_ASSIGN(LoopIVFloat64)
};

LAVA_CBASE_HIR_DEFINE(Tag=PROJECTION;Name="projection";Leaf=NoLeaf,
    Projection,public Expr) {
 public:
  inline static Projection* New( Graph* , Expr* , std::uint32_t index );
  Expr* operand() const { return operand_list()->First(); }
  // a specific value to indicate which part of the input operand
  // needs to be projected
  std::uint32_t index() const { return index_; }
  Projection( Graph* graph , std::uint32_t id , Expr* operand , std::uint32_t index ):
    Expr  (HIR_PROJECTION,id,graph),
    index_(index)
  {
    AddOperand(operand);
  }
  virtual std::uint64_t GVNHash() const {
    return GVNHash1(type_name(),index());
  }
  virtual bool Equal( const Expr* that ) const {
    return that->Is<Projection>() && (that->As<Projection>()->index() == index());
  }
 private:
  std::uint32_t index_;
  LAVA_DISALLOW_COPY_AND_ASSIGN(Projection)
};

} // namespace hir
} // namespace cbase
} // namespace lavascript

#endif // CBASE_HIR_PHI_H_
