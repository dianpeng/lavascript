#ifndef CBASE_EFFECT_H_
#define CBASE_EFFECT_H_
#include "hir.h"
#include "src/zone/vector.h"

namespace lavascript {
namespace cbase      {
namespace hir        {

class EffectGroup;
class RootEffectGroup;
class LeafEffectGroup;
class Effect;

// Hopefully this is the last design. This module has been extensively rewritten
// before due to the memory model is not very clear or the AA design is not very
// clear. Now we have a relatively good AA so this class can be extreamly simple,
// all it does is *nothing* but just form correct read/write chain.
class Effect {
 public:
  explicit Effect( WriteEffect* );

  Effect( const Effect& );
  // Add a read effect into the effect chain, forms a true depenendency, ie
  // read after write dependency.
  void AddReadEffect    ( ReadEffect* );
  // Add a write effect into the effect chain, forms a anti-dependency, ie
  // write after read dependency.
  void UpdateWriteEffect( WriteEffect* );
  // Return the write effect currently this effect object tracked
  WriteEffect* write_effect() const { return write_effect_; }
  // merge the input 2 effect object into another effect object, the input and output can be the same
  static void Merge( const Effect& , const Effect& , Effect* , Graph*  , ControlFlow* );
 private:
  WriteEffect* write_effect_;

  LAVA_DISALLOW_ASSIGN(Effect);
};

} // namespace hir
} // namespace cbase
} // namespace lavascript

#endif // CBASE_EFFECT_H_
