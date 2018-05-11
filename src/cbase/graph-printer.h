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
    enum { EFFECT_CHAIN , OPERAND_CHAIN , ALL_CHAIN };
    int  option;
    bool checkpoint;
    Option()                    : option(ALL_CHAIN), checkpoint(false) {}
    Option( int opt , bool cp ) : option(opt)      , checkpoint(cp)    {}
   public:
    bool ShouldRenderOperand() const {
      return option == ALL_CHAIN || option == OPERAND_CHAIN;
    }
    bool ShouldRenderEffect () const {
      return option == ALL_CHAIN || option == EFFECT_CHAIN;
    }
  };

  // Function to print the graph into dot format for visualization purpose
  static std::string Print( const Graph& , const Option& opt = Option() );
};

} // namespace hir
} // namespace cbase
} // namespace lavascript

#endif // CBASE_GRAPH_PRINTER_H_
