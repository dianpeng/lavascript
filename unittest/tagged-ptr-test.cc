#include <src/tagged-ptr.h>
#include <src/trace.h>
#include <gtest/gtest.h>

namespace lavascript {

TEST(TaggedPtr,TaggedPtr) {
  {
    int value = 0;
    TaggedPtr<int> tp(&value,1);

    ASSERT_TRUE(tp.ptr() == &value);
    ASSERT_EQ(1,tp.state());

    tp.set_state(2);
    ASSERT_EQ(2,tp.state());

    tp.set_state(0);
    ASSERT_EQ(0,tp.state());

    tp.set_state(3);
    ASSERT_EQ(3,tp.state());

    ASSERT_EQ(&value,tp.ptr());
  }

  {
    int value = 1;
    TaggedPtr<int> tp(&value);
    int value2 = 2;
    tp.set_state(3);
    ASSERT_EQ(3,tp.state());
    ASSERT_EQ(&value,tp.ptr());

    tp.set_ptr(&value2);
    ASSERT_EQ(3,tp.state());
    ASSERT_EQ(&value2,tp.ptr());

    int value3 = 3;
    tp.reset(&value3,1);
    ASSERT_EQ(1,tp.state());
    ASSERT_EQ(&value3,tp.ptr());
  }
}

} // namespace lavascript

int main( int argc, char* argv[] ) {
  ::lavascript::InitTrace("-");
  testing::InitGoogleTest(&argc,argv);
  return RUN_ALL_TESTS();
}
