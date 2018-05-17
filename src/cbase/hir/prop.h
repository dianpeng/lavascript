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
LAVA_CBASE_HIR_DEFINE(PGet,public HardBarrier) {
 public:
  inline static PGet* New( Graph* , Expr* , Expr* );

  Expr* object() const { return operand_list()->First(); }
  Expr* key   () const { return operand_list()->Last (); }

  PGet( Graph* graph , std::uint32_t id , Expr* object , Expr* index ):
    HardBarrier (HIR_PGET,id,graph)
  {
    AddOperand(object);
    AddOperand(index );
  }

 protected:
  PGet( IRType type , Graph* graph , std::uint32_t id , Expr* object , Expr* index ):
    HardBarrier (type,id,graph)
  {
    AddOperand(object);
    AddOperand(index );
  }
 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(PGet)
};

LAVA_CBASE_HIR_DEFINE(PSet,public HardBarrier) {
 public:
  inline static PSet* New( Graph* , Expr* , Expr* , Expr* );

  Expr* object() const { return operand_list()->First(); }
  Expr* key   () const { return Operand(1);}
  Expr* value () const { return operand_list()->Last (); }
 public:
  PSet( Graph* graph , std::uint32_t id , Expr* object , Expr* index , Expr* value ):
    HardBarrier (HIR_PSET,id,graph)
  {
    AddOperand(object);
    AddOperand(index );
    AddOperand(value );
  }

 protected:
  PSet(IRType type,Graph* graph,std::uint32_t id,Expr* object,Expr* index,Expr* value):
    HardBarrier (type,id,graph)
  {
    AddOperand(object);
    AddOperand(index );
    AddOperand(value );
  }

 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(PSet)
};

LAVA_CBASE_HIR_DEFINE(IGet,public HardBarrier) {
 public:
  inline static IGet* New( Graph* , Expr* , Expr* );

  Expr* object() const { return operand_list()->First(); }
  Expr* index () const { return operand_list()->Last (); }

  IGet( Graph* graph , std::uint32_t id , Expr* object , Expr* index ):
    HardBarrier (HIR_IGET,id,graph)
  {
    AddOperand(object);
    AddOperand(index );
  }

 protected:
  IGet( IRType type , Graph* graph , std::uint32_t id , Expr* object , Expr* index ):
    HardBarrier (type,id,graph)
  {
    AddOperand(object);
    AddOperand(index );
  }

 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(IGet)
};

LAVA_CBASE_HIR_DEFINE(ISet,public HardBarrier) {
 public:
  inline static ISet* New( Graph* , Expr* , Expr* , Expr* );

  Expr* object() const { return operand_list()->First(); }
  Expr* index () const { return Operand(1);}
  Expr* value () const { return operand_list()->Last (); }

  ISet( Graph* graph , std::uint32_t id , Expr* object , Expr* index , Expr* value ):
    HardBarrier (HIR_ISET,id,graph)
  {
    AddOperand(object);
    AddOperand(index );
    AddOperand(value );
  }

 protected:
  ISet(IRType type,Graph* graph,std::uint32_t id,Expr* object,Expr* index,Expr* value):
    HardBarrier (type,id,graph)
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

LAVA_CBASE_HIR_DEFINE(ObjectResize,public SoftBarrier) {
 public:
  ObjectResize( IRType type , std::uint32_t id , Graph* graph , Expr* object , Expr* key ):
    SoftBarrier(type,id,graph)
  {
    lava_debug(NORMAL,lava_verify(GetTypeInference(object) == TPKIND_OBJECT););
    AddOperand(object);
    AddOperand(key);
  }

  Expr* object() const { return operand_list()->First(); }
  Expr* key   () const { return operand_list()->Last (); }
};

LAVA_CBASE_HIR_DEFINE(ListResize,public SoftBarrier) {
 public:
  ListResize( IRType type , std::uint32_t id , Graph* graph , Expr* object ,
                                                              Expr* index ,
                                                              Checkpoint* cp ):
    SoftBarrier(type,id,graph)
  {
    lava_debug(NORMAL,lava_verify( GetTypeInference(object) == TPKIND_LIST ););
    AddOperand(object);
    AddOperand(index);
    AddOperand(cp);
  }

  Expr*           object() const { return operand_list()->First();  }
  Expr*           index () const { return Operand(1); }
  Checkpoint* checkpoint() const { return operand_list()->Last()->AsCheckpoint(); }
};

LAVA_CBASE_HIR_DEFINE(StaticRef,public ReadEffect) {
 public:
  StaticRef( IRType type , std::uint32_t id , Graph* graph ):
    ReadEffect(type,id,graph) {}
};


LAVA_CBASE_HIR_DEFINE(ObjectFind,public StaticRef) {
 public:
  static inline ObjectFind* New( Graph* , Expr* , Expr* , Checkpoint* );

  ObjectFind( Graph* graph , std::uint32_t id , Expr* object , Expr* key , Checkpoint* cp ):
    StaticRef(HIR_OBJECT_FIND,id,graph)
  {
    lava_debug(NORMAL,lava_verify(GetTypeInference(object) == TPKIND_OBJECT););
    AddOperand(object);
    AddOperand(key);
    AddOperand(cp);
  }

  Expr*           object() const { return operand_list()->First(); }
  Expr*           key   () const { return Operand(1);}
  Checkpoint* checkpoint() const { return operand_list()->Last()->AsCheckpoint(); }

 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(ObjectFind)
};

LAVA_CBASE_HIR_DEFINE(ObjectUpdate,public ObjectResize) {
 public:
  static inline ObjectUpdate* New( Graph* , Expr* , Expr* );
  ObjectUpdate( Graph* graph , std::uint32_t id , Expr* object , Expr* key ):
    ObjectResize(HIR_OBJECT_UPDATE,id,graph,object,key) {}
 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(ObjectUpdate)
};

LAVA_CBASE_HIR_DEFINE(ObjectInsert,public ObjectResize) {
 public:
  static inline ObjectInsert* New( Graph* , Expr* , Expr*  );
  ObjectInsert( Graph* graph , std::uint32_t id , Expr* object , Expr* key ):
    ObjectResize(HIR_OBJECT_INSERT,id,graph,object,key) {}
 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(ObjectInsert)
};

LAVA_CBASE_HIR_DEFINE(ListIndex,public StaticRef) {
 public:
  static inline ListIndex* New( Graph* , Expr* , Expr* , Checkpoint* checkpoint );

  ListIndex( Graph* graph , std::uint32_t id , Expr* object , Expr* index , Checkpoint* checkpoint ):
    StaticRef(HIR_LIST_INDEX,id,graph)
  {
    lava_debug(NORMAL,lava_verify( GetTypeInference(object) == TPKIND_LIST ););
    AddOperand(object);
    AddOperand(index);
    AddOperand(checkpoint);
  }

  Expr*           object() const { return operand_list()->First(); }
  Expr*           index () const { return Operand(1); }
  Checkpoint* checkpoint() const { return operand_list()->Last()->AsCheckpoint(); }
 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(ListIndex)
};

LAVA_CBASE_HIR_DEFINE(ListInsert,public ListResize) {
 public:
  static inline ListInsert* New( Graph* , Expr* , Expr* , Checkpoint* );
  ListInsert( Graph* graph , std::uint32_t id , Expr* object , Expr* index , Checkpoint* cp ):
    ListResize(HIR_LIST_INSERT,id,graph,object,index,cp) {}
 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(ListInsert)
};

// Helper wrapper class to put all reference into one types of node.
//
// Since HIR doesn't allow multiple inheritance in generaly except for
// simple helper class, we cannot represent concept of field reference
// of all the node. All the field reference node are listed as follow:
//
// 1) ListInsert
// 2) ListIndex
// 3) ObjectFind
// 4) ObjectInsert
// 5) ObjectUpdate
//
//
// The above 5 nodes actually represents a pointer/reference points to
// a specific filed/element inside of an object/list. We cannot use normal
// way to categorize them due to ListInsert/ObjectInsert/ObjectUpdate are
// destructive operation with *Resize* operation needed, our HIR doesn't
// lower these operation into *Resize* operation plus Index operation, so
// they cannot be simplified even more.
//
// The following helper class provides a easy way to construct an object
// which can be used to generalize operation of the above 3 different types
// nodes.
class FieldRefNode {
 public:
  // initialize FieldRefNode with reference node, failed with assertion crash
  inline explicit FieldRefNode( Expr* );
 public:
  Expr* node() const { return node_; }
 public:
  // Get the object of this FieldRefNode
  inline Expr* object() const;
  // Get the index/key component of this FieldRefNode
  inline Expr* comp  () const;
 public:
  // Whether this reference node reference into a list
  inline bool IsListRef  () const;
  // Whether this reference node reference into a object
  inline bool IsObjectRef() const;
  // Whether this reference node is just a read ref node, ie node doesn't do resize
  inline bool IsRead     () const;
  // Whether this reference node is a write ref node , ie node does resize if needed
  inline bool IsWrite    () const;
 private:
  Expr* node_;
};


LAVA_CBASE_HIR_DEFINE(RefGet,public ReadEffect) {
 public:
  RefGet( IRType type , std::uint32_t id , Graph* graph , Expr* oref ):
    ReadEffect(type,id,graph)
  { AddOperand(oref); }

  Expr* ref() const { return operand_list()->First(); }
};

LAVA_CBASE_HIR_DEFINE(RefSet,public WriteEffect) {
 public:
  RefSet( IRType type , std::uint32_t id , Graph* graph , Expr* oref,
                                                          Expr* value ):
    WriteEffect(type,id,graph)
  {
    AddOperand(oref);
    AddOperand(value);
  }

  Expr* ref  () const { return operand_list()->First(); }
  Expr* value() const { return operand_list()->Last(); }
};

LAVA_CBASE_HIR_DEFINE(ObjectRefGet,public RefGet) {
 public:
  static ObjectRefGet* New( Graph* , Expr* );

  ObjectRefGet( Graph* graph , std::uint32_t id , Expr* oref ):
    RefGet(HIR_OBJECT_REF_GET,id,graph,oref)
  {
    lava_debug(NORMAL,lava_verify( oref->IsObjectFind()   ||
                                   oref->IsObjectUpdate() ||
                                   oref->IsObjectInsert() ););
  }

 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(ObjectRefGet)
};

LAVA_CBASE_HIR_DEFINE(ObjectRefSet,public RefSet) {
 public:
  static ObjectRefSet* New( Graph* , Expr* , Expr* );

  ObjectRefSet( Graph* graph , std::uint32_t id , Expr* oref , Expr* value ):
    RefSet(HIR_OBJECT_REF_SET,id,graph,oref,value)
  {
    lava_debug(NORMAL,lava_verify( oref->IsObjectFind()   ||
                                   oref->IsObjectUpdate() ||
                                   oref->IsObjectInsert() ););
  }
 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(ObjectRefSet)
};

LAVA_CBASE_HIR_DEFINE(ListRefGet,public RefGet) {
 public:
  static ListRefGet* New( Graph* , Expr* );

  ListRefGet( Graph* graph , std::uint32_t id , Expr* lref ):
    RefGet(HIR_LIST_REF_GET,id,graph,lref)
  {
    lava_debug(NORMAL,lava_verify( lref->IsListIndex() ||
                                   lref->IsListInsert() ););
  }

 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(ListRefGet)
};

LAVA_CBASE_HIR_DEFINE(ListRefSet,public RefSet) {
 public:
  static ListRefSet* New( Graph* , Expr* , Expr* );

  ListRefSet( Graph* graph , std::uint32_t id , Expr* lref , Expr* value ):
    RefSet(HIR_LIST_REF_SET,id,graph,lref,value)
  {
    lava_debug(NORMAL,lava_verify( lref->IsListIndex()  ||
                                   lref->IsListInsert() ););
  }

 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(ListRefSet)
};

} // namespace hir
} // namespace cbase
} // namespace lavascript

#endif //CBASE_HIR_PROP_H_
