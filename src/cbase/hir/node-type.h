#ifndef CBASE_HIR_NODE_TYPE_H_
#define CBASE_HIR_NODE_TYPE_H_

// Constant node
#define CBASE_HIR_CONSTANT(__)                                         \
  __(Float64,FLOAT64     ,"float64"     ,Leaf,NoEffect)                \
  __(LString,LONG_STRING ,"lstring"     ,Leaf,NoEffect)                \
  __(SString,SMALL_STRING,"small_string",Leaf,NoEffect)                \
  __(Boolean,BOOLEAN     ,"boolean"     ,Leaf,NoEffect)                \
  __(Nil    ,NIL         ,"null"        ,Leaf,NoEffect)

// High level HIR node. Used to describe unttyped polymorphic operations
#define CBASE_HIR_EXPRESSION_HIGH(__)                                  \
  /* compound */                                                       \
  __(IRList       ,LIST     ,"list"     ,NoLeaf,Effect)                \
  __(IRObjectKV   ,OBJECT_KV,"object_kv",NoLeaf,Effect)                \
  __(IRObject     ,OBJECT   ,"object"   ,NoLeaf,Effect)                \
  /* closure */                                                        \
  __(Closure      ,CLOSURE  ,"closure"  ,Leaf  ,Effect)                \
  __(InitCls      ,INIT_CLS ,"init_cls" ,NoLeaf,Effect)                \
  /* argument node */                                                  \
  __(Arg          ,ARG      ,"arg"      ,Leaf,NoEffect)                \
  /* arithmetic/comparison node */                                     \
  __(Unary        ,UNARY    ,"unary"    ,NoLeaf,NoEffect)              \
  __(Arithmetic   ,ARITHMETIC,"arithmetic",NoLeaf,Effect)              \
  __(Compare      ,COMPARE  ,"compare"  ,NoLeaf,Effect)                \
  __(Logical      ,LOGICAL   ,"logical" ,NoLeaf,NoEffect)              \
  __(Ternary      ,TERNARY  ,"ternary"  ,NoLeaf,NoEffect)              \
  /* upvalue */                                                        \
  __(UGet         ,UGET     ,"uget"     ,Leaf  ,NoEffect)              \
  __(USet         ,USET     ,"uset"     ,Leaf  ,Effect)                \
  /* property/idx */                                                   \
  __(PGet         ,PGET     ,"pget"     ,NoLeaf,Effect)                \
  __(PSet         ,PSET     ,"pset"     ,NoLeaf,Effect)                \
  __(IGet         ,IGET     ,"iget"     ,NoLeaf,Effect)                \
  __(ISet         ,ISET     ,"iset"     ,NoLeaf,Effect)                \
  /* gget */                                                           \
  __(GGet         ,GGET     ,"gget"     ,NoLeaf,Effect)                \
  __(GSet         ,GSET     ,"gset"     ,NoLeaf,Effect)                \
  /* iterator */                                                       \
  __(ItrNew       ,ITR_NEW  ,"itr_new"  ,NoLeaf,Effect)                \
  __(ItrNext      ,ITR_NEXT ,"itr_next" ,NoLeaf,Effect)                \
  __(ItrTest      ,ITR_TEST ,"itr_test" ,NoLeaf,Effect)                \
  __(ItrDeref     ,ITR_DEREF,"itr_deref",NoLeaf,Effect)                \
  /* call     */                                                       \
  __(Call         ,CALL     ,"call"     ,NoLeaf,Effect)                \
  /* intrinsic call */                                                 \
  __(ICall        ,ICALL    ,"icall"    ,NoLeaf,Effect)                \
  /* phi */                                                            \
  __(Phi           ,PHI     ,"phi"      ,NoLeaf,Effect)                \
  /* misc */                                                           \
  __(Projection   ,PROJECTION      ,"projection",NoLeaf,NoEffect)      \
  /* osr */                                                            \
  __(OSRLoad      ,OSR_LOAD        ,"osr_load"  ,Leaf,Effect)          \
  /* checkpoints */                                                    \
  __(Checkpoint   ,CHECKPOINT      ,"checkpoint" ,NoLeaf,NoEffect)     \
  __(StackSlot    ,STACK_SLOT      ,"stack_slot" ,NoLeaf,NoEffect)     \
  /* effect */                                                         \
  __(LoopEffectPhi,LOOP_EFFECT_PHI ,"loop_effect_phi",NoLeaf,Effect)   \
  __(EffectPhi    ,EFFECT_PHI      ,"effect_phi" ,NoLeaf,Effect)       \
  __(InitBarrier  ,INIT_BARRIER    ,"init_barrier",NoLeaf,Effect)      \
  __(EmptyWriteEffect,EMPTY_WRITE_EFFECT,"empty_write_effect",NoLeaf,Effect)    \
  __(BranchStartEffect,BRANCH_START_EFFECT,"branch_start_effect",NoLeaf,Effect)

/**
 * These arithmetic and compare node are used to do typed arithmetic
 * or compare operations. It is used after we do speculative type guess.
 *
 * The node accepts unboxed value for specific type indicated by the node
 * name and output unboxed value. So in general, we should mark the output
 * with a box node. The input should be marked with unbox node or the node
 * itself is a typped node which produce unbox value.
 *
 */
#define CBASE_HIR_EXPRESSION_LOW_ARITHMETIC_AND_COMPARE(__)                     \
  __(Float64Negate    ,FLOAT64_NEGATE    ,"float64_negate"    ,NoLeaf,NoEffect) \
  __(Float64Arithmetic,FLOAT64_ARITHMETIC,"float64_arithmetic",NoLeaf,NoEffect) \
  __(Float64Bitwise   ,FLOAT64_BITWISE   ,"float64_bitwise"   ,NoLeaf,NoEffect) \
  __(Float64Compare   ,FLOAT64_COMPARE   ,"float64_compare"   ,NoLeaf,NoEffect) \
  __(BooleanNot       ,BOOLEAN_NOT       ,"boolean_not"       ,NoLeaf,NoEffect) \
  __(BooleanLogic     ,BOOLEAN_LOGIC     ,"boolean_logic"     ,NoLeaf,NoEffect) \
  __(StringCompare    ,STRING_COMPARE    ,"string_compare"    ,NoLeaf,NoEffect) \
  __(SStringEq        ,SSTRING_EQ        ,"sstring_eq"        ,NoLeaf,NoEffect) \
  __(SStringNe        ,SSTRING_NE        ,"sstring_ne"        ,NoLeaf,NoEffect) \

#define CBASE_HIR_EXPRESSION_LOW_PROPERTY(__)                         \
  __(ObjectFind   ,OBJECT_FIND   ,"object_find"   ,NoLeaf,Effect)     \
  __(ObjectUpdate ,OBJECT_UPDATE ,"object_update" ,NoLeaf,Effect)     \
  __(ObjectInsert ,OBJECT_INSERT ,"object_insert" ,NoLeaf,Effect)     \
  __(ListIndex    ,LIST_INDEX    ,"list_index"    ,NoLeaf,Effect)     \
  __(ListInsert   ,LIST_INSERT   ,"list_insert"   ,NoLeaf,Effect)     \
  __(ObjectRefSet ,OBJECT_REF_SET,"object_ref_set",NoLeaf,Effect)     \
  __(ObjectRefGet ,OBJECT_REF_GET,"object_ref_get",NoLeaf,Effect)     \
  __(ListRefSet   ,LIST_REF_SET  ,"list_ref_set"  ,NoLeaf,Effect)     \
  __(ListRefGet   ,LIST_REF_GET  ,"list_ref_get"  ,NoLeaf,Effect)


// All the low HIR nodes
#define CBASE_HIR_EXPRESSION_LOW(__)                                  \
  CBASE_HIR_EXPRESSION_LOW_ARITHMETIC_AND_COMPARE(__)                 \
  CBASE_HIR_EXPRESSION_LOW_PROPERTY(__)

// Guard node.
//
// Guard node is an expression node so it is floating and can be GVNed
#define CBASE_HIR_GUARD(__)                                           \
  __(Guard       ,GUARD         ,"guard"          ,NoLeaf,NoEffect)

// Guard conditional node , used to do type guess or speculative inline
//
// A null test is same as TestType(object,NULL) since null doesn't have
// any value. And during graph construction any null test , ie x == null,
// will be *normalized* into a TestType then we could just use predicate
// to do inference and have null redundancy removal automatically
#define CBASE_HIR_TEST(__)                                            \
  __(TestType    ,TEST_TYPE      ,"test_type"      ,NoLeaf,NoEffect)

/**
 * Box operation will wrap a value into the internal box representation
 * basically use Value object as wrapper
 * Unbox is on the contary, basically load the actual value inside of
 * Value object into its plain value.
 *
 * It means for primitive type, the actual primitive value will be loaded
 * out ; for heap type, the GCRef (pointer of pointer) will be loaded out.
 *
 * It is added after the lowering phase of the HIR
 */
#define CBASE_HIR_BOXOP(__)                                           \
  __(Box  ,BOX  ,"box"  ,NoLeaf,NoEffect)                             \
  __(Unbox,UNBOX,"unbox",NoLeaf,NoEffect)

/**
 * Cast
 * Cast a certain type's value into another type's value. These type cast
 * doesn't perform any test and should be guaranteed to be correct by
 * the compiler
 */
#define CBASE_HIR_CAST(__)                                            \
  __(ConvBoolean ,CONV_BOOLEAN ,"conv_boolean" ,NoLeaf,NoEffect)      \
  __(ConvNBoolean,CONV_NBOOLEAN,"conv_nboolean",NoLeaf,NoEffect)

/**
 * All the control flow node
 */
#define CBASE_HIR_CONTROL_FLOW(__)                                    \
  __(Start      ,START      ,"start"      ,NoLeaf,NoEffect)           \
  __(End        ,END        ,"end"        ,NoLeaf,NoEffect)           \
  /* osr */                                                           \
  __(OSRStart   ,OSR_START  ,"osr_start"  ,NoLeaf,NoEffect)           \
  __(OSREnd     ,OSR_END    ,"osr_end"    ,NoLeaf,NoEffect)           \
  /* inline */                                                        \
  __(InlineStart,INLINE_START,"inline_start",NoLeaf,NoEffect)         \
  __(InlineEnd  ,INLINE_END  ,"inline_end"  ,NoLeaf,NoEffect)         \
  __(LoopHeader ,LOOP_HEADER,"loop_header",NoLeaf,NoEffect)           \
  __(Loop       ,LOOP       ,"loop"       ,NoLeaf,NoEffect)           \
  __(LoopExit   ,LOOP_EXIT  ,"loop_exit"  ,NoLeaf,NoEffect)           \
  __(If         ,IF         ,"if"         ,NoLeaf,NoEffect)           \
  __(IfTrue     ,IF_TRUE    ,"if_true"    ,NoLeaf,NoEffect)           \
  __(IfFalse    ,IF_FALSE   ,"if_false"   ,NoLeaf,NoEffect)           \
  __(Jump       ,JUMP       ,"jump"       ,NoLeaf,NoEffect)           \
  __(Fail       ,FAIL       ,"fail"       ,Leaf  ,NoEffect)           \
  __(Success    ,SUCCESS    ,"success"    ,NoLeaf,NoEffect)           \
  __(Return     ,RETURN     ,"return"     ,NoLeaf,NoEffect)           \
  __(JumpValue  ,JUMP_VALUE ,"jump_value" ,NoLeaf,NoEffect)           \
  __(Region     ,REGION     ,"region"     ,NoLeaf,NoEffect)           \
  __(CondTrap   ,COND_TRAP  ,"cond_trap"  ,NoLeaf,NoEffect)           \
  __(Trap       ,TRAP       ,"trap"       ,NoLeaf,NoEffect)


#define CBASE_HIR_EXPRESSION(__)                                      \
  CBASE_HIR_CONSTANT(__)                                              \
  CBASE_HIR_EXPRESSION_HIGH(__)                                       \
  CBASE_HIR_EXPRESSION_LOW (__)                                       \
  CBASE_HIR_TEST(__)                                                  \
  CBASE_HIR_BOXOP(__)                                                 \
  CBASE_HIR_CAST (__)                                                 \
  CBASE_HIR_GUARD(__)                                                 \

#define CBASE_HIR_LIST(__)                                            \
  CBASE_HIR_EXPRESSION(__)                                            \
  CBASE_HIR_CONTROL_FLOW(__)

#endif // CBASE_HIR_NODE_TYPE_H_
