#ifndef CBASE_FOLD_FOLDER_H_
#define CBASE_FOLD_FOLDER_H_

#include "src/zone/zone.h"
#include "src/cbase/hir.h"

#include <vector>
#include <memory>

namespace lavascript {
namespace cbase      {
namespace hir        {

// We should not use Expr node since they are designed to be part of the graph instead
// of using as a standalone node structure. To workaround it, we have a special wrapper
// data structure to be used to submit a folding request.
enum FoldType {
  FOLD_UNARY,
  FOLD_BINARY,
  FOLD_TERNARY,
  FOLD_PHI ,
  FOLD_EXPR
};

struct UnaryFolderData;
struct BinaryFolderData;
struct PhiFolderData;
struct TernaryFolderData;
struct StoreFolderData;
struct LoadFolderData;
struct ExprFolderData;

class FolderData {
 public:
  FoldType fold_type() const { return fold_type_; }

  FolderData( FoldType type ) : fold_type_(type) {}
 private:
  FoldType fold_type_;
};

struct UnaryFolderData : public FolderData {
  Unary::Operator op;
  Expr*         node;

  UnaryFolderData( Unary::Operator o , Expr* n ): FolderData(FOLD_UNARY), op(o), node(n) {}
};

struct BinaryFolderData : public FolderData {
  Binary::Operator op;
  Expr*           lhs;
  Expr*           rhs;

  BinaryFolderData( Binary::Operator o , Expr* l , Expr* r ): FolderData(FOLD_BINARY), op(o), lhs(l), rhs(r) {}
};

struct PhiFolderData : public FolderData {
  Expr* lhs;
  Expr* rhs;
  ControlFlow* region;
  PhiFolderData( Expr* l , Expr* r , ControlFlow* cf ): FolderData(FOLD_PHI), lhs(l) , rhs(r), region(cf) {}
};

struct TernaryFolderData : public FolderData {
  Expr* cond;
  Expr* lhs ;
  Expr* rhs ;

  TernaryFolderData( Expr* c, Expr* l, Expr* r ): FolderData(FOLD_TERNARY), cond(c), lhs (l), rhs (r) {}
};

struct ExprFolderData : public FolderData {
  Expr* node;
  ExprFolderData( Expr* n ): FolderData(FOLD_EXPR), node(n) {}
};


class Folder {
 public:
  // Use to predicate whether this folder can work with this folder request
  // or not. If so, then the Fold callback function will be invoked
  virtual bool Predicate( const FolderData& ) const = 0;

  // Fold the input folder data request. If it can fold anything, it will
  // return the folded value as a node
  virtual Expr* Fold    ( Graph* , const FolderData& ) = 0;

  // dtor
  virtual ~Folder() {}
};

// Factory that is used to create concrete folder. Use it with the register
// macro to automatically register folder creation process internally
class FolderFactory {
 public:
  virtual std::unique_ptr<Folder> Create( zone::Zone* ) = 0;
  virtual ~FolderFactory() {}
 public:
  struct FolderFactoryEntry {
    std::string                       name;
    std::unique_ptr<FolderFactory> factory;

    FolderFactoryEntry( FolderFactoryEntry&& entry ):
      name(std::move(entry.name)),
      factory(std::move(entry.factory))
    {}
    FolderFactoryEntry( const char* n , std::unique_ptr<FolderFactory>&& ff ):
      name   (n),
      factory(std::move(ff))
    {}
  };

  typedef std::vector<FolderFactoryEntry> FolderFactoryEntryList;

  static void RegisterFactory( const char* name , std::unique_ptr<FolderFactory>&& );

  static FolderFactoryEntryList& GetFolderFactoryEntryList();
};

#define LAVA_REGISTER_FOLDER(NAME,FAC,OBJ)                                           \
  class FAC : public FolderFactory {                                                 \
   public:                                                                           \
    virtual std::unique_ptr<Folder> Create( zone::Zone* zone ) {                     \
      return std::unique_ptr<Folder>(new OBJ(zone));                                 \
    }                                                                                \
  };                                                                                 \
  class FAC##Registry {                                                              \
   public:                                                                           \
    FAC##Registry() {                                                                \
     FolderFactory::RegisterFactory(NAME,std::unique_ptr<FolderFactory>(new FAC));   \
    }                                                                                \
  };                                                                                 \
  static FAC##Registry k##FAC##StaticRegistry

// Main interface for performing the folding algorithm for simplifying expression
class FolderChain {
 public:
  FolderChain( zone::Zone* );

  // main entry for performing the folding algorithm
  Expr* Fold( Graph* , const FolderData& );
 private:
  zone::Zone* zone_;
  std::vector<std::unique_ptr<Folder>> chain_;
};


} // namespace hir
} // namespace cbase
} // namespace lavascript

#endif // CBASE_FOLD_FOLDER_H_
