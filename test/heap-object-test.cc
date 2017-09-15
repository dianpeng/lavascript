#include <src/heap-object-header.h>
#include <gtest/gtest.h>
#include <iostream>
#include <random>

namespace lavascript {

#ifdef min
#undef min
#endif

#ifdef max
#undef max
#endif


std::uint64_t RandUInt64() {
  std::random_device device;
  std::default_random_engine el(device());
  std::uniform_int_distribution<std::uint64_t> dist(1,std::numeric_limits<std::uint64_t>::max());
  std::uint64_t r = dist(el);
  return r;
}

TEST(HeapObjectHeader,SetterGetter) {
  {
    for ( std::size_t i = 0 ; i < 1000 ; ++i ) {
      HeapObjectHeader v(RandUInt64());
      v.set_type( TYPE_STRING );
      ASSERT_TRUE( v.IsString() );
      v.set_type( TYPE_SLICE );
      ASSERT_TRUE( v.IsSlice() );
      v.set_type( TYPE_ITERATOR );
      ASSERT_TRUE( v.IsIterator() );
      v.set_type( TYPE_LIST );
      ASSERT_TRUE( v.IsList() );
      v.set_type( TYPE_MAP );
      ASSERT_TRUE( v.IsMap() );
      v.set_type( TYPE_OBJECT );
      ASSERT_TRUE( v.IsObject() );
      v.set_type( TYPE_PROTOTYPE );
      ASSERT_TRUE( v.IsPrototype() );
      v.set_type( TYPE_CLOSURE );
      ASSERT_TRUE( v.IsClosure() );
      v.set_type( TYPE_EXTENSION );
      ASSERT_TRUE( v.IsExtension() );
    }
  }

  {
    for( std::size_t i = 0 ; i < 1000 ; ++i ) {
      HeapObjectHeader v(RandUInt64());

      v.set_type( TYPE_STRING );
      v.set_sso();
      ASSERT_TRUE( v.IsSSO() );
      v.set_long_string();
      ASSERT_TRUE( v.IsLongString() );
    }
  }

  {
    for( std::size_t i = 0 ; i < 1000 ; ++i ) {
      HeapObjectHeader v(RandUInt64());
      v.set_size(100);
      ASSERT_EQ(100,v.size());
      ASSERT_EQ(108,v.total_size());

      v.set_size( std::numeric_limits<std::uint32_t>::max() );
      ASSERT_EQ(std::numeric_limits<std::uint32_t>::max(),v.size());
      ASSERT_EQ(static_cast<std::size_t>(
            std::numeric_limits<std::uint32_t>::max()) + 8 , v.total_size());
    }
  }

  {
    for( std::size_t i = 0 ; i < 1000 ; ++i ) {
      HeapObjectHeader v(RandUInt64());
      v.set_end_of_chunk();
      ASSERT_TRUE(v.IsEndOfChunk());
      v.set_not_end_of_chunk();
      ASSERT_FALSE(v.IsEndOfChunk());
    }
  }
}

TEST(HeapObjectHeader,EncodeDecode) {
  HeapObjectHeader v(RandUInt64());
  v.set_type(TYPE_STRING);
  v.set_long_string();
  v.set_end_of_chunk();
  v.set_size(1024);

  std::uint64_t raw = v.raw();
  HeapObjectHeader result(raw);
  ASSERT_TRUE(result.IsString());
  ASSERT_TRUE(result.IsLongString());
  ASSERT_TRUE(result.IsEndOfChunk());
  ASSERT_TRUE(result.size() == 1024);
  ASSERT_TRUE(result.total_size() == 1024+8);
}

} // namespace lavascript

int main( int argc, char* argv[] ) {
  ::lavascript::InitTrace("-");
  testing::InitGoogleTest(&argc,argv);
  return RUN_ALL_TESTS();
}

