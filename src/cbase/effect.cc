#include "effect.h"

namespace lavascript {
namespace cbase      {

using namespace ::lavascript;

BasicEffectGroup::BasicEffectGroup( std::uint32_t id , zone::Zone* zone , MemoryWrite* w ):
  EffectGroup  (id),
  write_effect_(w),
  read_list_   (),
  children_    (),
  zone_        (zone)
{}

void BasicEffectGroup::Visit( const std::function< void(EffectGroup*) >& visitor ) {
  static const std::size_t kDefaultStackSize = 1024;
  // for at least 1024 node recording will be on stack, rest of them will be
  // resides on heap
  zone::StackZone<kDefaultStackSize> stack_zone(zone_);
  zone::OOLVector<bool>              visited   (&stack_zone,kDefaultStackSize);
  // start visiting recursively
  EffectGroup::DoVisit(&stack_zone,&visited,this,visitor);
}

void BasicEffectGroup::AddReadEffect( MemoryRead* read ) {
  Visit([=](EffectGroup* grp) {
    // try to dedup the effect list, maybe slow here
    read->AddEffectIfNotExit(grp->write_effect_);
    grp->read_list_.Add(zone_,read);
  });
}

void BasicEffectGroup::UpdateWriteEffect( MemoryWrite* write ) {
  Visit([=](EffectGroup* grp) {
    grp->VisitRead([=](MemoryRead* effect) { write->AddEffectIfNotExist(effect); });
    grp->read_list_.Clear();
    grp->write_effect_ = write;
  });
}

void BasicEffectGroup::VisitRead( const ReadVisitor& visitor ) {
  for( auto itr(read_list_.GetForwardIterator()); itr.HasNext(); itr.Move() ) {
    visitor(itr.value());
  }
}

void BasicEffectGroup::VisitChildren( const ChildrenVisitor& visitor ) {
  for( auto itr(children_.GetForwardIterator()); itr.HasNext(); itr.Move() ) {
    visitor(itr.value());
  }
}

void BasicEffectGroup::AssignEffectGroup( Expr* key , EffectGroup* grp ) {
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

EffectGroup* BasicEffectGroup::Resolve( Expr* key ) {
  auto itr = children_.FindIf([=](const Slice& slice) { return slice.key->IsEqual(key); });
  if(itr.HasNext()) {
    return itr.value().grp;
  }
  return this; // top most effect group, represent the most blur way to do effect analyze
}

void BasicEffectGroup::CopyFrom( const EffectGroup& grp ) {
  read_list_.Clear();
  children_.Clear();
  zone_         = grp.zone();
  read_list_.Reserve(zone_,grp.read_size());
  children_.Reserve(zone_,grp.children_size());
  grp.VisitRead    ([=](MemoryRead* effect) { read_list_.Add(zone_,effect);       });
  grp.VisitChildren([=](Slice* slice      ) { children_.Add(zone_,Slice(*slice)); });
}

// This constructor initialize an COWEffectGroup with copy semantic initially
COWEffectGroup::COWEffectGroup( std::uint32_t id , zone::Zone* zone , MemoryWrite* write ):
  EffectGroup(id),
  native_(zone,write),
  prev_  (NULL),
  copy_  (true)
{}

// Copy on write setup
COWEffectGroup::COWEffectGroup( std::uint32_t id , const EffectGroup* grp ) :
  EffectGroup(id),
  native_(zone,NULL),
  prev_  (grp),
  copy_  (false)
{}

MergeEffectGroup::MergeEffectGroup( ::lavascript::zone::Zone* zone , NoWriteEffect* no_write_effect ,
                                                                     EffectGroup* lhs,
                                                                     EffectGroup* rhs ) :
  no_write_effect_(no_write_effect),
  lhs_            (lhs),
  rhs_            (rhs),
  zone_           (zone)
{}

void MergeEffectGroup::VisitRead( const ReadVisitor& visitor ) const {
  lhs_->VisitRead(visitor);
  rhs_->VisitRead(visitor);
}

void MergeEffectGroup::Visit( const Visitor& visitor ) {
  static const std::size_t kDefaultStackSize = 1024;

  // for at least 1024 node recording will be on stack, rest of them will be
  // resides on heap
  zone::StackZone<kDefaultStackSize> stack_zone(zone_);
  zone::OOLVector<bool>              visited   (&stack_zone,kDefaultStackSize);

  visited[id()] = true; // mark itself visited before to avoid cycle and also we don't have
                        // anything inside of this node

 // Visit both lhs/rhs for code
 EffectGroup::DoVisit(&stack_zone,&visited,lhs,visitor);
 EffectGroup::DoVisit(&stack_zone,&visited,rhs,visitor);
}

void MergeEffectGroup::AddReadEffect( MemoryRead* read ) {
  Visit([=](EffectGroup* grp) {
    // try to dedup the effect list, maybe slow here
    read->AddEffectIfNotExit(grp->write_effect_);
    grp->read_list_.Add(zone_,read);
  });
}

void MergeEffectGroup::UpdateWriteEffect( MemoryWrite* write ) {
  Visit([=](EffectGroup* grp) {
    grp->VisitRead([=](MemoryRead* effect) { write->AddEffectIfNotExist(effect); });
    grp->read_list_.Clear();
    grp->write_effect_ = write;
  });
}

EffectGroupList::EffectGroupList( const EffectGroupList& that ):
  factory_(that.factory_),
  ool_    (that.zone())
{
  // reserve enough memory, and then do the allocation
  ool_.Reserve(zone(),that.ool_.size());
  // create bunch of COWEffectGroup
  for( auto itr(that.ool_.GetForwardIterator()); itr.HasNext(); itr.Move() ) {
    ool_.Add(zone(),factory_->NewCOWEffectGroup(itr.value()));
  }
}

namespace {

enum { EFFECT_GROUP , PHI , NONE };

int ResolveExpr( Expr* node ,  EffectGroupList* list , BasicEffectGroup* root ,
                                                       Phi**         phi ,
                                                       EffectGroup** grp ) {
  zone::StackZone<1024>      zone;
  zone::Vector<MemoryWrite*> vec(&zone);
  EffectGroup* eg = NULL;

  // find out the deepst node that has a tracking effect group
  do {
    switch(node->type()) {
      case IRTYPE_ARG : case IRTYPE_GGET  : case IRTYPE_UGET:
      case IRTYPE_LIST: case IRTYPE_OBJECT:
        eg = list->Get(node->id());
        lava_debug(NORMAL,lava_verify(eg););
        goto done;

      case IRTYPE_GGET: case IRTYPE_IGET: case IRTYPE_OBJECT_GET: case IRTYPE_LIST_GET:
        { // do a deeper probing
          auto mem = static_cast<MemoryWrite*>(node);
          node     = mem->Memory();
          // record this memory node for later usage
          vec.Add(&zone,mem);
        }
        break;
      case IRTYPE_PHI: *phi = node; return PHI;
      default:         return NONE;
    }
  } while(true);

done:
  // unwind the stack and then get the smallest effect group for resolution purpose
  for( auto itr(vec.GetBackwardIterator()); itr.HasNext(); itr.Move() ) {
    auto    n = itr.value();
    Expr* key = NULL;
    switch(n->type()) {
      case IRTYPE_GGET       : key = n->AsGGet()->key();      break;
      case IRTYPE_IGET       : key = n->AsIGet()->index();    break;
      case IRTYPE_OBJECT_GGET: key = n->AsObjectGet()->key(); break;
      case IRTYPE_OBJCT_IGET : key = n->AsListGet()->index(); break;
      default: lava_die(); break;
    }
    auto sub_eg = eg->Resolve(key);
    if(sub_eg == eg) break;
    eg = sub_eg;
  }

  *grp = eg;
  return EFFECT_GROUP;
}


} // namespace

} // namespace lavascript
} // namespace cbase
