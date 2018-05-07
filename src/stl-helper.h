#ifndef STL_HELPER_H_
#define STL_HELPER_H_
#include <vector>
#include <variant>
#include <typeinfo>
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

template< typename T >
void BitSetReset( T* set , bool value = false ) {
  for( std::size_t i = 0 ; i < set->size() ; ++i ) {
    set->at(i) = value;
  }
}

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

// A function that does update semantic for STL map like container.
// It tries to stay effcient and also doesn't require the value to
// have a default constructor which is different from the default
// operator [] provided by STL map container
template< typename T , typename K , typename V >
bool STLUpdateMap( T* map , const K& key , const V& v ) {
  auto itr = map->insert(std::make_pair(key,v));
  if(itr.second) {
    itr.first->second = v;
  }
  return itr.second;
}

template< typename T , typename K , typename V >
bool STLUpdateMap( T* map , K&& key , V&& v ) {
  auto itr = map->find(key);
  if(itr == map->end()) {
    map->insert(std::make_pair(std::move(key),std::move(v)));
    return true;
  }
  itr->second = std::move(v);
  return false;
}

// A wrapper around std::variant to avoid heap allocation
template< typename ... ARGS > class EmbedStorage {
 public:
  typedef std::variant<ARGS...> StorageType;

  template< typename T > T* Get();
  template< typename T > T* Set();

  int Index() const { return store_.index(); }

 private:
  StorageType store_;
};

template< typename ... ARGS >
template< typename T >
T* EmbedStorage<ARGS...>::Get() {
  try {
    T& val = std::get<T>(store_);
    return &val;
  } catch(...) {
    lava_assertF("unexpected get from variant with index %d and expected type %s",
        Index(),typeid(T).name());
    return NULL;
  }
}

template< typename ... ARGS >
template< typename T >
T* EmbedStorage<ARGS...>::Set() {
  store_ = T();
  return Get();
}

} // namespace lavascript

#endif // STL_HELPER_H_
