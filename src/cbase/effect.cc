#include "effect.h"

namespace lavascript {
namespace cbase      {

using namespace ::lavascript;

NaiveEffectGroup::NaiveEffectGroup( zone::Zone* zone , MemoryWrite* w ):
  write_effect_(w),
  read_list_   (),
  children_    (),
  zone_        (zone)
{}

NaiveEffectGroup::NaiveEffectGroup( const NaiveEffectGroup& that ):
  write_effect_(that.write_effect_),
  read_list_   (that.zone_,that.read_list_),
  children_    (that.zone_,that.children_ ),
  zone_        (that.zone_)
{}

// Recursively visiting effect group starts at *this* effect group. Use id()
// value as a key to index to an OOLVector to track whether a node is visited
// or not.
void NaiveEffectGroup::DoVisit( zone::Zone* zone , zone::OOLVector<bool>* visited , EffectGroup* grp ,
                                                                                    const Visitor& visitor ) {
  visited->Set(zone,grp->id(),true); // mark it as visited before
  visitor(this);
  // recursively visit all children inside of this EffectGroup
  for( auto itr(children_.GetForwardIterator()); itr.HasNext(); itr.Move() ) {
    auto grp = itr.value().grp;
    if(!visited->Get(grp->id())) {
      DoVisit(zone,visited,grp,visitor);
    }
  }
}

void NaiveEffectGroup::Visit( const std::function< void(EffectGroup*) >& visitor ) {
  static const std::size_t kDefaultStackSize = 1024;

  // for at least 1024 node recording will be on stack, rest of them will be
  // resides on heap
  zone::StackZone<kDefaultStackSize> stack_zone(zone_);
  zone::OOLVector<bool>              visited   (&stack_zone,kDefaultStackSize);
  // start visiting recursively
  DoVisit(&stack_zone,&visited,this,visitor);
}

void NaiveEffectGroup::AddReadEffect( MemoryRead* read ) {
  Visit([=](EffectGroup* grp) { read->AddEffect(grp->write_effect_); grp->read_list_.Add(zone_,read); });
}

void NaiveEffectGroup::UpdateWriteEffect( MemoryWrite* write ) {
  Visit([=](EffectGroup* grp) {
      // make this write happened at every read previously happend and observable from this
      // effect group. Basically maintains the write after read (anti) dependency
      for( auto itr(grp->read_list_.GetForwardIterator()); itr.HasNext(); itr.Move() ) {
        // tries to dedup the effect list since in current phase this is the most likely
        // place to introduce duplication which may hurt us in memory usage and runtime cost
        write->AddEffectIfNotExist(itr.value());
      }
      // clear its read list since all the reader happened previously has been properly
      // linearlized in terms of effect order
      grp->read_list_.Clear();
      // update itself to be the new write effect object
      grp->write_effect_ = write;
  });
}

void NaiveEffectGroup::AssignEffectGroup( Expr* key , EffectGroup* grp ) {
  // avoid obviously cycle effect group since if we don't assign cyclic effect group it
  // won't hurt any body since we already track it
  if(grp != this) {
    // find if there such effect group inside of the Vector object
    auto itr = children_.FindIf([=](const Slice& slice) { return slice.key->IsEqual(key); });
    if(itr.HasNext()) {
      auto v = itr.value();
      v.grp = grp;
      itr.set_value(v);
    } else {
      children_Add(zone_,Slice(key,grp));
    }
  }
}

EffectGroup* NaiveEffectGroup::Resolve( Expr* key ) {
  auto itr = children_.FindIf([=](const Slice& slice) { return slice.key->IsEqual(key); });
  if(itr.HasNext()) {
    return itr.value().grp;
  }
  return this; // top most effect group, represent the most blur way to do effect analyze
}

void NaiveEffectGroup::CopyFrom( const EffectGroup& grp ) {
  read_list_.Clear();
  children_.Clear();
  zone_         = grp.zone_;
  write_effect_ = grp.write_effect_;
  read_list_.CopyFrom(zone_,grp.read_list_);
  children_.CopyFrom (zone_,grp.children_ );
}




