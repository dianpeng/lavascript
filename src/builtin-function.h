#ifndef BUILTIN_FUNCTION_H_
#define BUILTIN_FUNCTION_H_

#include "src/bits.h"
#include "src/util.h"
#include "src/objects.h"
#include "src/gc.h"
#include "src/context.h"
#include "src/interpreter/intrinsic-call.h"

#include <cmath>

// inlined builtin functions implementation
namespace lavascript {
namespace builtin {

inline bool BuiltinMin( Context* ctx , const Value& lhs , const Value& rhs ,
                                                          Value* output ,
                                                          std::string* error ) {
  (void)ctx;

  if(lhs.IsReal() && rhs.IsReal()) {
    output->SetReal( std::min(lhs.GetReal(),rhs.GetReal()) );
    return true;
  }

  using namespace ::lavascript::interpreter;
  Format(error,GetIntrinsicCallErrorMessage(INTRINSIC_CALL_MIN));
  return false;
}

inline bool BuiltinMax( Context* ctx , const Value& lhs , const Value& rhs ,
                                                          Value* output ,
                                                          std::string* error ) {
  (void)ctx;

  if(lhs.IsReal() && rhs.IsReal()) {
    output->SetReal( std::max(lhs.GetReal(),rhs.GetReal()) );
    return true;
  }

  using namespace ::lavascript::interpreter;
  Format(error,GetIntrinsicCallErrorMessage(INTRINSIC_CALL_MAX));
  return false;
}

inline bool BuiltinSqrt( Context* ctx , const Value& operand , Value* output ,
                                                               std::string* error ) {
  (void)ctx;

  if(operand.IsReal()) {
    output->SetReal( std::sqrt(operand.GetReal()) );
    return true;
  }

  using namespace ::lavascript::interpreter;
  Format(error,GetIntrinsicCallErrorMessage(INTRINSIC_CALL_SQRT));
  return false;
}

inline bool BuiltinSin( Context* ctx, const Value& operand , Value* output ,
                                                             std::string* error ) {
  (void)ctx;

  if(operand.IsReal()) {
    output->SetReal( std::sin(operand.GetReal()) );
    return true;
  }

  using namespace ::lavascript::interpreter;
  Format(error,GetIntrinsicCallErrorMessage(INTRINSIC_CALL_SIN));
  return false;
}

inline bool BuiltinCos( Context* ctx, const Value& operand , Value* output ,
                                                             std::string* error ) {
  (void)ctx;

  if(operand.IsReal()) {
    output->SetReal( std::cos(operand.GetReal()) );
    return true;
  }

  using namespace ::lavascript::interpreter;
  Format(error,GetIntrinsicCallErrorMessage(INTRINSIC_CALL_COS));
  return false;
}

inline bool BuiltinTan( Context* ctx, const Value& operand , Value* output ,
                                                             std::string* error ) {
  (void)ctx;

  if(operand.IsReal()) {
    output->SetReal( std::tan(operand.GetReal()) );
    return true;
  }

  using namespace ::lavascript::interpreter;
  Format(error,GetIntrinsicCallErrorMessage(INTRINSIC_CALL_TAN));
  return false;
}

inline bool BuiltinAbs( Context* ctx, const Value& operand , Value* output ,
                                                             std::string* error ) {
  (void)ctx;

  if(operand.IsReal()) {
    output->SetReal( std::abs(operand.GetReal()) );
    return true;
  }

  using namespace ::lavascript::interpreter;
  Format(error,GetIntrinsicCallErrorMessage(INTRINSIC_CALL_ABS));
  return false;
}

inline bool BuiltinCeil( Context* ctx, const Value& operand , Value* output ,
                                                              std::string* error ) {
  (void)ctx;

  if(operand.IsReal()) {
    output->SetReal( std::ceil(operand.GetReal()) );
    return true;
  }

  using namespace ::lavascript::interpreter;
  Format(error,GetIntrinsicCallErrorMessage(INTRINSIC_CALL_CEIL));
  return false;
}

inline bool BuiltinFloor( Context* ctx, const Value& operand , Value* output ,
                                                               std::string* error ) {
  (void)ctx;

  if(operand.IsReal()) {
    output->SetReal( std::floor(operand.GetReal()) );
    return true;
  }

  using namespace ::lavascript::interpreter;
  Format(error,GetIntrinsicCallErrorMessage(INTRINSIC_CALL_FLOOR));
  return false;
}

inline bool BuiltinRShift( Context* ctx , const Value& lhs , const Value& rhs ,
                                                             Value* output ,
                                                             std::string* error ) {
  (void)ctx;

  if(lhs.IsReal() && rhs.IsReal()) {
    std::uint32_t lhs_value = static_cast<std::uint32_t>(lhs.GetReal());
    // don't do clamp on rhs_value since assembly code doesn't do clamp
    std::uint8_t rhs_value = static_cast<std::uint8_t>(rhs.GetReal());

    output->SetReal( static_cast<double>(lhs_value >> rhs_value) );
    return true;
  }

  using namespace ::lavascript::interpreter;
  Format(error,GetIntrinsicCallErrorMessage(INTRINSIC_CALL_RSHIFT));
  return false;
}

inline bool BuiltinLShift( Context* ctx , const Value& lhs , const Value& rhs ,
                                                             Value* output ,
                                                             std::string* error ) {
  (void)ctx;

  if(lhs.IsReal() && rhs.IsReal()) {
    std::uint32_t lhs_value = static_cast<std::uint32_t>(lhs.GetReal());

    std::uint8_t rhs_value = static_cast<std::uint8_t>(rhs.GetReal());

    output->SetReal( static_cast<double>(lhs_value << rhs_value) );
    return true;
  }

  using namespace ::lavascript::interpreter;
  Format(error,GetIntrinsicCallErrorMessage(INTRINSIC_CALL_LSHIFT));
  return false;
}

inline bool BuiltinLRo( Context* ctx , const Value& lhs , const Value& rhs ,
                                                          Value* output ,
                                                          std::string* error ) {
  (void)ctx;

  if(lhs.IsReal() && rhs.IsReal()) {
    std::uint32_t lhs_value = static_cast<std::uint32_t>(lhs.GetReal());

    std::uint8_t rhs_value = static_cast<std::uint8_t>(rhs.GetReal());

    output->SetReal(static_cast<double>(::lavascript::bits::BRol(lhs_value,rhs_value)));

    return true;
  }

  using namespace ::lavascript::interpreter;
  Format(error,GetIntrinsicCallErrorMessage(INTRINSIC_CALL_LRO));
  return false;
}

inline bool BuiltinRRo( Context* ctx , const Value& lhs , const Value& rhs ,
                                                          Value* output ,
                                                          std::string* error ) {
  (void)ctx;

  if(lhs.IsReal() && rhs.IsReal()) {
    std::uint32_t lhs_value = static_cast<std::uint32_t>(lhs.GetReal());

    std::uint8_t rhs_value = static_cast<std::uint8_t>(rhs.GetReal());

    output->SetReal(static_cast<double>(::lavascript::bits::BRor(lhs_value,rhs_value)));

    return true;
  }

  using namespace ::lavascript::interpreter;
  Format(error,GetIntrinsicCallErrorMessage(INTRINSIC_CALL_RRO));
  return false;
}

inline bool BuiltinBAnd( Context* ctx , const Value& lhs , const Value& rhs ,
                                                           Value* output ,
                                                           std::string* error ) {
  (void)ctx;

  if(lhs.IsReal() && rhs.IsReal()) {
    std::uint32_t lhs_value = static_cast<std::uint32_t>(lhs.GetReal());

    std::uint32_t rhs_value = static_cast<std::uint32_t>(rhs.GetReal());

    output->SetReal(static_cast<double>(lhs_value & rhs_value));

    return true;
  }

  using namespace ::lavascript::interpreter;
  Format(error,GetIntrinsicCallErrorMessage(INTRINSIC_CALL_BAND));
  return false;
}

inline bool BuiltinBOr( Context* ctx , const Value& lhs , const Value& rhs ,
                                                          Value* output ,
                                                          std::string* error ) {
  (void)ctx;

  if(lhs.IsReal() && rhs.IsReal()) {
    std::uint32_t lhs_value = static_cast<std::uint32_t>(lhs.GetReal());

    std::uint32_t rhs_value = static_cast<std::uint32_t>(rhs.GetReal());

    output->SetReal(static_cast<double>(lhs_value | rhs_value));

    return true;
  }

  using namespace ::lavascript::interpreter;
  Format(error,GetIntrinsicCallErrorMessage(INTRINSIC_CALL_BOR));
  return false;
}

inline bool BuiltinBXor( Context* ctx , const Value& lhs , const Value& rhs ,
                                                           Value* output ,
                                                           std::string* error ) {
  (void)ctx;

  if(lhs.IsReal() && rhs.IsReal()) {
    std::uint32_t lhs_value = static_cast<std::uint32_t>(lhs.GetReal());

    std::uint32_t rhs_value = static_cast<std::uint32_t>(rhs.GetReal());

    output->SetReal(static_cast<double>(lhs_value ^ rhs_value));

    return true;
  }

  using namespace ::lavascript::interpreter;
  Format(error,GetIntrinsicCallErrorMessage(INTRINSIC_CALL_BXOR));
  return false;
}

inline bool BuiltinInt( Context* ctx, const Value& operand , Value* output ,
                                                             std::string* error ) {
  using namespace ::lavascript::interpreter;
  (void)ctx;

  switch(operand.type()) {
    case TYPE_REAL:
        output->SetReal(static_cast<double>(static_cast<std::int32_t>(operand.GetReal())));
      return true;
    case TYPE_BOOLEAN:
      output->SetReal( operand.GetBoolean() ? 1.0 : 0.0 );
      return true;
    case TYPE_STRING:
      {
        std::int32_t ival;
        auto str = operand.GetString()->ToStdString();
        if(StringToInt( str.c_str() , &ival)) {
          output->SetReal( static_cast<double>(ival) );
          return true;
        } else {
          Format(error,"cannot convert string %s to integer", str.c_str());
          return false;
        }
      }
    default:
      Format(error,GetIntrinsicCallErrorMessage(INTRINSIC_CALL_INT));
      return false;
  }
}

inline bool BuiltinReal( Context* ctx , const Value& operand , Value* output ,
                                                               std::string* error ) {
  using namespace ::lavascript::interpreter;
  (void)ctx;

  switch(operand.type()) {
    case TYPE_REAL:
      *output = operand;
      return true;
    case TYPE_BOOLEAN:
      output->SetReal( operand.GetBoolean() ? 1.0 : 0.0 );
      return true;
    case TYPE_STRING:
      {
        double dval;
        auto str = operand.GetString()->ToStdString();
        if(StringToReal( str.c_str() , &dval )) {
          output->SetReal( dval );
          return true;
        } else {
          Format(error,"cannot convert string %s to real", str.c_str());
          return false;
        }
      }
    default:
      Format(error,GetIntrinsicCallErrorMessage(INTRINSIC_CALL_REAL));
      return false;
  }
}

inline bool BuiltinString( Context* ctx , const Value& operand , Value* output ,
                                                                 std::string* error ) {
  using namespace ::lavascript::interpreter;
  switch(operand.type()) {
    case TYPE_REAL:
      output->SetString(String::NewFromReal(ctx->gc(),operand.GetReal()));
      return true;
    case TYPE_BOOLEAN:
      output->SetString(String::NewFromBoolean(ctx->gc(),operand.GetBoolean()));
      return true;
    case TYPE_STRING:
      *output = operand;
      return true;
    default:
      Format(error,GetIntrinsicCallErrorMessage(INTRINSIC_CALL_REAL));
      return false;
  }
}

inline bool BuiltinBoolean( Context* ctx , const Value& operand , Value* output ,
                                                                  std::string* error ) {
  (void)ctx;
  (void)error;

  output->SetBoolean(operand.AsBoolean());
  return true;
}

inline bool BuiltinPush( Context* ctx , const Value& obj , const Value& val ,
                                                           Value* output,
                                                           std::string* error ) {
  (void)ctx;

  if(obj.IsList()) {
    auto list = obj.GetList();
    list->Push(ctx->gc(),val);
    output->SetTrue();
    return true;
  }

  using namespace ::lavascript::interpreter;
  Format(error,GetIntrinsicCallErrorMessage(INTRINSIC_CALL_PUSH));
  return false;
}

inline bool BuiltinPop( Context* ctx , const Value& obj , Value* output ,
                                                          std::string* error ) {
  (void)ctx;

  if(obj.IsList()) {
    auto list = obj.GetList();
    list->Pop();
    output->SetTrue();
    return true;
  }

  using namespace ::lavascript::interpreter;
  Format(error,GetIntrinsicCallErrorMessage(INTRINSIC_CALL_POP));
  return false;
}

inline bool BuiltinSet( Context* ctx , const Value& obj , const Value& idx ,
                                                          const Value& val ,
                                                          Value* output ,
                                                          std::string* error ) {
  (void)ctx;

  if(obj.IsObject() && idx.IsString()) {
    auto object = obj.GetObject();
    output->SetBoolean(object->Set(ctx->gc(),idx.GetString(),val));
    return true;
  }

  using namespace ::lavascript::interpreter;
  Format(error,GetIntrinsicCallErrorMessage(INTRINSIC_CALL_SET));
  return false;
}

inline bool BuiltinHas( Context* ctx , const Value& obj , const Value& idx ,
                                                          Value* output ,
                                                          std::string* error ) {
  (void)ctx;

  if(obj.IsObject() && idx.IsString()) {
    auto object = obj.GetObject();
    Value val;
    output->SetBoolean(object->Get(idx.GetString(),&val));
    (void)val;

    return true;
  }

  using namespace ::lavascript::interpreter;
  Format(error,GetIntrinsicCallErrorMessage(INTRINSIC_CALL_HAS));
  return false;
}

inline bool BuiltinGet( Context* ctx , const Value& obj , const Value& idx ,
                                                          Value* output ,
                                                          std::string* error ) {
  (void)ctx;

  if(obj.IsObject() && idx.IsString()) {
    auto object = obj.GetObject();
    if(!object->Get(idx.GetString(),output)) {
      Format(error,"function get key %s doesn't existed",
                   idx.GetString()->ToStdString().c_str());
      return false;
    } else {
      return true;
    }
  }

  using namespace ::lavascript::interpreter;
  Format(error,GetIntrinsicCallErrorMessage(INTRINSIC_CALL_GET));
  return false;
}

inline bool BuiltinUpdate( Context* ctx , const Value& obj , const Value& idx ,
                                                             const Value& val ,
                                                             Value* output ,
                                                             std::string* error ) {
  (void)ctx;

  if(obj.IsObject() && idx.IsString()) {
    auto object = obj.GetObject();
    output->SetBoolean(object->Update(ctx->gc(),idx.GetString(),val));
    return true;
  }

  using namespace ::lavascript::interpreter;
  Format(error,GetIntrinsicCallErrorMessage(INTRINSIC_CALL_UPDATE));
  return false;
}

inline bool BuiltinPut( Context* ctx , const Value& obj , const Value& idx ,
                                                          const Value& val ,
                                                          Value* output ,
                                                          std::string* error ) {
  (void)ctx;

  if(obj.IsObject() && idx.IsString()) {
    auto object = obj.GetObject();
    object->Put(ctx->gc(),idx.GetString(),val);
    output->SetBoolean(true);
    return true;
  }

  using namespace ::lavascript::interpreter;
  Format(error,GetIntrinsicCallErrorMessage(INTRINSIC_CALL_PUT));
  return false;
}

inline bool BuiltinDelete( Context* ctx , const Value& obj , const Value& idx ,
                                                             Value* output ,
                                                             std::string* error ) {
  (void)ctx;

  if(obj.IsObject() && idx.IsString()) {
    auto object = obj.GetObject();
    output->SetBoolean(object->Delete(idx.GetString()));
    return true;
  }

  using namespace ::lavascript::interpreter;
  Format(error,GetIntrinsicCallErrorMessage(INTRINSIC_CALL_DELETE));
  return false;
}

inline bool BuiltinClear( Context* ctx , const Value& obj , Value* output ,
                                                            std::string* error ) {
  (void)ctx;

  if(obj.IsObject()) {
    obj.GetObject()->Clear(ctx->gc());
    output->SetBoolean(true);
  } else if(obj.IsList()) {
    obj.GetList()->Clear();
    output->SetBoolean(true);
  } else {
    output->SetBoolean(false);
  }
  return true;
}

inline bool BuiltinType( Context* ctx , const Value& obj , Value* output ,
                                                           std::string* error ) {
  (void)ctx;
  (void)error;
  output->SetString( String::New(ctx->gc(),obj.type_name()) );
  return true;
}

inline bool BuiltinLen( Context* ctx , const Value& obj , Value* output ,
                                                         std::string* error ) {
  (void)ctx;

  if(obj.IsObject()) {
    output->SetReal(static_cast<double>(obj.GetObject()->size()));
    return true;
  } else if(obj.IsList()) {
    output->SetReal(static_cast<double>(obj.GetList()->size()));
    return true;
  } else if(obj.IsString()) {
    output->SetReal(static_cast<double>(obj.GetString()->size()));
    return true;
  } else if(obj.IsExtension()) {
    auto ext = obj.GetExtension();
    std::uint32_t sz;
    if(ext->Size(&sz,error)) {
      output->SetReal(static_cast<double>(sz));
      return true;
    }
  }

  Format(error,"function len cannot be applied on type %s",obj.type_name());
  return false;
}

inline bool BuiltinEmpty( Context* ctx , const Value& obj , Value* output ,
                                                            std::string* error ) {
  (void)ctx;

  if(obj.IsObject()) {
    output->SetBoolean(obj.GetObject()->size() ==0);
    return true;
  } else if(obj.IsList()) {
    output->SetBoolean(obj.GetList()->size() == 0);
    return true;
  } else if(obj.IsString()) {
    output->SetBoolean(obj.GetString()->size() == 0);
    return true;
  } else if(obj.IsExtension()) {
    auto ext = obj.GetExtension();
    std::uint32_t sz;
    if(ext->Size(&sz,error)) {
      output->SetBoolean(sz == 0);
      return true;
    }
  }

  Format(error,"function empty cannot be applied on type %s",obj.type_name());
  return false;
}

inline bool BuiltinIter( Context* ctx , const Value& obj , Value* output ,
                                                           std::string* error ) {
  (void)ctx;

  if(obj.IsObject()) {
    output->SetIterator(obj.GetObject()->NewIterator(ctx->gc(),obj.GetObject()));
  } else if(obj.IsList()) {
    output->SetIterator(obj.GetList()->NewIterator(ctx->gc(),obj.GetList()));
  } else if(obj.IsExtension()) {
    auto itr = obj.GetExtension()->NewIterator(ctx->gc(),obj.GetExtension(),error);
    if(itr) {
      output->SetIterator(itr);
    } else {
      output->SetNull();
    }
  } else {
    output->SetNull();
  }
  return true;
}

} // namespace builtin
#include "src/bits.h"
} // namespace lavascript

#endif // BUILTIN_FUNCTION_H_
