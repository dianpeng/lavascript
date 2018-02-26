#include <src/trace.h>
#include <src/cbase/value-range.h>

#include <gtest/gtest.h>

#include <cassert>
#include <bitset>
#include <vector>

namespace lavascript {
namespace cbase      {
namespace hir        {

TEST(ValueRange,F64Basic) {
  {
    Float64ValueRange range;
    range.Union(Binary::LT,-100);
    range.Union(Binary::LT,1);
    {
      DumpWriter writer;
      range.Dump(&writer);
    }
  }
}

} // namespace hir
} // namespace cbase
} // namespace lavascript

int main( int argc, char* argv[] ) {
  ::lavascript::InitTrace("-");
  testing::InitGoogleTest(&argc,argv);
  return RUN_ALL_TESTS();
}
