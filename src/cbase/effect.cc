#include "effect.h"

namespace lavascript {
namespace cbase      {
namespace hir        {

EffectGroup::EffectGroup( ::lavascript::zone::Zone* zone , WriteEffect* write ):
  write_effect_(write),
  read_list_   (),
  zone_        (zone)
{}

EffectGroup::EffectGroup( const EffectGroup& that ):
  write_effect_(that.write_effect_),
  read_list_   (that.zone_,that.read_list_),
  zone_        (that.zone_)
{}

void EffectGroup::UpdateWriteEffect( WriteEffect* node ) {
  node->HappenAfter(write_effect_);
  PropagateWriteEffect(node);
}

void EffectGroup::AddReadEffect( ReadEffect* node ) {
  node->SetWriteEffect(write_effect_);
  PropagateReadEffect(node);
}

void EffectGroup::PropagateWriteEffect( WriteEffect* effect ) {
  write_effect_ = effect;
  read_list_.Clear();
}

void EffectGroup::PropagateReadEffect( ReadEffect* node ) {
  read_list_.Add(zone_,node);
}

RootEffectGroup::RootEffectGroup( ::lavascript::zone::Zone* zone , WriteEffect* effect ):
  EffectGroup(zone,effect),
  list_      (NULL),
  object_    (NULL)
{}

RootEffectGroup::RootEffectGroup( const RootEffectGroup& that ):
  EffectGroup(that),
  list_      (that.list_),
  object_    (that.object_)
{}

void RootEffectGroup::UpdateWriteEffect( WriteEffect* effect ) {
  // due to the fact each group will propogate when read happened, so
  // only observe |this| read_list_ will see all the ReadEffect happened
  // before this write which is enough for us to establish the partial
  // order.
  EffectGroup::UpdateWriteEffect(effect);

  // Since this is a new *WriteEffect* , we should just propogate it back
  // to the list and object. The propogation could just set the write effect
  // to these 2 effect group directly
  list_->PropagateWriteEffect(effect);
  object_->PropagateWriteEffect(effect);
}

void RootEffectGroup::AddReadEffect( ReadEffect* effect ) {
  EffectGroup::AddReadEffect(effect);
  // propogate the read effect to the list and object node
  list_->PropagateReadEffect(effect);
  object_->PropagateReadEffect(effect);
}

LeafEffectGroup::LeafEffectGroup( ::lavascript::zone::Zone* zone , WriteEffect* effect ):
  EffectGroup(zone,effect), parent_(NULL)
{}

LeafEffectGroup::LeafEffectGroup( const LeafEffectGroup& that ):
  EffectGroup(that), parent_(NULL)
{}

void LeafEffectGroup::UpdateWriteEffect( WriteEffect* effect ) {
  EffectGroup::UpdateWriteEffect(effect);
  // Propagate back to the parental node with the write effect
  parent_->PropagateWriteEffect(effect);
}

void LeafEffectGroup::AddReadEffect( ReadEffect* effect ) {
  EffectGroup::AddReadEffect(effect);
  // propogate the read effect back to the parental node
  parent_->PropagateReadEffect(effect);
}

Effect::Effect( ::lavascript::zone::Zone* zone , WriteEffect* effect ):
  root_  (zone,effect),
  list_  (zone,effect),
  object_(zone,effect)
{
  root_.set_list    (&list_);
  root_.set_object  (&object_);
  list_.set_parent  (&root_);
  object_.set_parent(&root_);
}

Effect::Effect( const Effect& that ):
  root_  (that.root_),
  list_  (that.list_),
  object_(that.object_)
{}

void Effect::Merge( const Effect& lhs , const Effect& rhs , Effect* output , Graph* graph ,
                                                                             ControlFlow* region ) {
  auto lhs_root = lhs.root();
  auto rhs_root = rhs.root();
  // create an effect phi to join effect created by the root node
  auto effect_phi = WriteEffectPhi::New(graph,lhs_root->write_effect(),rhs_root->write_effect(),region);

  // propogate the effect
  output->root_.PropagateWriteEffect  (effect_phi);
  output->list_.PropagateWriteEffect  (effect_phi);
  output->object_.PropagateWriteEffect(effect_phi);
}

} // namespace hir
} // namespace lavascript
} // namespace cbase
