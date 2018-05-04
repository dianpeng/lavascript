#ifndef CBASE_HIR_PHI_H_
#define CBASE_HIR_PHI_H_
#include "expr.h"

namespace lavascript {
namespace cbase      {
namespace hir        {

// Phi node
// A Phi node is a control flow related merged node. It can accept *at most* 2 input nodes.
class Phi : public Expr {
 public:
  inline static Phi* New( Graph* , ControlFlow* );
  inline static Phi* New( Graph* , Expr* , Expr* , ControlFlow* );
  // Remove the phi node from its belonged region. The reason this one is just
  // a static function is because this function *doesn't* touch its ref_list,
  // so its ref_list will still have its belonged region's reference there and
  // it is invalid. This function should be used under strict condition.
  static inline void RemovePhiFromRegion( Phi* );
  // Get the boundede region
  ControlFlow* region() const { return region_; }
  // Check if this Phi node is not used. We cannot use HasRef function since
  // a Phi node may added to a region during setup time and there will be one
  // ref inside of the RefList. We just need to check that
  bool         IsUsed() const { return !(region() ? ref_list()->size() == 1 : (ref_list()->empty())); }

  // Check if this Phi node is in intermediate state. A phi node will generated
  // at the front the loop and it will only have on operand then. If phi is in
  // this stage, then it is an intermediate state
  bool IsIntermediateState() const { return operand_list()->size() == 1; }

  // Check if the phi node is a binary phi ,ie phi's operand list only has 2 nodes
  bool IsBinaryPhi() const { return operand_list()->size() == 2; }

  // Bounded control flow region node.
  // Each phi node is bounded to a control flow regional node
  // and by this we can easily decide which region contributs
  // to a certain input node of Phi node
  inline Phi( Graph* , std::uint32_t , ControlFlow* );
 private:
  ControlFlow* region_;
  LAVA_DISALLOW_COPY_AND_ASSIGN(Phi)
};

class Projection : public Expr {
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
    return that->IsProjection() && (that->AsProjection()->index() == index());
  }
 private:
  std::uint32_t index_;
  LAVA_DISALLOW_COPY_AND_ASSIGN(Projection)
};

} // namespace hir
} // namespace cbase
} // namespace lavascript

#endif // CBASE_HIR_PHI_H_
