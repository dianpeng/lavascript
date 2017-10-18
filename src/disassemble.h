#ifndef DISASSEMBLE_H_
#define DISASSEMBLE_H_
#include <cstdint>

namespace lavascript {
class DumpWriter;

// Disassemble a chunk of memory with length into human readable assembly into
// DumpWriter object
void Disassemble( void* buffer , std::size_t length , DumpWriter* );

} // namespace

#endif // DISASSEMBLE_H_
