#ifndef CBASE_HIR_PROP_H_
#define CBASE_HIR_PROP_H_
#include "memory.h"

namespace lavascript {
namespace cbase      {
namespace hir        {

// Property/Index , ie memory mutation operation
//
// For memory operation, we have 2 sets of memory operation :
//
// 1) HardBarrier based memory operation. These operations is slow fallback
//    operation that will do dynamic dispatch for property/index set and get.
//
// 2) Low level memory operation. These operations requires the type of the
//    node to be object or list. These operations will generate lookup opereation
//    and dereference/reference node.
class PGet : public HardBarrier {
 public:
  inline static PGet* New( Graph* , Expr* , Expr* );

  Expr* object() const { return operand_list()->First(); }
  Expr* key   () const { return operand_list()->Last (); }

  virtual std::uint64_t GVNHash() const {
    return GVNHash2(type_name(),object()->GVNHash(),key()->GVNHash());
  }
  virtual bool Equal( const Expr* that ) const {
    if(that->IsPGet()) {
      auto that_pget = that->AsPGet();
      return object()->Equal(that_pget->object()) && key()->Equal(that_pget->key());
    }
    return false;
  }

  PGet( Graph* graph , std::uint32_t id , Expr* object , Expr* index ):
    HardBarrier (HIR_PGET,graph,id)
  {
    AddOperand(object);
    AddOperand(index );
  }

 protected:
  PGet( IRType type , Graph* graph , std::uint32_t id , Expr* object , Expr* index ):
    HardBarrier (type,graph,id)
  {
    AddOperand(object);
    AddOperand(index );
  }
 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(PGet)
};

class PSet : public HardBarrier {
 public:
  inline static PSet* New( Graph* , Expr* , Expr* , Expr* );

  Expr* object() const { return operand_list()->First(); }
  Expr* key   () const { return operand_list()->Index(1);}
  Expr* value () const { return operand_list()->Last (); }
 public:
  virtual std::uint64_t GVNHash() const {
    return GVNHash3(type_name(),object()->GVNHash(),key()->GVNHash(),value()->GVNHash());
  }
  virtual bool Equal( const Expr* that ) const {
    if(that->IsPSet()) {
      auto that_pset = that->AsPSet();
      return object()->Equal(that_pset->object()) &&
             key   ()->Equal(that_pset->key())    &&
             value ()->Equal(that_pset->value());
    }
    return false;
  }
  PSet( Graph* graph , std::uint32_t id , Expr* object , Expr* index , Expr* value ):
    HardBarrier (HIR_PSET,graph,id)
  {
    AddOperand(object);
    AddOperand(index );
    AddOperand(value );
  }

 protected:
  PSet(IRType type,Graph* graph,std::uint32_t id,Expr* object,Expr* index,Expr* value):
    HardBarrier (type,graph,id)
  {
    AddOperand(object);
    AddOperand(index );
    AddOperand(value );
  }

 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(PSet)
};

class IGet : public HardBarrier {
 public:
  inline static IGet* New( Graph* , Expr* , Expr* );

  Expr* object() const { return operand_list()->First(); }
  Expr* index () const { return operand_list()->Last (); }

  IGet( Graph* graph , std::uint32_t id , Expr* object , Expr* index ):
    HardBarrier (HIR_IGET,graph,id)
  {
    AddOperand(object);
    AddOperand(index );
  }

  virtual std::uint64_t GVNHash() const {
    return GVNHash2(type_name(),object()->GVNHash(),index()->GVNHash());
  }

  virtual bool Equal( const Expr* that ) const {
    if(that->IsIGet()) {
      auto that_iget = that->AsIGet();
      return object()->Equal(that_iget->object()) && index ()->Equal(that_iget->index());
    }
    return false;
  }

 protected:
  IGet( IRType type , Graph* graph , std::uint32_t id , Expr* object , Expr* index ):
    HardBarrier (type,graph,id)
  {
    AddOperand(object);
    AddOperand(index );
  }

 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(IGet)
};

class ISet : public HardBarrier {
 public:
  inline static ISet* New( Graph* , Expr* , Expr* , Expr* );

  Expr* object() const { return operand_list()->First(); }
  Expr* index () const { return operand_list()->Index(1);}
  Expr* value () const { return operand_list()->Last (); }

  virtual std::uint64_t GVNHash() const {
    return GVNHash3(type_name(),object()->GVNHash(),index ()->GVNHash(),value ()->GVNHash());
  }
  virtual bool Eqaul( const Expr* that ) const {
    if(that->IsISet()) {
      auto that_iset = that->AsISet();
      return object()->Equal(that_iset->object()) &&
             index ()->Equal(that_iset->index())  &&
             value ()->Equal(that_iset->value());
    }
    return false;
  }

  ISet( Graph* graph , std::uint32_t id , Expr* object , Expr* index , Expr* value ):
    HardBarrier (HIR_ISET,graph,id)
  {
    AddOperand(object);
    AddOperand(index );
    AddOperand(value );
  }

 protected:
  ISet(IRType type,Graph* graph,std::uint32_t id,Expr* object,Expr* index,Expr* value):
    HardBarrier (type,graph,id)
  {
    AddOperand(object);
    AddOperand(index );
    AddOperand(value );
  }

 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(ISet)
};

// Low level memory mutation operation.
//
// All the low level operation requires type check since they are related to the type
// of the memory. These operations can be categorized into 2 different categories:
//
// 1) reference/pointer lookup node , basically used to lookup a pointer point to element
//    in a list or a object/hash entry
//
//    ObjectFind   --> lookup a object's reference with given key , failed out of native
//                     function if cannot find the reference
//    ObjectUpdate --> update a object's reference with a given key , cannot fail
//    ObjectInsert --> insert a object's reference with a given key , cannot fail
//    ListIndex    --> lookup a list element reference with given key
//    ListInsert   --> insert an element into a list
//
// 2) pointer set/get, basically used to set a value into a specific reference.
//
//    ObjectRefSet --> set a reference returned by   ObjectFind/ObjectUpdate/ObjectInsert
//    ObjectRefGet --> get a value from reference of ObjectFind/ObjectUpdate/ObjectInsert
//    ListRefSet   --> set a reference returned by   ListIndex/ListInsert
//    ListRefGet   --> get a value from reference of ListIndex/ListInsert

class ObjectFind : public ReadEffect {
 public:
  ObjectFind( Graph* graph , std::uint32_t id , Expr* object , Expr* key , Checkpoint* cp ):
    ReadEffect(HIR_OBJECT_FIND,id,graph)
  {
    lava_debug(NORMAL,lava_verify(GetTypeInference(object) == TPKIND_OBJECT););
    AddOperand(object);
    AddOperand(key);
    AddOperand(cp);
  }
  Expr* object() const { return operand_list()->First(); }
  Expr* key   () const { return operand_list()->Index(1);}
  Checkpoint* checkpoint() const { return operand_list()->Last()->AsCheckpoint(); }
 public: // gvn hash
  virtual std::uint64_t GVNHash() const {
    return GVNHash4(type_name(),object()->GVNHash(),key()->GVNHash(),
                                                    write_effect()->GVNHash(),
                                                    checkpoint()->GVNHash());
  }
  virtual bool Equal( const Expr* that ) const {
    if(that->IsObjectFind()) {
      auto oref = that->AsObjectFind();
      return write_effect()->Equal(oref->write_effect()) &&
             object()->Equal(oref->object())             &&
             key   ()->Equal(oref->key   ())             &&
             checkpoint()->Equal(oref->checkpoint());
    }
    return false;
  }
 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(ObjectFind)
};

class ObjectUpdate : public SoftBarrier {
 public:
  ObjectUpdate( Graph* graph , std::uint32_t id , Expr* object , Expr* key ):
    SoftBarrier(HIR_OBJECT_UPDATE,id,graph)
  {
    lava_debug(NORMAL,lava_verify(GetTypeInference(object) == TPKIND_OBJECT););
    AddOperand(object);
    AddOperand(key);
  }
  Expr* object() const { return operand_list()->First(); }
  Expr* key   () const { return operand_list()->Last (); }
 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(ObjectUpdate)
};

class ObjectInsert : public SoftBarrier {
 public:
  ObjectInsert( Graph* graph , std::uint32_t id , Expr* object , Expr* key ):
    SoftBarrier(HIR_OBJECT_INSERT,id,graph)
  {
    lava_debug(NORMAL,lava_verify(GetTypeInference(object) == TPKIND_OBJECT););
    AddOperand(object);
    AddOperand(key);
  }
  Expr* object() const { return operand_list()->First(); }
  Expr* key   () const { return operand_list()->Last (); }
 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(ObjectInsert)
};

// ListRef node represent list related reference lookup. One thing to note is that
// these operations *doesn't*
class ListRef : public ReadEffect {
 public:
  ListRef( IRType type , Graph* graph , std::uint32_t id , Expr* object , Expr* index ):
    ReadEffect(type,id,graph)
  {
    lava_debug(NORMAL,lava_verify( GetTypeInference(object) == TPKIND_LIST ););
    AddOperand(object);
    AddOperand(index);
  }

  Expr* object() const { return operand_list()->First();  }
  Expr* index () const { return operand_list()->Index(1); }
};

class ListIndex : public ReadEffect {
 public:
  ListIndex( Graph* graph , std::uint32_t id , Expr* object , Expr* index , TestABC* test ):
    ReadEffect(HIR_LIST_INDEX,id,graph)
  {
    lava_debug(NORMAL,lava_verify( GetTypeInference(object) == TPKIND_LIST ););
    AddOperand(object);
    AddOperand(index);
    AddOperand(test );
  }

  Expr* object() const { return operand_list()->First(); }
  Expr* index () const { return operand_list()->Index(1); }
  TestABC* abc() const { return operand_list()->Last()->AsTestABC(); }
 public: // gvn hash
  virtual std::uint64_t GVNHash() const {
    return GVNHash4(type_name(),object()->GVNHash(),key()->GVNHash(),abc()->GVNHash());
  }
  virtual bool Equal( const Expr* that ) const {
    if(that->IsObjectFind()) {
      auto oref = that->AsObjectFind();
      return write_effect()->Equal(oref->write_effect()) &&
             object()->Equal(oref->object())             &&
             index ()->Equal(oref->index ())             &&
             abc   ()->Equal(oref->abc   ());
    }
    return false;
  }

 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(ListIndex)
};

class ListInsert: public SoftBarrier {
 public:
  ListInsert( Graph* graph , std::uint32_t id , Expr* object , Expr* index ):
    SoftBarrier(HIR_LIST_INSERT,id,graph)
  {
    lava_debug(NORMAL,lava_verify( GetTypeInference(object) == TPKIND_LIST ););
    AddOperand(object);
    AddOperand(index);
  }
  Expr* object() const { return operand_list()->First();  }
  Expr* index () const { return operand_list()->Index(1); }
 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(ListInsert)
};

class ObjectRefGet : public ReadEffect {
 public:
  ObjectRefGet( Graph* graph , std::uint32_t id , Expr* oref ):
    ReadEffect(HIR_OBJECT_REF_GET,id,graph)
  {
    lava_debug(NORMAL,lava_verify( oref->IsObjectFind()   ||
                                   oref->IsObjectUpdate() ||
                                   oref->IsObjectInsert() ););
    AddOperand(oref);
  }

  Expr* ref() const { return operand_list()->First(); }
 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(ObjectRefGet)
};

class ObjectRefSet : public ReadEffect {
 public:
  ObjectRefSet( Graph* graph , std::uint32_t id , Expr* oref , Expr* value ):
    ReadEffect(HIR_OBJECT_REF_SET,id,graph)
  {
    lava_debug(NORMAL,lava_verify( oref->IsObjectFind()   ||
                                   oref->IsObjectUpdate() ||
                                   oref->IsObjectInsert() ););
    AddOperand(oref);
    AddOperand(value);
  }

  Expr* ref  () const { return operand_list()->First(); }
  Expr* value() const { return operand_list()->Last(); }
 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(ObjectRefSet)
};

class ListRefGet : public ReadEffect {
 public:
  ListRefSet( Graph* graph , std::uint32_t id , Expr* lref ):
    ReadEffect(HIR_LIST_REF_GET,id,graph)
  {
    lava_debug(NORMAL,lava_verify( lref->IsListIndex() ||
                                   lref->IsListInsert() ););
    AddOperand(lref);
  }

  Expr* ref  () const { return operand_list()->First(); }
 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(ListRefGet)
};

class ListRefSet : public ReadEffect {
 public:
  ListRefSet( Graph* graph , std::uint32_t id , Expr* lref , Expr* value ):
    ReadEffect(HIR_LIST_REF_SET,id,graph)
  {
    lava_debug(NORMAL,lava_verify( lref->IsListIndex()  ||
                                   lref->isListInsert() ););
    AddOperand(lref);
    AddOperand(value);
  }

  Expr* ref  () const { return operand_list()->First(); }
  Expr* value() const { return operand_list()->Last(); }
 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(ListRefSet)
};

} // namespace hir
} // namespace cbase
} // namespace lavascript

#endif //CBASE_HIR_PROP_H_
