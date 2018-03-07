#ifndef CBASE_SPARSE_MAP_H_
#define CBASE_SPARSE_MAP_H_
#include "src/trace.h"
#include "src/stl-helper.h"

#include <algorithm>
#include <map>
#include <vector>
#include <variant>

namespace lavascript {
namespace cbase      {

template< typename K , typename T > class BalanceTree;

// A linear list is just a vector that implements all the lookup function
// with linear search. It is the most simple implementation but it is good
// for small number of key value pair.
template< typename K , typename T >
class LinearList {
 public:
  typedef std::pair<K,T> ValueType;
  typedef std::vector<ValueType> Container;

  LinearList() : vec_() {}
  explicit LinearList( std::size_t reserve ) : vec_(reserve)  {}

 public:
  std::size_t size () const { return vec_.size();  }
  bool empty() const { return vec_.empty(); }
  const T* Find( const K& value ) const;
  bool Has( const K& value ) const { return Find(value) != NULL; }
  bool Remove( const K& value );
  bool Insert( const K& key , const T& value );
  bool Insert( K&& key      , T&& value      );

  void Clear() { vec_.clear(); }
  void Swap( LinearList* that ) { that->vec_.swap(vec_); }

 public:
  typedef STLForwardIteratorAdapter<Container>  ForwardIterator;
  typedef STLBackwardIteratorAdapter<Container> BackwardIterator;

  typedef STLConstForwardIteratorAdapter<Container>  ConstForwardIterator;
  typedef STLConstBackwardIteratorAdapter<Container> ConstBackwardIterator;

  ForwardIterator GetForwardIterator()
  { return ForwardIterator(vec_.begin(),vec_.end()); }

  ConstForwardIterator GetForwardIterator() const
  { return ConstForwardIterator(vec_.begin(),vec_.end()); }

  BackwardIterator GetBackwardIterator()
  { return BackwardIterator(vec_.rbegin(),vec_.rend()); }

  ConstBackwardIterator GetBackwardIterator() const
  { return ConstBackwardIterator(vec_.rbegin(),vec_.rend()); }

 private:
  Container vec_;
  friend class BalanceTree<K,T>;
};

// A balance tree is just a std::map wrapper to provide certain functions
template< typename K , typename T >
class BalanceTree {
 public:
  typedef std::map<K,T> Container;
  typedef typename Container::value_type ValueType;

  BalanceTree() : map_() {}

  // Helper function to migrate from old LinearList to BalanceTree
  void Insert( LinearList<K,T>&& ll );

  std::size_t size() const { return map_.size() ; }
  bool empty()const { return map_.empty(); }

  const T* Find( const K& value ) const;
  bool     Has ( const K& value ) const { return Find(value) != NULL; }
  bool     Remove( const K& value );
  bool     Insert( const K& , const T& );
  bool     Insert( K&&, T&& );

  void     Clear() { map_.clear(); }
  void     Swap( BalanceTree* bt ) { bt->map_.swap(map_); }

 public:
  typedef STLForwardIteratorAdapter<Container>  ForwardIterator;
  typedef STLBackwardIteratorAdapter<Container> BackwardIterator;

  typedef STLConstForwardIteratorAdapter<Container>  ConstForwardIterator;
  typedef STLConstBackwardIteratorAdapter<Container> ConstBackwardIterator;

  ForwardIterator GetForwardIterator()
  { return ForwardIterator(map_.begin(),map_.end()); }

  ConstForwardIterator GetForwardIterator() const
  { return ConstForwardIterator(map_.begin(),map_.end()); }

  BackwardIterator GetBackwardIterator()
  { return BackwardIterator(map_.rbegin(),map_.rend()); }

  ConstBackwardIterator GetBackwardIterator() const
  { return ConstBackwardIterator(map_.rbegin(),map_.rend()); }

 private:
  Container map_;
};

namespace detail {

template< typename I1, typename I2 , typename KeyType ,
                                     typename ValueType >
class SparseMapIterator {
 public:
  typedef std::pair<KeyType,ValueType> KeyValueType;

  explicit SparseMapIterator( const I1& i1 ) : iter_(i1) {}
  explicit SparseMapIterator( const I2& i2 ) : iter_(i2) {}

  bool IsC1() const { return iter_.index() == 0; }
  bool IsC2() const { return iter_.index() == 1; }

  bool HasNext() const;
  bool Move   () const;

  // This interface is different than most of the iterator due to the
  // fact that we cannot store std::pair<const T,V> into std::vector
  // since it doesn't support copy assignable trait
  const KeyType&   key  () const;
  const ValueType& value() const;

 private:
  mutable std::variant<I1,I2> iter_;
};

template< typename I1 , typename I2 , typename KT , typename VT >
bool SparseMapIterator<I1,I2,KT,VT>::HasNext() const {
  if(IsC1()) {
    auto &i1 = std::get<I1>(iter_);
    return i1.HasNext();
  } else {
    auto &i2 = std::get<I2>(iter_);
    return i2.HasNext();
  }
}

template< typename I1 , typename I2 , typename KT , typename VT >
bool SparseMapIterator<I1,I2,KT,VT>::Move() const {
  if(IsC1()) {
    auto &i1 = std::get<I1>(iter_);
    return i1.Move();
  } else {
    auto &i2 = std::get<I2>(iter_);
    return i2.Move();
  }
}

template< typename I1 , typename I2 , typename KT , typename VT >
const KT& SparseMapIterator<I1,I2,KT,VT>::key() const {
  if(IsC1()) {
    auto &i1 = std::get<I1>(iter_);
    return i1.value().first;
  } else {
    auto &i2 = std::get<I2>(iter_);
    return i2.value().first;
  }
}

template< typename I1 , typename I2 , typename KT , typename VT >
const VT& SparseMapIterator<I1,I2,KT,VT>::value() const {
  if(IsC1()) {
    auto &i1 = std::get<I1>(iter_);
    return i1.value().second;
  } else {
    auto &i2 = std::get<I2>(iter_);
    return i2.value().second;
  }
}

} // namespace detail

// A sparse map impelementation. It uses C1 container , linear list , at
// first when small amount of data is inserted. Once the data inserted
// bypass the specified threshold, it will start to use C2 container which is
// better than C1 for large amount of inserted element.
// The C1 is default to LinearList, and the C2 is default to BalanceTree
template< typename K , typename T >
class SparseMap {
 public:
  static const std::size_t kDefaultThreshold = 16;

  typedef LinearList<K,T>  C1;
  typedef BalanceTree<K,T> C2;

  explicit SparseMap( std::size_t threshold = kDefaultThreshold ) :
    map_() , threshold_(threshold) {}

  std::size_t size  () const;
  std::size_t threshold() const { return threshold_; }

  bool     empty () const { return size() == 0; }
  void     Clear ();
  const T* Find  ( const K& value ) const;
  bool     Has   ( const K& value ) const { return Find(value) != NULL; }
  bool     Remove( const K& value );
  bool     Insert( const K& , const T& );
  bool     Insert( K&& , T&& );

  bool IsC1() const { return map_.index() == 0; }
  bool IsC2() const { return map_.index() == 1; }

  C1* c1() const { lava_debug(NORMAL,lava_verify(IsC1());); return &std::get<C1>(map_); }
  C2* c2() const { lava_debug(NORMAL,lava_verify(IsC2());); return &std::get<C2>(map_); }

 public:

  typedef detail::SparseMapIterator<typename C1::ForwardIterator,
                                    typename C2::ForwardIterator,
                                    K, T> ForwardIterator;

  typedef detail::SparseMapIterator<typename C1::ConstForwardIterator,
                                    typename C2::ConstForwardIterator,
                                    K, T> ConstForwardIterator;

  typedef detail::SparseMapIterator<typename C1::BackwardIterator,
                                    typename C2::BackwardIterator,
                                    K, T> BackwardIterator;

  typedef detail::SparseMapIterator<typename C1::ConstBackwardIterator,
                                    typename C2::ConstBackwardIterator,
                                    K, T> ConstBackwardIterator;

  ForwardIterator GetForwardIterator();
  ConstForwardIterator GetForwardIterator() const;

  BackwardIterator GetBackwardIterator();
  ConstBackwardIterator GetBackwardIterator() const;

 private:
  bool Upgrade();

  std::variant<C1,C2> map_;
  std::size_t threshold_;
};

template< typename K , typename T >
const T* LinearList<K,T>::Find( const K& value ) const {
  auto itr = std::find(vec_.begin(),vec_.end(),value);
  return itr == vec_.end() ? NULL : &(itr->second);
}

template< typename K , typename T >
bool LinearList<K,T>::Remove( const K& value ) {
  auto itr = std::find(vec_.begin(),vec_.end(),value);
  if(itr != vec_.end()) {
    vec_.erase(itr);
    return true;
  }
  return false;
}

template< typename K , typename T >
typename SparseMap<K,T>::ForwardIterator
SparseMap<K,T>::GetForwardIterator() {
  if(IsC1())
    return ForwardIterator( std::get<C1>(map_).GetForwardIterator() );
  else
    return ForwardIterator( std::get<C2>(map_).GetForwardIterator() );
}

template< typename K , typename T >
typename SparseMap<K,T>::ConstForwardIterator
SparseMap<K,T>::GetForwardIterator() const {
  if(IsC1())
    return ConstForwardIterator( std::get<C1>(map_).GetForwardIterator() );
  else
    return ConstForwardIterator( std::get<C2>(map_).GetForwardIterator() );
}

template< typename K , typename T >
typename SparseMap<K,T>::BackwardIterator
SparseMap<K,T>::GetBackwardIterator() {
  if(IsC1())
    return BackwardIterator( std::get<C1>(map_).GetBackwardIterator() );
  else
    return BackwardIterator( std::get<C2>(map_).GetBackwardIterator() );
}

template< typename K , typename T >
typename SparseMap<K,T>::ConstBackwardIterator
SparseMap<K,T>::GetBackwardIterator() const {
  if(IsC1())
    return ConstBackwardIterator( std::get<C1>(map_).GetBackwardIterator() );
  else
    return ConstBackwardIterator( std::get<C2>(map_).GetBackwardIterator() );
}

template< typename K , typename T >
bool LinearList<K,T>::Insert( const K& key , const T& value ) {
  if(Has(key)) return false;
  vec_.push_back(std::make_pair(key,value));
  return true;
}

template< typename K , typename T >
bool LinearList<K,T>::Insert( K&& key , T&& value ) {
  if(Has(key)) return false;
  vec_.push_back(std::make_pair(std::move(key),std::move(value)));
  return true;
}

template< typename K, typename T >
void BalanceTree<K,T>::Insert( LinearList<K,T>&& ll ) {
  for( auto &e : ll.vec_ ) {
    map_.insert(std::make_pair(std::move(e.first),std::move(e.second)));
  }
}

template< typename K, typename T >
const T* BalanceTree<K,T>::Find( const K& key ) const {
  auto itr = map_.find(key);
  return itr == map_.end() ? NULL : &(itr->second);
}

template< typename K , typename T >
bool BalanceTree<K,T>::Remove( const K& value ) {
  auto itr = map_.find(value);
  if(itr != map_.end()) {
    map_.erase(itr);
    return true;
  }
  return false;
}

template< typename K , typename T >
bool BalanceTree<K,T>::Insert( const K& key , const T& value ) {
  if(Has(key)) return false;
  map_.insert(std::make_pair(key,value));
  return true;
}

template< typename K , typename T >
bool BalanceTree<K,T>::Insert( K&& key , T&& value ) {
  if(Has(key)) return false;
  map_.insert(std::make_pair(std::move(key),std::move(value)));
  return true;
}

template< typename K , typename T >
std::size_t SparseMap<K,T>::size() const {
  if(map_.index() == 0) {
    auto &c1 = std::get<C1>(map_);
    return c1.size();
  } else {
    auto &c2 = std::get<C2>(map_);
    return c2.size();
  }
}

template< typename K , typename T >
const T* SparseMap<K,T>::Find( const K& value ) const {
  if(map_.index() == 0)
    return std::get<C1>(map_).Find(value);
  else
    return std::get<C2>(map_).Find(value);
}

template< typename K , typename T >
bool SparseMap<K,T>::Remove( const K& value ) {
  if(map_.index() == 0) {
    return std::get<C1>(map_).Remove(value);
  } else {
    return std::get<C2>(map_).Remove(value);
  }
}

template< typename K , typename T >
bool SparseMap<K,T>::Upgrade() {
  if(map_.index() == 0) {
    auto &c1 = std::get<C1>(map_);
    if(c1.size() == threshold_) {
      C1 old; old.Swap(&c1);
      map_ = C2();
      auto &c2 = std::get<C2>(map_);
      c2.Insert(std::move(old));
      return true;
    }
  }
  return false;
}

template< typename K , typename T >
bool SparseMap<K,T>::Insert( const K& key , const T& value ) {
  if(!Upgrade()) {
    return std::get<C1>(map_).Insert(key,value);
  }
  return std::get<C2>(map_).Insert(key,value);
}

template< typename K , typename T >
bool SparseMap<K,T>::Insert( K&& key , T&& value ) {
  if(!Upgrade()) {
    return std::get<C1>(map_).Insert(std::move(key),std::move(value));
  }
  return std::get<C2>(map_).Insert(std::move(key),std::move(value));
}

template< typename K , typename T >
void SparseMap<K,T>::Clear() {
  if(map_.index() == 0) {
    std::get<C1>(map_).Clear();
  } else {
    map_ = C1(); // the old value gone
  }
}

} // namespace cbase
} // namespace lavascript

#endif // CBASE_SPARSE_MAP_H_
