#ifndef TRACE_H_
#define TRACE_H_

// trace
// The interpreter has states when do interpretation
//
// 1) Initially interpreter do the interpretation and do the *counting* for hot loops and
//    hot functions. Both are able to trigger a profile states
//
// 2) Once a hot loop or a hot function is identified, then interpreter starts to patch its
//    dispatch table and enter into profiling mode/state. The profiling mode means interpreter
//    starts to profile some of the instruction to get type information. This includes all
//    instruction that mark itself as FB , typically all the arithmetic and comparison BCs
//    and branch instruction like call and property related instruction
//
// 3) The recorded information is stored inside of hash table which uses the last 10 bits of
//    instruction's pointer. This means it will start to collide after 1024 BCs , if that
//    happen it is Okay, since we just genereate a code with wrong assumption. In practise
//    you should not see this too often since a function has 1024 BCs are rare to see.
//
// 4) The recorded will stop when the function calls is done *or* the loop finishes certain
//    number of iteration. And JIT job will be dispatch for that function.
//
// 5) The interpreter currently enters into state for *JIT* for certain BCs handler. It just
//    keep wathcing whether the JIT is done. If it is done, then we start to jump into the
//    jitted method either via osr or direct call

namespace lavascript {
namespace trace {

}
}

#endif // TRACE_H_
