#ifndef CBASE_INLINER_H_
#define CBASE_INLINER_H_
#include "src/objects.h"
#include "hir.h"

namespace lavascript {
namespace cbase      {
namespace hir        {

// Interface for checking whether we should inline a certain function or not
class Inliner {
 public:
  // Check whether we should inline this function with given prototype object and with
  // the nested depth. The nested depth can be checked by GraphBuilder object.
  virtual bool ShouldInline( std::size_t depth , const Handle<Prototype>& proto ) = 0;

  virtual ~Inliner() {}
};

// Static inliner performs basic inline heuristic guess and is used to for most
// general purpose. Other types of inliner can be implemented and hook into the graph
// builder to perform inline base on different heuristic method.
class StaticInliner : public Inliner {
 public:
  static const std::size_t kScaleFactor = 10;               // we assume each bytecode turns into 10 instructions
  static const std::size_t kMaxInlineBytecodeTotal = 10000; // maximum allowed inline bytecode in total
  static const std::size_t kMaxInlineBytecode = 200;        // maximum inlined bytecode allowed per function
  static const std::size_t kMaxInlineDepth = 32;            // maximum inlined bytecode depth

  // TODO:: make threshold configurable instead of using hard code one
  StaticInliner() :
    total_inlined_bytecode_      (),
    max_inline_bytecode_per_func_(kMaxInlineBytecode),
    max_inline_depth_            (kMaxInlineDepth),
    max_inline_bytecode_total_   (kMaxInlineBytecodeTotal)
  {}

  // internal states, otherwise return false and leave Inline object unchanged.
  virtual bool ShouldInline( std::size_t depth , const Handle<Prototype>& proto );

  virtual ~StaticInliner() {}
 private:
  std::size_t total_inlined_bytecode_;               // total number of inlined bytecode
  std::size_t max_inline_bytecode_per_func_;         // maximum allowed bytecode inlined for a function
  std::size_t max_inline_depth_;                     // maximum allowed inline depth
  std::size_t max_inline_bytecode_total_;            // maximum allowed bytecode inline in total
};


} // namespace hir
} // namespace cbase
} // namespace lavascript

#endif // CBASE_INLINER_H_
