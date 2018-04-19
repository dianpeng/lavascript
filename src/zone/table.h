#ifndef ZONE_TABLE_H_
#define ZONE_TABLE_H_
#include "zone.h"
#include "string.h"

#include "src/util.h"
#include "src/common.h"
#include "src/hash.h"
#include "src/bits.h"

#include <type_traits>
#include <algorithm>
#include <utility>

namespace lavascript {
namespace zone       {
namespace detail {

template< typename T > class TableIterator {
 public:
  typedef typename T::KeyType   KeyType;
  typedef typename T::ValueType ValueType;
  typedef            ValueType& ReferenceType;
  typedef      const ValueType& ConstReferenceType;
  friend T;
 public:
  TableIterator( T* table , std::size_t cursor = 0 );
  bool HasNext() const;
  bool Move   () const;
  const KeyType&     key  () const;
  ConstReferenceType value() const;
  void  set_value( ConstReferenceType );
  void  set_value( ValueType&& );

  bool operator == ( const TableIterator& that ) const {
    return table_ == that.table_ && cursor_ == that.cursor_;
  }
  bool operator != ( const TableIterator& that ) const {
    return !(*this == that);
  }
 private:
  mutable T*          table_;
  mutable std::size_t cursor_;
};

// =====================================================================
//
// Default builtin traits
//
// =====================================================================
template < typename T > struct DefaultTrait {
  static std::uint32_t Hash( const T& val ) {
    static_assert( std::is_integral<T>::value );
    static const std::uint32_t kMagic = 2654435761;
    std::uint32_t v = static_cast<std::uint32_t>(val);
    return v * kMagic;
  }

  static bool Equal( const T& lhs , const T& rhs ) {
    return lhs == rhs;
  }
};

template<> struct DefaultTrait<String*> {
  static std::uint32_t Hash( const String* str ) {
    return Hasher::Hash(str->data(),str->size());
  }

  static bool Equal( const String* left , const String* right ) {
    return *left == *right;
  }
};

template<> struct DefaultTrait<Str> {
  static std::uint32_t Hash( const Str& str ) {
    return Hasher::Hash(str.data,str.length);
  }
  static bool Eqaul( const Str& left , const Str& right ) {
    return Str::Cmp(left,right) == 0;
  }
};

template< typename T > struct DefaultTrait<T*> {
  static std::uint32_t Hash( const void* ptr ) {
    static const std::uint64_t kMagic = 2654435761U;
    std::uint64_t pval = reinterpret_cast<std::uint64_t>(ptr);
    auto res = pval * kMagic;
    return static_cast<std::uint32_t>(res);
  }

  static bool Equal( const void* lhs , const void* rhs ) {
    return lhs == rhs;
  }
};

} // namespace detail


// A table is a hash map that is used to help use to do lookup with zone allocator.
// The key can be anything that supports hash function and equal comparison. The
// most obvious use case is via String object in zone.
//
// Internally it uses open addressing + linear probing method to resolve chaining
// resolution which essentially means it is memory efficient.

template< typename K , typename V , typename Trait = detail::DefaultTrait<K> >
class Table {
 public:
  static const std::size_t kDefaultCap = 4;

  LAVASCRIPT_ZONE_CHECK_TYPE(K);
  LAVASCRIPT_ZONE_CHECK_TYPE(V);

  typedef K KeyType;
  typedef V ValueType;
  typedef Table<K,V,Trait> Self;

  // we will just do a memset(0) to the entry and treat it as initialized
  struct Entry {
    K key;
    V val;
    std::uint32_t hash;
    Entry*        next;
    bool          del;
    bool          use;

    bool IsUse()   const { return use && !del;}
    bool IsDel()   const { return use && del; }
    bool IsEmpty() const { return !use; }
  };

  explicit Table( Zone* , std::size_t cap = kDefaultCap );
  Table( Zone* , const Table& );

  typedef detail::TableIterator<Self> Iterator;
  typedef const Iterator ConstIterator;

 public:
  Iterator      GetIterator()       { return Iterator     (this); }
  ConstIterator GetIterator() const { return ConstIterator(const_cast<Table*>(this)); }

  Iterator      Find  ( const K& );
  ConstIterator Find  ( const K& ) const;

  bool Has   ( const K& ) const;
  bool Remove( const K& );
  bool Remove( const ConstIterator& itr );

  std::pair<Iterator,bool> Insert( Zone* , const K& , const V& );
  std::pair<Iterator,bool> Insert( Zone* , K&& , V&& );
  Iterator Update( Zone* , const K& , const V& );
  Iterator Update( Zone* , K&& , V&& );

  void Copy  ( Zone* , Table* ) const;
  void Swap  ( Table* );
  void Clear ();

 public:
  std::size_t capacity () const { return cap_; }
  std::size_t size     () const { return size_; }
  std::size_t slot_size() const { return slot_size_; }

  bool empty() const { return size_ == 0; }

 private:
  enum { INSERT , LOOKUP };

  // Find a slot inside of the table based on the input hash and
  // also option specified
  Entry* FindSlot( const K& , std::uint32_t , int );

  // Trigger a rehash operation, basically just expand the current
  // table 2 times more
  void Rehash    ( Zone* );

 private:
  Entry*      entry_;
  std::size_t cap_;
  std::size_t size_;
  std::size_t slot_size_;

  void operator = ( const Table& ) = delete; // cannot do assignment since we need zone

  friend class detail::TableIterator<Self>;
};

namespace detail {

template< typename T >
bool TableIterator<T>::HasNext() const {
  return cursor_ < table_->capacity();
}

template< typename T >
TableIterator<T>::TableIterator( T* table , std::size_t cursor ):
  table_ (table),
  cursor_(cursor)
{
  for( ; cursor_ < table_->capacity() ; ++cursor_ ) {
    auto e = table_->entry_ + cursor_;
    if(e->IsUse()) break;
  }
}

template< typename T >
bool TableIterator<T>::Move() const {
  for( ++cursor_ ; cursor_ < table_->capacity() ; ++cursor_ ) {
    auto e = table_->entry_ + cursor_;
    if(e->IsUse()) break;
  }
  return false;
}

template< typename T >
const typename TableIterator<T>::KeyType& TableIterator<T>::key() const {
  lava_debug(NORMAL,lava_verify(HasNext()););
  auto e = table_->entry_ + cursor_;
  lava_debug(NORMAL,lava_verify(e->IsUse()););
  return e->key;
}

template< typename T >
const typename TableIterator<T>::ValueType& TableIterator<T>::value() const {
  lava_debug(NORMAL,lava_verify(HasNext()););
  auto e = table_->entry_ + cursor_;
  lava_debug(NORMAL,lava_verify(e->IsUse()););
  return e->val;
}

template< typename T >
void TableIterator<T>::set_value( ConstReferenceType v ) {
  lava_debug(NORMAL,lava_verify(HasNext()););
  auto e = table_->entry_ + cursor_;
  lava_debug(NORMAL,lava_verify(e->IsUse()););
  e->val = v;
}

template< typename T >
void TableIterator<T>::set_value( ValueType&& v ) {
  lava_debug(NORMAL,lava_verify(HasNext()););
  auto e = table_->entry_ + cursor_;
  lava_debug(NORMAL,lava_verify(e->IsUse()););
  e->val = std::move(v);
}

} // namespace detail

template< typename K , typename V , typename Trait >
Table<K,V,Trait>::Table( Zone* zone , std::size_t cap ):
  entry_(NULL),
  cap_  (0),
  size_ (0),
  slot_size_(0)
{
  cap = (!cap) ? 2 : bits::NextPowerOf2(cap);
  entry_ = static_cast<Entry*>(zone->Malloc(sizeof(Entry) * cap));
  ZeroOut(entry_,cap);
  cap_ = cap;
}

template< typename K , typename V , typename Trait >
Table<K,V,Trait>::Table( Zone* zone , const Table& table ):
  entry_(NULL),
  cap_  (bits::NextPowerOf2(table.size_)),
  size_ (0),
  slot_size_(0)
{
  entry_ = static_cast<Entry*>(zone->Malloc(sizeof(Entry) * cap_));
  ZeroOut(entry_,cap_);
  for( auto itr(table.GetIterator()) ; itr.HasNext() ; itr.Move() )
    Insert(zone,itr.key(),itr.value());
}

template< typename K , typename V , typename Trait >
typename Table<K,V,Trait>::ConstIterator
Table<K,V,Trait>::Find( const K& key ) const {
  auto e = FindSlot(key,Trait::Hash(key),LOOKUP);
  if(e) {
    return ConstIterator(this,(e-entry_));
  }
  return ConstIterator(this,capacity());
}

template< typename K , typename V , typename Trait >
typename Table<K,V,Trait>::Iterator
Table<K,V,Trait>::Find( const K& key ) {
  auto e = FindSlot(key,Trait::Hash(key),LOOKUP);
  if(e) {
    return Iterator(this,(e-entry_));
  }
  return Iterator(this,capacity());
}

template< typename K , typename V , typename Trait >
bool Table<K,V,Trait>::Has( const K& key ) const {
  return FindSlot(key,Trait::Hash(key),LOOKUP) != NULL;
}

template< typename K , typename V , typename Trait >
bool Table<K,V,Trait>::Remove( const K& key ) {
  auto e = FindSlot(key,Trait::Hash(key),LOOKUP);
  if(e) {
    e->del = true;
    --size_;

    if(size_ == 0) {
      slot_size_ = 0;
      ZeroOut(entry_,cap_); // zero out the entry
    }

    return true;
  }
  return false;
}

template< typename K , typename V , typename Trait >
bool Table<K,V,Trait>::Remove( const ConstIterator& itr ) {
  lava_debug(NORMAL,lava_verify(itr.table_ == this););

  if(itr.Has()) {
    auto e = entry_ + itr.cursor_;
    e->del = true;
    return true;
  }
  return false;
}

template< typename K , typename V , typename Trait >
std::pair<typename Table<K,V,Trait>::Iterator,bool>
Table<K,V,Trait>::Insert( Zone* zone , const K& k , const V& v ) {
  if(cap_ == slot_size_) Rehash(zone);
  auto e = FindSlot(k,Trait::Hash(k),INSERT);
  lava_verify(e);

  if(e->IsEmpty()) {
    ConstructFromBuffer<K>(&(e->key),k);
    ConstructFromBuffer<V>(&(e->val),v);
    e->use = true;
    e->del = false;
    ++size_;
    return std::make_pair(Iterator(this,(e-entry_)),true);
  }
  return std::make_pair(Iterator(this,(e-entry_)),false);
}

template< typename K , typename V , typename Trait >
std::pair<typename Table<K,V,Trait>::Iterator,bool>
Table<K,V,Trait>::Insert( Zone* zone , K&& k , V&& v ) {
  if(cap_ == slot_size_) Rehash(zone);
  auto e = FindSlot(k,Trait::Hash(k),INSERT);
  lava_verify(e);

  if(e->IsEmpty()) {
    ConstructFromBuffer<K>(&(e->key),std::move(k));
    ConstructFromBuffer<V>(&(e->val),std::move(v));
    e->use = true;
    e->del = false;
    ++size_;
    return std::make_pair(Iterator(this,(e-entry_)),true);
  }
  return std::make_pair(Iterator(this,(e-entry_)),false);
}

template< typename K , typename V, typename Trait >
typename Table<K,V,Trait>::Iterator
Table<K,V,Trait>::Update( Zone* zone , K&& k , V&& v ) {
  if(cap_ == slot_size_) Rehash(zone);
  auto e = FindSlot(k,Trait::Hash(k),INSERT);
  lava_verify(e);

  if(e->IsEmpty()) {
    ConstructFromBuffer<K>(&(e->key),std::move(k));
    ConstructFromBuffer<V>(&(e->val),std::move(v));
    ++size_;
  } else {
    lava_debug(NORMAL,lava_verify(e->IsUse()););
    e->key = std::move(k);
    e->val = std::move(v);
  }

  e->use = true;
  e->del = false;
  return Iterator(this,(e-entry_));
}

template< typename K , typename V, typename Trait >
typename Table<K,V,Trait>::Iterator
Table<K,V,Trait>::Update( Zone* zone , const K& k , const V& v ) {
  if(cap_ == slot_size_) Rehash(zone);
  auto e = FindSlot(k,Trait::Hash(k),INSERT);
  lava_verify(e);

  if(e->IsEmpty()) {
    ConstructFromBuffer<K>(&(e->key),k);
    ConstructFromBuffer<V>(&(e->val),v);
    ++size_;
  } else {
    lava_debug(NORMAL,lava_verify(e->IsUse()););
    e->key = k;
    e->val = v;
  }

  e->use = true;
  e->del = false;
  return Iterator(this,(e-entry_));
}

template< typename K , typename V , typename Trait >
void Table<K,V,Trait>::Copy( Zone* zone , Table* t ) const {
  Table temp(zone,*this);
  t->Swap(&temp);
}

template< typename K , typename V , typename Trait >
void Table<K,V,Trait>::Swap( Table* t ) {
  std::swap(entry_,t->entry_);
  std::swap(cap_  ,t->cap_  );
  std::swap(size_ ,t->size_ );
  std::swap(slot_size_ , t->slot_size_);
}

template< typename K , typename V , typename Trait >
void Table<K,V,Trait>::Clear() {
  size_ = 0; slot_size_ = 0;
  ZeroOut(entry_,cap_);
}

template< typename K , typename V , typename Trait >
void Table<K,V,Trait>::Rehash( Zone* zone ) {
  Table<K,V,Trait> temp(zone,cap_*2);
  for( auto itr(GetIterator()); itr.HasNext() ; itr.Move() ) {
    auto e = entry_ + itr.cursor_; // to help me *steal* the data
    temp.Insert(zone,std::move(e->key),std::move(e->val));
  }
  Swap(&temp); // swap the old one and trash it
}

template< typename K , typename V , typename Trait >
typename Table<K,V,Trait>::Entry*
Table<K,V,Trait>::FindSlot( const K& key , std::uint32_t hash , int option ) {
  // 1. find the main position of the hash entry
  auto idx = hash & ( cap_ - 1 );
  auto e   = entry_ + idx;
  if(e->IsEmpty()) {
    if(option == INSERT) {
      e->hash = hash;
      ++slot_size_;
      return e;
    }
    return NULL;
  }
  // 2. find the collided one along with the chain
  while(true) {
    if(e->IsUse()) {
      if(e->hash == hash && Trait::Equal(e->key,key)) {
        return e; // regardless of wether it is a insert or lookup
      }
    }

    if(e->next) e = e->next; else break;
  }
  // 3. we find a e which doesn't have pending chain
  if(option == LOOKUP) return NULL;
  // do a linear probing here to find a slot to be chained with
  {
    auto h = hash;
    Entry* pos;
    do
      pos = entry_ + ((++h) & (cap_ -1));
    while(!pos->IsEmpty());

    e->next   = pos;
    pos->hash = hash;
    ++slot_size_;

    // we set nothing to the new entry just bump the slot_size since
    // the caller doesn't know whether we should bump the slot_size
    // field or not
    return pos;
  }
}

} // namespace zone
} // namespace lavascript


#endif // ZONE_TABLE_H_
