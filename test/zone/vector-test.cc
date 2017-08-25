#include <src/zone/vector.h>
#include <src/zone/zone.h>
#include <src/core/trace.h>
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

} // namespace zone
} // namespace lavascript

int main( int argc, char* argv[] ) {
  srand(0);
  ::lavascript::core::InitTrace("-");
  testing::InitGoogleTest(&argc,argv);
  return RUN_ALL_TESTS();
}
