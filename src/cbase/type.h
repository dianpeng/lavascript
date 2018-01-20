#ifndef CBASE_TYPE_H_
#define CBASE_TYPE_H_

#include "src/common.h"
#include "src/trace.h"
#include "src/util.h"


namespace lavascript {
namespace cbase {

/**
 * Type flag definitions
 *
 * The indent here indicate the parental relationship between each type
 */
enum TypeKind {
  TPKIND_UNKNOWN,
    TPKIND_PRIMITIVE,
      TPKIND_NUMBER ,
        TPKIND_FLOAT64,
        TPKIND_INT32,
        TPKIND_INT64,
        TPKIND_UINT32,
        TPKIND_UINT64,
        TPKIND_INDEX,

      TPKIND_STRING,
        TPKIND_LONG_STRING,
        TPKIND_SMALL_STRING,

      TPKIND_BOOLEAN,
      TPKIND_NIL,

    TPKIND_OBJECT,
      TPKIND_INDEXABLE,
        TPKIND_LIST,
        TPYE_OBJECT,
        /* TPKIND_EXTENSION */

      TPKIND_PROPTIABLE,
        /* TPKIND_OBJECT */
        /* TPKIND_EXTENSION */

      TPKIND_CALLABLE,
        TPKIND_CLOSURE,
        TPKIND_EXTENSION,

  SIZE_OF_TYPES
};

const char* GetTypeKindName( TypeKind );


} // namespace cbase
} // namespace lavascript

#endif // CBASE_TYPE_H_
