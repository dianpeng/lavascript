#ifndef BYTECODE_ANALYZE_H_
#define BYTECODE_ANALYZE_H_
#include "src/config.h"
#include "src/objects.h"
#include "src/interpreter/bytecode.h"
#include "src/interpreter/bytecode-iterator.h"

#include <map>
#include <bitset>

namespace lavascript {
class DumpWriter;

namespace cbase      {

typedef std::bitset<::lavascript::interpreter::kRegisterSize> InterpreterRegisterSet;

/**
 * This is a pre-pass before ir graph construction and it holds
 * several important information for bytecode and will be used
 * through out the backend compiler
 *
 *
 * It mainly solve following problem
 *
 * 1) generate basic block variable assignment , so basically we are
 *    able to tell what variables are defined for different basic block
 *
 * 2) loop header information which contains all entry to loop header and
 *    also information about what variable that is modified inside of the
 *    loop body. Important for generating PHI instruction during IR graph
 *    build for loop
 *
 * 3) nested loop information , used for OSR graph building
 */

class BytecodeAnalyze {
 public:
  struct BasicBlockVariable {
    const BasicBlockVariable* prev;  // parent scope
    InterpreterRegisterSet variable;
    const std::uint32_t* start;
    const std::uint32_t* end  ;      // this is end of the BB it stops when a 
                                     // jump/return happened
    BasicBlockVariable(): prev(NULL), variable(), start(NULL), end(NULL) {}

    bool IsAlive( std::uint8_t ) const;
    void Add( std::uint8_t reg ) { variable[reg] = true; }
  };

  // LoopHeaderInfo captures the loop's internal body information and its nested
  // information
  struct LoopHeaderInfo {
    const LoopHeaderInfo* prev ;  // this pointer points to its parental loop if it has one
    const BasicBlockVariable* bb; // corresponding basic block
    const std::uint32_t* start ;  // start of the bytecode
    const std::uint32_t* end   ;  // end   of the bytecode
    InterpreterRegisterSet phi ;  // variables that have been modified and need to insert PHI
                                  // ahead of the loop
    LoopHeaderInfo(): prev(NULL), bb(NULL), start(NULL), end(NULL) , phi() {}

    const BasicBlockVariable* enclosed_bb() const {
      return bb->prev;
    }
  };

  // Unordered map may not give us too much benifits since we should not have
  // many basic block entry inside of normal function. the std::unordered_map
  // tends to waste lots of memory for its typical underlying implementation
  typedef std::map<const std::uint32_t*,LoopHeaderInfo>     LoopHeaderInfoMap;
  typedef std::map<const std::uint32_t*,BasicBlockVariable> BasicBlockVariableMap;

 public:
  BytecodeAnalyze( const Handle<Prototype>& );
  BytecodeAnalyze( BytecodeAnalyze&& );

  // interfaces
  inline const BasicBlockVariable* LookUpBasicBlock( const std::uint32_t* pc );
  inline const LoopHeaderInfo*     LookUpLoopHeader( const std::uint32_t* pc );

 public:
  // dump the analyzed the result into the DumpWriter output stream
  void Dump( DumpWriter* ) const;

 private:
  // build the liveness for a single bytecode. If the bytecode is used to
  // terminate the basic block then this function returns false otherwise
  // it returns true. The bytecode that gonna terminate the basic block is
  // JMP/CONT/BRK/RET/RETNULL , but we shold never see a JMP inside of this
  // function due to the fact BuildIf will take care of it
  // Notes: other jump will *start* a new basic block , for BuildBytecode it
  // will return true for these types of bytecode.
  bool BuildBytecode    ( interpreter::BytecodeIterator* );
  void BuildBasicBlock  ( interpreter::BytecodeIterator* );
  bool BuildIfBlock     ( interpreter::BytecodeIterator* , const std::uint32_t* ,
                                                           const std::uint32_t** );
  // Check whether the current block is a else if block or just a else block
  // this is needed to tell the difference between else and elif which we use
  // them to *build* correct basic block entry instruction
  bool CheckElifBranch  ( interpreter::BytecodeIterator* , const std::uint32_t* );

  void BuildIf          ( interpreter::BytecodeIterator* );
  void BuildLogic       ( interpreter::BytecodeIterator* );
  void BuildTernary     ( interpreter::BytecodeIterator* );
  void BuildLoop        ( interpreter::BytecodeIterator* );
  void BuildForeverLoop ( interpreter::BytecodeIterator* );

 private:
  bool IsLocalVar   ( std::uint8_t reg ) const {
    return reg < max_local_var_size_;
  }

  // Properly handle register kill event
  void Kill( std::uint8_t reg );

  inline BasicBlockVariable* NewBasicBlockVar( const std::uint32_t* start );

  inline LoopHeaderInfo* NewLoopHeaderInfo( const BasicBlockVariable* bb ,
                                            const std::uint32_t* start );

  LoopHeaderInfo* current_loop()  { return loop_stack_.empty() ? NULL : loop_stack_.back(); }

  BasicBlockVariable* current_bb(){ return basic_block_stack_.back(); }
  const BasicBlockVariable* current_bb() const { return basic_block_stack_.back(); }

 private:
  // which prototype's bytecode needs to be analyzed
  Handle<Prototype> proto_;

  // local cache of max_local_var_size field inside of Prototype to avoid
  // expensive pointer chasing
  std::uint8_t max_local_var_size_;

  // loop header information , pay attention that all the memory is
  // owned by the std::map
  LoopHeaderInfoMap loop_header_info_;

  // basic block variable map information
  BasicBlockVariableMap basic_block_variable_;

  // loop stack , context/state information
  std::vector<LoopHeaderInfo*> loop_stack_;

  // basic block stack, context/state information
  std::vector<BasicBlockVariable*> basic_block_stack_;

  class LoopScope;
  class BasicBlockScope;
  friend class LoopScope;
  friend class BasicBlockScope;

  LAVA_DISALLOW_COPY_AND_ASSIGN(BytecodeAnalyze);
};

inline BytecodeAnalyze::BasicBlockVariable*
BytecodeAnalyze::NewBasicBlockVar( const std::uint32_t* start ) {
  std::pair<BasicBlockVariableMap::iterator,bool>
    ret = basic_block_variable_.insert(std::make_pair(start,BasicBlockVariable()));
  lava_debug(NORMAL,lava_verify(ret.second););
  BasicBlockVariable* node = &(ret.first->second);
  node->prev   = basic_block_stack_.empty() ? NULL : basic_block_stack_.back();
  node->start  = start;
  return node;
}

inline BytecodeAnalyze::LoopHeaderInfo*
BytecodeAnalyze::NewLoopHeaderInfo( const BasicBlockVariable* bb ,
                                    const std::uint32_t* start ) {
  std::pair<LoopHeaderInfoMap::iterator,bool>
    ret = loop_header_info_.insert(std::make_pair(start,LoopHeaderInfo()));
  lava_debug(NORMAL,lava_verify(ret.second););
  LoopHeaderInfo* node = &(ret.first->second);
  node->prev = loop_stack_.empty() ? NULL : loop_stack_.back();
  node->start= start;
  node->bb   = bb;
  return node;
}

inline const BytecodeAnalyze::BasicBlockVariable* BytecodeAnalyze::LookUpBasicBlock(
    const std::uint32_t* pc ) {
  BasicBlockVariableMap::const_iterator itr = basic_block_variable_.find(pc);
  return itr == basic_block_variable_.end() ? NULL : &(itr->second);
}

inline const BytecodeAnalyze::LoopHeaderInfo* BytecodeAnalyze::LookUpLoopHeader(
    const std::uint32_t* pc ) {
  LoopHeaderInfoMap::const_iterator itr = loop_header_info_.find(pc);
  return itr == loop_header_info_.end() ? NULL : &(itr->second);
}

} // namespace lavascript
} // namespace cbase

#endif // BYTECODE_ANALYZE_H_
