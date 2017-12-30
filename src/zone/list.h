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

template< typename T , typename Traits > class Iterator {
 public:
  Iterator( Node<T>* iter , Node<T>* end ): iter_(iter), end_(end) {}
  Iterator(): iter_(NULL) , end_(NULL) {}

 public:
  bool HasNext() const { return iter_ != end_; }
  bool Move() const    { return Traits::Move(&iter_,end_); }

  const T& value() const { lava_debug(NORMAL,lava_verify(HasNext());); return iter_->value; }
  void set_value( const T& val ) { lava_debug(NORMAL,lava_verify(HasNext());); iter_->value = val;  }

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
  static void CopyFrom( Zone* , List* , const List& );

  List() : end_ () , size_(0) {}

  List( Zone* zone , const List& that ) : end_() , size_(0) {
    CopyFrom(zone,this,that);
  }

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
typename List<T>::ForwardIterator List<T>::PushBack( Zone* zone , const T& value ) {
  return Insert(zone,ConstForwardIterator(end(),end()),value);
}

template< typename T >
typename List<T>::ForwardIterator List<T>::PopBack() {
  return Remove(ConstForwardIterator(end_.prev,end()));
}

template< typename T >
typename List<T>::ForwardIterator List<T>::Insert( Zone* zone , ConstForwardIterator& iter ,
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
  return ForwardIterator(node,end());
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
void List<T>::CopyFrom( Zone* zone , List<T>* dest , const List<T>& that ) {
  if(dest != &that) {
    dest->Clear();
    ConstForwardIterator itr(that.GetIterator());
    for( ; itr.HasNext(); itr.Move() ) {
      dest->PushBack(zone,itr.value());
    }
  }
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
