#include <src/zone/list.h>
#include <src/zone/zone.h>
#include <src/trace.h>
#include <gtest/gtest.h>

namespace lavascript {
namespace zone {

TEST(List,List) {
  Zone zone(4,4);

  {
    List<int> l;

    ASSERT_TRUE(l.empty());
    ASSERT_EQ(0,l.size());

    l.PushBack( &zone , 1 );
    l.PushBack( &zone , 2 );

    ASSERT_FALSE(l.empty());
    ASSERT_EQ   (2,l.size());

    ASSERT_EQ(1,l.First());
    ASSERT_EQ(2,l.Last() );

    {
      List<int>::ForwardIterator itr(l.GetForwardIterator());
      int i = 1;
      for( ; itr.HasNext(); itr.Move() ) {
        ASSERT_EQ(i,itr.value());
        ++i;
      }
      ASSERT_EQ(3,i);
    }
  }

  // for insert/delete
  {
    List<int> l;
    ASSERT_TRUE(l.empty());
    ASSERT_EQ  (0,l.size());

    l.PushBack(&zone,0);
    l.PushBack(&zone,1);
    l.PushBack(&zone,2);
    l.PushBack(&zone,3);

    ASSERT_EQ(0,l.First());
    ASSERT_EQ(3,l.Last ());

    List<int>::ForwardIterator itr(l.GetForwardIterator());
    ASSERT_TRUE(itr.HasNext());
    itr = l.Remove(itr);

    ASSERT_TRUE(itr.HasNext());
    ASSERT_EQ(1,itr.value());
    ASSERT_EQ(3,l.size());
    ASSERT_EQ(1,l.First());
    ASSERT_EQ(3,l.Last());

    {
      List<int>::ForwardIterator itr(l.GetForwardIterator());
      int i = 1;
      for( ; itr.HasNext(); itr.Move() ) {
        ASSERT_EQ(i,itr.value());
        ++i;
      }
      ASSERT_EQ(4,i);
    }

    ASSERT_TRUE(itr.Move()); // move to 2
    itr= l.Remove(itr);      // remove 2
    ASSERT_EQ(3,itr.value());
    ASSERT_EQ(1,l.First());
    ASSERT_EQ(3,l.Last ());
    ASSERT_EQ(2,l.size ());

    {
      List<int>::ForwardIterator itr(l.GetForwardIterator());
      ASSERT_TRUE(itr.HasNext());
      ASSERT_EQ(1,itr.value());
      itr = l.Insert(&zone,itr,0);
      ASSERT_EQ(0,itr.value());
      ASSERT_EQ(itr,l.GetForwardIterator());

      // move iterator to the last element
      itr = l.GetForwardIterator();
      ASSERT_TRUE(itr.Move());  // 0 --> 1
      ASSERT_TRUE(itr.Move());  // 1 --> 3
      itr = l.Insert(&zone,itr,2);
      ASSERT_EQ(2,itr.value());
    }

    {
      List<int>::ForwardIterator itr(l.GetForwardIterator());
      int i = 0;
      for( ; itr.HasNext() ; itr.Move() ) {
        ASSERT_EQ(i,itr.value());
        ++i;
      }
      ASSERT_EQ(4,i);
      ASSERT_EQ(4,l.size());
    }
  }

  // empty list insert and delete
  {
    List<int> l;
    ASSERT_TRUE(l.empty());
    List<int>::ForwardIterator itr(l.GetForwardIterator());
    l.Insert(&zone,itr,0);
    l.Insert(&zone,itr,1);
    l.Insert(&zone,itr,2);
    l.Insert(&zone,itr,3);

    {
      int i = 0;
      for(itr = l.GetForwardIterator(); itr.HasNext() ; itr.Move()) {
        ASSERT_EQ(i,itr.value());
        ++i;
      }
    }

    {
      ASSERT_EQ(4,l.size());
      int i = 0;
      for( itr = l.GetForwardIterator(); itr.HasNext() ; ++i ) {
        itr = l.Remove(itr);
      }
      ASSERT_EQ(0,l.size());
      ASSERT_EQ(4,i);
    }
  }

  // test iterator
  {
    List<int> l;
    for( int i = 0 ; i < 100 ; ++i ) {
      l.PushBack(&zone,i);
    }

    {
      int i = 0 ;
      for( List<int>::ConstForwardIterator itr = l.GetForwardIterator() ; itr.HasNext() ; itr.Move() ) {
        ASSERT_EQ(i,itr.value());
        ++i;
      }
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
