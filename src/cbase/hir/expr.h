#ifndef CBASE_HIR_EXPR_H_
#define CBASE_HIR_EXPR_H_
#include "node.h"
#include "src/iterator.h"
#include <functional>

namespace lavascript {
namespace cbase      {
namespace hir        {

// Expr:
//   This node is the mother all other expression node and its solo
//   goal is to expose def-use and use-def chain into different types
class Expr : public Node {
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
  // default GVNHash function implementation
  virtual std::uint64_t GVNHash()        const { return static_cast<std::uint64_t>(id()); }

  // default GVN Equal comparison function implementation
  virtual bool Equal( const Expr* that ) const { return IsIdentical(that); }

  // default operation to test whether 2 nodes are identical or not. it should be prefered
  // when 2 nodes are compared against identity. it means if they are equal , then one node
  // can be used to replace other
  inline bool IsReplaceable( const Expr* that ) const;
 public:
  // This list returns a list of operands used by this Expr/IR node. Most
  // of time operand_list will return at most 3 operands except for call
  // function
  const OperandList* operand_list() const { return &operand_list_; }
  // Helper accessor
  const Expr* Operand( std::size_t index ) const { return operand_list()->Index(index); }
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
  // Helper accessor
  const OperandRef&     Ref( std::size_t index ) { return ref_list()->Index(index); }
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
  // immutable iterator, so user cannot set value
  typedef PolyIterator<Expr*> DependencyIterator;

  // get dependency iteration iterator
  virtual DependencyIterator GetDependencyIterator() const { return DependencyIterator(); }

  // get the dependnecy size
  virtual std::size_t dependency_size() const { return 0; }

  // check whether this node has dependency
  bool HasDependency() const { return dependency_size() != 0; }
 public:
  // constructor
  Expr( IRType type , std::uint32_t id , Graph* graph ):
    Node            (type,id,graph),
    operand_list_   (),
    ref_list_       (),
    pin_            ()
  {}
 private:
  OperandList        operand_list_;
  OperandRefList     ref_list_;
  PinEdge            pin_;
};

} // namespace hir
} // namespace cbase
} // namespace lavascript

#endif // CBASE_HIR_EXPR_H_
