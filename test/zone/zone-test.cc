#include <src/core/trace.h>
#include <src/zone/zone.h>
#include <gtest/gtest.h>

namespace lavascript {
namespace zone {

TEST(Zone,Zone) {
  Zone zone( sizeof(int) , sizeof(int) * 4 );
  int* arr[1024];

  for( size_t i = 0 ; i < 1024 ; ++i ) {
    arr[i] = zone.Malloc<int>();
    *arr[i]= static_cast<int>(i);
  }

  for( size_t i = 0 ; i < 1024 ; ++i ) {
    ASSERT_EQ(static_cast<int>(i),*arr[i]);
  }
}

} // namespace zone
} // namespace lavascript

int main( int argc, char* argv[] ) {
  ::lavascript::core::InitTrace("-");
  testing::InitGoogleTest(&argc,argv);
  return RUN_ALL_TESTS();
}
