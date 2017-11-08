#include <src/trace.h>
#include <src/config.h>
#include <gtest/gtest.h>


namespace lavascript {

LAVA_DEFINE_INT32(Test,int32_factor,"This is a int32 factor value",1);
LAVA_DEFINE_INT64(Test,int64_factor,"This is a int64 factor value",1);
LAVA_DEFINE_DOUBLE(Test,double_factor,"This is a double factor value",1.0);
LAVA_DEFINE_BOOLEAN(Test,boolean_factor,"This is a boolean factor value",false);
LAVA_DEFINE_STRING(Test,string_factor,"This is a string factor","Haha");

TEST(Config,Basic) {
  ASSERT_EQ(1,LAVA_OPTION(Test,int32_factor));
  ASSERT_EQ(1,LAVA_OPTION(Test,int64_factor));
  ASSERT_EQ(1.0,LAVA_OPTION(Test,double_factor));
  ASSERT_EQ(false,LAVA_OPTION(Test,boolean_factor));
  ASSERT_TRUE(LAVA_OPTION(Test,string_factor) == "Haha") << LAVA_OPTION(Test,string_factor) << std::endl;
}

LAVA_DEFINE_INT32(RT,int32_factor,"RT int32",0);
LAVA_DEFINE_INT64(RT,int64_factor,"RT int64",1);
LAVA_DEFINE_DOUBLE(RT,double_factor,"RT double",2.0);
LAVA_DEFINE_BOOLEAN(RT,boolean_factor,"RT boolean",false);
LAVA_DEFINE_STRING(RT,string_factor,"RT string","Vivi");

TEST(Config,Dynamic) {
  std::string err;

  // Simulate we have following command
  char** parg = new char*[7];
  parg[0] = strdup("My-test"); // skipped
  parg[1] = strdup("--RT.boolean_factor");
  parg[2] = strdup("--RT.int32_factor=2000");
  parg[3] = strdup("--RT.int64_factor");
  parg[4] = strdup("64656666711111");
  parg[5] = strdup("--RT.string_factor=huhahaha");
  parg[6] = strdup("--RT.double_factor=1.23");

  ASSERT_TRUE( ::lavascript::DConfigInit( 7 , parg , &err ) );

  for( std::size_t i = 0 ; i < 7 ; ++i ) {
    free(parg[i]);
  }
  delete [] parg;


  ASSERT_EQ(true,LAVA_OPTION(RT,boolean_factor)) << LAVA_OPTION(RT,boolean_factor);
  ASSERT_EQ(2000,LAVA_OPTION(RT,int32_factor)) << LAVA_OPTION(RT,int32_factor) ;
  ASSERT_EQ(1.23,LAVA_OPTION(RT,double_factor)) << LAVA_OPTION(RT,double_factor);
  ASSERT_EQ(64656666711111UL,LAVA_OPTION(RT,int64_factor)) << LAVA_OPTION(RT,int64_factor);
  ASSERT_TRUE( LAVA_OPTION(RT,string_factor) == "huhahaha" ) << LAVA_OPTION(RT,string_factor);
}

} // namespace lavascript

int main( int argc, char* argv[] ) {
  ::lavascript::InitTrace("-");
  testing::InitGoogleTest(&argc,argv);
  return RUN_ALL_TESTS();
}
