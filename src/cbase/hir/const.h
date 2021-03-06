#ifndef CBASE_HIR_CONST_H_
#define CBASE_HIR_CONST_H_
#include "expr.h"

namespace lavascript {
namespace cbase      {
namespace hir        {

// The specialized narrow integer representation. We choose int64 since a double's
// conversion will not overflow int64 , but this also put a constraint for us is
// that a int64 can overflow a double number.
LAVA_CBASE_HIR_DEFINE(Tag=INT64;Name="int64";Leaf=Leaf;Box=Unbox,
    Int64,public Expr) {
 public:
  inline static Int64* New( Graph* , std::int64_t );
  std::int64_t       value() const { return value_; }

  Int64( Graph* graph , std::uint32_t id , std::int64_t value ):
    Expr(HIR_INT64,id,graph), value_(value) {}
 public:
  virtual std::uint64_t GVNHash() const {
    return GVNHash1(type_name(),value_);
  }
  virtual bool Equal( const Expr* that ) const {
    return that->Is<Int64>() && (that->As<Int64>()->value() == value_);
  }
 private:
  std::int64_t value_;
  LAVA_DISALLOW_COPY_AND_ASSIGN(Int64)
};

LAVA_CBASE_HIR_DEFINE(Tag=FLOAT64;Name="float64";Leaf=Leaf;Box=Both,
    Float64,public Expr) {
 public:
  inline static Float64* New( Graph* , double );
  double               value() const { return value_; }
  Float64( Graph* graph , std::uint32_t id , double value ): Expr(HIR_FLOAT64,id,graph), value_(value) {}
 public:
  virtual std::uint64_t GVNHash() const {
    return GVNHash1(type_name(),value_);
  }
  virtual bool Equal( const Expr* that ) const {
    return that->Is<Float64>() && (that->As<Float64>()->value() == value_);
  }
 private:
  double value_;
  LAVA_DISALLOW_COPY_AND_ASSIGN(Float64)
};

LAVA_CBASE_HIR_DEFINE(Tag=BOOLEAN;Name="boolean";Leaf=Leaf,
    Boolean,public Expr) {
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
    return that->Is<Boolean>() && (that->As<Boolean>()->value() == value_);
  }
 private:
  bool value_;
  LAVA_DISALLOW_COPY_AND_ASSIGN(Boolean)
};

LAVA_CBASE_HIR_DEFINE(HIR_INTERNAL,StringNode,public Expr) {
 public:
  StringNode( IRType type , std::uint32_t id , Graph* graph ): Expr(type,id,graph) {}
};

LAVA_CBASE_HIR_DEFINE(Tag=LONG_STRING;Name="lstring";Leaf=Leaf,
    LString,public StringNode) {
 public:
  inline static LString* New( Graph* , const LongString& );
  inline static LString* New( Graph* , const char* );
  inline static LString* New( Graph* , const zone::String* );
  const zone::String* value() const { return value_; }
  LString( Graph* graph , std::uint32_t id , const zone::String* value ):
    StringNode(HIR_LONG_STRING,id,graph),
    value_    (value)
  {}
  virtual std::uint64_t GVNHash() const {
    return GVNHash1(type_name(),reinterpret_cast<std::uint64_t>(value_));
  }
  virtual bool Equal( const Expr* that ) const {
    return that->Is<LString>() && (*(that->As<LString>()->value()) == *value_);
  }
 private:
  const zone::String* value_;
  LAVA_DISALLOW_COPY_AND_ASSIGN(LString)
};

LAVA_CBASE_HIR_DEFINE(Tag=SMALL_STRING;Name="sstring";Leaf=Leaf,
    SString,public StringNode) {
 public:
  inline static SString* New( Graph* , const SSO& );
  inline static SString* New( Graph* , const char* );
  inline static SString* New( Graph* , const zone::String* );
  const zone::String* value() const { return value_; }
  SString( Graph* graph , std::uint32_t id , const zone::String* value ):
    StringNode(HIR_SMALL_STRING,id,graph),
    value_    (value)
  {}
  virtual std::uint64_t GVNHash() const {
    return GVNHash1(type_name(),reinterpret_cast<std::uint64_t>(value_));
  }
  virtual bool Equal( const Expr* that ) const {
    return that->Is<SString>() && (*(that->As<SString>()->value()) == *value_);
  }
 private:
  const zone::String* value_;
  LAVA_DISALLOW_COPY_AND_ASSIGN(SString)
};

LAVA_CBASE_HIR_DEFINE(Tag=NIL;Name="nil";Leaf=Leaf,
    Nil,public Expr) {
 public:
  inline static Nil* New( Graph* );
  Nil( Graph* graph , std::uint32_t id ): Expr(HIR_NIL,id,graph) {}
  virtual std::uint64_t GVNHash() const {
    return GVNHash0(type_name());
  }
  virtual bool Equal( const Expr* that ) const {
    return that->Is<Nil>();
  }
 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(Nil)
};

} // namespace hir
} // namespace cbase
} // namespace lavascript

#endif // CBASE_HIR_CONST_H_
