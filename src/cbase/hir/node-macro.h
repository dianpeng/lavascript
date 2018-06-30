#ifndef CBASE_HIR_NODE_MACRO_H_
#define CBASE_HIR_NODE_MACRO_H_

// Use the macro to define the *HIR* node. This specific macro will be recognized
// by a preprocessor which will figure out the class heirachy and generate a file
// which will be used to do simple type mapping. This method will avoid the need to
// use hand crafted type mapping and make core easier to maintain.
#define LAVA_CBASE_HIR_DEFINE(META,NAME,...) class NAME : __VA_ARGS__

// Marker to mark the define macro to be with no meta node
#define HIR_INTERNAL

#endif // CBASE_HIR_NODE_MACRO_H_
