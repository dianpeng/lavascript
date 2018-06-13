#include "fold-arith.h"
#include "folder.h"

#include "src/cbase/type-inference.h"
#include "src/bits.h"

#include <cmath>

namespace lavascript {
namespace cbase      {
namespace hir        {
namespace            {

using namespace ::lavascript::interpreter;

// Folder implementation
class ArithFolder : public Folder {
 public:
  ArithFolder( zone::Zone* zone ) { (void)zone; }
  virtual bool CanFold( const FolderData& data ) const;
  virtual Expr* Fold( Graph* graph , const FolderData& data );
 private:
  inline bool IsUnaryMinus( Expr* );
  inline bool IsUnaryNot  ( Expr* );
  inline bool IsTrue      ( Expr* , TypeKind );
  inline bool IsFalse     ( Expr* , TypeKind );
  template< typename T > inline bool IsNumber( Expr* , T );

  Expr* Fold                  ( Graph* , Unary::Operator  , Expr* );
  Expr* Float64Reassociate    ( Graph* , Binary::Operator , Expr* , Expr* );
  Expr* SimplifyLogicAnd      ( Graph* , TypeKind , TypeKind , Expr* , Expr* );
  Expr* SimplifyLogicOr       ( Graph* , TypeKind , TypeKind , Expr* , Expr* );
  Expr* SimplifyBooleanCompare( Graph* , Binary::Operator , TypeKind , TypeKind , Expr* , Expr* );
  Expr* SimplifyBinary        ( Graph* , Binary::Operator , Expr* , Expr* );
  Expr* MatchBinaryPattern    ( Graph* , Binary::Operator , Expr* , Expr* );
  Expr* Fold                  ( Graph* , Binary::Operator , Expr* , Expr* );
  Expr* Fold                  ( Graph* , Expr* , Expr* , Expr* );
};

LAVA_REGISTER_FOLDER("arith-folder",ArithFolderFactory,ArithFolder);

inline bool ArithFolder::IsUnaryMinus( Expr* node ) {
  return node->IsUnary() && node->AsUnary()->op() == Unary::MINUS;
}

inline bool ArithFolder::IsUnaryNot  ( Expr* node ) {
  return node->IsUnary() && node->AsUnary()->op() == Unary::NOT;
}

inline bool ArithFolder::IsTrue( Expr* node , TypeKind tp ) {
  if(node->IsBoolean() && node->AsBoolean()->value())
    return true;
  else {
    bool bval;
    if(TPKind::ToBoolean(tp,&bval)) {
      return bval;
    }
    return false;
  }
}

inline bool ArithFolder::IsFalse( Expr* node , TypeKind tp ) {
  if(node->IsBoolean() && !node->AsBoolean()->value())
    return true;
  else {
    bool bval;
    if(TPKind::ToBoolean(tp,&bval))
      return !bval;
  }
  return false;
}

template< typename T >
inline bool ArithFolder::IsNumber( Expr* node , T value ) {
  return node->IsFloat64() ? (static_cast<double>(value) == node->AsFloat64()->value()) : false;
}

Expr* ArithFolder::Fold( Graph* graph , Unary::Operator op , Expr* expr ) {
  if(op == Unary::MINUS) {
    if(expr->IsFloat64()) {
      return Float64::New(graph,-expr->AsFloat64()->value());
    } else {
      // Handle cases that we have multiple nested negate operator,
      // example like:
      //  --a   ==> a
      //  ---a  ==> -a
      Expr* output = NULL;
      auto temp = expr;
      while(IsUnaryMinus(temp)) {
        output = temp->AsUnary()->operand();
        temp   = output;
        // check whether temp is yet another unary operation with - operator
        // every iteration we solve 2 leve of nested unary operation
        if(IsUnaryMinus(temp)) {
          temp = temp->AsUnary()->operand();
        } else {
          break;
        }
      }
      return output;
    }
  } else if(op == Unary::NOT) {
    switch(expr->type()) {
      case HIR_FLOAT64:
      case HIR_SMALL_STRING:
      case HIR_LONG_STRING:
      case HIR_LIST:
      case HIR_OBJECT:
        return Boolean::New(graph,false);
      case HIR_BOOLEAN:
        return Boolean::New(graph,!expr->AsBoolean()->value());
      case HIR_NIL:
        return Boolean::New(graph,true);
      default:
        {
          // fallback to use static type inference to do folding
          auto t = GetTypeInference(expr);
          bool bv;
          if(TPKind::ToBoolean(t,&bv)) {
            return Boolean::New(graph,!bv);
          }
        }
        break;
    }
  }
  return NULL;
}

Expr* ArithFolder::Float64Reassociate( Graph* graph , Binary::Operator op , Expr* lhs , Expr* rhs ) {
  if(IsUnaryMinus(lhs) && op == Binary::ADD) {
    // 1. (-a) + b  => b - a
    auto l = NewUnboxNode(graph,rhs                      ,TPKIND_FLOAT64);
    auto r = NewUnboxNode(graph,lhs->AsUnary()->operand(),TPKIND_FLOAT64);
    return NewBoxNode<Float64Arithmetic>(graph,TPKIND_FLOAT64,l,r,Binary::SUB);
  } else if(IsUnaryMinus(rhs) && op == Binary::ADD) {
    auto l = NewUnboxNode(graph,lhs                      ,TPKIND_FLOAT64);
    auto r = NewUnboxNode(graph,rhs->AsUnary()->operand(),TPKIND_FLOAT64);
    // 2. a + (-b) => a - b
    return NewBoxNode<Float64Arithmetic>(graph,TPKIND_FLOAT64,l,r,Binary::SUB);
  } else if(IsUnaryMinus(lhs) && op == Binary::SUB) {
    // 3. -a - b => -b - a
    auto new_lhs = Float64Negate::New( graph , NewUnboxNode(graph,rhs,TPKIND_FLOAT64));
    auto l       = NewUnboxNode(graph,new_lhs                  ,TPKIND_FLOAT64);
    auto r       = NewUnboxNode(graph,lhs->AsUnary()->operand(),TPKIND_FLOAT64);
    return NewBoxNode<Float64Arithmetic>(graph,TPKIND_FLOAT64,l,r,Binary::SUB);

  } else if(IsUnaryMinus(rhs) && op == Binary::SUB) {
    // 4. a - (-b) => a + b
    auto l  = NewUnboxNode(graph,                      lhs,TPKIND_FLOAT64);
    auto r  = NewUnboxNode(graph,rhs->AsUnary()->operand(),TPKIND_FLOAT64);
    return NewBoxNode<Float64Arithmetic>(graph ,TPKIND_FLOAT64,l,r,Binary::ADD);
  } else if(op == Binary::DIV && IsNumber(rhs,1)) {
    // 5. a / 1 => a
    return lhs;
  } else if(op == Binary::DIV && IsNumber(rhs,-1)) {
    // 6. a / -1 => -a
    return NewBoxNode<Float64Negate>(graph,TPKIND_FLOAT64,NewUnboxNode(graph,lhs,TPKIND_FLOAT64));
  } else if(IsUnaryMinus(lhs) && IsUnaryMinus(rhs) && op == Binary::MUL) {
    // 7. -a * -b => a * b
    auto l = NewUnboxNode(graph,lhs->AsUnary()->operand(),TPKIND_FLOAT64);
    auto r = NewUnboxNode(graph,rhs->AsUnary()->operand(),TPKIND_FLOAT64);
    return NewBoxNode<Float64Arithmetic>(graph,TPKIND_FLOAT64,l,r,Binary::MUL);
  } else if(lhs->Equal(rhs)) {
    // 8. a - a => 0
    return Float64::New(graph,0);
  }

  return NULL;
}

Expr* ArithFolder::SimplifyLogicAnd( Graph* graph , TypeKind lhs_type , TypeKind rhs_type ,
                                                                        Expr*    lhs ,
                                                                        Expr*    rhs ) {
  (void)lhs_type;
  (void)rhs_type;

  if(IsFalse(lhs,lhs_type)) { return Boolean::New(graph,false); }  // false && any ==> false
  if(IsTrue (lhs,lhs_type)) { return rhs;                       }  // true  && any ==> any
  if(lhs->Equal(rhs)) return lhs; // a && a ==> a
  if(IsUnaryNot(lhs) && lhs->AsUnary()->operand() == rhs) {
    // !a && a ==> false
    return Boolean::New(graph,false);
  }
  if(IsUnaryNot(rhs) && rhs->AsUnary()->operand() == lhs) {
    // a && !a ==> false
    return Boolean::New(graph,false);
  }
  return NULL;
}

Expr* ArithFolder::SimplifyLogicOr ( Graph* graph , TypeKind lhs_type , TypeKind rhs_type ,
                                                                        Expr*    lhs,
                                                                        Expr*    rhs ) {
  (void)rhs_type;

  if(IsTrue (lhs,lhs_type)) { return Boolean::New(graph,true); }  // true || any ==> true
  if(IsFalse(lhs,lhs_type)) { return rhs;                      }  // false|| any ==> any
  if(lhs->Equal(rhs)) return lhs; // a || a ==> a
  if(IsUnaryNot(lhs) && lhs->AsUnary()->operand() == rhs) {
    // !a || a ==> true
    return Boolean::New(graph,true);
  }
  if(IsUnaryNot(rhs) && rhs->AsUnary()->operand() == lhs) {
    // a || !a ==> true
    return Boolean::New(graph,true);
  }
  return NULL;
}

Expr* ArithFolder::SimplifyBooleanCompare( Graph* graph , Binary::Operator op, TypeKind lhs_type ,
                                                                               TypeKind rhs_type ,
                                                                               Expr*    lhs,
                                                                               Expr*    rhs ) {
  (void)op;
  if(lhs_type == TPKIND_BOOLEAN && rhs->IsBoolean()) {
    return rhs->AsBoolean()->value() ?  lhs :
      NewBoxNode<BooleanNot>(graph,TPKIND_BOOLEAN,NewUnboxNode(graph,lhs,TPKIND_FLOAT64));
  } else if(rhs_type == TPKIND_BOOLEAN && lhs->IsBoolean()) {
    return lhs->AsBoolean()->value() ? rhs :
      NewBoxNode<BooleanNot>(graph,TPKIND_BOOLEAN,NewUnboxNode(graph,rhs,TPKIND_FLOAT64));
  }
  return NULL;
}

Expr* ArithFolder::SimplifyBinary( Graph* graph , Binary::Operator op , Expr* lhs , Expr* rhs ) {
  auto lhs_type = GetTypeInference(lhs);
  auto rhs_type = GetTypeInference(rhs);
  if(lhs_type == TPKIND_FLOAT64 && rhs_type == TPKIND_FLOAT64) {
    return Float64Reassociate(graph,op,lhs,rhs);
  } else if(op == Binary::AND) {
    return SimplifyLogicAnd(graph,lhs_type,rhs_type,lhs,rhs);
  } else if(op == Binary::OR) {
    return SimplifyLogicOr (graph,lhs_type,rhs_type,lhs,rhs);
  } else if(((lhs_type == TPKIND_BOOLEAN && rhs->IsBoolean()) ||
             (rhs_type == TPKIND_BOOLEAN && lhs->IsBoolean()))&&
             (op == Binary::EQ || op == Binary::NE)) {
    // rewrite if(a == true) ==> if(a) and if(a == false) ==> if(!a)
    return SimplifyBooleanCompare(graph,op,lhs_type,rhs_type,lhs,rhs);
  }
  return NULL;
}

Expr* ArithFolder::MatchBinaryPattern( Graph* graph , Binary::Operator op , Expr* lhs ,
                                                                            Expr* rhs ) {
  // This function try to capture certain types of binary operation and lower
  // them into internal graph node
  if(op == Binary::EQ || op == Binary::NE) {
    if((lhs->IsICall() && rhs->Is<StringNode>()) || (rhs->IsICall() && lhs->Is<StringNode>())) {
      // convert : type(var) == "type-name" ==> TestType node
      auto icall = lhs->IsICall() ? lhs->AsICall()      : rhs->AsICall();
      auto type  = lhs->Is<StringNode>()? lhs->AsZoneString() : rhs->AsZoneString();

      if(type == "real") {
        return TestType::New(graph,TPKIND_FLOAT64  ,icall->GetArgument(0));
      } else if(type == "boolean") {
        return TestType::New(graph,TPKIND_BOOLEAN  ,icall->GetArgument(0));
      } else if(type == "null") {
        return TestType::New(graph,TPKIND_NIL      ,icall->GetArgument(0));
      } else if(type == "list") {
        return TestType::New(graph,TPKIND_LIST     ,icall->GetArgument(0));
      } else if(type == "object") {
        return TestType::New(graph,TPKIND_OBJECT   ,icall->GetArgument(0));
      } else if(type == "closure") {
        return TestType::New(graph,TPKIND_CLOSURE  ,icall->GetArgument(0));
      } else if(type == "iterator") {
        return TestType::New(graph,TPKIND_ITERATOR ,icall->GetArgument(0));
      } else if(type == "extension") {
        return TestType::New(graph,TPKIND_EXTENSION,icall->GetArgument(0));
      }
    }
  }
  return NULL; // fallback

}

Expr* ArithFolder::Fold( Graph* graph , Binary::Operator op , Expr* lhs , Expr* rhs ) {
  if(lhs->IsFloat64() && rhs->IsFloat64()) {
    auto lval = lhs->AsFloat64()->value();
    auto rval = rhs->AsFloat64()->value();
    switch(op) {
      case Binary::ADD: return Float64::New(graph,lval+rval);
      case Binary::SUB: return Float64::New(graph,lval-rval);
      case Binary::MUL: return Float64::New(graph,lval*rval);
      case Binary::DIV: return Float64::New(graph,lval/rval);
      case Binary::MOD:
        {
          auto lint = static_cast<std::int64_t>(lval);
          auto rint = static_cast<std::int64_t>(rval);
          return rint == 0 ? NULL : Float64::New(graph,lint % rint);
        }
      case Binary::POW: return Float64::New(graph,std::pow(lval,rval));
      case Binary::LT:  return Boolean::New(graph,lval <  rval);
      case Binary::LE:  return Boolean::New(graph,lval <= rval);
      case Binary::GT:  return Boolean::New(graph,lval >  rval);
      case Binary::GE:  return Boolean::New(graph,lval >= rval);
      case Binary::EQ:  return Boolean::New(graph,lval == rval);
      case Binary::NE:  return Boolean::New(graph,lval != rval);
      case Binary::AND: return Float64::New(graph,rval);
      case Binary::OR:  return Float64::New(graph,lval);
      default: lava_die(); return NULL;
    }
  } else if(lhs->Is<StringNode>() && rhs->Is<StringNode>()) {
    const zone::String* lstr = lhs->IsSString() ? lhs->AsSString()->value() : lhs->AsLString()->value() ;
    const zone::String* rstr = rhs->IsSString() ? rhs->AsSString()->value() : rhs->AsLString()->value() ;
    switch(op) {
      case Binary::LT: return Boolean::New(graph,*lstr <  *rstr);
      case Binary::LE: return Boolean::New(graph,*lstr <= *rstr);
      case Binary::GT: return Boolean::New(graph,*lstr >  *rstr);
      case Binary::GE: return Boolean::New(graph,*lstr >= *rstr);
      case Binary::EQ: return Boolean::New(graph,*lstr == *rstr);
      case Binary::NE: return Boolean::New(graph,*lstr != *rstr);
      default: return NULL;
    }
  } else if(lhs->IsNil() || rhs->IsNil()) {
    if(op == Binary::NE) {
      return Boolean::New(graph,lhs->IsNil() ^  rhs->IsNil());
    } else if(op == Binary::EQ) {
      return Boolean::New(graph,lhs->IsNil() && rhs->IsNil());
    } else {
      return NULL;
    }
  }

  // try other types of folding rules
  if(auto ret = SimplifyBinary    (graph,op,lhs,rhs); ret) return ret;
  if(auto ret = MatchBinaryPattern(graph,op,lhs,rhs); ret) return ret;

  return NULL;
}

Expr* ArithFolder::Fold( Graph* graph , Expr* cond , Expr* lhs , Expr* rhs ) {
  switch(cond->type()) {
    case HIR_FLOAT64:
    case HIR_LONG_STRING:
    case HIR_SMALL_STRING:
    case HIR_LIST:
    case HIR_OBJECT:
      return lhs;
    case HIR_NIL:
      return rhs;
    case HIR_BOOLEAN:
      return (cond->AsBoolean()->value() ? lhs : rhs);
    default:
      {
        // do a static type inference to check which value should return
        bool bv;
        auto t = GetTypeInference(cond);
        if(TPKind::ToBoolean(t,&bv)) {
          return bv ? lhs : rhs;
        }
      }
      break;
  }
  // 1. check if lhs and rhs are the same if so check if cond is side effect free
  //    if it is side effect free then just return lhs/rhs
  if(lhs->Equal(rhs)) return lhs;
  // 2. check following cases
  // 1) value = cond ? true : false ==> value = conv_boolean (cond)
  // 2) value = cond ? false: true  ==> value = conv_nboolean(cond)
  if( lhs->IsBoolean() && rhs->IsBoolean() ) {
    auto lb = lhs->AsBoolean()->value();
    auto rb = rhs->AsBoolean()->value();
    if(lb) {
      lava_debug(NORMAL,lava_verify(!rb););
      return ConvBoolean::NewBox(graph,cond);
    } else {
      lava_debug(NORMAL,lava_verify(rb););
      return ConvNBoolean::NewBox(graph,cond);
    }
  }

  return NULL;
}


bool ArithFolder::CanFold( const FolderData& data ) const {
  return data.fold_type() == FOLD_UNARY   ||
         data.fold_type() == FOLD_BINARY  ||
         data.fold_type() == FOLD_TERNARY;
}

Expr* ArithFolder::Fold( Graph* graph , const FolderData& data ) {
  switch(data.fold_type()) {
    case FOLD_UNARY:
      {
        auto d = static_cast<const UnaryFolderData&>(data);
        return Fold(graph,d.op,d.node);
      }
    case FOLD_BINARY:
      {
        auto d = static_cast<const BinaryFolderData&>(data);
        return Fold(graph,d.op,d.lhs,d.rhs);
      }
    case FOLD_TERNARY:
      {
        auto d = static_cast<const TernaryFolderData&>(data);
        return Fold(graph,d.cond,d.lhs,d.rhs);
      }
    default: lava_die(); return NULL;
  }
}

} // namespace

Expr* FoldBinary ( Graph* graph , Binary::Operator op , Expr* lhs , Expr* rhs ) {
  ArithFolder folder(NULL);
  return folder.Fold(graph,BinaryFolderData{op,lhs,rhs});
}

Expr* FoldTernary( Graph* graph , Expr* cond , Expr* lhs , Expr* rhs ) {
  ArithFolder folder(NULL);
  return folder.Fold(graph,TernaryFolderData{cond,lhs,rhs});
}

} // namespace hir
} // namespace cbase
} // namespace lavascript
