#ifndef CBASE_HIR_EFFECT_H_
#define CBASE_HIR_EFFECT_H_
#include "expr.h"

namespace lavascript {
namespace cbase      {
namespace hir        {

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

// Represents a general read which should depend on certain node's side effect.
class ReadEffect : public Expr {
 public:
  // read effect constructor
  ReadEffect( IRType type , std::uint32_t id , Graph* graph ):
    Expr(type,id,graph) , effect_edge_() {}

  // get attached write effect generated Checkpoint node
  Checkpoint* GetCheckpoint() const;

 public: // dependency implementation
  virtual DependencyIterator GetDependencyIterator() const;
  // only one dependency
  virtual std::size_t dependency_size() const { return effect_edge_.IsEmpty() ? 0 : 1; }

  void  set_effect_edge( const ReadEffectListIterator& itr , WriteEffect* node )
  { effect_edge_.id = itr; effect_edge_.node = node; }
  const ReadEffectEdge& effect_edge () const { return effect_edge_; }
  WriteEffect*          write_effect() const { return effect_edge_.node; }
 private:
  ReadEffectEdge effect_edge_; // record the read effect and write effect relationship

  class ReadEffectDependencyIterator;
  friend class ReadEffectDependencyIterator;
};

// Represent a general write which should bring some side effect
class WriteEffect: public Expr {
 public:
  // write effect constructor
  WriteEffect( IRType type , std::uint32_t id , Graph* graph ):
    Expr(type,id,graph) ,
    prev_(NULL),
    read_effect_()
  {}

 public:
  virtual DependencyIterator GetDependencyIterator() const;
  virtual std::size_t              dependency_size() const {
    return prev_ ? prev_->read_effect_.size() : 0;
  }
 public:
  // insert |this| *before* input WriteEffect node; this operation basically means the
  // |this| WriteEffect node must happen *After* the input WriteEffect node
  virtual void HappenAfter( WriteEffect* input );
  // add a new read effect
  void  AddReadEffect( ReadEffect* effect );
  // get the read effect list , ie all the read happened after this write effect
  const ReadEffectList* read_effect() const { return &read_effect_; }
  // get the previous write effect
  virtual WriteEffect* prev() const { return prev_; }
 private:
  // double linked list field for chaining all the write effect node together
  WriteEffect* prev_;
  ReadEffectList read_effect_;    // all read effect that read |this| write effect

  class WriteEffectDependencyIterator;
  friend class WriteEffectDependencyIterator;
};

// Represent a memory region mutation
// This is a very important node since it represents a potential resize of all
// the memory node inside of the function which means all the *reference* node
// invalid. When a barrier node is emitted it means this node is *pinned* into
// the control flow block and cannot be moved. So these nodes are not floating
// essentially.
class EffectBarrier: public WriteEffect {
 public:
  HardBarrier( IRType type , Graph* graph , std::uint32_t id ):
    WriteEffect  (type,id,graph), ref_list_() {}
 public:
  virtual PloyIterator GetDependencyIterator() const;
  virtual std::size_t  dependency_size      () const;
  virtual bool         HasDependency        () const;
  virtual void         AddEffectRef         ( ReadEffect* );
 public:
  HardBarrier*         prev_barrier() const;
  const ReadEffectList& ref_list    () const;
 private:
  // reference list, reference lookup goes into this list and other write/read
  // goes into the read_effect_ list inherited from WriteEffect. this just gives
  // better chance for us to do alias analyzing
  ReadEffectList ref_list_;
};

// HardBarrier is a type of barrier that cannot be moved , basically no way to
// do code motion. It is a hard barrier that prevents any operations happend
// after hoisting and also operation happened before sinking.
class HardBarrier : public EffectBarrier {
 public:
  HardBarrier( IRType type , Graph* graph , std::uint32_t id ):
    EffectBarrier(type,graph,id) {}
 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(HardBarrier)
};

// SoftBarrier is a type of barrier that can be moved. It is mainly serve as a
// way to mark the barrier chain at certain conrol flow node, ie branch or loop
// The operation happened after the SoftBarrier can be moved *cross* the barrier
// node.
class SoftBarrier : public EffectBarrier {
 public:
  SoftBarrier( IRType type , Graph* graph , std::uint32_t id ):
    EffectBarrier(type,graph,id) {}
 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(SoftBarrier)
};

// LoopEffect node is a node that mark the effect region inside of the loop.
// Instead of having a Phi node , which have a one input points to itself,
// we just use LoopEffect node. LoopEffect node links to its previous effect
// node of fallthrough path and only get one predecessor.
class LoopEffect: public SoftBarrier {
 public:
  LoopBarrier( Graph* graph , std::uint32_t id ): SoftBarrier(HIR_LOOP_EFFECT,id,graph) {}
 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(LoopEffect)
};

// EffectPhi is a phi node inserted at merge region used to fan in all the branch's
// phi node. Each branch will use a DummyBarrier to separate each branch's effect
// chain regardlessly
class EffectPhi : public SoftBarrier {
 public:
  EffectPhi( Graph* graph , std::uint32_t id ) : SoftBarrier(HIR_EFFECT_PHI,id,graph) {}
 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(EffectPhi)
};

// DummyBarrier is an object to separate effect chain in lexical scope. It is mainly to
// use mark the start of the effect chain belonged to a certain control flow region. As
// its name represents, it is a dummy barrier. The dummy barrier also participate into
// the GVN. the GVN is delegate to this barrier's previous barrier since it doesn't bring
// any change into the effect chain.
class DummyBarrier : public SoftBarrier {
 public:
  DummyBarrier( Graph* graph , std::uint32_t id ) : SoftBarrier(HIR_DUMMY_BARRIER,id,graph) {}
  virtual std::uint64_t GVNHash(                  ) const { return prev()->GVNHash(); }
  virtual bool          Equal  ( const Expr* that ) const { return prev()->Equal(that);}
 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(DummyBarrier)
};

// DummyWriteEffect is a fake write effect basically just do nothing , serving as a placeholder
class DummyWriteEffect: public WriteEffect {
 public:
  inline static DummyWriteEffect* New( Graph* );
  DummyWriteEffect( Graph* graph , std::uint32_t id ): WriteEffect(HIR_NO_WRITE_EFFECT,id,graph) {}
 public:
  virtual DependencyIterator GetDependencyIterator() const { return DependencyIterator(); }
  virtual std::size_t              dependency_size() const { return 0; }
 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(DummyWriteEffect);
};

} // namespace hir
} // namespace cbase
} // namespace lavascript

#endif // CBASE_HIR_EFFECT_H_
