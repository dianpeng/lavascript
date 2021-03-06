#include "runtime-trace.h"
#include "src/interpreter/bytecode-iterator.h"

namespace lavascript {

bool RuntimeTrace::AddTrace( BytecodeAddress addr , const Value& d1 ,
                                                    const Value& d2 ,
                                                    const Value& d3 ,
                                                    std::uint32_t extra ) {
  if(forbidden_set_.find(addr) != forbidden_set_.end()) return false;

  auto itr = map_.insert( std::make_pair(addr,TypeTracePoint(d1,d2,d3,extra)) );
  if(!itr.second) {
    auto &old = itr.first->second;

    if(!old.data[0].Equal(d1) || !old.data[1].Equal(d2) ||
       !old.data[2].Equal(d3) || old.extra != extra) {
      /**
       * the type changed from last profile , so it means the type is
       * not stable at all. We mark it to be forbidden and let JIT to
       * generate a full polymorphic operator
       */
      map_.erase(itr.first); // remove the old one since this type feedback is not
                             // stable , so later on JIT can generte dynamic dispatch
      forbidden_set_.insert(addr);
      return false;
    }
  }
  return true;
}

void RuntimeTrace::Dump( DumpWriter* writer ) {
  writer->WriteL("***************************************");
  writer->WriteL("          Runtime Trace                ");
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
