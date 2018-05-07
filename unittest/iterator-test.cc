#include <src/iterator.h>
#include <src/trace.h>
#include <src/util.h>
#include <gtest/gtest.h>

#include <vector>
#include <list>

namespace lavascript {

// Iterator type A
template< typename T >
class VectorIterator {
 public:
  typedef T ValueType;
  typedef T& ReferenceType;
  typedef const T& ConstReferenceType;

  bool HasNext() const { return cursor_ < vec_->size(); }
  bool Move   () const { return cursor_++ < vec_->size(); }
  const T& value() const { return vec_->at(cursor_); }
  T& value() { return vec_->at(cursor_); }
  void set_value( const T& value ) { vec_->at(cursor_) = value; }

  VectorIterator( std::vector<T>* vec ): vec_(vec), cursor_(0) {}
  VectorIterator( const VectorIterator& that ) : vec_(that.vec_) , cursor_(that.cursor_) {}
 private:
  std::vector<T>* vec_;
  mutable std::size_t     cursor_;
};

template< typename T >
class ListIterator {
 public:
  typedef T ValueType;
  typedef T& ReferenceType;
  typedef const T& ConstReferenceType;

  bool HasNext() const { return itr_ != end_; }
  bool Move   () const { ++itr_; return HasNext(); }
  const T& value() const { return *itr_; }
  T&       value()       { return *itr_; }
  void set_value( const T& value ) { *itr_ = value; }

  ListIterator( std::list<T>& list ):itr_(list.begin()),end_(list.end()) {}
  ListIterator( const ListIterator& that ) :itr_(that.itr_), end_(that.end_) {}
 private:
  mutable typename std::list<T>::iterator itr_,end_;
};

TEST(Iterator,Basic) {
  std::vector<int> vec{{1,2,3,4,5,6}};
  std::list  <int> lst{{10,9,8,7,6}};

  auto vec_itr = PolyIterator<int>( VectorIterator<int>(&vec) );
  auto lst_itr = PolyIterator<int>( ListIterator<int>  (lst ) );

  {
    std::size_t i = 0;
    lava_foreach( auto k , vec_itr ) {
      ASSERT_EQ(k,vec[i]); ++i;
    }
  }

  {
    auto itr = lst.begin();
    lava_foreach( auto k , lst_itr ) {
      ASSERT_EQ(*itr,k); ++itr;
    }
  }

  {
    auto temp = PolyIterator<int>( VectorIterator<int>(&vec) );
    temp.Move();
    auto that = PolyIterator<int>( temp );

    std::size_t i = 1;
    lava_foreach( auto k , that ) {
      ASSERT_EQ(k,vec[i]); ++i;
    }
  }

  {
    auto temp = PolyIterator<int>( ListIterator<int> (lst) );
    temp.Move(); temp.Move();
    auto that = PolyIterator<int>( temp );

    auto itr = lst.begin();
    ++itr;++itr;
    lava_foreach( auto k , that ) {
      ASSERT_EQ(k,*itr); ++itr;
    }
  }
}

} // namespace

int main( int argc, char* argv[] ) {
  ::lavascript::InitTrace("-");
  testing::InitGoogleTest(&argc,argv);
  return RUN_ALL_TESTS();
}
