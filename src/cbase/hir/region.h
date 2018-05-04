#ifndef CBASE_HIR_REGION_H_
#define CBASE_HIR_REGION_H_
#include "control-flow.h"

namespace lavascript {
namespace cbase      {
namespace hir        {

class Region : public ControlFlow {
 public:
  inline static Region* New( Graph* );
  inline static Region* New( Graph* , ControlFlow* );
  Region( Graph* graph , std::uint32_t id ): ControlFlow(HIR_REGION,id,graph) {}
 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(Region)
};

// Fail node represents abnormal way to abort the execution. The most common reason
// is because we failed at type guard or obviouse code bug.
class Fail : public ControlFlow {
 public:
  inline static Fail* New( Graph* );
  Fail( Graph* graph , std::uint32_t id ): ControlFlow(HIR_FAIL,id,graph) {}
 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(Fail)
};

class Success : public ControlFlow {
 public:
  inline static Success* New( Graph* );

  Expr* return_value() const { return operand_list()->First(); }

  Success( Graph* graph , std::uint32_t id ):
    ControlFlow  (HIR_SUCCESS,id,graph)
  {}

 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(Success)
};

// Special node of the graph
class Start : public ControlFlow {
 public:
  inline static Start* New( Graph* );
  Start( Graph* graph , std::uint32_t id ): ControlFlow(HIR_START,id,graph) {}
 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(Start)
};

class End : public ControlFlow {
 public:
  inline static End* New( Graph* , Success* , Fail* );
  Success* success() const { return backward_edge()->First()->AsSuccess(); }
  Fail*    fail()    const { return backward_edge()->Last ()->AsFail   (); }
  End( Graph* graph , std::uint32_t id , Success* s , Fail* f ):
    ControlFlow(HIR_END,id,graph)
  {
    AddBackwardEdge(s);
    AddBackwardEdge(f);
  }
 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(End)
};

class OSRStart : public ControlFlow {
 public:
  inline static OSRStart* New( Graph* );

  OSRStart( Graph* graph  , std::uint32_t id ):
    ControlFlow(HIR_OSR_START,id,graph)
  {}

 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(OSRStart)
};

class OSREnd : public ControlFlow {
 public:
  inline static OSREnd* New( Graph* , Success* succ , Fail* f );

  Success* success() const { return backward_edge()->First()->AsSuccess(); }
  Fail*    fail   () const { return backward_edge()->Last()->AsFail(); }

  OSREnd( Graph* graph , std::uint32_t id , Success* succ , Fail* f ):
    ControlFlow(HIR_OSR_END,id,graph)
  {
    AddBackwardEdge(succ);
    AddBackwardEdge(f);
  }

 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(OSREnd)
};

class InlineStart : public ControlFlow {
 public:
  inline static InlineStart* New( Graph* , ControlFlow* );

  InlineStart( Graph* graph , std::uint32_t id , ControlFlow* region ):
    ControlFlow (HIR_INLINE_START,id,graph,region)
  {}

 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(InlineStart)
};

class InlineEnd : public ControlFlow {
 public:
  inline static InlineEnd* New( Graph* , ControlFlow* );

  InlineEnd( Graph* graph , std::uint32_t id , ControlFlow* region ):
    ControlFlow (HIR_INLINE_END,id,graph,region)
  {}

 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(InlineEnd)
};

} // namespace hir
} // namespace cbase
} // namespace lavascript

#endif // CBASE_HIR_REGION_H_
