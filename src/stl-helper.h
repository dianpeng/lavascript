#ifndef STL_HELPER_H_
#define STL_HELPER_H_
#include <vector>
#include <algorithm>

namespace std {

template< typename K , typename T >
bool operator == ( const pair<K,T>& p , const K& k ) {
  return p.first == k;
}

template< typename K , typename T >
bool operator <  ( const pair<K,T>& p , const K& k ) {
  return p.first < k;
}

template< typename K , typename T >
bool operator == ( const pair<const K,T>& p , const K& k ) {
  return p.first == k;
}

template< typename K , typename T >
bool operator <  ( const pair<const K,T>& p , const K& k ) {
  return p.first < k;
}

} // namespace std

namespace lavascript {

// Use DynamicBitSet to represent the dynamic bitset and underly implementation should
// use real bit set to make memory efficient.
typedef std::vector<bool> DynamicBitSet;

void BitSetReset( DynamicBitSet* set , bool value = false );

template< typename T >
typename T::iterator IteratorAt( T& container , std::size_t pos ) {
  typename T::iterator p(container.begin());
  std::advance(p,pos);
  return p;
}

template< typename T >
typename T::const_iterator IteratorAt( const T& container , std::size_t pos ) {
  typename T::cosnt_iterator p(container.begin());
  std::advance(p,pos);
  return p;
}

template< typename C , typename ITER > class STLConstIteratorAdapter {
 public:
  typedef typename C::value_type ValueType;
  typedef ITER IterType;

  STLConstIteratorAdapter( const IterType& start , const IterType& end ):
    start_(start),
    end_  (end)
  {}

  bool HasNext() const { return start_ != end_;      }
  bool Move   () const { ++start_; return HasNext(); }
  const ValueType& value() const { return *start_; }
  void Advance( std::size_t offset ) { std::advance(start_,offset); }

 protected:
  mutable IterType start_, end_;
};

template< typename C , typename ITER >
class STLIteratorAdapter : public STLConstIteratorAdapter<C,ITER> {
 public:
  typedef STLConstIteratorAdapter<C,ITER> Base;
  typedef typename Base::ValueType ValueType;
  typedef typename Base::IterType IterType;

  STLIteratorAdapter( const IterType& start ,  const IterType& end ):
    Base(start,end)
  {}

  void set_value( const ValueType& value ) { *Base::start_ = value; }
};

template< typename C > class STLForwardIteratorAdapter :
  public STLIteratorAdapter<C,typename C::iterator> {
 public:
  typedef typename C::iterator IterType;
  typedef STLIteratorAdapter<C,typename C::iterator> Base;

  STLForwardIteratorAdapter( const IterType& start , const IterType& end ):
    Base(start,end) {}
};

template< typename C > class STLBackwardIteratorAdapter :
  public STLIteratorAdapter<C,typename C::reverse_iterator> {
 public:
  typedef typename C::reverse_iterator IterType;
  typedef STLIteratorAdapter<C,typename C::reverse_iterator> Base;

  STLBackwardIteratorAdapter( const IterType& start , const IterType& end ):
    Base(start,end) {}
};

template< typename C > class STLConstForwardIteratorAdapter :
  public STLConstIteratorAdapter<C,typename C::const_iterator> {
 public:
  typedef typename C::const_iterator IterType;
  typedef STLConstIteratorAdapter<C,typename C::const_iterator> Base;

  STLConstForwardIteratorAdapter( const IterType& start , const IterType& end ):
    Base(start,end) {}
};

template< typename C > class STLConstBackwardIteratorAdapter :
  public STLConstIteratorAdapter<C,typename C::const_reverse_iterator> {
 public:
  typedef typename C::const_reverse_iterator IterType;
  typedef STLConstIteratorAdapter<C,typename C::const_reverse_iterator> Base;

  STLConstBackwardIteratorAdapter( const IterType& start , const IterType& end ):
    Base(start,end) {}
};

} // namespace lavascript

#endif // STL_HELPER_H_
