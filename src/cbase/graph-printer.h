#ifndef CBASE_GRAPH_PRINTER_H_
#define CBASE_GRAPH_PRINTER_H_
#include "src/all-static.h"

#include <string>

namespace lavascript {
namespace cbase      {
namespace hir        {
class Graph;

class GraphPrinter : public AllStatic {
 public:
  struct Option {
    bool checkpoint;
    Option() : checkpoint(false) {}
  };

  // Function to print the graph into dot format for visualization purpose
  static std::string Print( const Graph& , const Option& opt = Option() );
};

} // namespace hir
} // namespace cbase
} // namespace lavascript

#endif // CBASE_GRAPH_PRINTER_H_
