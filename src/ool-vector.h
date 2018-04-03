#ifndef OOL_VECTOR_H_
#define OOL_VECTOR_H_

#include <vector>

namespace lavascript {

// A OOLVector that will automatically enlarge its internal size
// when you access array in an out of bound fashion. It is just a
// wrapper around std::vector. For zone based OOLVector please check
// src/zone/ool-vector.h objects which uses *zone*
template< typename T >
class OOLVector : public std::vector<T> {
 public:
  typedef std::vector<T> Base;
  explicit OOLVector( std::size_t size = 0 ): Base(size) {}
  OOLVector() : Base() {}
  template< typename IDX >
  inline T& operator [] ( IDX index );
  template< typename IDX >
  const T& operator [] ( IDX index ) const {
    return const_cast<OOLVector>(*this)->operator [](index);
  }
};

template< typename T >
template< typename IDX >
inline T& OOLVector<T>::operator [] ( IDX index ) {
  if( index >= Base::size() ) {
    Base::resize(index+1);
  }
  return Base::operator [](index);
}

} // namespace lavascript

#endif // OOL_VECTOR_H_
