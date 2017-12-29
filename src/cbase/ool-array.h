#ifndef OOL_ARRAY_H_
#define OOL_ARRAY_H_

#include "src/trace.h"
#include "src/zone/zone.h"
#include "src/zone/vector.h"

namespace lavascript {
namespace cbase {

// This is a simple wrapper around the *zone::Vector* to allow
// automatically resize the internal array and reset them to be
// NULL pointer
template< typename T > class OOLArray {
 public:
  OOLArray( ::lavascript::zone::Zone* zone , std::size_t size = 0 ):
    zone_(zone),
    vec_ (zone,size)
  {}

 public:
  zone::Zone* zone() const { return zone_; }

  const T& operator [] ( std::size_t index ) const {
    if(index >= vec_.size()) vec_.Resize(zone_,index+1);
    return vec_[index];
  }

  T& operator [] ( std::size_t index ) {
    if(index >= vec_.size()) vec_.Resize(zone_,index+1);
    return vec_[index];
  }

 private:
  zone::Zone*     zone_;
  zone::Vector<T> vec_;

  LAVA_DISALLOW_COPY_AND_ASSIGN(OOLArray)
};

} // namespace cbase
} // namespace lavascript

#endif // OOL_ARRAY_H_
