#include "cpu-capability.h"
#include "trace.h"

#include <string.h>

namespace lavascript {

/**
 * The following code are heavily relied on project :
 * https://github.com/Mysticial/FeatureDetector
 */
CPUCapability::CPUCapability():
  is_sse_  (false),
  is_sse2_ (false),
  is_sse3_ (false),
  is_ssse3_(false),
  is_sse41_(false),
  is_sse42_(false),
  is_sse4a_(false),
  is_amd_  (false),
  is_intel_(false) {
  // get vendor string
  {
    std::int32_t info[4];
    cpuid_info(0,info);
    char name[13];
    memcpy(name+0,reinterpret_cast<void*>(info+1),4);
    memcpy(name+4,reinterpret_cast<void*>(info+3),4);
    memcpy(name+8,reinterpret_cast<void*>(info+2),4);
    name[12] = '\0';

    if(strcmp(name,"AuthenticAMD")==0) {
      is_amd_ = true;
    } else if(strcmp(name,"GenuineIntel")==0) {
      is_intel_ = true;
    }
  }

  // detect features
  {
    std::int32_t info[4];
    cpuid_info(0,info);
    std::uint32_t ids = static_cast<std::uint32_t>(info[0]);
    if(ids >= 0x00000001) {
      cpuid_info(0x00000001,info);
      is_sse_   = (info[3] & (static_cast<std::int32_t>(1)<<25)) != 0;
      is_sse2_  = (info[3] & (static_cast<std::int32_t>(1)<<26)) != 0;
      is_sse3_  = (info[2] & (static_cast<std::int32_t>(1)<< 0)) != 0;
      is_ssse3_ = (info[2] & (static_cast<std::int32_t>(1)<< 9)) != 0;
      is_sse41_ = (info[2] & (static_cast<std::int32_t>(1)<<19)) != 0;
      is_sse42_ = (info[2] & (static_cast<std::int32_t>(1)<<20)) != 0;
    }
  }
}

void CPUCapability::Dump( DumpWriter* writer ) const {
  const char* vendor;
  if(is_amd_) {
    vendor = "amd";
  } else if(is_intel_) {
    vendor = "intel";
  } else {
    vendor = "unknown";
  }
  {
    DumpWriter::Section section(writer,"CPU Capability");
    writer->WriteL("Vendor:%s",vendor);
    writer->WriteL("SSE1:%s",is_sse_ ? "true" : "false");
    writer->WriteL("SSE2:%s",is_sse2_? "true" : "false");
    writer->WriteL("SSE3:%s",is_sse3_? "true" : "false");
    writer->WriteL("SSSE3:%s",is_ssse3_? "true" : "false");
    writer->WriteL("SSE41:%s",is_sse41_? "true" : "false");
    writer->WriteL("SSE42:%s",is_sse42_? "true" : "false");
    writer->WriteL("SSE4a:%s",is_sse4a_? "true" : "false");
  }
}

} // namespace
