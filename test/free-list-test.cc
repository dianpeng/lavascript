#include <src/free-list.h>
#include <cstdint>
#include <gtest/gtest.h>
#include <vector>

namespace lavascript {

struct Object {
  std::uint64_t number;
  std::int32_t value;
  Object( std::uint64_t a , std::int32_t b ):
    number(a),
    value (b)
  {}
  Object():number(0),value(0){}
};

TEST(FreeList,Grab) {

  FreeList<Object> pool(1,2,NULL);
  std::vector<Object*> object_pool;

  for( std::size_t i = 0 ;  i < 100000 ; ++i ) {
    Object* p = pool.Grab();
    p->number = i;
    p->value = static_cast<std::int32_t>(i);
    object_pool.push_back(p);
  }

  size_t count = 0;
  for( auto &e : object_pool ) {
    ASSERT_EQ(count,e->number);
    ASSERT_EQ(count,static_cast<std::size_t>(e->value));
    ++count;
  }

  ASSERT_TRUE( pool.size() == 100000 );
}

TEST(FreeList,Drop) {
  FreeList<Object> pool(1,2,NULL);
  std::vector<Object*> object_pool;

  for( std::size_t i = 0 ;  i < 100000 ; ++i ) {
    Object* p = pool.Grab();
    p->number = i;
    p->value = static_cast<std::int32_t>(i);
    object_pool.push_back(p);
  }

  size_t count = 0;
  for( auto &e : object_pool ) {
    ASSERT_EQ(count,e->number);
    ASSERT_EQ(count,static_cast<std::size_t>(e->value));
    ++count;
  }

  ASSERT_TRUE( pool.size() == 100000 );

  count = 0;
  for( auto &e : object_pool ) {
    pool.Drop(e);
    ++count;
    ASSERT_TRUE(pool.size() == (100000-count));
  }
  object_pool.clear();

  size_t ck_size = pool.chunk_size();

  for( std::size_t i = 0 ;  i < 100000 ; ++i ) {
    Object* p = pool.Grab();
    p->number = i*i;
    p->value = static_cast<std::int32_t>(i);
    object_pool.push_back(p);
  }

  count = 0;
  for( auto &e : object_pool ) {
    ASSERT_EQ(count*count,e->number);
    ASSERT_EQ(count,static_cast<std::size_t>(e->value));
    ++count;
  }

  ASSERT_TRUE( pool.size() == 100000 );
  ASSERT_TRUE( ck_size == pool.chunk_size() );
}

} // namespace lavascript

int main( int argc, char* argv[] ) {
  ::lavascript::InitTrace("-");
  testing::InitGoogleTest(&argc,argv);
  return RUN_ALL_TESTS();
}
