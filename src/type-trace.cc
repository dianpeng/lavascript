#include "type-trace.h"
#include "src/interpreter/bytecode-iterator.h"

namespace lavascript {

bool TypeTrace::AddTrace( BytecodeAddress addr , const Value& d1 ,
                                                 const Value& d2 ,
                                                 const Value& d3 ,
                                                 std::uint32_t extra ) {
  if(forbidden_set_.find(addr) != forbidden_set_.end()) return false;

  auto itr = map_.insert( std::make_pair(addr,TypeTracePoint(d1,d2,d3,extra)) );
  if(!itr.second) {
    const TypeTracePoint& old = itr.first->second;

    if(!old.data[0].Equal(d1) || !old.data[1].Equal(d2) ||
       !old.data[2].Equal(d3) || old.extra != extra) {
      /**
       * the type changed from last profile , so it means the type is
       * not stable at all. We mark it to be forbidden and let JIT to
       * generate a full polymorphic operator
       */
      forbidden_set_.insert(addr);
      return false;
    }
  }
  return true;
}

void TypeTrace::Dump( DumpWriter* writer ) {
  writer->WriteL("***************************************");
  writer->WriteL("          Type Trace                   ");
  writer->WriteL("***************************************");

  {
    DumpWriter::Section header(writer,"Forbidden Set");
    for( auto &e : forbidden_set_ ) {
      writer->WriteL("%s", interpreter::GetBytecodeRepresentation(e).c_str());
    }
  }

  {
    DumpWriter::Section header(writer,"Trace Set");
    for( auto &e : map_ ) {
      writer->WriteL("%s",interpreter::GetBytecodeRepresentation(e.first).c_str());
    }
  }
}

} // namespace lavascript
