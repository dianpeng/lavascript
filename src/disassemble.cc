#include "disassemble.h"
#include "trace.h"
#include <Zydis/Zydis.h>

namespace lavascript {

void Disassemble( void* buffer , std::size_t length , DumpWriter* writer ) {
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
    char buffer[256];
    ZydisFormatterFormatInstruction(
        &formatter,&instr,buffer,sizeof(buffer));
    writer->WriteL("%016" PRIX64 " " "%s",pc,buffer);
    rp += instr.length;
    size -= instr.length;
    pc += instr.length;
  }
}

} // namespace lavascript
