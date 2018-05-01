#ifndef RUNTIME_TRACE_H_
#define RUNTIME_TRACE_H_

#include "objects.h"

#include <unordered_map>
#include <unordered_set>

namespace lavascript {

class DumpWriter;

/**
 * type trace
 *
 * type trace is a special phase during interpretation. We don't have inline cache
 * like system to collect runtime type information, instead of our interpreter will
 * enter record status once it decides to JIT a certain loop.
 *
 * During this record status, the interpreter will start to *trace* each bytecode
 * along with its input argument. This type trace result will be handed to the JIT
 * backend to be used to do speculative code generation.
 */

struct TypeTracePoint {
  Value data[3];
  std::uint32_t extra;

  TypeTracePoint(): data() , extra() {}

  TypeTracePoint( const Value& d1 , const Value& d2 , const Value& d3 , std::uint32_t e ):
    data(),
    extra(e)
  {
    data[0] = d1;
    data[1] = d2;
    data[2] = d3;
  }
};

class RuntimeTrace {
 public:
  typedef const std::uint32_t* BytecodeAddress;

  RuntimeTrace() : forbidden_set_() , map_() {}

  // Add a new trace into the trace map. If such entry is existed, then
  // we compare t2o TypeTracePoint's value and make sure they are same
  // otherwise we add this address into ForbiddenSet to indicate the type
  // is not stable for this BytecodeAddress. JIT will just emit a polymorphic
  // operator code for this BytecodeAddress
  bool AddTrace( BytecodeAddress addr , const Value& d1 = Value() ,
                                        const Value& d2 = Value() ,
                                        const Value& d3 = Value() ,
                                        std::uint32_t extra = 0 );

  inline const TypeTracePoint* GetTrace( BytecodeAddress addr ) const;

 public:
  // For debugging purpose
  void Dump( DumpWriter* );

 private:
  typedef std::unordered_set<BytecodeAddress> ForbiddenSet;
  typedef std::unordered_map<BytecodeAddress,TypeTracePoint> RuntimeTraceMap;

  ForbiddenSet forbidden_set_;
  RuntimeTraceMap map_;
};

inline const TypeTracePoint* RuntimeTrace::GetTrace( BytecodeAddress addr ) const {
  auto itr = map_.find(addr);
  if(itr == map_.end()) return NULL;
  return &(itr->second);
}

} // namespace lavascript

#endif // RUNTIME_TRACE_H_
