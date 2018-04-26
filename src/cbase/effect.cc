#include "effect.h"

namespace lavascript {
namespace cbase      {
namespace hir        {

EffectGroup::EffectGroup( ::lavascript::zone::Zone* zone , WriteEffect* write ):
  write_effect_(write),
  read_list_   (),
  zone_        (zone)
{}

EffectGroup::EffectGroup( const Effect& that ):
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
  list_->DoAddReadEffect(effect);
  object_->DoAddReadEffect(effect);
}

LeafEffectGroup::LeafEffectGroup( ::lavascript::zone::Zone* zone , WriteEffect* effect ):
  EffectGroup(zone,effect),
  list_      (NULL),
  object_    (NULL)
{}

LeafEffectGroup::LeafEffectGroup( const RootEffectGroup& that ):
  EffectGroup(that),
  list_      (that.list_),
  object_    (that.object_)
{}

void LeafEffectGroup::UpdateWriteEffect( WriteEffect* effect ) {
  DoUpdateWriteEffect(effect);
  parent_->DoUpdateWriteEffect(effect);
}

void LeafEffectGroup::AddReadEffect( ReadEffect* effect ) {
  DoAddReadEffect(effect);
  parent_->DoAddReadEffect(effect);
}

Effect::Effect( ::lavascript::zone::Zone* zone , WriteEffect* effect ):
  root_  (zone,effect),
  list_  (zone,effect),
  object_(zone,effect)
{
  root_.set_list(&list_);
  root_.set_object(&object_);
  object_.set


} // namespace hir
} // namespace lavascript
} // namespace cbase
