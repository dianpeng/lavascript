#include "fold-intrinsic.h"
#include "hir.h"

namespace lavascript {
namespace cbase      {
namespace hir        {
namespace {

using namespace ::lavascript::interpreter;

inline bool AsUInt8( Expr* node , std::uint8_t* value ) {
  if(node->IsFloat64()) {
    // we don't care about the shifting overflow, the underly ISA
    // only allows a 8bit register serve as how many bits shifted.
    *value = static_cast<std::uint8_t>(node->AsFloat64()->value());
    return true;
  }
  return false;
}

inline bool AsUInt32( Expr* node , std::uint32_t* value ) {
  if(node->IsFloat64()) {
    *value = static_cast<std::uint32_t>(node->AsFloat64()->value());
    return true;
  }
  return false;
}

inline bool AsReal  ( Expr* node , double* real ) {
  if(node->IsFloat64()) {
    *real = node->AsFloat64()->value();
    return true;
  }
  return false;
}

Expr* FoldICall( Graph* graph , ICall* node ) {
  switch(node->ic()) {
    case INTRINSIC_CALL_MAX:
      {
        double a1 ,a2;
        if(AsReal(node->operand_list()->Index(0),&a1) &&
           AsReal(node->operand_list()->Index(1),&a2)) {
          return (Float64::New(graph,std::max(a1,a2),node->ir_info()));
        }
      }
      break;
    case INTRINSIC_CALL_MIN:
      {
        double a1,a2;
        if(AsReal(node->operand_list()->Index(0),&a1) &&
           AsReal(node->operand_list()->Index(1),&a2)) {
          return (Float64::New(graph,std::min(a1,a2),node->ir_info()));
        }
      }
      break;
    case INTRINSIC_CALL_SQRT:
      {
        double a1;
        if(AsReal(node->operand_list()->Index(0),&a1)) {
          return (Float64::New(graph,std::sqrt(a1),node->ir_info()));
        }
      }
      break;
    case INTRINSIC_CALL_SIN:
      {
        double a1;
        if(AsReal(node->operand_list()->Index(0),&a1)) {
          return (Float64::New(graph,std::sin(a1),node->ir_info()));
        }
      }
      break;
    case INTRINSIC_CALL_COS:
      {
        double a1;
        if(AsReal(node->operand_list()->Index(0),&a1)) {
          return (Float64::New(graph,std::cos(a1),node->ir_info()));
        }
      }
      break;
    case INTRINSIC_CALL_TAN:
      {
        double a1;
        if(AsReal(node->operand_list()->Index(0),&a1)) {
          return (Float64::New(graph,std::tan(a1),node->ir_info()));
        }
      }
      break;
    case INTRINSIC_CALL_ABS:
      {
        double a1;
        if(AsReal(node->operand_list()->Index(0),&a1)) {
          return (Float64::New(graph,std::abs(a1),node->ir_info()));
        }
      }
      break;
    case INTRINSIC_CALL_CEIL:
      {
        double a1;
        if(AsReal(node->operand_list()->Index(0),&a1)) {
          return (Float64::New(graph,std::ceil(a1),node->ir_info()));
        }
      }
      break;
    case INTRINSIC_CALL_FLOOR:
      {
        double a1;
        if(AsReal(node->operand_list()->Index(0),&a1)) {
          return (Float64::New(graph,std::floor(a1),node->ir_info()));
        }
      }
      break;
    case INTRINSIC_CALL_LSHIFT:
      {
        std::uint32_t a1;
        std::uint8_t  a2;
        if(AsUInt32(node->operand_list()->Index(0),&a1) &&
           AsUInt8(node->operand_list()->Index(1),&a2)) {
          return (Float64::New(graph,static_cast<double>(a1 << a2),node->ir_info()));
        }
      }
      break;
    case INTRINSIC_CALL_RSHIFT:
      {
        std::uint32_t a1;
        std::uint8_t  a2;
        if(AsUInt32(node->operand_list()->Index(0),&a1) &&
           AsUInt8 (node->operand_list()->Index(1),&a2)) {
          return (Float64::New(graph,static_cast<double>(a1 >> a2), node->ir_info()));
        }
      }
      break;
    case INTRINSIC_CALL_LRO:
      {
        std::uint32_t a1;
        std::uint8_t  a2;
        if(AsUInt32(node->operand_list()->Index(0),&a1) &&
           AsUInt8 (node->operand_list()->Index(1),&a2)) {
          return (Float64::New(graph,static_cast<double>(bits::BRol(a1,a2)),node->ir_info()));
        }
      }
      break;
    case INTRINSIC_CALL_RRO:
      {
        std::uint32_t a1;
        std::uint8_t  a2;
        if(AsUInt32(node->operand_list()->Index(0),&a1) &&
           AsUInt8 (node->operand_list()->Index(1),&a2)) {
          return (Float64::New(graph,static_cast<double>(bits::BRor(a1,a2)),node->ir_info()));
        }
      }
      break;
    case INTRINSIC_CALL_BAND:
      {
        std::uint32_t a1;
        std::uint32_t a2;
        if(AsUInt32(node->operand_list()->Index(0),&a1) &&
           AsUInt32(node->operand_list()->Index(1),&a2)) {
          return (Float64::New(graph,static_cast<double>((a1 & a2)),node->ir_info()));
        }
      }
      break;
    case INTRINSIC_CALL_BOR:
      {
        std::uint32_t a1;
        std::uint32_t a2;
        if(AsUInt32(node->operand_list()->Index(0),&a1) &&
           AsUInt32(node->operand_list()->Index(1),&a2)) {
          return (Float64::New(graph,static_cast<double>(a1 | a2),node->ir_info()));
        }
      }
      break;
    case INTRINSIC_CALL_BXOR:
      {
        std::uint32_t a1;
        std::uint32_t a2;
        if(AsUInt32(node->operand_list()->Index(0),&a1) &&
           AsUInt32(node->operand_list()->Index(1),&a2)) {
          return (Float64::New(graph,static_cast<double>(a1 ^ a2),node->ir_info()));
        }
      }
      break;
    case INTRINSIC_CALL_INT:
      {
        auto n1 = node->operand_list()->Index(0);
        switch(n1->type()) {
          case IRTYPE_FLOAT64:
            return Float64::New(graph, CastRealAndStoreAsReal<std::int32_t>(n1->AsFloat64()->value()), n1->ir_info());
          case IRTYPE_LONG_STRING: case IRTYPE_SMALL_STRING:
            {
              double dv;
              if(LexicalCast(n1->AsZoneString().data(),&dv)) {
                return Float64::New(graph,
                                    CastRealAndStoreAsReal<std::int32_t>(dv),
                                    n1->ir_info());
              }
            }
            break;
          case IRTYPE_BOOLEAN:
            return Float64::New(graph,n1->AsBoolean()->value() ? 1.0 : 0.0,n1->ir_info());
          default:
            break;
        }
      }
      break;
    case INTRINSIC_CALL_REAL:
      {
        auto n1 = node->operand_list()->Index(0);
        switch(n1->type()) {
          case IRTYPE_FLOAT64:
            return Float64::New(graph,n1->AsFloat64()->value(),n1->ir_info());
          case IRTYPE_LONG_STRING: case IRTYPE_SMALL_STRING:
            {
              double val;
              if(LexicalCast(n1->AsZoneString().data(),&val)) {
                return Float64::New(graph,val,n1->ir_info());
              }
            }
            break;
          case IRTYPE_BOOLEAN:
            return Float64::New(graph,n1->AsBoolean()->value() ? 1.0 : 0.0 , n1->ir_info());
          default:
            break;
        }
      }
      break;
    case INTRINSIC_CALL_STRING:
      {
        auto n1 = node->operand_list()->Index(0);
        switch(n1->type()) {
          case IRTYPE_FLOAT64:
            return NewStringFromReal( graph , n1->AsFloat64()->value() , n1->ir_info());
          case IRTYPE_LONG_STRING:
            return LString::New(graph,n1->AsLString()->value(),n1->ir_info());
          case IRTYPE_SMALL_STRING:
            return SString::New(graph,n1->AsSString()->value(),n1->ir_info());
          case IRTYPE_BOOLEAN:
            return NewStringFromBoolean(graph,n1->AsBoolean()->value(),n1->ir_info());
          default:
            break;
        }
      }
      break;
    case INTRINSIC_CALL_PUSH:
      {
        auto n1 = node->operand_list()->Index(0);
        if(n1->IsIRList()) {
          auto new_list = IRList::Clone(graph,*n1->AsIRList());
          new_list->Add( node->operand_list()->Index(1) );
          return new_list;
        }
      }
      break;
    case INTRINSIC_CALL_POP:
      {
        auto n1 = node->operand_list()->Index(0);
        if(n1->IsIRList()) {
          return IRList::CloneExceptLastOne(graph,*n1->AsIRList());
        }
      }
      break;
    default:
      break;
  }
  return NULL; // nothing can be done
}

} // namespace

Expr* FoldIntrinsicCall( Graph* graph , ICall* icall ) {
  return FoldICall(graph,icall);
}

} // namespace hir
} // namespace cbase
} // namespace lavascript
