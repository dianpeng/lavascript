#include <src/zone/vector.h>
#include <src/zone/zone.h>
#include <src/trace.h>
#include <gtest/gtest.h>

namespace lavascript {
namespace zone {

TEST(Vector,Vector) {
  Zone zone(4,4);
  {
    Vector<int> vec;
    ASSERT_TRUE(vec.empty());
    ASSERT_EQ(0,vec.size());
    ASSERT_EQ(0,vec.capacity());
  }

  {
    Vector<int> vec;

    for( size_t i = 0 ; i < 10240 ; ++i ) {
      vec.Add(&zone,static_cast<int>(i));
    }

    for( size_t i = 0 ; i < 10240 ; ++i ) {
      ASSERT_EQ( vec[i] , static_cast<int>(i) );
    }
  }
}

TEST(Vector,InsertRemove) {
  Zone zone(4,4);
  {
    Vector<int> vec;
    vec.Add(&zone,1);
    vec.Add(&zone,2);
    ASSERT_TRUE(vec.size() == 2);
    ASSERT_EQ(1,vec[0]);
    ASSERT_EQ(2,vec[1]);

    // insert the value at front
    vec.Insert( &zone , vec.GetForwardIterator() , 0 );
    ASSERT_EQ(0,vec[0]);
    ASSERT_EQ(1,vec[1]);
    ASSERT_EQ(2,vec[2]);

    ASSERT_EQ(3,vec.size());

    auto itr = vec.GetForwardIterator();
    itr.Advance(1);

    vec.Insert( &zone , itr , 100 );
    ASSERT_EQ(0,vec[0]);
    ASSERT_EQ(100,vec[1]);
    ASSERT_EQ(1,vec[2]);
    ASSERT_EQ(2,vec[3]);
    ASSERT_EQ(4,vec.size());

    // remove the iterator out
    {
      auto itr = vec.GetForwardIterator();
      auto end = vec.GetForwardIterator();
      end.Advance(2);
      vec.Remove(itr,end);
      ASSERT_EQ(1,vec[0]);
      ASSERT_EQ(2,vec[1]);
      ASSERT_EQ(2,vec.size());

    }

    {
      auto itr = vec.GetForwardIterator();
      auto end = vec.GetForwardIterator();
      end.Advance(2);
      auto res = vec.Remove(itr,end);
      ASSERT_TRUE(!res.HasNext());
      ASSERT_TRUE(vec.empty());
      ASSERT_EQ(0,vec.size());
    }

    {
      auto itr = vec.GetForwardIterator();
      auto res = vec.Insert(&zone,itr,100);
      ASSERT_EQ(100,res.value());
      ASSERT_EQ(100,vec[0]);
      ASSERT_EQ(1,vec.size());
    }
  }
}

} // namespace zone
} // namespace lavascript

int main( int argc, char* argv[] ) {
  srand(0);
  ::lavascript::InitTrace("-");
  testing::InitGoogleTest(&argc,argv);
  return RUN_ALL_TESTS();
}
