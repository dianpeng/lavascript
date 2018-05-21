#include "folder.h"
#include "src/cbase/hir.h"

namespace lavascript {
namespace cbase      {
namespace hir        {
namespace {

using namespace ::lavascript::interpreter;

#define LAVA_DEFINE_INTRINSIC_FOLD(__)        \
   /* arithmetic */                           \
   __(Min,MIN,2)                              \
   __(Max,MAX,2)                              \
   __(Sqrt,SQRT,1)                            \
   __(Sin,SIN,1)                              \
   __(Cos,COS,1)                              \
   __(Tan,TAN,1)                              \
   __(Abs,ABS,1)                              \
   __(Ceil,CEIL,1)                            \
   __(Floor,FLOOR,1)                          \
   /* bits */                                 \
   __(LShift,LSHIFT,2)                        \
   __(RShift,RSHIFT,2)                        \
   __(LRo,LRO,2)                              \
   __(RRo,RRO,2)                              \
   __(BAnd,BAND,2)                            \
   __(BOr ,BOR ,2)                            \
   __(BXor,BXOR,2)                            \
   /* conversion */                           \
   __(Int,INT,1)                              \
   __(Real,REAL,1)                            \
   __(String,STRING,1)                        \
   __(Boolean,BOOLEAN,1)                      \
   /* misc */                                 \
   __(Type,TYPE,1)                            \
   __(Len,LEN,1)                              \
   __(Empty,EMPTY,1)

// Folder implementation
class IntrinsicFolder : public Folder {
 public:
  IntrinsicFolder( zone::Zone* zone ) { (void)zone; }

  virtual bool CanFold( const FolderData& ) const;
  virtual Expr* Fold    ( Graph* , const FolderData& );
 private:
   inline bool AsUInt8 ( Expr* , std::uint8_t* );
   inline bool AsUInt32( Expr* , std::uint32_t* );
   inline bool AsReal  ( Expr* , double* );

 private:
   Expr* FoldICall     ( Graph*, ICall* );

#define _1 Expr*
#define _2 Expr* , Expr*
#define _3 Expr* , Expr* , Expr*
#define __(A,B,C) Expr* Fold##A( Graph* , _##C );

LAVA_DEFINE_INTRINSIC_FOLD(__)

#undef __ // __
#undef _3 // _3
#undef _2 // _2
#undef _1 // _1

};

LAVA_REGISTER_FOLDER("intrinsic-folder",IntrinsicFolderFactory,IntrinsicFolder);


bool IntrinsicFolder::CanFold( const FolderData& data ) const {
  if(data.fold_type() == FOLD_EXPR) {
    auto d = static_cast<const ExprFolderData&>(data);
    return d.node->Is<ICall>();
  }
  return false;
}

Expr* IntrinsicFolder::Fold( Graph* graph , const FolderData& data ) {
  lava_debug(NORMAL,lava_verify(data.fold_type() == FOLD_EXPR););
  auto d = static_cast<const ExprFolderData&>(data);
  return FoldICall(graph,d.node->As<ICall>());
}

inline bool IntrinsicFolder::AsUInt8( Expr* node , std::uint8_t* value ) {
  if(node->IsFloat64()) {
    // we don't care about the shifting overflow, the underly ISA
    // only allows a 8bit register serve as how many bits shifted.
    *value = static_cast<std::uint8_t>(node->AsFloat64()->value());
    return true;
  }
  return false;
}

inline bool IntrinsicFolder::AsUInt32( Expr* node , std::uint32_t* value ) {
  if(node->IsFloat64()) {
    *value = static_cast<std::uint32_t>(node->AsFloat64()->value());
    return true;
  }
  return false;
}

inline bool IntrinsicFolder::AsReal  ( Expr* node , double* real ) {
  if(node->IsFloat64()) {
    *real = node->AsFloat64()->value();
    return true;
  }
  return false;
}

// ----------------------------------------------------------------------
// Fold
// ----------------------------------------------------------------------
Expr* IntrinsicFolder::FoldMin( Graph* graph , Expr* lhs , Expr* rhs ) {
  double lv , rv;
  if( AsReal(lhs,&lv) && AsReal(rhs,&rv) ) {
    return Float64::New(graph,std::min(lv,rv));
  }
  return NULL;
}

Expr* IntrinsicFolder::FoldMax( Graph* graph , Expr* lhs , Expr* rhs ) {
  double lv , rv;
  if( AsReal(lhs,&lv) && AsReal(rhs,&rv) ) {
    return Float64::New(graph,std::max(lv,rv));
  }
  return NULL;
}

Expr* IntrinsicFolder::FoldSqrt( Graph* graph , Expr* lhs ) {
  if(double opr; AsReal(lhs,&opr)) {
    return Float64::New(graph,std::sqrt(opr));
  }
  return NULL;
}

Expr* IntrinsicFolder::FoldSin( Graph* graph , Expr* lhs ) {
  if(double opr; AsReal(lhs,&opr)) {
    return Float64::New(graph,std::sin(opr));
  }
  return NULL;
}

Expr* IntrinsicFolder::FoldCos( Graph* graph , Expr* lhs ) {
  if(double opr; AsReal(lhs,&opr)) {
    return Float64::New(graph,std::cos(opr));
  }
  return NULL;
}

Expr* IntrinsicFolder::FoldTan( Graph* graph , Expr* lhs ) {
  if(double opr; AsReal(lhs,&opr)) {
    return Float64::New(graph,std::tan(opr));
  }
  return NULL;
}

Expr* IntrinsicFolder::FoldAbs( Graph* graph , Expr* lhs ) {
  if(double opr; AsReal(lhs,&opr)) {
    return Float64::New(graph,std::abs(opr));
  }
  return NULL;
}

Expr* IntrinsicFolder::FoldCeil( Graph* graph , Expr* lhs ) {
  if(double opr; AsReal(lhs,&opr)) {
    return Float64::New(graph,std::ceil(opr));
  }
  return NULL;
}

Expr* IntrinsicFolder::FoldFloor( Graph* graph , Expr* lhs ) {
  if(double opr; AsReal(lhs,&opr)) {
    return Float64::New(graph,std::floor(opr));
  }
  return NULL;
}

Expr* IntrinsicFolder::FoldLShift( Graph* graph , Expr* lhs , Expr* rhs ) {
  std::uint32_t lv;
  std::uint8_t  rv;
  if(AsUInt32(lhs,&lv) && AsUInt8(rhs,&rv)) {
    return Float64::New(graph,static_cast<double>(lv << rv));
  }
  return NULL;
}

Expr* IntrinsicFolder::FoldRShift( Graph* graph , Expr* lhs , Expr* rhs ) {
  std::uint32_t lv;
  std::uint8_t  rv;
  if(AsUInt32(lhs,&lv) && AsUInt8(rhs,&rv)) {
    return Float64::New(graph,static_cast<double>(lv >> rv));
  }
  return NULL;
}

Expr* IntrinsicFolder::FoldLRo   ( Graph* graph , Expr* lhs , Expr* rhs ) {
  std::uint32_t lv;
  std::uint8_t  rv;
  if(AsUInt32(lhs,&lv) && AsUInt8(rhs,&rv)) {
    return Float64::New(graph,static_cast<double>(bits::BRol(lv,rv)));
  }
  return NULL;
}

Expr* IntrinsicFolder::FoldRRo   ( Graph* graph , Expr* lhs , Expr* rhs ) {
  std::uint32_t lv;
  std::uint8_t  rv;
  if(AsUInt32(lhs,&lv) && AsUInt8(rhs,&rv)) {
    return Float64::New(graph,static_cast<double>(bits::BRor(lv,rv)));
  }
  return NULL;
}

Expr* IntrinsicFolder::FoldBAnd  ( Graph* graph , Expr* lhs , Expr* rhs ) {
  std::uint32_t lv,rv;
  if(AsUInt32(lhs,&lv) && AsUInt32(rhs,&rv)) {
    return Float64::New(graph,static_cast<double>(lv & rv));
  }
  return NULL;
}

Expr* IntrinsicFolder::FoldBOr   ( Graph* graph , Expr* lhs , Expr* rhs ) {
  std::uint32_t lv,rv;
  if(AsUInt32(lhs,&lv) && AsUInt32(rhs,&rv)) {
    return Float64::New(graph,static_cast<double>(lv | rv));
  }
  return NULL;
}

Expr* IntrinsicFolder::FoldBXor  ( Graph* graph , Expr* lhs , Expr* rhs ) {
  std::uint32_t lv,rv;
  if(AsUInt32(lhs,&lv) && AsUInt32(rhs,&rv)) {
    return Float64::New(graph,static_cast<double>(lv ^ rv));
  }
  return NULL;
}

Expr* IntrinsicFolder::FoldInt( Graph* graph , Expr* lhs ) {
  return NULL;
}

Expr* IntrinsicFolder::FoldReal( Graph* graph , Expr* lhs ) {
  return NULL;
}

Expr* IntrinsicFolder::FoldString( Graph* graph , Expr* lhs ) {
  return NULL;
}

Expr* IntrinsicFolder::FoldBoolean( Graph* graph , Expr* lhs ) {
  return NULL;
}

Expr* IntrinsicFolder::FoldType( Graph* graph , Expr* lhs ) {
  return NULL;
}

Expr* IntrinsicFolder::FoldLen( Graph* graph , Expr* lhs ) {
  return NULL;
}

Expr* IntrinsicFolder::FoldEmpty( Graph* graph , Expr* lhs ) {
  return NULL;
}

Expr* IntrinsicFolder::FoldICall( Graph* graph , ICall* node ) {

#define _1        node->Operand(0)
#define _2        node->Operand(0) , node->Operand(1)
#define _3        node->Operand(0) , node->Operand(1) , node->Operand(2)
#define __(A,B,C) case INTRINSIC_CALL_##B: return Fold##A( graph , _##C );

  switch(node->ic()) {
    LAVA_DEFINE_INTRINSIC_FOLD(__)
    default: return NULL;
  }

#undef __ // __
#undef _3 // _3
#undef _2 // _2
#undef _1 // _1

  lava_die(); return NULL;
}

} // namespace
} // namespace hir
} // namespace cbase
} // namespace lavascript
