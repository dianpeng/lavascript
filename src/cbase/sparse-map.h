#ifndef CBASE_SPARSE_MAP_H_
#define CBASE_SPARSE_MAP_H_

#include "src/trace.h"

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
  LinearList() : vec_() {}
  LinearList( std::size_t reserve ) : vec_(reserve)  {}
  LinearList( const ContainerType& vec ) : vec_(vec) {}

 public:
  bool size () const { return vec_.size();  }
  bool empty() const { return vec_.empty(); }
  const T* Find( const K& value ) const;
  bool Has( const K& value ) const { return Find(value) != NULL; }
  bool Remove( const K& value );
  bool Insert( const K& key , const T& value );
  bool Insert( K&& key      , T&& value      );

  void Clear() { vec_.clear(); }
 private:
  struct KeyValue {
    K key;
    T val;

    KeyValue() : key(), val() {}
    KeyValue( const K& k , const T& v ) : key(k) , val(v) {}
    KeyValue( K&& k , T&& v ) : key(std::move(k)), val(std::move(v)) {}
    KeyValue( const KeyValue& kv ) : key(kv.key) , val(kv.val) {}
    KeyValue( KeyValue&& kv      ) : key(std::move(kv.key)) , val(std::move(kv.val)) {}

    bool operator < ( const KeyValue& kv ) const { return key < kv.key; }
  };

  std::vector<KeyValue> vec_;

  friend class BalanceTree<K,T>;
};

// A balance tree is just a std::map wrapper to provide certain functions
template< typename K , typename T >
class BalanceTree {
 public:
  BalanceTree() : map_() {}

  // Helper function to migrate from old LinearList to BalanceTree
  void Insert( LinearList<K,T>&& ll );

  bool size() const { return map_.size() ; }
  bool empty()const { return map_.empty(); }

  const T* Find( const K& value ) const;
  bool     Has ( const K& value ) const { return Find(value) != NULL; }
  bool     Remove( const K& value );
  bool     Insert( const K& , const T& );
  bool     Insert( K&&, T&& );

  void     Clear() { map_.clear(); }

 private:
  ContainerType map_;
};

// A sparse map impelementation. It uses C1 container , linear list , at
// first when small amount of data is inserted. Once the data inserted
// bypass the specified threshold, it will start to use C2 container which is
// better than C1 for large amount of inserted element.
// The C1 is default to LinearList, and the C2 is default to BalanceTree
template< typename K , typename T >
class SparseMap {
 public:
  typedef LinearList<K,T>  C1;
  typedef BalanceTree<K,T> C2;

  SparseMap( std::size_t threshold ) : map_() , threshold_(threshold_) {}

  bool     size  () const;
  bool     empty () const { return size() == 0; }
  void     Clear ();
  const T* Find  ( const K& value ) const;
  bool     Has   ( const K& value ) const { return Find(value) != NULL; }
  bool     Remove( const K& value );
  bool     Insert( const K& , const T& );
  bool     Insert( K&& , T&& );

 private:
  bool Upgrade();

 private:
  std::variant<C1,C2> map_;
  std::size_t threshold_;
};

template< typename K , typename T >
const T* LinearList<K,T>::Find( const K& value ) const {
  auto itr = std::find(vec_.begin(),vec_.end(),value);
  return itr == vec_.end() ? NULL : &(itr->val);
}

template< typename K , typename T >
bool LinearList<K,T>::Remove( const K& value ) const {
  auto itr = std::find(vec_.begin(),vec_.end(),value);
  if(itr != vec_.end()) {
    vec_.erase(itr);
    return true;
  }
  return false;
}

template< typename K , typename T >
bool LinearList<K,T>::Insert( const K& key , const T& value ) {
  if(Has(key)) return false;
  vec_.push_back(KeyValue(key,value));
  return true;
}

template< typename K , typename T >
bool LinearList<K,T>::Insert( K&& key , T&& value ) {
  if(Has(key)) return false;
  vec_.push_back(KeyValue(std::move(key),std::move(value)));
  return true;
}

template< typename K, typename T >
void BalanceTree<K,T>::Insert( LinearList<K,T>&& ll ) {
  for( auto &e : ll.vec_ ) {
    map_.insert(std::make_pair(std::move(e.key),std::move(e.val)));
  }
}

template< typename K, typename T >
const T* BalanceTree<K,T>::Find( const K& key ) {
  auto itr = map_.find(key);
  return itr == map_.end() ? NULL : &(itr->second);
}

template< typename K , typename T >
bool BalanceTree::Remove( const K& value ) {
  auto itr = map_.find(value);
  if(itr != map_.end()) {
    map_.erase(itr);
    return true;
  }
  return false;
}

template< typename K , typename T >
bool BalanceTree::Insert( const K& key , const T& value ) {
  if(Has(key)) return false;
  map_.insert(std::make_pair(key,value));
  return true;
}

template< typename K , typename T >
bool BalanceTree::Insert( K&& key , T&& value ) {
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
  bool ret;
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
      auto old = std::move(c1);
      map_ = C2();
      auto &c2 = std::get<C2>(map_);
      c2.Insert(std::move(c1));
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

} // namespace cbase
} // namespace lavascript

#endif // CBASE_SPARSE_MAP_H_
