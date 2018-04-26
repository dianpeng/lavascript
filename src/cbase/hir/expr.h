#ifndef CBASE_HIR_EXPR_H_
#define CBASE_HIR_EXPR_H_
#include "node.h"

namespace lavascript {
namespace cbase      {
namespace hir        {

// Expr:
//   This node is the mother all other expression node and its solo
//   goal is to expose def-use and use-def chain into different types
class Expr : public Node {
  static const std::uint32_t kHasSideEffect = 1;
  static const std::uint32_t kNoSideEffect  = 0;
 public:
  bool  IsPin            ()                   const { return pin_.HasRef(); }
  void  set_pin_edge     ( const PinEdge& st )      { pin_= st; }
  const PinEdge& pin_edge()                   const { return pin_; }
 public:
  // Replace *this* node with the input expression node. This replace
  // will modify all reference to |this| with reference to input node.
  // After replacement, |this| node should be treated as *invalid* and
  // are not supposed to be used again.
  virtual void Replace( Expr* );
 public:
  // get node's GVN hash value
  virtual std::uint64_t GVNHash()        const { return GVNHash1(type_name(),id()); }

  // check the input node is equal with |this| node in terms of GVN
  virtual bool Equal( const Expr* that ) const { return IsIdentical(that);          }

  // default operation to test whether 2 nodes are identical or not. it should be prefered
  // when 2 nodes are compared against identity. it means if they are equal , then one node
  // can be used to replace other
  inline bool IsReplaceable( const Expr* that ) const;
 public:

  // This list returns a list of operands used by this Expr/IR node. Most
  // of time operand_list will return at most 3 operands except for call
  // function
  const OperandList* operand_list() const { return &operand_list_; }
  // This function will add the input node into this node's operand list and
  // it will take care of the input node's ref list as well
  inline void AddOperand( Expr* node );
  // Replace an existed operand with input operand at position
  inline void ReplaceOperand( std::size_t , Expr* );
  // Clear all the operand inside of this node's operand list
  void ClearOperand();
 public:

  // This list returns a list of Ref object which allow user to
  //   1) get a list of expression that uses this expression, ie who uses me
  //   2) get the corresponding iterator where *me* is inserted into the list
  //      so we can fast modify/remove us from its list
  const OperandRefList* ref_list() const { return &ref_list_; }

  // Add the referece into the reference list
  void AddRef( Node* who_uses_me , const OperandIterator& iter ) {
    ref_list_.PushBack(zone(),OperandRef(iter,who_uses_me));
  }

  // Remove a reference from the reference list whose ID == itr
  bool RemoveRef( const OperandIterator& itr , Node* who_uses_me );
  // check if this expression is used by any other expression, basically
  // check whether ref_list is empty or not
  //
  // this check may not be accurate once the node is deleted/removed since
  // once a node is removed, we don't clean its ref_list but it is not used
  // essentially
  bool HasRef() const { return !ref_list()->empty(); }
 public:

  // Used to develop dependency between expression which cannot be expressed
  // as data flow operation. Mainly used to order certain operations
  // Effect list is essentially loosed and it will have duplicated node. To
  // avoid too much duplicated node we can use AddEffectIfNotExist to check
  // whether we have that value added ; but it does a linear search so it is
  // not performant. The effect list maintain is a best effort in terms of dedup.
  const EffectList* effect_list() const { return &effect_list_; }

  // add a node into the effect list, it will refuse to add effect node when
  // it is either NoMemoryRead/NoMemoryWrite since these nodes are just placeholders
  inline void AddEffect   ( Expr* node );

  // add a node only when the effect node not show up inside of the effect list.
  // this function should be invoked with cautious since it is time costy due to
  // the linear finding internally
  void AddEffectIfNotExist( Expr* );
 public:
  // Check whether this expression has side effect , or namely one of its descendent
  // operands has a none empty effect list
  bool HasSideEffect()     const { return state_ == kHasSideEffect; }

  // constructor
  Expr( IRType type , std::uint32_t id , Graph* graph ):
    Node             (type,id,graph),
    operand_list_    (),
    effect_list_     (),
    ref_list_        (),
    pin_            (),
    state_           ()
  {}
 private:
  // mark this node has side effect
  void SetHasSideEffect() { state_ = kHasSideEffect; }

  OperandList        operand_list_;
  EffectList         effect_list_;
  OperandRefList     ref_list_;
  PinEdge            pin_;
  std::uint64_t      state_;
};

template<> struct MapIRClassToIRType<Expr> {
  static bool Test( IRType type ) {
#define __(A,B,...) case HIR_##B: return true;
    switch(type) { CBASE_HIR_EXPRESSION(__) default: return false; }
#undef __ // __
  }
};

} // namespace hir
} // namespace cbase
} // namespace lavascript

#endif // CBASE_HIR_EXPR_H_
