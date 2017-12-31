#include <src/interpreter/bytecode-builder.h>
#include <src/trace.h>
#include <src/source-code-info.h>
#include <gtest/gtest.h>

namespace lavascript {
namespace interpreter {

TEST(BytecodeBuilder,AllBytecodeType) {
  // TYPE_B
  {
    BytecodeBuilder bb;
    bb.feend(0,SourceCodeInfo(),1,2);
    BytecodeIterator itr(bb.GetIterator());
    ASSERT_TRUE(itr.HasNext());
    ASSERT_TRUE(itr.type() == TYPE_B);
    ASSERT_EQ(BC_FEEND,itr.opcode()) << itr.opcode_name();
    std::uint8_t a1;
    std::uint16_t a2;
    itr.GetOperand(&a1,&a2);
    ASSERT_EQ(a1,1);
    ASSERT_EQ(a2,2);
  }

  // TYPE_C
  {
    BytecodeBuilder bb;
    bb.loadcls(0,SourceCodeInfo(),1,2);
    BytecodeIterator itr(bb.GetIterator());
    ASSERT_TRUE(itr.HasNext());
    ASSERT_TRUE(itr.type() == TYPE_B);
    ASSERT_EQ(BC_LOADCLS,itr.opcode()) << itr.opcode_name();
    std::uint8_t a1;
    std::uint16_t a2;
    itr.GetOperand(&a1,&a2);
    ASSERT_EQ(a1,1);
    ASSERT_EQ(a2,2);
  }

  // TYPE_E
  {
    BytecodeBuilder bb;
    bb.move(0,SourceCodeInfo(),1,255);
    BytecodeIterator itr(bb.GetIterator());
    ASSERT_TRUE(itr.HasNext());
    ASSERT_TRUE(itr.type() == TYPE_E);
    ASSERT_EQ(BC_MOVE,itr.opcode()) << itr.opcode_name();
    std::uint8_t a1,a2;
    itr.GetOperand(&a1,&a2);
    ASSERT_EQ(1,a1);
    ASSERT_EQ(255,a2);
  }

  // TYPE_D
  {
    BytecodeBuilder bb;
    bb.loadobj1(0,SourceCodeInfo(),1,2,3);
    BytecodeIterator itr(bb.GetIterator());
    ASSERT_TRUE(itr.HasNext());
    ASSERT_TRUE(itr.type() == TYPE_D);
    ASSERT_EQ(BC_LOADOBJ1,itr.opcode()) << itr.opcode_name();
    std::uint8_t a1,a2,a3;
    itr.GetOperand(&a1,&a2,&a3);
    ASSERT_EQ(1,a1);
    ASSERT_EQ(2,a2);
    ASSERT_EQ(3,a3);
  }

  // TYPE_F
  {
    BytecodeBuilder bb;
    bb.load0(0,SourceCodeInfo(),1);
    BytecodeIterator itr(bb.GetIterator());
    ASSERT_TRUE(itr.HasNext());
    ASSERT_TRUE(itr.type() == TYPE_F);
    ASSERT_EQ(BC_LOAD0,itr.opcode()) << itr.opcode_name();
    std::uint8_t a1;
    itr.GetOperand(&a1);
    ASSERT_EQ(1,a1);
  }

  // TYPE_G
  {
    BytecodeBuilder bb;
    bb.jmp(0,SourceCodeInfo(),65534);
    BytecodeIterator itr(bb.GetIterator());
    ASSERT_TRUE(itr.HasNext());
    ASSERT_TRUE(itr.type() == TYPE_G);
    ASSERT_EQ(BC_JMP,itr.opcode()) << itr.opcode_name();
    std::uint16_t a;
    itr.GetOperand(&a);
    ASSERT_EQ(65534,a);
  }
}

TEST(BytecodeBuilder,Coverage) {
  /** --------------------------------
   * try to generate *all* bytecodes
   * inside of the bytecode builder
   * --------------------------------*/
#define __(A,B,C,...) GE_##A(C);

#define GE_B(FUNC) bb.FUNC(0,SourceCodeInfo(),1,65535)
#define GE_C(FUNC) bb.FUNC(0,SourceCodeInfo(),65535,1)
#define GE_D(FUNC) bb.FUNC(0,SourceCodeInfo(),1,2,3)
#define GE_E(FUNC) bb.FUNC(0,SourceCodeInfo(),1,2)
#define GE_F(FUNC) bb.FUNC(0,SourceCodeInfo(),1)
#define GE_G(FUNC) bb.FUNC(0,SourceCodeInfo(),65535)
#define GE_H(FUNC)
#define GE_X(FUNC) bb.FUNC(0,SourceCodeInfo())

  BytecodeBuilder bb;
  LAVASCRIPT_BYTECODE_LIST(__);

#undef GE_B
#undef GE_C
#undef GE_D
#undef GE_F
#undef GE_G
#undef GE_H
#undef GE_X
#undef __ // __

  /* ----------------------------------------
   * now we need to test against it here.   |
   * ---------------------------------------*/

#define __(A,B,C,...) \
  do {                                      \
    ++count;                                \
    ASSERT_TRUE(itr.HasNext());             \
    if(GetBytecodeType(BC_##B) != TYPE_H) { \
      ASSERT_EQ(BC_##B,itr.opcode()) << itr.opcode_name();     \
   }                                                           \
    BCTEST_##A();                           \
  } while(false);

#define BCTEST_B() \
  do {                                      \
    std::uint8_t a1; std::uint16_t a2;      \
    itr.GetOperand(&a1,&a2);                \
    ASSERT_EQ(1,a1);                        \
    ASSERT_EQ(65535,a2);                    \
    itr.Move();                             \
  } while(false)

#define BCTEST_C() \
  do {                                      \
    std::uint16_t a1; std::uint8_t a2;      \
    itr.GetOperand(&a1,&a2);                \
    ASSERT_EQ(65535,a1);                    \
    ASSERT_EQ(1,a2);                        \
    itr.Move();                             \
  } while(false)

#define BCTEST_D() \
  do {                                      \
    std::uint8_t a1; std::uint8_t a2; std::uint8_t a3; \
    itr.GetOperand(&a1,&a2,&a3);            \
    ASSERT_EQ(1,a1);                        \
    ASSERT_EQ(2,a2);                        \
    ASSERT_EQ(3,a3);                        \
    itr.Move();                             \
  } while(false)

#define BCTEST_E() \
  do {                                      \
    std::uint8_t a1,a2;                     \
    itr.GetOperand(&a1,&a2);                \
    ASSERT_EQ(1,a1);                        \
    ASSERT_EQ(2,a2);                        \
    itr.Move();                             \
  } while(false)

#define BCTEST_F() \
  do {                                      \
    std::uint8_t a1;                        \
    itr.GetOperand(&a1);                    \
    ASSERT_EQ(1,a1);                        \
    itr.Move();                             \
  } while(false)

#define BCTEST_G() \
  do {                                      \
    std::uint16_t a1;                       \
    itr.GetOperand(&a1);                    \
    ASSERT_EQ(65535,a1);                    \
    itr.Move();                             \
  } while(false)

#define BCTEST_H() do {} while(false)

#define BCTEST_X() do { itr.Move(); } while(false)

  BytecodeIterator itr(bb.GetIterator());
  std::size_t count = 0;

  LAVASCRIPT_BYTECODE_LIST(__);
  ASSERT_FALSE(itr.HasNext());
  ASSERT_EQ(count,SIZE_OF_BYTECODE);

#undef BCTEST_B
#undef BCTEST_C
#undef BCTEST_D
#undef BCTEST_E
#undef BCTEST_F
#undef BCTEST_G
#undef BCTEST_X
#undef BCTEST_H
#undef __
}

TEST(BytecodeBuilder,Patch) {
  BytecodeBuilder bb;
  BytecodeBuilder::Label l;
  l = (bb.jmpt(0,SourceCodeInfo(),255));
  l.Patch(1024);

  l = (bb.jmpf(0,SourceCodeInfo(),255));
  l.Patch(1024);

  l = (bb.and_(0,SourceCodeInfo(),255,255));
  l.Patch(1024);

  l = (bb.or_ (0,SourceCodeInfo(),255,255));
  l.Patch(1024);

  l = (bb.jmp(0,SourceCodeInfo()));
  l.Patch(1024);

  l = (bb.brk(0,SourceCodeInfo()));
  l.Patch(1024);

  l = (bb.cont(0,SourceCodeInfo()));
  l.Patch(1024);

  l = (bb.fstart(0,SourceCodeInfo(),255));
  l.Patch(1024);

  l = (bb.festart(0,SourceCodeInfo(),255));
  l.Patch(1024);

  l = (bb.tern(0,SourceCodeInfo(),255,255));
  l.Patch(1024);

  BytecodeIterator itr(bb.GetIterator());

#define TEST1(OPCODE) \
  do {                                                   \
    ASSERT_TRUE(itr.HasNext());                          \
    ASSERT_EQ(OPCODE,itr.opcode()) << itr.opcode_name(); \
    std::uint16_t a1;                                    \
    itr.GetOperand(&a1);                                 \
    ASSERT_EQ(1024,a1);                                  \
  } while(false)

#define TEST2(OPCODE) \
  do {                                                   \
    ASSERT_TRUE(itr.HasNext());                          \
    ASSERT_EQ(OPCODE,itr.opcode()) << itr.opcode_name(); \
    std::uint8_t a1; std::uint16_t a2;                   \
    itr.GetOperand(&a1,&a2);                             \
    ASSERT_EQ(255,a1);                                   \
    ASSERT_EQ(1024,a2);                                  \
  } while(false)

#define TEST3(OPCODE) \
  do {                                                   \
    ASSERT_TRUE(itr.HasNext());                          \
    ASSERT_EQ(OPCODE,itr.opcode()) << itr.opcode_name(); \
    std::uint8_t a1; std::uint8_t a2;                    \
    std::uint8_t a3; std::uint32_t a4;                   \
    itr.GetOperand(&a1,&a2,&a3,&a4);                     \
    ASSERT_EQ(255,a1);                                   \
    ASSERT_EQ(1024,a4);                                  \
  } while(false)


  TEST2(BC_JMPT); ASSERT_TRUE(itr.Move());
  TEST2(BC_JMPF); ASSERT_TRUE(itr.Move());
  TEST3(BC_AND) ; ASSERT_TRUE(itr.Move());
  TEST3(BC_OR)  ; ASSERT_TRUE(itr.Move());
  TEST1(BC_JMP) ; ASSERT_TRUE(itr.Move());
  TEST1(BC_BRK) ; ASSERT_TRUE(itr.Move());
  TEST1(BC_CONT); ASSERT_TRUE(itr.Move());
  TEST2(BC_FSTART); ASSERT_TRUE(itr.Move());
  TEST2(BC_FESTART);ASSERT_TRUE(itr.Move());
  TEST3(BC_TERN); ASSERT_FALSE(itr.Move());

#undef TEST1  // TEST1
#undef TEST2  // TEST2

}


} // namespace interpreter
} // namespace lavascript

int main( int argc, char* argv[] ) {
  ::lavascript::InitTrace("-");
  testing::InitGoogleTest(&argc,argv);
  return RUN_ALL_TESTS();
}
