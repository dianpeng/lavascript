#ifndef CBASE_HIR_EFFECT_H_
#define CBASE_HIR_EFFECT_H_
#include "src/util.h"
#include "expr.h"

namespace lavascript {
namespace cbase      {
namespace hir        {

class EffectBarrier;
class EffectMergeRegion;

/**
 * Effect -----------------------------------------------------------
 *
 * To have good optimization, we need good alias analyzing. Typically
 * for language that uses weak type , it is extreamly hard to get good
 * alias analyzing. So during the IR construction, we should construct
 * more high level construct to make our AA easier to work or at least
 * easier to analyze.
 * ------------------------------------------------------------------*/

typedef zone::List<ReadEffect*>         ReadEffectList;
typedef ReadEffectList::ForwardIterator ReadEffectListIterator;

struct ReadEffectEdge {
  ReadEffectListIterator id;
  WriteEffect*         node;
  ReadEffectEdge( const ReadEffectListIterator& itr , WriteEffect* n ): id(itr), node(n) {}
  ReadEffectEdge() : id(), node() {}
  bool IsEmpty() const { return node == NULL; }
};

LAVA_CBASE_HIR_DEFINE(HIR_INTERNAL,EffectNode,public Expr) {
 public:
  EffectNode( IRType type , std::uint32_t id , Graph* graph ): Expr(type,id,graph) {}
};

// Represents a general read which should depend on certain node's side effect.
LAVA_CBASE_HIR_DEFINE(HIR_INTERNAL,ReadEffect,public EffectNode) {
 public:
  // read effect constructor
  ReadEffect( IRType type , std::uint32_t id , Graph* graph ):
    EffectNode(type,id,graph) , effect_edge_() {}

 public: // dependency implementation
  virtual DependencyIterator GetDependencyIterator() const;

  // only one dependency
  virtual std::size_t dependency_size() const { return 1; }

  void  set_effect_edge( const ReadEffectListIterator& itr , WriteEffect* node )
  { effect_edge_.id = itr; effect_edge_.node = node; }
  const ReadEffectEdge& effect_edge () const { return effect_edge_; }
  WriteEffect*          write_effect() const { return effect_edge_.node; }
  inline void SetWriteEffect( WriteEffect* );
 public:
  // Replace operations
  virtual void Replace( Expr* );

 private:
  ReadEffectEdge effect_edge_;               // record the read effect and write effect relationship
  class ReadEffectDependencyIterator;
  friend class ReadEffectDependencyIterator;
};

// Represent a general write which should bring some side effect
LAVA_CBASE_HIR_DEFINE(HIR_INTERNAL,WriteEffect,public EffectNode ,public DoubleLinkNode<WriteEffect>) {
 public:
  // write effect constructor
  WriteEffect( IRType type , std::uint32_t id , Graph* graph ):
    EffectNode                 (type,id,graph),
    DoubleLinkNode<WriteEffect>(),
    read_effect_               ()
  {}

  virtual DependencyIterator GetDependencyIterator() const;
  virtual std::size_t              dependency_size() const {
    if(NextLink()) {
      auto sz = NextLink()->read_effect_.size();
      return sz ? sz : 1;
    } else {
      return 0;
    }
  }
 public:
  // remove |this| write from the effect chain. The read operation is handled properly by
  // forwarding it to its *NextWrite*.
  void RemoveFromEffectChain( WriteEffect* write = NULL );

  // return the next write effect node ,if a effect phi node is met then it returns NULL
  // since user should not use NextWrite to examine the next barrier node
  //
  // Notes: NextWrite returns a write that happened *before* this write. The effect chain
  //        is linked reversely
  WriteEffect*   NextWrite() const {
    auto ret = NextLink();
    lava_debug(NORMAL,lava_verify(ret););
    return ret;
  }

  WriteEffect*   PrevWrite() const {
    auto ret = PrevLink();
    lava_debug(NORMAL,lava_verify(ret););
    return ret;
  }

  // return barrier that is closest to |this| node. If |this| node is a barrier,
  // then just return |this|
  EffectBarrier* FirstBarrier() const;

  // find another barrier that is closes to |this| node. If |this| node is a barrier,
  // function will not return |this| but the nearest barrier happened before this.
  EffectBarrier* NextBarrier() const;

  // insert |this| *before* input WriteEffect node; this operation basically means the
  // |this| WriteEffect node must happen *After* the input WriteEffect node
  void  HappenAfter( WriteEffect* input );

  // add a new read effect
  ReadEffectListIterator AddReadEffect( ReadEffect* effect );

  // get the read effect list , ie all the read happened after this write effect
  const ReadEffectList* read_effect() const { return &read_effect_; }

  void RemoveReadEffect( ReadEffectEdge* edge ) {
    lava_debug(NORMAL,lava_verify(edge->node == this););
    read_effect_.Remove(edge->id);
    edge->node = NULL;
  }

 public:
  virtual void Replace ( Expr* );

  // replace this node with a range of effect node. The 1st EffectNode should be used
  // to replace |this| node ; and the 2nd EffectNode is the write effect that all the
  // read happened after |this| that should be moved to this write effect node.
  void ReplacePair( EffectNode* , WriteEffect* );

 private:

  ReadEffectList read_effect_;    // all read effect that read |this| write effect
  class WriteEffectDependencyIterator;
  friend class WriteEffectDependencyIterator;
};

// Represent a memory region mutation
// This is a very important node since it represents a potential resize of all
// the memory node inside of the function which means all the *reference* node
// invalid. When a barrier node is emitted it means this node is *pinned* into
// the control flow block and cannot be moved. So these nodes are not floating
// essentially
LAVA_CBASE_HIR_DEFINE(HIR_INTERNAL,EffectBarrier,public WriteEffect) {
 public:
  EffectBarrier( IRType type , std::uint32_t id , Graph* graph ):
    WriteEffect  (type,id,graph) {}
};

// HardBarrier is a type of barrier that cannot be moved , basically no way to
// do code motion. It is a hard barrier that prevents any operations happend
// after hoisting and also operation happened before sinking.
LAVA_CBASE_HIR_DEFINE(HIR_INTERNAL,HardBarrier,public EffectBarrier) {
 public:
  HardBarrier( IRType type , std::uint32_t id , Graph* graph ):
    EffectBarrier(type,id,graph) {}
};

// SoftBarrier is a type of barrier that can be moved. It is mainly serve as a
// way to mark the barrier chain at certain conrol flow node, ie branch or loop
// The operation happened after the SoftBarrier can be moved *cross* the barrier
// node.
LAVA_CBASE_HIR_DEFINE(HIR_INTERNAL,SoftBarrier,public EffectBarrier) {
 public:
  SoftBarrier( IRType type , std::uint32_t id , Graph* graph ):
    EffectBarrier(type,id,graph) {}
};

LAVA_CBASE_HIR_DEFINE(HIR_INTERNAL,EffectMergeBase,public HardBarrier) {
 public:
  EffectMergeBase( IRType type , std::uint32_t id , Graph* graph ) :
    HardBarrier(type,id,graph) , region_(NULL) {}
 public:
  EffectMergeRegion* region    ()                       const { return region_;   }
  void               set_region( EffectMergeRegion* region )  { region_ = region; }
  void               ResetRegion()                            { region_ = NULL;   }

  void set_lhs_effect( WriteEffect* effect ) { AddOperand(effect); }
  void set_rhs_effect( WriteEffect* effect ) { AddOperand(effect); }

  WriteEffect* lhs_effect() const { return Operand(0)->As<WriteEffect>(); }
  WriteEffect* rhs_effect() const { return Operand(1)->As<WriteEffect>(); }
 public:
  virtual DependencyIterator GetDependencyIterator() const;
  virtual std::size_t              dependency_size() const;
 private:
  class EffectMergeBaseDependencyIterator;
  friend class EffectMergeBaseDependencyIterator;
  EffectMergeRegion* region_;
  LAVA_DISALLOW_COPY_AND_ASSIGN(EffectMergeBase)
};

// EffectMerge is a phi node inserted at merge region used to fan in all the branch's
// phi node. Each branch will use a InitBarrier to separate each branch's effect
// chain regardlessly
LAVA_CBASE_HIR_DEFINE(Tag=EFFECT_MERGE;Name="effect_merge";Leaf=NoLeaf,
    EffectMerge,public EffectMergeBase) {
 public:
  static inline EffectMerge* New( Graph* );
  static inline EffectMerge* New( Graph* , WriteEffect* , WriteEffect* );

  EffectMerge( Graph* graph , std::uint32_t id ) : EffectMergeBase(HIR_EFFECT_MERGE,id,graph) {}
 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(EffectMerge)
};

// LoopEffectStart
// This phi is a special effect node that will appear at the Loop region node to mark the
// effect chain is inside of the loop body. The node will forms a cycle like other loop
// induction variable phi. It is mainly to prevent optimization of memory forwarding to
// cross loop carried dependency boundary. Due to the cycle , when do analyze of aliasing
// or previous store, one will have to visit the store happened *after the loop*. And it
// also means the only the fly memory optimization cannot be applied to stuff in the loop
LAVA_CBASE_HIR_DEFINE(Tag=LOOP_EFFECT_START;Name="loop_effect_start";Leaf=NoLeaf,
    LoopEffectStart,public EffectMergeBase) {
 public:
  // The loop effect phi node will be created right before entering into the loop, so at
  // that moment only one fallthrough branch's WriteEffect node is known. The new function
  // will take that WriteEffect as its precedence.
  static inline LoopEffectStart* New( Graph* , WriteEffect* );
 public:
  // Set the backwards pointed effect of this LoopEffectStart. This backward pointed effect
  // points from the bottom of the loop (loop exit) back to the start of the loop effect
  // phi node.
  void SetBackwardEffect( WriteEffect* effect ) { set_rhs_effect(effect); }
  // Constructor of the LoopEffectStart
  LoopEffectStart( Graph* graph , std::uint32_t id ) :
    EffectMergeBase(HIR_LOOP_EFFECT_START,id,graph) {}
 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(LoopEffectStart)
};

// InitBarrier is an object to separate effect chain in lexical scope. It is mainly to
// use mark the start of the effect chain
LAVA_CBASE_HIR_DEFINE(Tag=INIT_BARRIER;Name="init_barrier";Leaf=NoLeaf,
    InitBarrier,public HardBarrier) {
 public:
  static inline InitBarrier* New( Graph* );
  InitBarrier( Graph* graph , std::uint32_t id ) : HardBarrier(HIR_INIT_BARRIER,id,graph) {}
 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(InitBarrier)
};

// BranchStartEffect is an object to be used to *mark* the control flow. It doesn't have any
// actual barrier impact but just to mark the separation of control flow region, ie *If* node.
LAVA_CBASE_HIR_DEFINE(Tag=BRANCH_START_EFFECT;Name="branch_start_effect";Leaf=NoLeaf,
    BranchStartEffect,public HardBarrier) {
 public:
  static inline BranchStartEffect* New( Graph* );
  static inline BranchStartEffect* New( Graph*  , WriteEffect* );

  BranchStartEffect( Graph* graph , std::uint32_t id ) :
    HardBarrier(HIR_BRANCH_START_EFFECT,id,graph) {}
 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(BranchStartEffect)
};

LAVA_CBASE_HIR_DEFINE(Tag=EMPTY_WRITE_EFFECT;Name="empty_write_effect";Leaf=NoLeaf,
    EmptyWriteEffect,public WriteEffect) {
 public:
  static inline EmptyWriteEffect* New( Graph* );
  static inline EmptyWriteEffect* New( Graph* , WriteEffect* );

  EmptyWriteEffect( Graph* graph , std::uint32_t id ):
    WriteEffect(HIR_EMPTY_WRITE_EFFECT,id,graph) {}
 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(EmptyWriteEffect)
};

} // namespace hir
} // namespace cbase
} // namespace lavascript

#endif // CBASE_HIR_EFFECT_H_
