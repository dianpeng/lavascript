#ifndef ITERATOR_H_
#define ITERATOR_H_
#include "macro.h"
#include <utility>
#include <memory>

namespace lavascript {

// Here we implement a polymorphic iterator abstraction to help us to define certain
// interface. The typical way to work around normal iterator's static type inheritance
// is:
//
// 1) using template
// 2) using foreach callback style interface.
// 3) directly expose specific container that is in use.
//
// The above 3 ways are okay but not good enough for some specific use. The polymorphic
// iterator serve as a iterator wrapper and provides normal iterator concept that we have
// already. Since it is polymorphic and needs to be copyable , it costs a heap allocation
// normally ; a copy also costs a heap allocation since it needs a deep copy.

namespace detail {

template< typename T > class IteratorInterface {
 public:
  virtual bool     HasNext() const = 0;
  virtual bool     Move   () const = 0;
  virtual T&       value  ()       = 0;
  virtual const T& value  () const = 0;
  virtual void set_value  ( const T& ) = 0;

  virtual std::unique_ptr<IteratorInterface<T>> Clone() const =0;
  virtual ~IteratorInterface() {}
 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(IteratorInterface)
};

template< typename T , typename ITR >
class IteratorInterfaceImpl : public IteratorInterface<T> {
 public:
  virtual bool  HasNext   () const { return itr_.HasNext(); }
  virtual bool  Move      () const { return itr_.Move();    }
  virtual T&    value     ()       { return static_cast<T&>(itr_.value()); }
  virtual const T& value  () const { return static_cast<const T&>(itr_.value()); }
  virtual void set_value ( const T& value ) { itr_.set_value(value); }

  virtual std::unique_ptr<IteratorInterface<T>> Clone() const {
    return std::make_unique<IteratorInterface<T>>(itr_);
  }

  IteratorInterfaceImpl( const ITR& itr ) : itr_(itr) {}
 private:
  ITR itr_;

  LAVA_DISALLOW_COPY_AND_ASSIGN(IteratorInterfaceImpl)
};

} // namespace detail

template< typename T > class PloyIterator {
 public:
  typedef detail::IteratorInterface<T> Interface;
  typedef T        ValueType;
  typedef T&       ReferenceType;
  typedef const T& ConstReferenceType;
 public:
  template< typename ITR >
  explicit PloyIterator( const ITR& itr ):
    impl_( new detail::IteratorInterfaceImpl<T,ITR>(itr) )
  {}

  PloyIterator( PloyIterator&& that      ): impl_( std::move(that.impl_) ) {}
  PloyIterator( const PloyIterator& that ): impl_( that.impl_->Clone() )   {}
  PloyIterator& operator = ( const PloyIterator& that ) {
    if(this != &that) impl_ = that->impl_->Clone();
    return *this;
  }
  PloyIterator& operator = ( PloyIterator&& that ) {
    impl_ = std::move(that.impl_);
    return *this;
  }
 public:
  bool HasNext() const { return impl_->HasNext(); }
  bool Move   () const { return impl_->Move   (); }
  ReferenceType      value  ()       { return impl_->value  (); }
  ConstReferenceType value  () const { return impl_->value  (); }
  void  set_value( ConstReferenceType value ) { impl_->set_value(value); }
 private:
  std::unique_ptr<Interface> impl_;
};

} // namespace lavascript

#endif // ITERATOR_H_
