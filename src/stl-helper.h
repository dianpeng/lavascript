#ifndef STL_HELPER_H_
#define STL_HELPER_H_
#include <vector>
#include <algorithm>

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
  typedef typename C::value_type ValueType;
  typedef ITER IterType;
 public:
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
  typedef STLConstIteratorAdapter<C,ITER> Base;
  typedef typename Base::ValueType ValueType;
  typedef typename Base::IterType IterType;
 public:
  STLIteratorAdapter( const IterType& start ,  const IterType& end ):
    Base(start,end)
  {}

  void set_value( const ValueType& value ) { *Base::start_ = value; }
};

template< typename C > class STLForwardIteratorAdapter :
  public STLIteratorAdapter<C,typename C::iterator> {
  typedef typename C::iterator IterType;
  typedef STLIteratorAdapter<C,typename C::iterator> Base;
 public:
  STLForwardIteratorAdapter( const IterType& start , const IterType& end ):
    Base(start,end) {}
};

template< typename C > class STLBackwardIteratorAdapter :
  public STLIteratorAdapter<C,typename C::reverse_iterator> {
  typedef typename C::reverse_iterator IterType;
  typedef STLIteratorAdapter<C,typename C::reverse_iterator> Base;
 public:
  STLBackwardIteratorAdapter( const IterType& start , const IterType& end ):
    Base(start,end) {}
};

template< typename C > class STLConstForwardIteratorAdapter :
  public STLConstIteratorAdapter<C,typename C::const_iterator> {
  typedef typename C::const_iterator IterType;
  typedef STLConstIteratorAdapter<C,typename C::const_iterator> Base;
 public:
  STLConstForwardIteratorAdapter( const IterType& start , const IterType& end ):
    Base(start,end) {}
};

template< typename C > class STLConstBackwardIteratorAdapter :
  public STLConstIteratorAdapter<C,typename C::const_reverse_iterator> {
  typedef typename C::const_reverse_iterator IterType;
  typedef STLConstIteratorAdapter<C,typename C::const_reverse_iterator> Base;
 public:
  STLConstBackwardIteratorAdapter( const IterType& start , const IterType& end ):
    Base(start,end) {}
};

} // namespace lavascript

#endif // STL_HELPER_H_
