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
  // Represent a frame of a call. Used to deoptimize certain code if needed
  struct FrameInfo {

  };

 public:
  Graph( const Handle<Closure>& closure , const std::uint32_t* osr_bc   );
  // Build the graph
  bool BuildGraph();
 public:
  Zone* zone() { return &zone_; }
  NodeFactory* node_factory() { return &node_factory_; }

 private:
  Zone zone_;
  NodeFactory node_factory_;
  const std::uint32_t* osr_bc_;

  ir::Start* start_;
  ir::End*   end_  ;
  zone::Vector<FrameInfo*> frame_info_;
  Handle<Closure> closure_;

  LAVA_DISALLOW_COPY_AND_ASSIGN(Graph)
};

} // namespace cbase
} // namespace lavascript


#endif // CBASE_GRAPH_H_
