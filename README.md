#Lavascript

This project is a self project that aims to learn and research JIT compilation techinique and compiler knowledge w.r.t
weak type language. The lavascript is a small light weight script that looks like javascript but really assemble Lua.
The language feature is pretty limited , including ,
1) weak type
2) first order function
3) Lua style closure and upvalue access
4) C++ object extension

The goal of the project is to implement a efficient enough JIT compiler for this language.
This project is under development.

# Techinique Detail

The project features a 1) assembly written interpreter 2) method based JIT compiler.

## Interpreter

The interpreter currently is implemented via LuaJIT's awesome DynAsm library in X64 platform. It is a hand crafted
interpreter does code threading plus assembly trick. The performance of it , when testing against recursive fibonacci ,
is nearly the same as LuaJIT's interpreter on my devbox. As with other code , the interpreter obviously has similar
performance with LuaJIT's interpreter , though some code it is little slower but some other code it is a little bit faster.
I've tested it against official Lua and it is easily 50%-100% faster.

The interpreter is *mostly* done with exception of:

1) the tail call implementation is not really tail call in order to maintain debug stack trace. This can be optimized
2) some intrinsic call is not implemented.
3) lacks good code test coverage

## Compiler

Currently I am working on the method JIT called cbase. The method JIT is a normal method JIT which is designed to do

### machine independent
1) OSR
2) normal method entry

It features a sea-of-nodes style IR, and the following optimization will be or have been implemented:

1) expression level optimization , like constant folding, strength reduction, reassociation, algebra/phi simplification
2) global value numbering
3) memory optimization with store forwarding and collapsing. dead memory elimination is performed implicitly
4) loop/loop induction variable analyze, for typping the loop iv since loop iv's type is not decided
5) dead code elimination (TODO)
6) inline
7) due to SSA style IR, we implicitly performs many optimization, like constant forwarding , copy elimination , etc
8) global code motion    (TODO)
9) if applicable, I want to do a loop induction variable range propogation plus scalar evoluation analysis based on
   reocurrance algebra. This also can help me finish loop unswitch and loop predication. Currently array bounds check
   is not optimized except for global value numbering
10) inference. This implements a simple logic system and perform global inference on predicate of if/else and guard to
    help simplify predicate and also eliminate guard

### machine dependent
11) instruction selection/scheduling (TODO)
12) RA by linear scan (TODO)
13) peephole (TODO)

## GC
Currently GC is *not* implemented , the code doesn't work right now. The planning of GC is a normal incremental,
generational GC. It will be added once I finish the compiler. Though compiler definitly takes care of the stack/register
mapping to allow GC enter and also safepoint.

# Notes
Since I don't have too much spare time to work on it, I will just try my best to spend cycle on it. Plus it is a learning
process instead of trying to achieve something that is useful. So in general the idea implemented inside of it is either
kind of crazy or unstable. Don't count on this project for anything. The purpose of this project is for me to learn the
corresponding paper and technique. It is out of the fraustration of the fact that not much useful and deep material existed
in compiler world , let along weak type language JIT compiler. They are either too complicated , hard to understand or
too simple , like writing a brainfuck compiler in blablaba.If one day I finish this project, I will work out documentation
to document what I learned , the method I use to help other people.
