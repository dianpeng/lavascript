#ifndef CBASE_HIR_CALL_H_
#define CBASE_HIR_CALL_H_
#include "expr.h"
#include "src/interpreter/intrinsic-call.h"

namespace lavascript {
namespace cbase      {
namespace hir        {

class Call : public Expr {
 public:
  inline static Call* New( Graph* graph , Expr* , std::uint8_t , std::uint8_t );
  Call( Graph* graph , std::uint32_t id , Expr* obj , std::uint8_t base , std::uint8_t narg ):
    Expr  (HIR_CALL,id,graph),
    base_ (base),
    narg_ (narg)
  {
    AddOperand(obj);
  }
 private:
  std::uint8_t base_;
  std::uint8_t narg_;
  LAVA_DISALLOW_COPY_AND_ASSIGN(Call)
};

// intrinsic function call
class ICall : public Expr {
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
  // Global value numbering
  virtual std::uint64_t GVNHash() const;
  virtual bool Equal( const Expr* ) const;

  ICall( Graph* graph , std::uint32_t id , interpreter::IntrinsicCall ic , bool tail ):
    Expr(HIR_ICALL,id,graph),
    ic_ (ic),
    tail_call_(tail)
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
