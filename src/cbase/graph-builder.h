#ifndef CBASE_GRAPH_BUILDER_H_
#define CBASE_GRAPH_BUILDER_H_
#include "src/objects.h"

namespace lavascript {
class RuntimeTrace;
namespace cbase      {
namespace hir        {
class Graph;

// Build a prototype object into a Graph object
bool BuildPrototype   ( const Handle<Script>& , const Handle<Prototype>& , const RuntimeTrace& , Graph* );

// Build a prototype object starting at certain address into a Graph object with OSR style
bool BuildPrototypeOSR( const Handle<Script>& , const Handle<Prototype>& , const RuntimeTrace& ,
                                                                           const std::uint32_t*,
                                                                           Graph* );

} // namespace hir
} // namespace cbase
} // namespace lavascript

#endif // CBASE_GRAPH_BUILDER_H_
