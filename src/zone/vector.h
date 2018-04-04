#ifndef ZONE_VECTOR_H_
#define ZONE_VECTOR_H_
#include "zone.h"
#include "src/util.h"

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
  void Advance( std::size_t offset ) { Traits::Advance(vec_,offset,&cursor_); }
  std::int64_t cursor() const { return cursor_; }
  inline const T& value() const;
  inline T& value();
  inline void set_value( const T& value );
  inline bool operator == ( const IteratorBase& that ) const {
    return vec_ == that.vec_ && cursor_ == that.cursor_;
  }
  inline bool operator != ( const IteratorBase& that ) const {
    return !(*this == that);
  }
 private:
  IteratorBase( typename Traits::VectorType* vec ):
    vec_(vec), cursor_(Traits::InitCursor(vec))
  {}

  IteratorBase( typename Traits::VectorType* vec , std::int64_t cursor ):
    vec_(vec), cursor_(cursor)
  {}

  typename Traits::VectorType* vec_;
  mutable std::int64_t cursor_;
  friend class Vector<T>;
};

template< typename T > struct VectorForwardTraits {
 public:
  typedef Vector<T> VectorType;
  inline static std::int64_t InitCursor( VectorType* vec );
  inline static bool HasNext( VectorType* vec , std::int64_t cursor );
  inline static bool Move   ( VectorType* vec , std::int64_t* cursor );
  inline static void Advance( VectorType* vec , std::size_t     , std::int64_t* cursor );
};

template< typename T > struct VectorBackwardTraits {
 public:
  typedef const Vector<T> VectorType;
  inline static std::int64_t InitCursor( VectorType* vec );
  inline static bool HasNext( VectorType* vec , std::int64_t cursor );
  inline static bool Move   ( VectorType* vec , std::int64_t* cursor );
  inline static void Advance( VectorType* vec , std::size_t, std::int64_t* cursor );
};

} // namespace detail

template< typename T >
class Vector : ZoneObject {
  LAVASCRIPT_ZONE_CHECK_TYPE(T);
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

 public:
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

 public: // Remove or Insert
  // Insert an element *before* this iterator
  ConstForwardIterator Insert( Zone* , ConstForwardIterator& pos , const T& );
  ConstForwardIterator Insert( Zone* zone , std::int64_t index , const T& value ) {
    auto n(GetForwardIterator()); n.Advance(index);
    return Insert(zone,n,value);
  }
  ConstForwardIterator Remove( ConstForwardIterator& start , ConstForwardIterator& end );
  ConstForwardIterator Remove( ConstForwardIterator& pos ) {
    auto n(pos); n.Move();
    return Remove(pos,n);
  }
  ConstForwardIterator Remove( std::int64_t index ) {
    auto itr(GetForwardIterator()); itr.Advance(index);
    return Remove(itr);
  }
  ConstForwardIterator Remove( std::int64_t start , std::int64_t end ) {
    auto itr (GetForwardIterator()); itr.Advance(start);
    auto iend(GetForwardIterator()); iend.Advance(end);
    return Remove(itr,iend);
  }

 private:
  std::int64_t IterToCursor( ConstForwardIterator& itr ) {
    auto cursor = static_cast<std::size_t>(itr.cursor_);
    return static_cast<std::int64_t>(cursor > size_ ? size_ : cursor);
  }

  T* ptr_;                    // Pointer to the start of the memory
  std::size_t size_;          // Size of the current vector
  std::size_t capacity_;      // Capacity of the current vector

  LAVA_DISALLOW_COPY_AND_ASSIGN(Vector);
};

// ===============================================================================
// OOLVector

// A default enlarge policy. basically whenever we need to enlarge the array
// size, we multiply by 2
template< typename T >
struct OOLVectorDefaultEnlargePolicy {
 public:
  // return enlarged size number
  T GetSize( const T& value ) { auto idx = value ? value : 1 ; return idx*2; }
};

// A vector that will automatically enlarge array when an random index is provided.
// Used for helping node tracking and construction
template< typename T , typename Policy = OOLVectorDefaultEnlargePolicy<T>>
class OOLVector : public Vector<T> {
 public:
  OOLVector( Zone* zone , std::size_t size = 0 ): Vector<T>(zone,size) {}
  template< typename IDX > T& Get( Zone* zone , const IDX& idx ) {
    if(size() <= idx) Resize(zone,Policy().GetSize(idx));
    return Index(static_cast<int>(idx));
  }
  template< typename IDX > const T& Get( Zone* zone , const IDX& idx ) const {
    return const_cast<OOLVector*>(this)->Get(zone,idx);
  }
  template< typename IDX > void Set( Zone* zone , const IDX& idx , const T& val ) {
    if(size() <= idx) Resize(zone,Policy().GetSize(idx));
    Index(static_cast<int>(idx)) = val;
  }
};

// =============================================================================
//
// Function definition
//
// =============================================================================
template< typename T > Vector<T>::Vector() : ptr_(NULL), size_(0), capacity_(0) {}

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
  MemCopy<T>(ptr_,that.ptr_,that.size());
  size_ = that.size();
}

template< typename T > void Vector<T>::Add( Zone* zone , const T& value ) {
  if( size_ == capacity_ ) Reserve( zone , size_ ? size_ * 2 : 2 );
  ptr_[size_++] = value;
}

template< typename T > void Vector<T>::Reserve( Zone* zone , std::size_t length ) {
  if(length > capacity_) {
    void* new_buffer = zone->Malloc( length * sizeof(T) );
    if(ptr_) MemCopy<T>(static_cast<T*>(new_buffer),ptr_,size_);
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

template< typename T > typename Vector<T>::ConstForwardIterator
Vector<T>::Insert( Zone* zone , ConstForwardIterator& pos , const T& value ) {
  auto cursor = IterToCursor(pos);

  // 1. ensure the memory or capacity
  if(size_ == capacity_) Reserve(zone,capacity_*2);

  // 2. move the element from cursor position
  if(static_cast<std::size_t>(cursor) < size_)
    MemMove<T>(static_cast<T*>(BufferOffset<T>(ptr_,cursor+1)),
               static_cast<T*>(BufferOffset<T>(ptr_,cursor)),(size_-cursor));

  // 3. place the element at the cursor position
  ConstructFromBuffer<T>(ptr_+cursor,value);

  ++size_;
  return ConstForwardIterator(this,cursor);
}

template< typename T > typename Vector<T>::ConstForwardIterator
Vector<T>::Remove( ConstForwardIterator& start , ConstForwardIterator& end ) {
  auto pos_start = IterToCursor(start);
  auto pos_end   = IterToCursor(end);

  // move the element starting from pos_end until the real end to
  // position pointed by the pos_start
  if(static_cast<std::size_t>(pos_end) < size_)
    MemMove(static_cast<T*>(BufferOffset<T>(ptr_,pos_start)),
            static_cast<T*>(BufferOffset<T>(ptr_,pos_end)),(size_-pos_end));

  size_ -= (pos_end - pos_start);
  return ConstForwardIterator(this,pos_start);
}

namespace detail {

template< typename T >
inline std::int64_t VectorForwardTraits<T>::InitCursor( VectorType* vec ) {
  (void)vec;
  return 0;
}

template< typename T >
inline bool VectorForwardTraits<T>::HasNext( VectorType* vec , std::int64_t cursor ) {
  return static_cast<std::size_t>(cursor) < vec->size();
}

template< typename T >
inline bool VectorForwardTraits<T>::Move( VectorType* vec , std::int64_t* cursor ) {
  *cursor = *cursor+1;
  return HasNext(vec,*cursor);
}

template< typename T >
inline void VectorForwardTraits<T>::Advance( VectorType* vec , std::size_t offset ,
                                                               std::int64_t* cursor ) {
  *cursor += offset;
  if(static_cast<std::size_t>(*cursor) > vec->size())
    *cursor = static_cast<std::int64_t>(vec->size());
}

template< typename T >
inline std::int64_t VectorBackwardTraits<T>::InitCursor( VectorType* vec ) {
  std::int64_t sz = static_cast<std::int64_t>(vec->size());
  return (sz - 1);
}

template< typename T >
inline bool VectorBackwardTraits<T>::HasNext( VectorType* vec , std::int64_t cursor ) {
  return cursor >= 0;
}

template< typename T >
inline bool VectorBackwardTraits<T>::Move( VectorType* vec , std::int64_t* cursor ) {
  *cursor = *cursor - 1;
  return HasNext(vec,*cursor);
}

template< typename T >
inline void VectorBackwardTraits<T>::Advance( VectorType* vec , std::size_t offset ,
                                                                std::int64_t* cursor ) {
  (void)vec;

  *cursor -= offset;
  if(*cursor < 0) *cursor = -1;
}

template< typename T , typename Traits >
inline const T& IteratorBase<T,Traits>::value() const {
  return vec_->Index(cursor_);
}

template< typename T , typename Traits >
inline T& IteratorBase<T,Traits>::value() {
  // Super ugly, but we need to get the none const type out and const
  // cast the vec_ back to none const pointer to be able to call the
  // correct Index, this is really a hack for const iterator , since
  // we just typedef const iterator. And a const iterator should never
  // be able to invoke this API since it doesn't have a const modifier
  typedef typename std::remove_const<typename Traits::VectorType>::type
    NoneConstVectorType;

  return const_cast<NoneConstVectorType*>(vec_)->Index(cursor_);
}

template< typename T , typename Traits >
inline void IteratorBase<T,Traits>::set_value( const T& value ) {
  vec_->Index(cursor_) = value;
}

} // namespace detail

} // namespace zone
} // namespace lavascript

#endif // ZONE_VECTOR_H_
