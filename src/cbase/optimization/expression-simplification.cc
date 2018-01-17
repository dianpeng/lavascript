#include "expression-simplification.h"
#include "src/cbase/hir-visitor.h"

#include <cmath>

namespace lavascript {
namespace cbase {
namespace hir {

namespace {

Expr* Fold( Graph* graph , Unary::Operator op , Expr* expr ,
                                                const std::function<IRInfo*()>& irinfo ) {
  if(op == Unary::MINUS && expr->IsFloat64()) {
    return Float64::New(graph,-expr->AsFloat64()->value(),irinfo());
  } else if(op == Unary::NOT) {
    switch(expr->type()) {
      case IRTYPE_FLOAT64:
      case IRTYPE_SMALL_STRING:
      case IRTYPE_LONG_STRING:
      case IRTYPE_LIST:
      case IRTYPE_OBJECT:
        return Boolean::New(graph,false,irinfo());
      case IRTYPE_BOOLEAN:
        return Boolean::New(graph,!expr->AsBoolean()->value(),irinfo());
      case IRTYPE_NIL:
        return Boolean::New(graph,true,irinfo());
      default:
        break;
    }
  }
  return NULL;
}

Expr* Fold( Graph* graph , Binary::Operator op , Expr* lhs ,
                                                 Expr* rhs ,
                                                 const std::function<IRInfo* ()>& irinfo ) {
  if(lhs->IsFloat64() && rhs->IsFloat64()) {
    auto lval = lhs->AsFloat64()->value();
    auto rval = rhs->AsFloat64()->value();

    switch(op) {
      case Binary::ADD: return Float64::New(graph,lval+rval,irinfo());
      case Binary::SUB: return Float64::New(graph,lval-rval,irinfo());
      case Binary::MUL: return Float64::New(graph,lval*rval,irinfo());
      case Binary::DIV: return Float64::New(graph,lval/rval,irinfo());
      case Binary::MOD:
        {
          auto lint = static_cast<std::int64_t>(lval);
          auto rint = static_cast<std::int64_t>(rval);
          return rint == 0 ? NULL : Float64::New(graph,lint % rint, irinfo());
        }
      case Binary::POW: return Float64::New(graph,std::pow(lval,rval),irinfo());
      case Binary::LT:  return Boolean::New(graph,lval <  rval,irinfo());
      case Binary::LE:  return Boolean::New(graph,lval <= rval,irinfo());
      case Binary::GT:  return Boolean::New(graph,lval >  rval,irinfo());
      case Binary::GE:  return Boolean::New(graph,lval >= rval,irinfo());
      case Binary::EQ:  return Boolean::New(graph,lval == rval,irinfo());
      case Binary::NE:  return Boolean::New(graph,lval != rval,irinfo());
      case Binary::AND: return Float64::New(graph,rval,irinfo());
      case Binary::OR:  return Float64::New(graph,lval,irinfo());
      default: lava_die(); return NULL;
    }
  } else if(lhs->IsString() && rhs->IsString()) {
    const zone::String* lstr = lhs->IsSString() ? lhs->AsSString()->value() :
                                                  lhs->AsLString()->value() ;

    const zone::String* rstr = rhs->IsSString() ? rhs->AsSString()->value() :
                                                  rhs->AsLString()->value() ;

    switch(op) {
      case Binary::LT: return Boolean::New(graph,*lstr <  *rstr, irinfo());
      case Binary::LE: return Boolean::New(graph,*lstr <= *rstr, irinfo());
      case Binary::GT: return Boolean::New(graph,*lstr >  *rstr, irinfo());
      case Binary::GE: return Boolean::New(graph,*lstr >= *rstr, irinfo());
      case Binary::EQ: return Boolean::New(graph,*lstr == *rstr, irinfo());
      case Binary::NE: return Boolean::New(graph,*lstr != *rstr, irinfo());
      default: return NULL;
    }
  } else if(lhs->IsNil() || rhs->IsNil()) {
    if(op == Binary::NE) {
      return Boolean::New(graph,lhs->IsNil() ^  rhs->IsNil(),irinfo());
    } else if(op == Binary::EQ) {
      return Boolean::New(graph,lhs->IsNil() && rhs->IsNil(),irinfo());
    } else {
      return NULL;
    }
  }

  return NULL;
}

Expr* Fold( Graph* graph , Expr* cond , Expr* lhs , Expr* rhs ,
                                                    const std::function<IRInfo*()>& irinfo) {
  switch(cond->type()) {
    case IRTYPE_FLOAT64:
    case IRTYPE_LONG_STRING:
    case IRTYPE_SMALL_STRING:
    case IRTYPE_LIST:
    case IRTYPE_OBJECT:
      return lhs;
    case IRTYPE_NIL:
      return rhs;
    case IRTYPE_BOOLEAN:
      return (cond->AsBoolean()->value() ? lhs : rhs);
    default:
      break;
  }
  return NULL;
}

// -----------------------------------------------------------------------
// Expression visitor for doing expression simplification
//
class Simplifier : public ExprVisitor {
 public:
  // normal arithmetic operations
  virtual bool VisitUnary ( Unary* );
  virtual bool VisitBinary( Binary* );
  virtual bool VisitTernary( Ternary* );

  // index get and index set
  virtual bool VisitIGet( IGet* );
  virtual bool VisitISet( ISet* );

  virtual bool VisitPGet( PGet* );
  virtual bool VisitPSet( PSet* );

  // intrinsic function call
  virtual bool VisitICall( ICall* );

 private:

  void FoldSet( Expr* , Expr* , Expr* , Expr* );
  void FoldGet( Expr* , Expr* , Expr* );

 private:
  Graph* graph_;
};

bool Simplifier::VisitUnary ( Unary* node ) {
  auto opr = node->operand();
  auto result = Fold(graph_,node->op(),opr,[node](){ return node->ir_info(); });
  if(result) node->Replace(result);
  return true;
}

bool Simplifier::VisitBinary( Binary* node ) {
  auto lhs = node->lhs();
  auto rhs = node->rhs();
  auto result = Fold(graph_,node->op(),lhs,rhs,[node]() { return node->ir_info(); });
  if(result) node->Replace(result);
  return true;
}

bool Simplifier::VisitTernary( Ternary* node ) {
  auto cond = node->condition();
  auto lhs  = node->lhs();
  auto rhs  = node->rhs();
  auto result = Fold(graph_,cond,lhs,rhs,[node]() { return node->ir_info(); });
  if(result) node->Replace(result);
  return true;
}

// -------------------------------------------------
//
// index get and index set
//
// -------------------------------------------------
void Simplifier::FoldGet( Expr* obj, Expr* key, Expr* node ) {
  if(key->IsFloat64()) {
    auto index_value = static_cast<std::uint32_t>(key->AsFloat64()->value());
    if(obj->IsIRList()) {
      auto list = obj->AsIRList();
      if(index_value < list->Size()) {
        auto value = obj->operand_list()->Index(index_value);
        node->Replace(value);
      }
    }
  } else if(key->IsString()) {
    auto string = key->ToZoneString();

    if(obj->IsIRObject()) {
      auto object = obj->AsIRObject();

      // try to find where the value is in the IRObject and then we can do a fold
      auto itr    = object->operand_list()->Find(
          [string]( const Expr::OperandList::ConstForwardIterator& itr ) {
            auto kv = itr.value();
            if(kv->key()->IsString() && (*kv->key()->ToZoneString() == *string))
              return true;
            return false;
          });

      if(itr.HasNext()) {
        node->Replace(itr.value());
      }
    }
  }
}

void Simplifier::FoldSet( Expr* obj , Expr* key , Expr* value, Expr* node ) {

  if(key->IsFloat64()) {
    std::uint32_t idx = static_cast<std::uint32_t>(key->AsFloat64()->value());
    if(obj->IsIRList()) {
      auto list = obj->AsIRList();
      if(idx < list->Size()) {
        auto new_list = list->Clone();
        new_list->operand_list()->Set(idx,value);
        node->Replace(new_list);
      }
    }
  } else if(key->IsString()) {
    auto string = key->ToZoneString();
    if(obj->IsIRObject()) {
      auto object = obj->AsObject();
      auto slot = object->operand_list()->Find(
          [string]( const Expr::OperandList::ConstForwardIterator& itr ) {
            auto kv = itr.value();
            if( kv->key()->IsString() && (*kv->key()->ToZoneString() == *string))
              return true;
            return false;
          }
      );
      auto target = slot.value()->AsIRObjectKV(); // should never fail

      if(slot.HasNext()) {

        // create a new node except the one that needs to be modified to the patched value
        auto new_object = IRObject::New(graph_,object->Size(),object->ir_info());
        for( auto itr(object->operand_list()->GetForwardIterator());
             itr.HasNext(); itr.Move() ) {
          auto ele = itr.value();
          if(ele == target) {
            // except this one needs to be modified
            auto new_kv = IRObjectKV::New(graph_,target->key(),value,target->ir_info());
            new_object->AddOperand(new_kb);
          } else {
            new_object->AddOperand(itr.value());
          }
        }

        node->Replace(new_object);
      }
    }
  }
}

bool Simplifier::VisitIGet( IGet* node ) {
  FoldGet(node->object(),node->index(),node);
  return true;
}

bool Simplifier::VisitISet( ISet* node ) {
  FoldSet(node->object(),node->index(),node->value(),node);
  return true;
}

bool Simplifier::VisitPGet( PGet* node ) {
  FoldGet(node->object(),node->index(),node);
  return true;
}

bool Simplifier::VisitPSet( PSet* node ) {
  FoldSet(node->object(),node->key(),node->value(),node);
  return true;
}

bool Simplifier::VisitICall( ICall* node ) {
}

} // namespace

Expr* SimplifyUnary  ( Graph* graph , Unary::Operator op , Expr* expr ,
                                                           const std::function<IRInfo*()>& irinfo ) {
  return Fold(graph,op,expr,irinfo);
}

Expr* SimplifyBinary ( Graph* graph , Binary::Operator op , Expr* lhs ,
                                                            Expr* rhs ,
                                                            const std::function<IRInfo* ()>& irinfo ) {
  return Fold(graph,op,lhs,rhs,irinfo);
}

Expr* SimplifyTernary( Graph* graph , Expr* cond , Expr* lhs , Expr* rhs ,
                                                               const std::function<IRInfo*()>& irinfo) {
  return Fold(graph,cond,lhs,rhs,irinfo);
}

} // namespace hir
} // namespace cbase
} // namespace lavascript
