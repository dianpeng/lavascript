#ifndef ZONE_LIST_H_
#define ZONE_LIST_H_
#include "zone.h"

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

template< typename T > class Iterator {
 public:
  Iterator( Node<T>* iter , Node<T>* end ): iter_(iter), end_(end) {}
  Iterator(): iter_(NULL) , end_(NULL) {}

 public:
  bool HasNext() const { return iter_ != end_; }
  bool Next() const;
  bool Advance( std::size_t times ) const;

  T& value() { return iter_->value; }
  const T& value() const { return iter_->value; }
  void set_value( const T& val ) { iter_->value = val; }

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

template< typename T > bool Iterator<T>::Next() const {
  lava_debug(NORMAL,lava_verify(HasNext()););
  iter_ = iter_->next;
  return HasNext();
}

template< typename T > bool Iterator<T>::Advance( std::size_t times ) const {
  for( ; times > 0 ; --times )
    if(!Next()) return false;
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

} // namespace detail

// A linked list implementation as ZoneObject. We mainly need it because
// linked list supports way better method of deletion of a certain node.
template< typename T > class List : ZoneObject {
  typedef detail::Node<T> NodeType;
  typedef detail::NodeBase<NodeType> NodeBaseType;
 public:
  static List* New( Zone* zone ) { return zone->New<List<T>>(); }
  static void CopyFrom( Zone* , List* , const List& );

  List() : end_ () , size_(0) {}

  List( Zone* zone , const List& that ) : end_() , size_(0) {
    CopyFrom(zone,this,that);
  }

 public:
  typedef detail::Iterator<T> Iterator;
  typedef const detail::Iterator<T> ConstIterator;

  Iterator GetIterator() {
    return Iterator(end_.next, end());
  }

  ConstIterator GetIterator() const {
    return ConstIterator(end_.next,end());
  }

 public:
  Iterator PushBack( Zone* , const T& );
  Iterator PopBack ();
  Iterator Insert  ( Zone* , ConstIterator& , const T& );
  Iterator Remove  ( ConstIterator& );

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
  // resize the linked list to hold certain size/amount of nodes
  void Resize( Zone* , std::size_t );

  // index a certain position's node inside of linked list , linear
  // complexity
  const T& Index( std::size_t ) const;

  // index a certain position's node inside of linked list , linear
  // complexity
  T& Index( std::size_t );

 public:
  std::size_t size() const { return size_; }
  bool empty() const { return size() == 0; }

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
typename List<T>::Iterator List<T>::PushBack( Zone* zone , const T& value ) {
  return Insert(zone,ConstIterator(end(),end()),value);
}

template< typename T >
typename List<T>::Iterator List<T>::PopBack() {
  return Remove(ConstIterator(end_.prev,end()));
}

template< typename T >
typename List<T>::Iterator List<T>::Insert( Zone* zone , ConstIterator& iter ,
                                                         const T& value ) {
  lava_debug(NORMAL,lava_verify(iter.end_ == end()););
  NodeType* node = zone->New<NodeType>(value);

  NodeType* pos = iter.iter_;
  NodeType* prev= pos->prev;
  prev->next = node;
  node->prev = prev;
  node->next = pos;
  pos->prev  = node;
  ++size_;
  return Iterator(node,end());
}

template< typename T >
typename List<T>::Iterator List<T>::Remove( ConstIterator& iter ) {
  lava_debug(NORMAL,lava_verify(iter.end_ == end()););
  lava_debug(NORMAL,lava_verify(!empty()););

  NodeType* pos = iter.iter_;
  pos->prev->next = pos->next;
  pos->next->prev = pos->prev;
  --size_;

  // Notes: We use pos node which is deleted here. This is Okay since
  //        zone allocator doesn't really deaclloate memory
  return Iterator(pos->next,end());
}

template< typename T >
void List<T>::CopyFrom( Zone* zone , List<T>* dest , const List<T>& that ) {
  if(dest != &that) {
    dest->Clear();

    ConstIterator itr(that.GetIterator());
    for( ; itr.HasNext(); itr.Next() ) {
      dest->PushBack(zone,itr.value());
    }
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
T& List<T>::Index( std::size_t index ) {
  lava_debug(NORMAL,lava_verify(index < size_););
  Iterator itr(GetIterator());
  itr.Advance(index);
  return itr.value();
}

template< typename T >
const T& List<T>::Index( std::size_t index ) const {
  lava_debug(NORMAL,lava_verify(index < size_););
  Iterator itr(GetIterator());
  itr.Advance(index);
  return itr.value();
}

} // namespace zone
} // namespace lavascript

#endif // ZONE_LIST_H_
