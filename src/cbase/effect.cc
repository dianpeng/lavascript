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
  if(read_list_.empty()) {
    node->AddEffect(write_effect_); // we don't have any read effect node
  } else {
    lava_foreach(MemoryRead* read,read_list_.GetForwardIterator()) {
      node->AddEffect(read);
    }
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
  lava_foreach( MemoryRead* read , lhs.read_list_.GetForwardIterator() ) {
    effect_phi->AddEffect(read);
  }
  lava_foreach( MemoryRead* read , rhs.read_list_.GetForwardIterator() ) {
    effect_phi->AddEffectIfNotExist(read);
  }
  output->write_effect_ = effect_phi;
  output->read_list_.Clear();
}

} // namespace hir
} // namespace lavascript
} // namespace cbase
