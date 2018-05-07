#ifndef ITERATOR_H_
#define ITERATOR_H_
#include "macro.h"
#include "trace.h"
#include <utility>
#include <memory>
#include <type_traits>

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
//
// The ploymoriphic iterator is read only , no plan to support modification and all the
// function are const.

namespace detail {

template< typename T > struct ReturnValueType {
  typedef typename std::conditional<std::is_pod<T>::value,T,const T&>::type
    ValueType;
};

template< typename T > class IteratorInterface {
 public:
  typedef typename ReturnValueType<T>::ValueType ReturnType;
  virtual bool       HasNext() const = 0;
  virtual bool       Move   () const = 0;
  virtual ReturnType value  () const = 0;

  virtual std::unique_ptr<IteratorInterface<T>> Clone() const =0;
  virtual ~IteratorInterface() {}
};

template< typename T , typename ITR >
class IteratorInterfaceImpl : public IteratorInterface<T> {
 public:
  typedef typename ReturnValueType<T>::ValueType ReturnType;
  virtual bool       HasNext   () const { return itr_.HasNext(); }
  virtual bool       Move      () const { return itr_.Move();    }
  virtual ReturnType value     () const { return static_cast<ReturnType>(itr_.value()); }

  virtual std::unique_ptr<IteratorInterface<T>> Clone() const {
    return std::unique_ptr<IteratorInterface<T>>(new IteratorInterfaceImpl<T,ITR>(itr_));
  }

  IteratorInterfaceImpl( const ITR& itr ) : itr_(itr) {}
 private:
  ITR itr_;

  LAVA_DISALLOW_COPY_AND_ASSIGN(IteratorInterfaceImpl)
};


} // namespace detail

template< typename T > class PolyIterator {
 public:
  typedef detail::IteratorInterface<T> Interface;
  typedef T        ValueType;
  typedef typename detail::ReturnValueType<ValueType>::ValueType ReturnType;

 public:
  PolyIterator() : impl_() {}

  template< typename ITR >
  explicit PolyIterator( const ITR& itr ):
    impl_( new detail::IteratorInterfaceImpl<T,ITR>(itr) )
  {}

  PolyIterator( PolyIterator&& that      ): impl_( std::move(that.impl_) ) {}
  PolyIterator( const PolyIterator& that ): impl_() {
    if(that.impl_) impl_ = (that.impl_->Clone());
  }
  PolyIterator& operator = ( const PolyIterator& that ) {
    if(this != &that) {
      if(that->impl_)
        impl_ = that->impl_->Clone();
      else
        impl_.reset();
    }
    return *this;
  }
  PolyIterator& operator = ( PolyIterator&& that ) {
    impl_ = std::move(that.impl_);
    return *this;
  }
 public:
  bool HasNext() const { return impl_ && impl_->HasNext(); }
  bool Move   () const { return impl_ && impl_->Move   (); }
  ReturnType value  () const {
    lava_debug(NORMAL,lava_verify(HasNext()););
    return impl_->value();
  }
 private:
  std::unique_ptr<Interface> impl_;
};

} // namespace lavascript

#endif // ITERATOR_H_
