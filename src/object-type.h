#ifndef OBJECT_TYPE_H_
#define OBJECT_TYPE_H_

namespace lavascript {

#define LAVASCRIPT_HEAP_OBJECT_LIST(__)                               \
  __( TYPE_ITERATOR,  Iterator, "iterator")                           \
  __( TYPE_LIST    ,  List    , "list"    )                           \
  __( TYPE_SLICE   ,  Slice   , "slice"   )                           \
  __( TYPE_OBJECT  ,  Object  , "object"  )                           \
  __( TYPE_MAP     ,  Map     , "map"     )                           \
  __( TYPE_STRING  ,  String  , "string"  )                           \
  __( TYPE_PROTOTYPE, Prototype,"prototype")                          \
  __( TYPE_CLOSURE ,  Closure , "closure" )                           \
  __( TYPE_EXTENSION, Extension,"extension")                          \
  __( TYPE_SCRIPT  ,  Script ,  "script"   )

#define LAVASCRIPT_PRIMITIVE_TYPE_LIST(__)                            \
  __( TYPE_REAL    , Real    , "real"    )                            \
  __( TYPE_BOOLEAN , Boolean , "boolean" )                            \
  __( TYPE_NULL    , Null    , "null"    )                            \


/* NOTES: Order matters */
#define LAVASCRIPT_VALUE_TYPE_LIST(__)                                \
  LAVASCRIPT_HEAP_OBJECT_LIST(__)                                     \
  LAVASCRIPT_PRIMITIVE_TYPE_LIST(__)

#define __(A,B,C) LAVASCRIPT_UNUSED_##A,
enum {
  LAVASCRIPT_HEAP_OBJECT_LIST(__)
  SIZE_OF_HEAP_OBJECT
};
#undef __ // __

#define __(A,B,C) LAVASCRIPT_UNUSED_##A,
enum {
  LAVASCRIPT_PRIMITIVE_TYPE_LIST(__)
  SIZE_OF_PRIMITIVE_TYPE
};
#undef __ // __

enum ValueType {
#define __(A,B,C) A,
  LAVASCRIPT_VALUE_TYPE_LIST(__)
  SIZE_OF_VALUE_TYPES
#undef __ // __
};

const char* GetValueTypeName( ValueType );

template< typename T > struct GetObjectType {};

#define __(A,B,C)                               \
  class B; template<> struct GetObjectType<B> { \
    static const ValueType value = A;           \
  };

LAVASCRIPT_HEAP_OBJECT_LIST(__)

#undef __ // __

} // namespace lavascript

#endif // OBJECT_TYPE_H_
