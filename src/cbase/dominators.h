#ifndef CBASE_DOMINATORS_H_
#define CBASE_DOMINATORS_H_
#include "src/util.h"

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
class Dominators {
 public:
  typedef std::vector<ControlFlow*> DominatorSet;

  // This function is used to construct the dominator information and
  // it can be called multiple times whenever the graph changes
  void Build( const Graph& );
 public:
  // Get the dominator set for a specific node. The dominator set
  // returned by this function will contain the node itself
  const DominatorSet& GetDominatorSet( ControlFlow* ) const;

  // Get common dominator set for 2 nodes
  DominatorSet GetCommonDominatorSet ( ControlFlow* , ControlFlow* ) const;

  // Check whether the first node is dominator of the second node
  bool IsDominator( ControlFlow* , ControlFlow* ) const;

 public:
  // Print the dominator information into dot format for debugging purpose
  std::string PrintToDotFormat() const;

 private: // Helper to mainpulate the std::vector
  void AddSet  ( DominatorSet* , ControlFlow* ) const;
  void IntersectSet( DominatorSet* , const DominatorSet& ) const;
  void IntersectSet( DominatorSet* , const DominatorSet& , const DominatorSet& ) const;
  DominatorSet* GetDomSet( const Graph& , const std::vector<ControlFlow*>& , ControlFlow* );

 private: // Helper to generate dot format representation of dominators
  std::string GetNodeName( ControlFlow* node ) const;

 private:
  std::map<ControlFlow*,DominatorSet> dominators_;
};

} // namespace hir
} // namespace cbase
} // namespace lavascript

#endif // CBASE_DOMINATORS_H_
