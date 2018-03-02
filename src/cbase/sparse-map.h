#ifndef CBASE_SPARSE_MAP_H_
#define CBASE_SPARSE_MAP_H_

#include "src/trace.h"
#include <vector>
#include <algorithm>

namespace lavascript {
namespace cbase      {

// A sparse map is a map that supports holding sparse element and support
// lookup as well. The sparse here means the number of element inside of
// the array is not too much ; otherwise performance will degrade.
//
// The internal implementation is just use std::vector with linear search.
// If we see performance degradation , one option will be bailout to another
// internal implementation that uses std::map/std::unordered_map.
template< typename K , typename T >
class SparseMap {
 public:
  typedef typename std::vector<T> ContainerType;
  SparseMap( std::size_t reserve ) : vec_(reserve)  {}
  SparseMap( const ContainerType& vec ) : vec_(vec) {}

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
};

template< typename K , typename T >
const T* SparseMap<K,T>::Find( const K& value ) const {
  auto itr = std::find(vec_.begin(),vec_.end(),value);
  return itr == vec_.end() ? NULL : &(itr->val);
}

template< typename K , typename T >
bool SparseMap<K,T>::Remove( const K& value ) const {
  auto itr = std::find(vec_.begin(),vec_.end(),value);
  if(itr != vec_.end()) {
    vec_.erase(itr);
    return true;
  }
  return false;
}

template< typename K , typename T >
bool SparseMap<K,T>::Insert( const K& key , const T& value ) {
  if(Has(key)) return false;
  vec_.push_back(KeyValue(key,value));
  return true;
}

template< typename K , typename T >
bool SparseMap<K,T>::Insert( K&& key , T&& value ) {
  if(Has(key)) return false;
  vec_.push_back(KeyValue(std::move(key),std::move(value)));
  return true;
}

} // namespace cbase
} // namespace lavascript

#endif // CBASE_SPARSE_MAP_H_
