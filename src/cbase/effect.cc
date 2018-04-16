#include "effect.h"

namespace lavascript {
namespace cbase      {
namespace hir        {

Effect::Effect( ::lavascript::zone::Zone* zone , MemoryWrite* write ):
  write_effect_(write),
  read_list_   (),
  zone_        (zone)
{}

Effect::Effect( const Effect& that ):
  write_effect_(that.write_effect_),
  read_list_   (that.zone_,that.read_list_),
  zone_        (that.zone_)
{}

void Effect::UpdateWriteEffect( MemoryWrite* node ) {
  for( auto itr(read_list_.GetForwardIterator()); itr.HasNext(); itr.Move() ) {
    node->AddEffect(itr.value());
  }
  write_effect_ = node;
  read_list_.Clear();
}

void Effect::AddReadEffect( MemoryRead* node ) {
  node->AddEffect(write_effect_);
  read_list_.Add(zone_,node);
}

void Effect::Merge( const Effect& lhs, const Effect& rhs, Effect* output , Graph* graph ,
                                                                           ControlFlow* region,
                                                                           IRInfo* info ) {
  auto effect_phi = WriteEffectPhi::New(graph,lhs.write_effect_,rhs.write_effect_,region,info);
  // merge all the read effect
  for( auto itr(lhs.read_list_.GetForwardIterator()); itr.HasNext(); itr.Move() ) {
    effect_phi->AddEffect(itr.value());
  }
  for( auto itr(rhs.read_list_.GetForwardIterator()); itr.HasNext(); itr.Move() ) {
    effect_phi->AddEffectIfNotExist(itr.value());
  }
  output->write_effect_ = effect_phi;
  output->read_list_.Clear();
}

} // namespace hir
} // namespace lavascript
} // namespace cbase
