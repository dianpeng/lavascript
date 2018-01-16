#include "intrinsic-call.h"

#include "src/util.h"

namespace lavascript {
namespace interpreter{

namespace {

struct ICData {
  const char*  name;
  std::uint8_t arg_count;
  const char*  error_message;

  ICData( const char* n , std::uint8_t ac , const char* msg ):
    name         (n),
    arg_count    (ac),
    error_message(msg)
  {}

  ICData(): name(NULL), arg_count(0), error_message(NULL) {}
};

static const ICData kICData[] = {

#define __(A,B,C,D,E) ICData(#A,D,E),
  LAVASCRIPT_BUILTIN_FUNCTIONS(__)
#undef __ // __
  ICData()
};

#define ICDATA_SIZE (ArraySize(kICData) - 1)

} // namespace

IntrinsicCall MapIntrinsicCallIndex( const char* name ) {
  for( std::size_t i = 0 ; i < ICDATA_SIZE ; ++i ) {
    if(strcmp(kICData[i].name,name) == 0)
      return static_cast<IntrinsicCall>(i);
  }
  return SIZE_OF_INTRINSIC_CALL;
}

std::uint8_t GetIntrinsicCallArgumentSize( IntrinsicCall ic ) {
  lava_debug(NORMAL,lava_verify(ic >= 0 && ic < SIZE_OF_INTRINSIC_CALL););
  return kICData[ic].arg_count;
}

const char* GetIntrinsicCallName( IntrinsicCall ic ) {
  lava_debug(NORMAL,lava_verify(ic >=0 && ic < SIZE_OF_INTRINSIC_CALL););
  return kICData[ic].name;
}

const char* GetIntrinsicCallErrorMessage( IntrinsicCall ic ) {
  lava_debug(NORMAL,lava_verify(ic >=0 && ic < SIZE_OF_INTRINSIC_CALL););
  return kICData[ic].error_message;
}

} // namespace interpreter
} // namespace lavascript
