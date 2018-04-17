#ifndef ZONE_LIST_H_
#define ZONE_LIST_H_
#include "zone.h"

#include <functional>

namespace lavascript {
namespace zone {
template< typename T > class List;

namespace detail {

template< typename C >
struct NodeBase {
  C* prev;
  C* next;
  NodeBase( C* p , C* n ): prev(p),next(n) {}
  NodeBase();
  void Reset();
};

template< typename T > struct Node : public NodeBase<Node<T>> {
  typedef NodeBase<Node> Parent;
  T value;
  Node( const T& v ): Parent(NULL,NULL), value(v) {}
};

template< typename T , typename Traits > class Iterator {
 public:
  typedef                T ValueType;
  typedef       ValueType& ReferenceType;
  typedef const ValueType& ConstReferenceType;

  Iterator( Node<T>* iter , Node<T>* end ): iter_(iter), end_(end) {}
  Iterator(): iter_(NULL) , end_(NULL) {}

 public:
  bool HasNext() const { return iter_ != end_; }
  bool Advance( std::size_t times ) const;
  bool Move() const    { return Traits::Move(&iter_,end_); }

  ConstReferenceType value() const { lava_debug(NORMAL,lava_verify(HasNext());); return iter_->value; }
  ReferenceType      value() { lava_debug(NORMAL,lava_verify(HasNext());); return iter_->value; }
  void set_value( ConstReferenceType val ) {
    lava_debug(NORMAL,lava_verify(HasNext());); iter_->value = val;
  }

  bool operator == ( const Iterator& that ) const {
    return iter_ == that.iter_ && end_ == that.end_;
  }
  bool operator != ( const Iterator& that ) const {
    return !(*this == that);
  }
 private:
  mutable Node<T>* iter_;
  Node<T>* end_ ;
  friend class List<T>;
};

template< typename T , typename Traits >
bool Iterator<T,Traits>::Advance( std::size_t times ) const {
  for( ; times > 0 ; --times )
    if(!Move()) return false;
  return true;
}

template< typename C > NodeBase<C>::NodeBase(): prev(NULL), next(NULL) {
  prev = static_cast<C*>(this);
  next = static_cast<C*>(this);
}

template< typename C > void NodeBase<C>::Reset() {
  prev = static_cast<C*>(this);
  next = static_cast<C*>(this);
}

template< typename T > struct ListForwardTraits {
  inline static bool Move( Node<T>** , Node<T>* );
};

template< typename T > struct ListBackwardTraits {
  inline static bool Move( Node<T>** , Node<T>* );
};

} // namespace detail

// A linked list implementation as ZoneObject. We mainly need it because
// linked list supports way better method of deletion of a certain node.
template< typename T > class List : ZoneObject {
  typedef detail::Node<T> NodeType;
  typedef detail::NodeBase<NodeType> NodeBaseType;
 public:
  static List* New( Zone* zone ) { return zone->New<List<T>>(); }
  List() : end_ () , size_(0) {}
  List( Zone* zone , const List& that ) : end_() , size_(0) { Assign(zone,this,that); }
 public:
  typedef detail::Iterator<T,detail::ListForwardTraits<T>> ForwardIterator;
  typedef const ForwardIterator ConstForwardIterator;
  typedef detail::Iterator<T,detail::ListBackwardTraits<T>> BackwardIterator;
  typedef const BackwardIterator ConstBackwardIterator;

  ForwardIterator  GetForwardIterator()  { return ForwardIterator(end_.next,end()); }
  BackwardIterator GetBackwardIterator() { return BackwardIterator(end_.prev,end()); }

  ConstForwardIterator  GetForwardIterator()  const { return ConstForwardIterator(end_.next,end()); }
  ConstBackwardIterator GetBackwardIterator() const { return ConstBackwardIterator(end_.prev,end());}

 public:
  ForwardIterator PushBack( Zone* , const T& );
  ForwardIterator PopBack ();
  ForwardIterator Insert  ( Zone* , ConstForwardIterator& , const T& );
  ForwardIterator Remove  ( ConstForwardIterator& );

  const T& First   () const {
    lava_debug(NORMAL,lava_verify(!empty()););
    return end_.next->value;
  }

  const T& Last    () const {
    lava_debug(NORMAL,lava_verify(!empty()););
    return end_.prev->value;
  }

  T& First() {
    lava_debug(NORMAL,lava_verify(!empty()););
    return end_.next->value;
  }

  T& Last () {
    lava_debug(NORMAL,lava_verify(!empty()););
    return end_.prev->value;
  }

  void Clear() { size_ = 0; end_.Reset(); }
 public:
  void Assign( Zone* , const List& );
  template< typename ITR >
  void Assign( Zone* , const ITR& );

  void Append( Zone* , const List& );
  template< typename ITR >
  void Append( Zone* , const ITR& );
  // merge another list into this list, after the merging the input list becomes empty
  void Merge( List* list ) { Merge(list,ConstForwardIterator(end(),end())); }
  void Merge( List* , ConstForwardIterator );
 public:
  // resize the linked list to hold certain size/amount of nodes
  void Resize( Zone* , std::size_t );

  // index a certain position's node inside of linked list , linear
  // complexity
  const T& Index( std::size_t ) const;

  // set a certain value to an index
  void Set( std::size_t , const T& );

  ForwardIterator      FindIf( const std::function<bool (const T&) >& );

  ConstForwardIterator FindIf( const std::function<bool (const T&) >& ) const;

  ForwardIterator      Find( const T& value ) {
    return FindIf([value]( const T& v ) { return v == value; });
  }

  ConstForwardIterator Find( const T& value ) const {
    return FindIf([value]( const T& v ) { return v == value; });
  }

 public:
  std::size_t size() const { return size_; }
  bool empty() const { return size() == 0; }

 private:
  ForwardIterator InsertNode( ConstForwardIterator& , NodeType* );

 private:
  NodeType* end() const {
    return const_cast<NodeType*>(
        static_cast<const NodeType*>(&end_));
  }

  NodeBaseType end_;
  std::size_t size_;

  LAVA_DISALLOW_COPY_AND_ASSIGN(List);
};

template< typename T >
typename List<T>::ForwardIterator List<T>::PushBack( Zone* zone , const T& value ) {
  return Insert(zone,ConstForwardIterator(end(),end()),value);
}

template< typename T >
typename List<T>::ForwardIterator List<T>::PopBack() {
  return Remove(ConstForwardIterator(end_.prev,end()));
}

template< typename T >
typename List<T>::ForwardIterator List<T>::InsertNode( ConstForwardIterator& iter ,
                                                       NodeType* node ) {
  NodeType* pos = iter.iter_;
  NodeType* prev= pos->prev;
  prev->next = node;
  node->prev = prev;
  node->next = pos;
  pos->prev  = node;
  ++size_;
  return ForwardIterator(node,end());
}

template< typename T >
typename List<T>::ForwardIterator List<T>::Insert( Zone* zone , ConstForwardIterator& iter ,
                                                                const T& value ) {
  lava_debug(NORMAL,lava_verify(iter.end_ == end()););
  NodeType* node = zone->New<NodeType>(value);
  return InsertNode(iter,node);
}

template< typename T >
typename List<T>::ForwardIterator List<T>::Remove( ConstForwardIterator& iter ) {
  lava_debug(NORMAL,lava_verify(iter.end_ == end()););
  lava_debug(NORMAL,lava_verify(!empty()););

  NodeType* pos = iter.iter_;
  pos->prev->next = pos->next;
  pos->next->prev = pos->prev;
  --size_;

  // Notes: We use pos node which is deleted here. This is Okay since
  //        zone allocator doesn't really deaclloate memory
  return ForwardIterator(pos->next,end());
}

template< typename T >
void List<T>::Assign( Zone* zone , const List<T>& that ) {
  if(this != &that ) {
    return Assign(zone,GetForwardIterator());
  }
}

template< typename T >
template< typename ITR >
void List<T>::Assign( Zone* zone , const ITR& itr ) {
  Clear();
  lava_foreach( const T& v , itr ) {
    PushBack(zone,v);
  }
}

template< typename T >
void List<T>::Append( Zone* zone , const List<T>& that ) {
  if(this != &that) {
    return Append(zone,that.GetForwardIterator());
  }
}

template< typename T >
template< typename ITR >
void List<T>::Append( Zone* zone , const ITR& itr ) {
  lava_foreach( const T& v , itr ) {
    PushBack(zone,v);
  }
}

template< typename T >
void List<T>::Merge( List* another , ConstForwardIterator pos ) {
  auto p = pos;
  auto itr(another->GetForwardIterator());
  while( itr.HasNext() ) {
    auto node = itr.iter_;
    itr.Move(); // move first since node will be relinked
    p = InsertNode(p,node);
    p.Move();
  }
}

template< typename T >
void List<T>::Resize( Zone* zone , std::size_t size ) {
  if( size_ > size ) {
    for( std::size_t i = 0 ; i < (size_ - size); ++i )
      PopBack();
  } else if(size_ < size) {
    for( std::size_t i = 0 ; i < (size - size_); ++i )
      PushBack(zone,T());
  }
}

template< typename T >
const T& List<T>::Index( std::size_t index ) const {
  lava_debug(NORMAL,lava_verify(index < size_););
  ForwardIterator itr(GetForwardIterator());
  itr.Advance(index);
  return itr.value();
}

template< typename T >
void List<T>::Set( std::size_t index , const T& value ) {
  lava_debug(NORMAL,lava_verify(index < size_););
  ForwardIterator itr(GetForwardIterator());
  itr.Advance(index);
  itr.set_value(index,value);
}

template< typename T >
typename List<T>::ForwardIterator List<T>::FindIf( const std::function<bool (const T&)>& predicate ) {
  return ::lavascript::FindIf(GetForwardIterator(),predicate);
}

template< typename T >
typename List<T>::ConstForwardIterator List<T>::FindIf( const std::function<bool (const T&)>& predicate ) const {
  return ::lavascript::FindIf(GetForwardIterator(),predicate);
}

namespace detail {

template< typename T >
inline bool ListForwardTraits<T>::Move( Node<T>** iter , Node<T>* end ) {
  Node<T>* temp = *iter;
  temp = temp->next;
  *iter = temp;
  return temp != end;
}

template< typename T >
inline bool ListBackwardTraits<T>::Move( Node<T>** iter , Node<T>* end ) {
  Node<T>* temp = *iter;
  temp = temp->prev;
  *iter = temp;
  return temp != end;
}

} // namespaced detail

} // namespace zone
} // namespace lavascript

#endif // ZONE_LIST_H_
