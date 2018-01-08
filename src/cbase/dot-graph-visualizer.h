#ifndef CBASE_DOT_GRAPH_VISUALIZER_H_
#define CBASE_DOT_GRAPH_VISUALIZER_H_
#include "src/cbase/ir.h"
#include "src/stl-helper.h"
#include <string>
#include <sstream>

namespace lavascript {
namespace cbase {
namespace ir {

// Visualize Graph object by using DOT language. Tools like Graphviz can
// provide actual visualization funcationality
class DotGraphVisualizer {
 public:
  DotGraphVisualizer(): graph_(NULL), existed_(NULL), output_(NULL) {}

  // Visiualize the graph into DOT representation and return the string
  std::string Visualize( const Graph& );
 private:
  void RenderControlFlow( const std::string& , ControlFlow* );
  void RenderExpr       ( const std::string& , Expr* );
  void RenderEdge       ( ControlFlow* , ControlFlow* );

  std::stringstream& Indent( int level );
  std::string GetNodeName( Node* );
 private:
  const Graph* graph_;
  DynamicBitSet* existed_;
  std::stringstream* output_;
};

} // namespace ir
} // namespace cbase
} // namespace lavascript

#endif // CBASE_DOT_GRAPH_VISUALIZRE_H_
