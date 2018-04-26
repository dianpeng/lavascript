#ifndef CBASE_HIR_EFFECT_H_
#define CBASE_HIR_EFFECT_H_
#include "expr.h"

namespace lavascript {
namespace cbase      {
namespace hir        {

// Effect: Used to tag a class that has side effect
class ReadEffect : public Expr {
 public:
  // read effect constructor
  ReadEffect( IRType type , std::uint32_t id , Graph* graph ): Expr(type,id,graph) {}
  // get attached write effect generated Checkpoint node
  Checkpoint* GetCheckpoint() const;
};

class WriteEffect: public Expr {
 public:
  // write effect constructor
  WriteEffect( IRType type , std::uint32_t id , Graph* graph ): Expr(type,id,graph) { }
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
