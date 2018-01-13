#include "intrinsic-call.h"

#include "src/util.h"

namespace lavascript {
namespace interpreter{

static const char* kICallName[] = {

#define __(A,B,C) #A,

  LAVASCRIPT_BUILTIN_FUNCTIONS(__)
  NULL

#undef __ // __

};

static std::uint8_t kICallArgSize[] = {

#define __(A,B,C) C,

  LAVASCRIPT_BUILTIN_FUNCTIONS(__)
  0

#undef __ // __

};

IntrinsicCall MapIntrinsicCallIndex( const char* name ) {
  for( std::size_t i = 0 ; i < ArraySize(kICallName); ++i ) {
    if(strcmp(kICallName[i],name) == 0)
      return static_cast<IntrinsicCall>(i);
  }
  return SIZE_OF_INTRINSIC_CALL;
}

std::uint8_t GetIntrinsicCallArgumentSize( IntrinsicCall ic ) {
  lava_debug(NORMAL,lava_verify(ic >= 0 && ic < SIZE_OF_INTRINSIC_CALL););
  return kICallArgSize[ic];
}

const char* GetIntrinsicCallName( IntrinsicCall ic ) {
  lava_debug(NORMAL,lava_verify(ic >=0 && ic < SIZE_OF_INTRINSIC_CALL););
  return kICallName[ic];
}

} // namespace interpreter
} // namespace lavascript
