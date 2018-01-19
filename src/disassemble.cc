#include "disassemble.h"
#include "trace.h"
#include "util.h"

#include "Zydis/Zydis.h"

namespace lavascript {

void SimpleDisassemble( void* buffer , std::size_t length , DumpWriter* writer ) {
  ZydisDecoder decoder;
  ZydisDecoderInit(
      &decoder,
      ZYDIS_MACHINE_MODE_LONG_64,
      ZYDIS_ADDRESS_WIDTH_64);

  ZydisFormatter formatter;
  ZydisFormatterInit(&formatter,ZYDIS_FORMATTER_STYLE_INTEL);

  std::uint64_t pc = reinterpret_cast<std::uint64_t>(buffer);
  std::uint8_t* rp = static_cast<std::uint8_t*>(buffer);
  std::size_t size = length;

  ZydisDecodedInstruction instr;
  while(ZYDIS_SUCCESS(
        ZydisDecoderDecodeBuffer(&decoder,rp,size,pc,&instr))) {
    char b[256];
    ZydisFormatterFormatInstruction(
        &formatter,&instr,b,sizeof(b));
    writer->WriteL("%016" LAVA_FMTU64 " (%d) %s",pc,instr.length,b);
    rp += instr.length;
    size -= instr.length;
    pc += instr.length;
  }
}

} // namespace lavascript
