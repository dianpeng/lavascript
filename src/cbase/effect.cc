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

void NaiveEffectGroup::AddReadEffect( MemoryRead* read ) {
}

