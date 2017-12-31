#ifndef ZONE_VECTOR_H_
#define ZONE_VECTOR_H_
#include "zone.h"

#include <cstdint>
#include <algorithm>
#include <cstring>
#include <type_traits>

namespace lavascript {
namespace zone {

template< typename T > class Vector;

namespace detail {

// Iterator for vector class
template< typename T , typename Traits >
class IteratorBase {
 public:
  bool HasNext() const { return Traits::HasNext(vec_,cursor_); }
  bool Move   ()       { return Traits::Move(vec_,&cursor_);   }
  inline const T& value() const;
  inline void set_value( const T& value );
 private:
  IteratorBase( Vector<T>* vec ): vec_(vec), cursor_(Traits::InitCursor(vec)) {}

  Vector<T>* vec_;
  mutable std::int64_t cursor_;
  friend class Vector<T>;
};

template< typename T > struct VectorForwardTraits {
 public:
  inline static std::int64_t InitCursor( Vector<T>* vec );
  inline static bool HasNext( Vector<T>* vec , std::int64_t cursor );
  inline static bool Move   ( Vector<T>* vec , std::int64_t* cursor );
};

template< typename T > struct VectorBackwardTraits {
 public:
  inline static std::int64_t InitCursor( Vector<T>* vec );
  inline static bool HasNext( Vector<T>* vec , std::int64_t cursor );
  inline static bool Move   ( Vector<T>* vec , std::int64_t* cursor );
};

} // namespace detail

template< typename T >
class Vector : ZoneObject {
  static_assert( std::is_pod<T>::value || std::is_base_of<ZoneObject,T>::value );
  static_assert( std::is_pod<T>::value || std::is_trivially_destructible<T>::value );
 public:
  Vector();
  Vector( Zone* , std::size_t size );
  Vector( Zone* , const Vector& );

  static Vector* New( Zone* zone )
  { return ::new (zone->Malloc<Vector>()) Vector(); }

  static Vector* New( Zone* zone , std::size_t length )
  { return ::new (zone->Malloc<Vector>()) Vector(zone,length); }

  static Vector* New( Zone* zone , const Vector& that )
  { return ::new (zone->Malloc<Vector>()) Vector(zone,that); }

 public:
  std::size_t size() const { return size_; }
  std::size_t capacity() const { return capacity_; }
  bool empty() const { return size_ == 0; }
  void Reserve( Zone* zone , std::size_t size );
  void Resize ( Zone* zone , std::size_t size );

  void Add( Zone* zone , const T& );
  void Set( int index , const T& value ) {
    lava_assert( index >= 0 && index < static_cast<int>(size_) , "Index out of boundary" );
    ptr_[index] = value;
  }
  void Del() { lava_assert(!empty(),"Del() on empty vector!"); --size_; }
  void Clear() { size_ = 0; }
  T& First() { lava_assert(!empty(),"First() on empty vector!"); return ptr_[0]; }
  const T& First() const { return const_cast<Vector*>(this)->First(); }
  T& Last() { lava_assert(!empty(),"Last() on empty vector!"); return ptr_[size_-1]; }
  const T& Last() const { return const_cast<Vector*>(this)->Last(); }
  T& Index( int index ) {
    lava_assert( index >= 0 && index < static_cast<int>(size_) ,"Index out of boundary!");
    return ptr_[index];
  }
  const T& Index( int index ) const
  { return const_cast<Vector*>(this)->Index(index); }
  T& operator [] ( int index ) { return Index(index); }
  const T& operator [] ( int index ) const { return Index(index); }
  void Swap( Vector* );

 public:
  typedef detail::IteratorBase<T,detail::VectorForwardTraits<T>> ForwardIterator;
  typedef const ForwardIterator ConstForwardIterator;

  typedef detail::IteratorBase<T,detail::VectorBackwardTraits<T>> BackwardIterator;
  typedef const BackwardIterator ConstBackwardIterator;

  ForwardIterator  GetForwardIterator()  { return ForwardIterator(this); }
  BackwardIterator GetBackwardIterator() { return BackwardIterator(this); }
  ConstForwardIterator  GetForwardIterator()  const { return ConstForwardIterator(this); }
  ConstBackwardIterator GetBackwardIterator() const { return ConstBackwardIterator(this); }

 private:
  T* ptr_;                    // Pointer to the start of the memory
  std::size_t size_;               // Size of the current vector
  std::size_t capacity_;           // Capacity of the current vector

  LAVA_DISALLOW_COPY_AND_ASSIGN(Vector);
};

template< typename T > Vector<T>::Vector() :
  ptr_(NULL),
  size_(0),
  capacity_(0)
{}

template< typename T > Vector<T>::Vector( Zone* zone , std::size_t size ):
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

template< typename T > void Vector<T>::Reserve( Zone* zone , std::size_t length ) {
  if(length > capacity_) {
    void* new_buffer = zone->Malloc( length * sizeof(T) );
    if(ptr_) memcpy(new_buffer,ptr_,sizeof(T)*size_);
    ptr_ = static_cast<T*>(new_buffer);
    capacity_ = length;
  }
}

template< typename T > void Vector<T>::Resize( Zone* zone , std::size_t length ) {
  if(length > capacity_) {
    Reserve(zone,length);
  }

  // initialize the certain part to be T()
  {
    std::size_t diff = (size_ > length) ? (size_ - length) : 0;
    for( std::size_t i = 0 ; i < diff ; ++i ) {
      ConstructFromBuffer<T>(ptr_+size_+i);
    }
  }

  size_ = length;
}

template< typename T > void Vector<T>::Swap( Vector* that ) {
  std::swap(ptr_,that->ptr_);
  std::swap(size_,that->size_);
  std::swap(capacity_,that->capacity_);
}

namespace detail {

template< typename T >
inline std::int64_t VectorForwardTraits<T>::InitCursor( Vector<T>* vec ) {
  (void)vec;
  return 0;
}

template< typename T >
inline bool VectorForwardTraits<T>::HasNext( Vector<T>* vec , std::int64_t cursor ) {
  return cursor < vec->size();
}

template< typename T >
inline bool VectorForwardTraits<T>::Move( Vector<T>* vec , std::int64_t* cursor ) {
  *cursor = *cursor+1;
  return HasNext(vec,*cursor);
}

template< typename T >
inline std::int64_t VectorBackwardTraits<T>::InitCursor( Vector<T>* vec ) {
  std::int64_t sz = static_cast<std::int64_t>(vec->size());
  return (sz - 1);
}

template< typename T >
inline bool VectorBackwardTraits<T>::HasNext( Vector<T>* vec , std::int64_t cursor ) {
  return cursor >= 0;
}

template< typename T >
inline bool VectorBackwardTraits<T>::Move( Vector<T>* vec , std::int64_t* cursor ) {
  *cursor = *cursor - 1;
  return HasNext(vec,*cursor);
}

template< typename T , typename Traits >
inline const T& IteratorBase<T,Traits>::value() const { return vec_->Index(cursor_); }

template< typename T , typename Traits >
inline void IteratorBase<T,Traits>::set_value( const T& value ) {
  vec_->Index(cursor_) = value;
}

} // namespace detail

} // namespace zone
} // namespace lavascript

#endif // ZONE_VECTOR_H_
