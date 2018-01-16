#include <cstdint>
#include <vector>
#include <src/bump-allocator.h>
#include <src/trace.h>
#include <gtest/gtest.h>

namespace lavascript {

TEST(BumpAllocator,Grab) {
  {
    BumpAllocator allocator( 1 , 2 , NULL ); // Tons of refill pool operations
    std::vector<std::uint64_t*> vec;

    for( size_t i = 0 ; i < 10000 ; ++i ) {
      std::uint64_t* p = allocator.Grab<std::uint64_t>();
      *p = i;
      vec.push_back(p);
    }

    std::size_t count = 0;
    for( auto &e : vec ) {
      ASSERT_TRUE(*e == count);
      ++count;
    }
  }
}

} // namespace lavascript

int main( int argc, char* argv[] ) {
  ::lavascript::InitTrace("-");
  testing::InitGoogleTest(&argc,argv);
  return RUN_ALL_TESTS();
}
