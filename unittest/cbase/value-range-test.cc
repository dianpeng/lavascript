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
  Float64ValueRange range;
  range.Union(Binary::GT,3);
  range.Union(Binary::GE,2);
  range.Union(Binary::GE,5);
  range.Union(Binary::LT,1);
  range.Union(Binary::LE,1);
  {
    DumpWriter wr;
    range.Dump(&wr);
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
