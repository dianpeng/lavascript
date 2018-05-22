#ifndef CBASE_HIR_MEMORY_H_
#define CBASE_HIR_MEMORY_H_
#include "effect.h"

namespace lavascript {
namespace cbase      {
namespace hir        {

// MemoryNode
// This node represents those nodes that is 1) mutable and 2) stay on heap or potentially
// stay on heap.
// 1. Arg
// 2. IRList
// 3. IRObject
//
// The above nodes are memory node since the mutation on these objects generates a observable
// side effect which must be serialized. For each operation , we will find its memory node
// if applicable and then all the operations will be serialized during graph building phase to
// ensure correct program behavior
LAVA_CBASE_HIR_DEFINE(NO_META,MemoryNode,public Expr) {
 public:
  MemoryNode( IRType type , std::uint32_t id , Graph* g ): Expr(type,id,g){}
};


// --------------------------------------------------------------------------
// Argument
LAVA_CBASE_HIR_DEFINE(Tag=ARG;Name="arg";Leaf=Leaf;Effect=Effect,
    Arg,public MemoryNode) {
 public:
  inline static Arg* New( Graph* , std::uint32_t );
  std::uint32_t index() const { return index_; }
  Arg( Graph* graph , std::uint32_t id , std::uint32_t index ):
    MemoryNode (HIR_ARG,id,graph),
    index_(index)
  {}
 public:
  virtual std::uint64_t GVNHash() const {
    return GVNHash1(type_name(),index());
  }
  virtual bool Equal( const Expr* that ) const {
    return that->IsArg() && (that->AsArg()->index() == index());
  }
 private:
  std::uint32_t index_;
  LAVA_DISALLOW_COPY_AND_ASSIGN(Arg)
};

// --------------------------------------------------------------------------
// OSRLoad
LAVA_CBASE_HIR_DEFINE(Tag=OSR_LOAD;Name="osr_load";Leaf=Leaf;Effect=Effect,
    OSRLoad,public Expr) {
 public:
  inline static OSRLoad* New( Graph* , std::uint32_t );
  // Offset in sizeof(Value)/8 bytes to load this value from osr input buffer
  std::uint32_t index() const { return index_; }
  virtual std::uint64_t GVNHash() const {
    return GVNHash1(type_name(),index());
  }
  virtual bool Equal( const Expr* that ) const {
    return that->IsOSRLoad() && (that->AsOSRLoad()->index() == index());
  }
  OSRLoad( Graph* graph , std::uint32_t id , std::uint32_t index ):
    Expr  ( HIR_OSR_LOAD , id , graph ),
    index_(index)
  {}
 private:
  std::uint32_t index_;
  LAVA_DISALLOW_COPY_AND_ASSIGN(OSRLoad)
};

// To make lookup a subcomponent from a list or object have uniform
// interface, we create this class to help us out. Many cases that
// optimization pass needs to treat IRList and IRObject as a lookup
// interface.
class ComponentBase {
 public:
  // Get a node with index or key from this object , if it cannot
  // find one with the input key specified as Expr node , then it
  // returns NULL
  virtual Expr* Load ( Expr*         ) const = 0;
  virtual bool  Store( Expr* , Expr* )       = 0;
};

// --------------------------------------------------------------------------
// IRList
LAVA_CBASE_HIR_DEFINE(Tag=LIST;Name="list";Leaf=NoLeaf;Effect=Effect,
    IRList,public WriteEffect,public ComponentBase) {
 public:
  inline static IRList* New( Graph* , std::size_t size );
  void Add( Expr* node ) { AddOperand(node); }
  std::size_t Size() const { return operand_list()->size(); }
  IRList( Graph* graph , std::uint32_t id , std::size_t size ):
    WriteEffect(HIR_LIST,id,graph)
  {
    (void)size; // implicit indicated by the size of operand_list()
  }

  virtual Expr* Load ( Expr*         ) const;
  virtual bool  Store( Expr* , Expr* ) ;
 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(IRList)
};

// --------------------------------------------------------------------------
// IRObjectKV
LAVA_CBASE_HIR_DEFINE(Tag=OBJECT_KV;Name="object_kv";Leaf=NoLeaf;Effect=NoEffect,
    IRObjectKV,public Expr) {
 public:
  inline static IRObjectKV* New( Graph* , Expr* , Expr* );
  Expr* key  () const { return operand_list()->First(); }
  Expr* value() const { return operand_list()->Last (); }
  void set_key  ( Expr* key ) { lava_debug(NORMAL,lava_verify(key->IsString());); ReplaceOperand(0,key); }
  void set_value( Expr* val ) { ReplaceOperand(1,val); }
  IRObjectKV( Graph* graph , std::uint32_t id , Expr* key , Expr* val ):
    Expr(HIR_OBJECT_KV,id,graph)
  {
    lava_debug(NORMAL,lava_verify(key->IsString()););
    AddOperand(key);
    AddOperand(val);
  }
 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(IRObjectKV)
};

// --------------------------------------------------------------------------
// IRObject
LAVA_CBASE_HIR_DEFINE(Tag=OBJECT;Name="object";Leaf=NoLeaf;Effect=Effect,
    IRObject,public WriteEffect,public ComponentBase) {
 public:
  inline static IRObject* New( Graph* , std::size_t size );
  void Add( Expr* key , Expr* val ) {
    lava_debug(NORMAL,lava_verify(key->IsString()););
    AddOperand(IRObjectKV::New(graph(),key,val));
  }
  std::size_t Size() const { return operand_list()->size(); }
  IRObject( Graph* graph , std::uint32_t id , std::size_t size ):
    WriteEffect(HIR_OBJECT,id,graph)
  {
    (void)size;
  }

  virtual Expr* Load ( Expr*         ) const;
  virtual bool  Store( Expr* , Expr* ) ;
 private:
  LAVA_DISALLOW_COPY_AND_ASSIGN(IRObject)
};

} // namespace hir
} // namespace cbase
} // namespace lavascript
#endif // CBASE_HIR_MEMORY_H_
