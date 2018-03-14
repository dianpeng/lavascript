#include <src/zone/zone.h>
#include <src/zone/string.h>
#include <src/zone/table.h>

#include <src/trace.h>
#include <gtest/gtest.h>

namespace lavascript {
namespace zone {


TEST(Zone,Table) {
  Zone zone(4,4);
  Table<String*,int,StringTrait> table(&zone,2);

  ASSERT_TRUE(table.empty());
  ASSERT_EQ(0,table.size());
  ASSERT_EQ(0,table.slot_size());
  ASSERT_EQ(2,table.capacity());

  {
    {
      auto itr = table.Insert( &zone ,String::New(&zone,"a") , 1 );
      ASSERT_TRUE(itr.second);
      ASSERT_EQ(itr.first.value(),1);
      ASSERT_TRUE(*(itr.first.key()) == "a");
      ASSERT_EQ(1,table.size());
      ASSERT_EQ(1,table.slot_size());
      ASSERT_FALSE(table.empty());
    }
    {
      auto itr = table.Find( String::New(&zone,"a") );
      ASSERT_TRUE(itr.HasNext());
      ASSERT_TRUE(*(itr.key()) == "a");
      ASSERT_TRUE(itr.value() == 1);
    }

    {
      auto itr = table.Insert( &zone ,String::New(&zone,"b") , 2 );
      ASSERT_TRUE(itr.second);
      ASSERT_EQ(itr.first.value(),2);
      ASSERT_TRUE(*(itr.first.key()) == "b");
      ASSERT_EQ(2,table.size());
      ASSERT_EQ(2,table.slot_size());
      ASSERT_FALSE(table.empty());
    }
    {
      auto itr = table.Find( String::New(&zone,"b") );
      ASSERT_TRUE(itr.HasNext());
      ASSERT_TRUE(*(itr.key()) == "b");
      ASSERT_TRUE(itr.value() == 2);
    }

    {
      auto itr = table.Insert( &zone ,String::New(&zone,"c") , 3 );
      ASSERT_TRUE(itr.second);
      ASSERT_EQ(itr.first.value(),3);
      ASSERT_TRUE(*(itr.first.key()) == "c");
      ASSERT_EQ(3,table.size());
      ASSERT_EQ(3,table.slot_size());
      ASSERT_FALSE(table.empty());
    }
    {
      auto itr = table.Find( String::New(&zone,"c") );
      ASSERT_TRUE(itr.HasNext());
      ASSERT_TRUE(*(itr.key()) == "c");
      ASSERT_TRUE(itr.value() == 3);
    }

    ASSERT_EQ(4,table.capacity());

    // find a node that is not existed
    {
      auto itr = table.Find( String::New(&zone,"xx") );
      ASSERT_FALSE(itr.HasNext());
    }

    // remove b
    {
      ASSERT_TRUE(table.Remove( String::New(&zone,"b")) );
      ASSERT_FALSE(table.Find(String::New(&zone,"b")).HasNext());
      ASSERT_EQ(2,table.size());
      ASSERT_EQ(3,table.slot_size());
    }

    // remove a
    {
      ASSERT_TRUE(table.Remove( String::New(&zone,"a")) );
      ASSERT_FALSE(table.Find(String::New(&zone,"a")).HasNext());
      ASSERT_EQ(1,table.size());
      ASSERT_EQ(3,table.slot_size());
    }

    {
      ASSERT_TRUE(table.Remove( String::New(&zone,"c")) );
      ASSERT_FALSE(table.Find(String::New(&zone,"c")).HasNext());
      ASSERT_EQ(0,table.size());
      ASSERT_TRUE(table.empty());
      ASSERT_EQ(0,table.slot_size());
    }
  }

  {
    // do an insertion
    ASSERT_TRUE(table.Insert(&zone,String::New(&zone,"A"),1).second);
    // do an insertion again should fail
    {
      auto itr = table.Insert(&zone,String::New(&zone,"A"),1);
      ASSERT_FALSE(itr.second);
      ASSERT_TRUE (itr.first.HasNext());
      ASSERT_EQ   (1,itr.first.value());
    }

    // do an update
    {
      auto itr = table.Update(&zone,String::New(&zone,"A"),100);
      ASSERT_TRUE (itr.HasNext());
      ASSERT_EQ   (100,itr.value());
    }

    // check the iterator
    {
      auto itr = table.GetIterator();
      ASSERT_TRUE(itr.HasNext());
      ASSERT_TRUE(*(itr.key()) == "A");
      ASSERT_EQ  (100,itr.value());

      ASSERT_FALSE(itr.Move());
      ASSERT_FALSE(itr.HasNext());
    }

    table.Clear();
  }

  {
    // collision :
    // key xxx , bbb and ddd will collide when &(4-1)
    ASSERT_EQ(4,table.capacity());
    ASSERT_EQ(0,table.size());
    ASSERT_EQ(0,table.slot_size());

    ASSERT_TRUE(table.Insert(&zone,String::New(&zone,"xxx"),1).second);
    ASSERT_TRUE(table.Insert(&zone,String::New(&zone,"bbb"),2).second);
    ASSERT_TRUE(table.Insert(&zone,String::New(&zone,"ddd"),3).second);

    ASSERT_TRUE(table.Find(String::New(&zone,"xxx")).value() == 1);
    ASSERT_TRUE(table.Find(String::New(&zone,"bbb")).value() == 2);
    ASSERT_TRUE(table.Find(String::New(&zone,"ddd")).value() == 3);

    ASSERT_EQ(3,table.size());
    ASSERT_EQ(3,table.slot_size());
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
