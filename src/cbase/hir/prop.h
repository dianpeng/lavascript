#ifndef CBASE_HIR_PROP_H_
#define CBASE_HIR_PROP_H_
#include "memory.h"

namespace lavascript {
namespace cbase      {
namespace hir        {

// -------------------------------------------------------------------------
// property set/get (side effect)
// -------------------------------------------------------------------------
class PGet : public MemoryRead {
 public:
  inline static PGet* New( Graph* , Expr* , Expr* , ControlFlow* );
  Expr* object() const { return operand_list()->First(); }
  Expr* key   () const { return operand_list()->Last (); }
  virtual Expr* Memory() const { return object(); }

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
    MemoryRead (HIR_PGET,id,graph)
  {
    AddOperand(object);
    AddOperand(index );
  }
 protected:
  PGet( IRType type , Graph* graph , std::uint32_t id , Expr* object , Expr* index ):
    MemoryRead(type,id,graph)
  {
    AddOperand(object);
    AddOperand(index );
  }
 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(PGet)
};

class PSet : public MemoryWrite {
 public:
  inline static PSet* New( Graph* , Expr* , Expr* , Expr* , ControlFlow* );
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
    MemoryWrite(HIR_PSET,id,graph)
  {
    AddOperand(object);
    AddOperand(index );
    AddOperand(value );
  }

 protected:
  PSet(IRType type,Graph* graph,std::uint32_t id,Expr* object,Expr* index,Expr* value):
    MemoryWrite(type,id,graph)
  {
    AddOperand(object);
    AddOperand(index );
    AddOperand(value );
  }

 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(PSet)
};

class IGet : public MemoryRead {
 public:
  inline static IGet* New( Graph* , Expr* , Expr* , ControlFlow* );
  Expr* object() const { return operand_list()->First(); }
  Expr* index () const { return operand_list()->Last (); }
  virtual Expr* Memory() const { return object(); }

  IGet( Graph* graph , std::uint32_t id , Expr* object , Expr* index ):
    MemoryRead (HIR_IGET,id,graph)
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
    MemoryRead(type,id,graph)
  {
    AddOperand(object);
    AddOperand(index );
  }

 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(IGet)
};

class ISet : public MemoryWrite {
 public:
  inline static ISet* New( Graph* , Expr* , Expr* , Expr* , ControlFlow* );
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
    MemoryWrite(HIR_ISET,id,graph)
  {
    AddOperand(object);
    AddOperand(index );
    AddOperand(value );
  }

 protected:
  ISet(IRType type,Graph* graph,std::uint32_t id,Expr* object,Expr* index,Expr* value):
    MemoryWrite(type,id,graph)
  {
    AddOperand(object);
    AddOperand(index );
    AddOperand(value );
  }

 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(ISet)
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
