#ifndef CBASE_HIR_EFFECT_H_
#define CBASE_HIR_EFFECT_H_
#include "expr.h"

namespace lavascript {
namespace cbase      {
namespace hir        {

typedef zone::Vector<ReadEffect*>       ReadEffectList;
typedef ReadEffectList::ForwardIterator ReadEffectListIterator;

struct ReadEffectEdge {
  ReadEffectListIterator id;
  WriteEffect*         node;
  ReadEffectEdge( const ReadEffectListIterator& itr , WriteEffect* n ): id(itr), node(n) {}
  ReadEffectEdge() : id(), node() {}
  bool IsEmpty() const { return node == NULL; }
};

// Represent a read effect. A read effect can only watch one write effect
class ReadEffect : public Expr {
 public:
  // read effect constructor
  ReadEffect( IRType type , std::uint32_t id , Graph* graph ): Expr(type,id,graph) , effect_edge_() {}

  // get attached write effect generated Checkpoint node
  Checkpoint* GetCheckpoint() const;

 public: // dependency implementation
  virtual bool VisitDependency( const DependencyVisitor& visitor ) const {
    if(!effect_edge_.IsEmpty()) return visitor(effect_edge_->node);
  }

  // only one dependency
  virtual std::size_t dependency_size() const { return effect_edge_.IsEmpty() ? 0 : 1; }

  // set the write effect this read effect needs to depend on
  inline void SetWriteEffect( WriteEffect* );
 private:
  ReadEffectEdge effect_edge_;    // record the read effect and write effect relationship
};

// WriteEffect
//
// a write effect node is a node that represents a write operation
class WriteEffect: public Effect {
 public:
  // write effect constructor
  WriteEffect( IRType type , std::uint32_t id , Graph* graph ):
    Effect(type,id,graph) ,
    next_(NULL),
    prev_(NULL),
    read_effect_()
  {}
 public:
  virtual bool           VisitDependency( const DependencyVisitor& ) const;
  virtual std::size_t    dependency_size() const { return next_->read_effect_.size(); }
  ReadEffectListIterator AddReadEffect( ReadEffect* effect ) { return read_effect_.PushBack(effect); }
  // chained write effect node
  WriteEffect* NextWriteEffect() const { return next_; }
  WriteEffect* PrevWriteEffect() const { return prev_; }
  // insert |this| *before* input WriteEffect node; this operation basically means the
  // |this| WriteEffect node must happen *After* the input WriteEffect node
  inline void HappenAfter( WriteEffect* input );
 public:
  ReadEffectList*       read_effect()       { return &read_effect_; }
  const ReadEffectList* read_effect() const { return &read_effect_; }
 private:
  // double linked list field for chaining all the write effect node together
  WriteEffect* next_;
  WriteEffect* prev_;
  ReadEffectList read_effect_;    // all read effect that read |this| write effect
};

// EffectPhi
//
// A phi node that is used to merge effect right after the control flow. It
// will only be used inside of some expression's effect list
class ReadEffectPhi : public ReadEffect {
 public:
  inline static ReadEffectPhi* New( Graph* , ControlFlow* );
  inline static ReadEffectPhi* New( Graph* , ReadEffect* , ReadEffect* , ControlFlow* );
  ControlFlow* region() const { return region_; }
  inline ReadEffectPhi( Graph* , std::uint32_t , ControlFlow* );
 private:
  ControlFlow* region_;
  LAVA_DISALLOW_COPY_AND_ASSIGN(ReadEffectPhi);
};

class WriteEffectPhi : public WriteEffect {
 public:
  inline static WriteEffectPhi* New( Graph* , ControlFlow* );
  inline static WriteEffectPhi* New( Graph* , WriteEffect* , WriteEffect* , ControlFlow* );
  ControlFlow* region() const { return region_; }
  inline WriteEffectPhi( Graph* , std::uint32_t , ControlFlow* );
 private:
  ControlFlow* region_;
  LAVA_DISALLOW_COPY_AND_ASSIGN(WriteEffectPhi);
};

// placeholder for empty read/write effect to avoid checking NULL pointer
class NoReadEffect : public ReadEffect {
 public:
  inline static NoReadEffect* New( Graph* );
  NoReadEffect( Graph* graph , std::uint32_t id ): ReadEffect(HIR_NO_READ_EFFECT,id,graph) {}
 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(NoReadEffect);
};

class NoWriteEffect: public WriteEffect {
 public:
  inline static NoWriteEffect* New( Graph* );
  NoWriteEffect( Graph* graph , std::uint32_t id ): WriteEffect(HIR_NO_WRITE_EFFECT,id,graph) {}
 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(NoWriteEffect);
};

} // namespace hir
} // namespace cbase
} // namespace lavascript

#endif // CBASE_HIR_EFFECT_H_
