#ifndef CBASE_HIR_CALL_H_
#define CBASE_HIR_CALL_H_
#include "effect.h"
#include "src/interpreter/intrinsic-call.h"

namespace lavascript {
namespace cbase      {
namespace hir        {

// A call node. Calls into an external functions.It is type of a WriteEffect and also it will
// definitely change the effect chain in the scope
class Call : public WriteEffect {
 public:
  inline static Call* New( Graph* graph , Expr* , std::uint8_t , std::uint8_t , bool );
  Call( Graph* graph , std::uint32_t id , Expr* obj , std::uint8_t base , std::uint8_t narg , bool tcall ):
    WriteEffect (HIR_CALL,id,graph),
    base_       (base),
    narg_       (narg),
    tail_call_  (tcall)
  {
    AddOperand(obj);
  }
 private:
  std::uint8_t base_;
  std::uint8_t narg_;
  bool         tail_call_;
  LAVA_DISALLOW_COPY_AND_ASSIGN(Call)
};

// intrinsic function call node. this node may never be used due to the fact we probably
// will lower all the intrinsic call directly into the graph instead of generating a call
// node. it will only make sense when the graph is super blowed so we may just call an
// external intrinsic call function
class ICall : public WriteEffect {
 public:
  inline static ICall* New( Graph* , interpreter::IntrinsicCall , bool tail );
  // add argument back to the ICall's argument list
  void AddArgument( Expr* expr ) {
    lava_debug(NORMAL,lava_verify(
          operand_list()->size() < interpreter::GetIntrinsicCallArgumentSize(ic_)););
    AddOperand(expr);
  }
  Expr* GetArgument( std::uint8_t arg ) {
    lava_debug(NORMAL,lava_verify(arg < operand_list()->size()););
    return operand_list()->Index(arg);
  }
  // intrinsic call method index
  interpreter::IntrinsicCall ic() const { return ic_; }
  // whether this call is a tail call
  bool tail_call() const { return tail_call_; }

  ICall( Graph* graph , std::uint32_t id , interpreter::IntrinsicCall ic , bool tail ):
    WriteEffect (HIR_ICALL,id,graph),
    ic_         (ic),
    tail_call_  (tail)
  {}
 private:
  interpreter::IntrinsicCall ic_;
  bool tail_call_;
  LAVA_DISALLOW_COPY_AND_ASSIGN(ICall)
};

} // namespace hir
} // namespace cbase
} // namespace lavascript

#endif // CBASE_HIR_CALL_H_
