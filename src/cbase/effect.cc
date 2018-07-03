#include "effect.h"

namespace lavascript {
namespace cbase      {
namespace hir        {

Effect::Effect( WriteEffect* effect ): write_effect_(effect)             {}
Effect::Effect( const Effect& that  ): write_effect_(that.write_effect_) {}

void Effect::AddReadEffect( ReadEffect* effect ) {
  effect->SetWriteEffect(write_effect_);
}

void Effect::UpdateWriteEffect( WriteEffect* effect ) {
  effect->HappenAfter(write_effect_);
  write_effect_ = effect;
}

void Effect::MergeEffect( const Effect& lhs , const Effect& rhs , Effect* output , Graph*              graph ,
                                                                                   EffectMergeRegion* region ) {
  auto lhs_eff = lhs.write_effect();
  auto rhs_eff = rhs.write_effect();
  if(!lhs_eff->IsIdentical(rhs_eff)) {
    // create an effect phi to join effect created by the root node
    auto effect_phi = EffectMerge::New(graph,lhs_eff,rhs_eff);
    output->write_effect_ = effect_phi;
    region->AddEffectMerge(effect_phi);
  } else {
    // propogate the no_write effect to output effect group since lhs and rhs
    // are both no write effect group or basically means they are not needed
    output->write_effect_ = lhs_eff;
  }
}

} // namespace hir
} // namespace lavascript
} // namespace cbase
