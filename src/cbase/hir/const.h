#ifndef CBASE_HIR_CONST_H_
#define CBASE_HIR_CONST_H_
#include "expr.h"

namespace lavascript {
namespace cbase      {
namespace hir        {

LAVA_CBASE_HIR_DEFINE(Float64,public Expr) {
 public:
  inline static Float64* New( Graph* , double );
  double               value() const { return value_; }
  Float64( Graph* graph , std::uint32_t id , double value ): Expr(HIR_FLOAT64,id,graph), value_(value) {}
 public:
  virtual std::uint64_t GVNHash() const {
    return GVNHash1(type_name(),value_);
  }
  virtual bool Equal( const Expr* that ) const {
    return that->IsFloat64() && (that->AsFloat64()->value() == value_);
  }
 private:
  double value_;
  LAVA_DISALLOW_COPY_AND_ASSIGN(Float64)
};

LAVA_CBASE_HIR_DEFINE(Boolean,public Expr) {
 public:
  inline static Boolean* New( Graph* , bool );
  bool value() const { return value_; }
  Boolean( Graph* graph , std::uint32_t id , bool value ):
    Expr  (HIR_BOOLEAN,id,graph),
    value_(value)
  {}
  virtual std::uint64_t GVNHash() const {
    return GVNHash1(type_name(),value_ ? 1 : 0);
  }
  virtual bool Equal( const Expr* that ) const {
    return that->IsBoolean() && (that->AsBoolean()->value() == value_);
  }
 private:
  bool value_;
  LAVA_DISALLOW_COPY_AND_ASSIGN(Boolean)
};

LAVA_CBASE_HIR_DEFINE(LString,public Expr) {
 public:
  inline static LString* New( Graph* , const LongString& );
  inline static LString* New( Graph* , const char* );
  inline static LString* New( Graph* , const zone::String* );
  const zone::String* value() const { return value_; }
  LString( Graph* graph , std::uint32_t id , const zone::String* value ):
    Expr  (HIR_LONG_STRING,id,graph),
    value_(value)
  {}
  virtual std::uint64_t GVNHash() const {
    return GVNHash1(type_name(),reinterpret_cast<std::uint64_t>(value_));
  }
  virtual bool Equal( const Expr* that ) const {
    return that->IsLString() && (*(that->AsLString()->value()) == *value_);
  }
 private:
  const zone::String* value_;
  LAVA_DISALLOW_COPY_AND_ASSIGN(LString)
};

LAVA_CBASE_HIR_DEFINE(SString,public Expr) {
 public:
  inline static SString* New( Graph* , const SSO& );
  inline static SString* New( Graph* , const char* );
  inline static SString* New( Graph* , const zone::String* );
  const zone::String* value() const { return value_; }
  SString( Graph* graph , std::uint32_t id , const zone::String* value ):
    Expr (HIR_SMALL_STRING,id,graph),
    value_(value)
  {}
  virtual std::uint64_t GVNHash() const {
    return GVNHash1(type_name(),reinterpret_cast<std::uint64_t>(value_));
  }
  virtual bool Equal( const Expr* that ) const {
    return that->IsSString() && (*(that->AsSString()->value()) == *value_);
  }
 private:
  const zone::String* value_;
  LAVA_DISALLOW_COPY_AND_ASSIGN(SString)
};

LAVA_CBASE_HIR_DEFINE(Nil,public Expr) {
 public:
  inline static Nil* New( Graph* );
  Nil( Graph* graph , std::uint32_t id ): Expr(HIR_NIL,id,graph) {}
  virtual std::uint64_t GVNHash() const {
    return GVNHash0(type_name());
  }
  virtual bool Equal( const Expr* that ) const {
    return that->IsNil();
  }
 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(Nil)
};

} // namespace hir
} // namespace cbase
} // namespace lavascript

#endif // CBASE_HIR_CONST_H_
