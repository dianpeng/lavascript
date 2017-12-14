#ifndef CBASE_GRAPH_H_
#define CBASE_GRAPH_H_
#include "ir.h"
#include "src/objects.h"
#include "src/zone.h"

namespace lavascript {
namespace cbase {

// Represent everything about the IR construction
class Graph {
 public:
  Graph( const Handle<Closure>& closure , const Handle<Script> & script  ,
                                          const std::uint32_t* osr_bc   );
  // Build the graph
  bool BuildGraph();
 public:
  Zone* zone() { return &zone_; }

 private:
  Zone zone_;
  ir::Start* start_;
  ir::End*   end_  ;
  OSRStart* osr_start_;
  Vector<OSRExit*> osr_exit_;
  Handle<Closure> closure_;
  Handle<Script>  script_;

  LAVA_DISALLOW_COPY_AND_ASSIGN(Graph)
};

} // namespace cbase
} // namespace lavascript


#endif // CBASE_GRAPH_H_
