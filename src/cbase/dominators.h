#ifndef CBASE_DOMINATORS_H_
#define CBASE_DOMINATORS_H_
#include "src/util.h"
#include "src/zone/zone.h"
#include "src/zone/stl.h"

#include <vector>
#include <map>
#include <string>

namespace lavascript {
namespace cbase      {
namespace hir        {
class ControlFlow;
class Graph;

// Simple dominator calculation for HIR graph. It uses straitforward
// data flow algorithm to calculate
//
// This data structure uses STL since it is transient and not hold by
// the graph data structure
class Dominators {
 public:
  typedef zone::stl::ZoneVector<ControlFlow*> DominatorSet;
  // create a dominator set based on input graph
  Dominators( zone::Zone* , const Graph& );
 public:
  // Get the dominator set for a specific node. The dominator set
  // returned by this function will contain the node itself
  const DominatorSet& GetDominatorSet( ControlFlow* ) const;
  // Get the immediate dominator for the input control flow node
  ControlFlow*        GetImmDominator( ControlFlow* ) const;
  // Check whether the first node is dominator of the second node
  bool  IsDominator   ( ControlFlow* , ControlFlow* ) const;
  // Get common dominator set for 2 nodes
  void  GetCommonDominatorSet( ControlFlow* , ControlFlow* , DominatorSet* ) const;
  // Print the dominator information into dot format for debugging purpose
  std::string PrintToDotFormat() const;
 private: // Helper to mainpulate the std::vector
  void Build       ( const Graph& );
  void AddSet      ( DominatorSet* , ControlFlow* ) const;
  void IntersectSet( DominatorSet* , const DominatorSet& ) const;
  void IntersectSet( DominatorSet* , const DominatorSet& , const DominatorSet& ) const;

  DominatorSet* GetDomSet( const Graph& , const DominatorSet& , ControlFlow* );
 private: // Helper to generate dot format representation of dominators
  std::string GetNodeName( ControlFlow* node ) const;

  zone::stl::ZoneMap<ControlFlow*,DominatorSet> dominators_;
  zone::stl::ZoneMap<ControlFlow*,ControlFlow*> imm_dominators_;
  zone::Zone* zone_;
};

} // namespace hir
} // namespace cbase
} // namespace lavascript

#endif // CBASE_DOMINATORS_H_
