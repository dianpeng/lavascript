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

void EffectGroup::DoUpdateWriteEffect( WriteEffect* node ) {
  if(read_list_.empty()) {
    node->AddEffect(write_effect_); // we don't have any read effect node
  } else {
    lava_foreach(ReadEffect* read,read_list_.GetForwardIterator()) {
      node->AddEffect(read);
    }
  }
  write_effect_ = node;
  read_list_.Clear();
}

void EffectGroup::DoAddReadEffect( ReadEffect* node ) {
  node->AddEffect(write_effect_);
  read_list_.Add(zone_,node);
}

void EffectGroup::set_write_effect( WriteEffect* effect ) {
  write_effect_ = effect;
  read_list_.Clear();
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
  DoUpdateWriteEffect(effect);
  list_->DoUpdateWriteEffect(effect);
  object_->DoUpdateWriteEffect(effect);
}

void RootEffectGroup::AddReadEffect( ReadEffect* effect ) {
  DoAddReadEffect(effect);
}

LeafEffectGroup::LeafEffectGroup( ::lavascript::zone::Zone* zone , WriteEffect* effect ):
  EffectGroup(zone,effect), parent_(NULL)
{}

LeafEffectGroup::LeafEffectGroup( const LeafEffectGroup& that ):
  EffectGroup(that), parent_(NULL)
{}

void LeafEffectGroup::UpdateWriteEffect( WriteEffect* effect ) {
  DoUpdateWriteEffect(effect);
  parent_->DoUpdateWriteEffect(effect);
}

void LeafEffectGroup::AddReadEffect( ReadEffect* effect ) {
  DoAddReadEffect(effect);
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
  // merge all the read effect from this effect phi
  lava_foreach( auto read , lhs_root->read_list().GetForwardIterator() ) {
    effect_phi->AddEffect(read);
  }
  lava_foreach( auto read , rhs_root->read_list().GetForwardIterator() ) {
    effect_phi->AddEffect(read);
  }
  output->root_.set_write_effect  (effect_phi);
  output->list_.set_write_effect  (effect_phi);
  output->object_.set_write_effect(effect_phi);
}

} // namespace hir
} // namespace lavascript
} // namespace cbase
