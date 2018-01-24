#ifndef CBASE_TYPE_H_
#define CBASE_TYPE_H_

namespace lavascript {
namespace cbase {

/**
 * Type flag definitions
 *
 * The indent here indicate the parental relationship between each type
 */
#define LAVASCRIPT_CBASE_TYPE_KIND_LIST(__) \
  __(unknown,UNKNOWN)                                   \
  __(primitive,PRIMITIVE)                               \
  __(number,NUMBER)                                     \
  __(float64,FLOAT64)                                   \
  __(int32  ,INT32  )                                   \
  __(int64  ,INT64  )                                   \
  __(uint32 ,UINT32 )                                   \
  __(uint64 ,UINT64 )                                   \
  __(index  ,INDEX  )                                   \
  __(string ,STRING )                                   \
  __(long_string,LONG_STRING)                           \
  __(small_string,SMALL_STRING)                         \
  __(boolean,BOOLEAN)                                   \
  __(nil,NIL)                                           \
  __(object,OBJECT)                                     \
  __(list,LIST)                                         \
  __(indexable,INDEXABLE)                               \
  __(proptiable,PROPTIABLE)                             \
  __(array_indexable,ARRAY_INDEXABLE)                   \
  __(callable,CALLABLE)                                 \
  __(closure,CLOSURE)                                   \
  __(extension,EXTENSION)


enum TypeKind {

#define __(A,B) TPKIND_##B,

  LAVASCRIPT_CBASE_TYPE_KIND_LIST(__)

#undef __ // __

  SIZE_OF_TYPE_KIND
};

const char* GetTypeKindName( TypeKind );

} // namespace cbase
} // namespace lavascript

#endif // CBASE_TYPE_H_
