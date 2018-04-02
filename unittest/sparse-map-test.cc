#include <src/trace.h>
#include <src/sparse-map.h>
#include <gtest/gtest.h>
#include <iostream>
#include <cassert>

namespace lavascript {

TEST(LinearList,Basic) {
  {
    LinearList<int,std::string> ll;

    ll.Insert(0,std::string("A"));
    ll.Insert(1,std::string("B"));
    ll.Insert(2,std::string("C"));

    ASSERT_EQ(3,ll.size());
    {
      auto v = ll.Find(0);
      ASSERT_TRUE(v);
      ASSERT_TRUE(*v == "A");
    }
    {
      auto v = ll.Find(1);
      ASSERT_TRUE(v);
      ASSERT_TRUE(*v == "B");
    }
    {
      auto v = ll.Find(2);
      ASSERT_TRUE(v);
      ASSERT_TRUE(*v == "C");
    }

    // remove the element
    ASSERT_TRUE(ll.Remove(2));
    ASSERT_FALSE(ll.Has(2));

    ASSERT_TRUE(ll.Remove(1));
    ASSERT_FALSE(ll.Has(1));

    ASSERT_TRUE(ll.Remove(0));
    ASSERT_FALSE(ll.Has(0));
  }

  {
    LinearList<int,std::string> ll;
    ll.Insert(0,std::string("A"));
    ll.Insert(1,std::string("B"));

    auto itr = ll.GetForwardIterator();
    ASSERT_TRUE(itr.HasNext());
    ASSERT_TRUE(itr.value().first == 0);
    ASSERT_TRUE(itr.value().second  == "A");

    ASSERT_TRUE(itr.Move());
    ASSERT_TRUE(itr.value().first == 1);
    ASSERT_TRUE(itr.value().second == "B");

    ASSERT_FALSE(itr.Move());
  }
}

TEST(BalanceTree,Basic) {
  {
    BalanceTree<std::string,int> bt;
    bt.Insert("A",0);
    bt.Insert("B",1);
    bt.Insert("C",2);

    ASSERT_EQ(3,bt.size());
    {
      auto v = bt.Find("A");
      ASSERT_TRUE(v);
      ASSERT_TRUE(*v == 0);
    }
    {
      auto v = bt.Find("B");
      ASSERT_TRUE(v);
      ASSERT_TRUE(*v == 1);
    }
    {
      auto v = bt.Find("C");
      ASSERT_TRUE(v);
      ASSERT_TRUE(*v == 2);
    }

    ASSERT_TRUE(bt.Remove("A"));
    ASSERT_FALSE(bt.Has("A"));

    ASSERT_TRUE(bt.Remove("B"));
    ASSERT_FALSE(bt.Has("B"));

    ASSERT_TRUE(bt.Remove("C"));
    ASSERT_FALSE(bt.Has("C"));
  }

  {
    BalanceTree<std::string,int> bt;
    bt.Insert("A",0);
    bt.Insert("B",1);

    auto itr = bt.GetForwardIterator();
    ASSERT_TRUE(itr.HasNext());
    ASSERT_TRUE(itr.value().first == "A");
    ASSERT_TRUE(itr.value().second == 0);

    ASSERT_TRUE(itr.Move());
    ASSERT_TRUE(itr.value().first == "B");
    ASSERT_TRUE(itr.value().second==  1 );

    ASSERT_FALSE(itr.Move());
  }
}

TEST(SparseMap,Basic) {
  {
    SparseMap<std::string,int> sm(1);
    ASSERT_TRUE(sm.size() == 0);
    ASSERT_TRUE(sm.empty());

    sm.Insert("A",1);
    ASSERT_TRUE(sm.size() == 1);
    ASSERT_FALSE(sm.empty());

    sm.Insert("B",2);
    ASSERT_TRUE(sm.size() == 2) << sm.size();
    ASSERT_FALSE(sm.empty());

    {
      auto k = sm.Find("A");
      ASSERT_TRUE(k);
      ASSERT_TRUE(*k == 1);
    }

    {
      auto k = sm.Find("B");
      ASSERT_TRUE(k);
      ASSERT_TRUE(*k == 2);
    }

    ASSERT_TRUE(sm.IsC2());

    // the remove will not down grade the container type
    ASSERT_TRUE(sm.Remove("A"));
    ASSERT_FALSE(sm.Has("A"));

    ASSERT_TRUE(sm.Remove("B"));
    ASSERT_FALSE(sm.Has("B"));

    ASSERT_EQ(0,sm.size());
    ASSERT_TRUE(sm.empty());
  }

  {
    SparseMap<std::string,int> sm(1);
    ASSERT_TRUE(sm.Insert("A",1));
    ASSERT_TRUE(sm.Insert("B",2));
    ASSERT_TRUE(sm.IsC2());

    auto itr(sm.GetForwardIterator());
    ASSERT_TRUE(itr.HasNext());
    ASSERT_TRUE(itr.key()== "A");
    ASSERT_TRUE(itr.value()== 1 );

    ASSERT_TRUE(itr.Move());
    ASSERT_TRUE(itr.key()== "B");
    ASSERT_TRUE(itr.value()== 2 );

    ASSERT_FALSE(itr.Move());
  }
}

} // namespace lavascript

int main( int argc, char* argv[] ) {
  ::lavascript::InitTrace("-");
  testing::InitGoogleTest(&argc,argv);
  return RUN_ALL_TESTS();
}
