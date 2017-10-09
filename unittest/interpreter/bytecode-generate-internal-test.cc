#include <gtest/gtest.h>

namespace lavascript {
namespace interpreter{

TEST(Interpreter,Register) {
  /* -----------------------
   * register allocator    |
   * ----------------------*/
  {
    RegisterAllocator ra;
    ASSERT_TRUE(ra.base() == 0);

    // Enter into a lexical scope
    {
      std::uint8_t base;
      ASSERT_TRUE(ra.EnterScope(10,&base));
      ASSERT_EQ(0,base);
      ASSERT_EQ(10,ra.base());
      for( std::size_t i = 0 ; i < 10 ; ++i ) {
        ASSERT_TRUE(ra.IsReserved(Register(i)));
      }
      ra.LeaveScope();
    }

    ASSERT_EQ(0,ra.base());
    {
      for( std::size_t i = 0 ; i < 255 ; ++i ) {
        Optional<Register> r(ra.Grab());
        ASSERT_TRUE(r);
        ASSERT_EQ(i,r.Get().index());
      }

      ASSERT_TRUE(ra.IsEmpty());

      for( std::size_t i = 0 ; i < 255 ; ++i ) {
        ASSERT_TRUE(ra.IsUsed(Register(i)));
      }

      for( std::size_t i = 0 ; i < 255 ; ++i ) {
        ASSERT_FALSE(ra.IsAvailable(Register(i)));
      }

      for( std::size_t i = 0 ; i < 255 ; ++i ) {
        ra.Drop(Register(i));
      }

      ASSERT_FALSE(ra.IsEmpty());
      ASSERT_EQ(0,ra.base());
    }
  }
}

} // namespace interpreter
} // namespace lavascript

int main( int argc, char* argv[] ) {
  ::lavascript::InitTrace("-");
  testing::InitGoogleTest(&argc,argv);
  return RUN_ALL_TESTS();
}
