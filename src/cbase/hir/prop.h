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
// 1) WriteBarrier based memory operation. These operations is slow fallback
//    operation that will do dynamic dispatch for property/index set and get.
//
// 2) Low level memory operation. These operations requires the type of the
//    node to be object or list. These operations will generate lookup opereation
//    and dereference/reference node.
class PGet : public WriteBarrier {
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
    WriteBarrier (HIR_PGET,graph,id)
  {
    AddOperand(object);
    AddOperand(index );
  }

 protected:
  PGet( IRType type , Graph* graph , std::uint32_t id , Expr* object , Expr* index ):
    WriteBarrier (type,graph,id)
  {
    AddOperand(object);
    AddOperand(index );
  }
 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(PGet)
};

class PSet : public WriteBarrier {
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
    WriteBarrier (HIR_PSET,graph,id)
  {
    AddOperand(object);
    AddOperand(index );
    AddOperand(value );
  }

 protected:
  PSet(IRType type,Graph* graph,std::uint32_t id,Expr* object,Expr* index,Expr* value):
    WriteBarrier (type,graph,id)
  {
    AddOperand(object);
    AddOperand(index );
    AddOperand(value );
  }

 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(PSet)
};

class IGet : public WriteBarrier {
 public:
  inline static IGet* New( Graph* , Expr* , Expr* );

  Expr* object() const { return operand_list()->First(); }
  Expr* index () const { return operand_list()->Last (); }

  IGet( Graph* graph , std::uint32_t id , Expr* object , Expr* index ):
    WriteBarrier (HIR_IGET,graph,id)
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
    WriteBarrier (type,graph,id)
  {
    AddOperand(object);
    AddOperand(index );
  }

 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(IGet)
};

class ISet : public WriteBarrier {
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
    WriteBarrier (HIR_ISET,graph,id)
  {
    AddOperand(object);
    AddOperand(index );
    AddOperand(value );
  }

 protected:
  ISet(IRType type,Graph* graph,std::uint32_t id,Expr* object,Expr* index,Expr* value):
    WriteBarrier (type,graph,id)
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

class ObjectFind : public MemoryRead {
 public:
  ObjectFind( std::uint32_t id , Graph* graph , Expr* object , Expr* key , Checkpoint* cp ):
    MemoryRead(HIR_OBJECT_FIND,id,graph)
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

class ObjectUpdate : public WriteBarrier {
 public:
  ObjectUpdate( std::uint32_t id , Graph* graph , Expr* object , Expr* key ):
    WirteBarrier(HIR_OBJECT_UPDATE,id,graph)
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

class ObjectInsert : public WriteBarrier {
 public:
  ObjectInsert( std::uint32_t id , Graph* graph , Expr* object , Expr* key ):
    WriteBarrier(HIR_OBJECT_INSERT,id,graph)
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
class ListRef : public MemoryRead {
 public:
  ListRef( IRType type , std::uint32_t id , Graph* graph , Expr* object , Expr* index ):
    MemoryRead(type,id,graph)
  {
    lava_debug(NORMAL,lava_verify( GetTypeInference(object) == TPKIND_LIST ););
    AddOperand(object);
    AddOperand(index);
  }

  Expr* object() const { return operand_list()->First();  }
  Expr* index () const { return operand_list()->Index(1); }
};

class ListIndex : public MemoryRead {
 public:
  ListIndex( std::uint32_t id , Graph* graph , Expr* object , Expr* index , TestABC* test ):
    MemoryRead(HIR_LIST_INDEX,id,graph)
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

class ListInsert: public WriteBarrier {
 public:
  ListInset( std::uint32_t id , Graph* graph , Expr* object , Expr* index ):
    WriteBarrier(HIR_LIST_INSERT,id,graph)
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

class ObjectGet : public PGet {
 public:
  inline static ObjectGet* New( Graph* , Expr* , Expr* );
  ObjectGet( Graph* graph , std::uint32_t id , Expr* object , Expr* key ):
    PGet(HIR_OBJECT_GET,graph,id,object,key)
  {}

 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(ObjectGet)
};

class ObjectSet : public PSet {
 public:
  inline static ObjectSet* New( Graph* , Expr* , Expr* , Expr* );
  ObjectSet( Graph* graph , std::uint32_t id , Expr* object , Expr* key, Expr* value ):
    PSet(HIR_OBJECT_SET,graph,id,object,key,value)
  {}

 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(ObjectSet)
};

class ListGet : public IGet {
 public:
  inline static ListGet* New( Graph* , Expr* , Expr* );
  ListGet( Graph* graph , std::uint32_t id, Expr* object, Expr* index ):
    IGet(HIR_LIST_GET,graph,id,object,index)
  {}

 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(ListGet)
};

class ListSet : public ISet {
 public:
  inline static ListSet* New( Graph* , Expr* , Expr* , Expr* );
  ListSet( Graph* graph , std::uint32_t id , Expr* object , Expr* index  , Expr* value ):
    ISet(HIR_LIST_SET,graph,id,object,index,value)
  {}

 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(ListSet)
};

} // namespace hir
} // namespace cbase
} // namespace lavascript

#endif //CBASE_HIR_PROP_H_
