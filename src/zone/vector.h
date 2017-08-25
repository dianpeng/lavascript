#ifndef ZONE_VECTOR_H_
#define ZONE_VECTOR_H_
#include "zone.h"

#include <cstring>
#include <type_traits>

namespace lavascript {
namespace zone {

template< typename T >
class Vector : ZoneObject {
  static_assert( std::is_pod<T>::value || std::is_base_of<ZoneObject,T>::value );
  static_assert( std::is_pod<T>::value || std::is_trivially_destructible<T>::value );
 public:
  Vector();
  Vector( Zone* , size_t size );
  Vector( Zone* , const Vector& );

  static Vector* New( Zone* zone ) { return ::new (zone->Malloc<Zone>()) Zone(); }
  static Vector* New( Zone* zone , size_t length )
  { return ::new (zone->Malloc<Zone>()) Vector(zone,length); }
  static Vector* New( Zone* zone , const Vector& that )
  { return ::new (zone->Malloc<Zone>()) Vector(zone,that); }

 public:
  size_t size() const { return size_; }
  size_t capacity() const { return capacity_; }
  bool empty() const { return size_ == 0; }
  void Reserve( Zone* zone , size_t size );
  void Add( Zone* zone , const T& );
  void Del() { lava_assert(!empty(),"Del() on empty vector!"); --size_; }
  void Clear() { size_ = 0; }
  T& First() { lava_assert(!empty(),"First() on empty vector!"); return ptr_[0]; }
  const T& First() const { return const_cast<Vector*>(this)->First(); }
  T& Last() { lava_assert(!empty(),"Last() on empty vector!"); return ptr_[size_-1]; }
  const T& Last() const { return const_cast<Vector*>(this)->Last(); }
  T& Index( int index ) {
    lava_assert( index < static_cast<int>(size_) ,"Index out of boundary!");
    return ptr_[index];
  }
  const T& Index( int index ) const
  { return const_cast<Vector*>(this)->Index(index); }
  T& operator [] ( int index ) { return Index(index); }
  const T& operator [] ( int index ) const { return Index(index); }

 private:
  T* ptr_;                    // Pointer to the start of the memory
  size_t size_;               // Size of the current vector
  size_t capacity_;           // Capacity of the current vector

  LAVA_DISALLOW_COPY_AND_ASSIGN(Vector);
};

template< typename T > Vector<T>::Vector() :
  ptr_(NULL),
  size_(0),
  capacity_(0)
{}

template< typename T > Vector<T>::Vector( Zone* zone , size_t size ):
  ptr_(NULL),
  size_(0),
  capacity_(0)
{ Reserve( zone , size) ; }

template< typename T > Vector<T>::Vector( Zone* zone , const Vector& that ):
  ptr_(NULL),
  size_(0),
  capacity_(0)
{
  Reserve(zone,that.size());
  memcpy(ptr_,that.ptr_,that.size() * sizeof(T));
  size_ = that.size();
}

template< typename T > void Vector<T>::Add( Zone* zone , const T& value ) {
  if( size_ == capacity_ ) Reserve( zone , size_ ? size_ * 2 : 2 );
  ptr_[size_++] = value;
}

template< typename T > void Vector<T>::Reserve( Zone* zone , size_t length ) {
  if(length > capacity_) {
    void* new_buffer = zone->Malloc( length * sizeof(T) );
    if(ptr_) memcpy(new_buffer,ptr_,sizeof(T)*size_);
    ptr_ = static_cast<T*>(new_buffer);
    capacity_ = length;
  }
}

} // namespace zone
} // namespace lavascript

#endif // ZONE_VECTOR_H_
