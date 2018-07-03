#ifndef CBASE_HIR_EXPR_H_
#define CBASE_HIR_EXPR_H_
#include "node.h"
#include "src/hash.h"
#include "src/iterator.h"
#include <functional>

namespace lavascript {
namespace cbase      {
namespace hir        {

// Expr:
//   This node is the mother all other expression node and its solo
//   goal is to expose def-use and use-def chain into different types
LAVA_CBASE_HIR_DEFINE(HIR_INTERNAL,Expr,public Node) {
 public:
  bool  IsUnboxNode        () const;
  bool  IsBoxNode          () const;
 public:
  // Replace *this* node with the input expression node. This replace
  // will modify all reference to |this| with reference to input node.
  // After replacement, |this| node should be treated as *invalid* and
  // are not supposed to be used again.
  virtual void Replace( Expr* );
 public:
  // The GVN can work on node that doesn't have side effect. Node can opt in GVN by
  // rewriting the following 2 functions. The default implementation does a strict
  // comparison, basically identical comparison.
  // default GVNHash function implementation
  virtual std::uint64_t GVNHash()        const { return Hasher::Hash(id()); }

  // default GVN Equal comparison function implementation
  virtual bool Equal( const Expr* that ) const { return IsIdentical(that); }
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
  // Helper accessor
  Expr*       Operand( std::size_t index ) const { return operand_list_.Index(index); }
  std::size_t OperandSize()                const { return operand_list_.size(); }
  bool        OperandEmpty()               const { return operand_list_.empty(); }
 public:
  // This list returns a list of Ref object which allow user to
  //   1) get a list of expression that uses this expression, ie who uses me
  //   2) get the corresponding iterator where *me* is inserted into the list
  //      so we can fast modify/remove us from its list
  const OperandRefList* ref_list() const { return &ref_list_; }
  // Helper accessor
  const OperandRef&     Ref( std::size_t index ) { return ref_list_.Index(index); }
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
  virtual DependencyIterator GetDependencyIterator() const {
    return DependencyIterator();
  }

  // get the dependnecy size
  virtual std::size_t dependency_size() const { return 0; }

  // check whether this node has dependency
  virtual bool HasDependency() const { return dependency_size() != 0; }
 public:
  // constructor
  Expr( IRType type , std::uint32_t id , Graph* graph ):
    Node            (type,id,graph),
    operand_list_   (),
    ref_list_       ()
  {}

 protected:
  OperandList        operand_list_;
  OperandRefList     ref_list_;
};

} // namespace hir
} // namespace cbase
} // namespace lavascript

#endif // CBASE_HIR_EXPR_H_
