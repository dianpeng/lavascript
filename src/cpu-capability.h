#ifndef CPU_CAPABILITY_H_
#define CPU_CAPABILITY_H_
#include <cstdint>
#include "macro.h"

namespace lavascript {
class DumpWriter;

// CPUCapability class represents what kind of instruction set this
// CPU supports. Our compiler can only run on X64 but we need to detect
// certain SSE and AVX version to generate code accordingly.
class CPUCapability {
 public:
  // Common C++11 style singleton and it is thread safe for now due to
  // new memory order model added into the language.
  static CPUCapability& GetInstance();
 public:
  // At least SSE2 instruction set is needed to make our interpreter work
  bool IsSSE2() const { return is_sse2_; }
  bool IsSSE3() const { return is_sse3_; }
  bool IsSSSE3() const{ return is_ssse3_; }
  bool IsSSE41()const { return is_sse41_; }
  bool IsSSE42()const { return is_sse42_; }
  bool IsSSE4a() const { return is_sse4a_; }
  // Get the vender type
  bool IsAMD() const { return is_amd_; }
  bool IsIntel() const { return is_intel_; }
  void Dump( DumpWriter* ) const;
 private:
  CPUCapability();

  bool is_sse_;
  bool is_sse2_;
  bool is_sse3_;
  bool is_ssse3_;
  bool is_sse41_;
  bool is_sse42_;
  bool is_sse4a_;
  bool is_amd_;
  bool is_intel_;

  LAVA_DISALLOW_COPY_AND_ASSIGN(CPUCapability);
};

} // namespace lavascript

#ifdef __GNUG__
#include <cpuid.h>

inline void cpuid_info( std::int32_t category , std::int32_t output[4] ) {
  __cpuid_count(category,0,output[0],output[1],output[2],output[3]);
}

#else
#error "Not implemented"
#endif // __GNUG__

#endif // CPU_CAPABILITY_
