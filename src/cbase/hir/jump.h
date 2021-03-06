#ifndef CBASE_HIR_JUMP_H_
#define CBASE_HIR_JUMP_H_
#include "control-flow.h"

namespace lavascript {
namespace cbase      {
namespace hir        {

LAVA_CBASE_HIR_DEFINE(Tag=JUMP;Name="jump";Leaf=NoLeaf,
    Jump,public ControlFlow) {
 public:
  inline static Jump* New( Graph* , const std::uint32_t* , ControlFlow* );
  // which target this jump jumps to
  ControlFlow* target() const { return target_; }
  inline bool TrySetTarget( const std::uint32_t* , ControlFlow* );

  Jump( Graph* graph , std::uint32_t id , ControlFlow* region , const std::uint32_t* bytecode_bc ):
    ControlFlow(HIR_JUMP,id,graph,region),
    target_(NULL),
    bytecode_pc_(bytecode_bc)
  {}

 private:
  ControlFlow* target_; // where this Jump node jumps to
  const std::uint32_t* bytecode_pc_;
  LAVA_DISALLOW_COPY_AND_ASSIGN(Jump)
};

LAVA_CBASE_HIR_DEFINE(HIR_INTERNAL,JumpWithValue,public ControlFlow) {
 public:
  Expr* value() const { return operand_list()->First(); }
  JumpWithValue( IRType type , Graph* graph , std::uint32_t id , Expr* value ,
                                                                 ControlFlow* region ):
    ControlFlow(type,id,graph,region)
  {
    AddOperand(value);
  }

 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(JumpWithValue)
};

LAVA_CBASE_HIR_DEFINE(Tag=RETURN;Name="return";Leaf=NoLeaf,
    Return,public JumpWithValue) {
 public:
  inline static Return* New( Graph* , Expr* , ControlFlow* );
  Return( Graph* graph , std::uint32_t id , Expr* value , ControlFlow* region ):
    JumpWithValue(HIR_RETURN,graph,id,value,region) {}
 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(Return)
};

// JumpValue node is used to generate a unconditional jump and also carry out a value.
// this node is used during inline frame since we cannot generate return in inline
// frame though mostly it is just a Return but don't return from the current function
// frame.
LAVA_CBASE_HIR_DEFINE(Tag=JUMP_VALUE;Name="jump_value";Leaf=NoLeaf,
    JumpValue,public JumpWithValue) {
 public:
  inline static JumpValue* New( Graph* , Expr* , ControlFlow* );

  JumpValue( Graph* graph , std::uint32_t id , Expr* value , ControlFlow* region ):
    JumpWithValue(HIR_JUMP_VALUE,graph,id,value,region) {}
 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(JumpValue)
};

} // namespace hir
} // namespace cbase
} // namespace lavascript

#endif // CBASE_HIR_JUMP_H_
