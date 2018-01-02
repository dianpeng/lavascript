#ifndef CBASE_HELPER_H_
#define CBASE_HELPER_H_
#include "src/config.h"
#include "src/util.h"
#include "src/interpreter/bytecode-iterator.h"

// Some helper class or function to help the cbase compiler
// code easier to write
namespace lavascript {
namespace cbase {
namespace helper{

// Help to back up an iterator. It records the current position of
// a iterator and once the destructor is called , the iterator will
// be rewund back to prevoius position.
class BackupBytecodeIterator {
 public:
  BackupBytecodeIterator( ::lavascript::interpreter::BytecodeIterator* itr ):
    old_(itr->cursor()),
    itr_(itr)
  {}

  ~BackupBytecodeIterator() {
    itr_->BranchTo(old_);
  }
 private:
  std::size_t old_;
  ::lavascript::interpreter::BytecodeIterator* itr_;
};

} // namespace lavascript
} // namespace cbase
} // namespace lavascript

#endif // CBASE_HELPER_H_
