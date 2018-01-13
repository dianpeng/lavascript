#ifndef BUILTINS_H_
#define BUILTINS_H_


/**
 * List of builtin functions for lavascript
 */
#define LAVASCRIPT_BUILTIN_FUNCTIONS(__) \
  /* name  , tag , argument-count */     \
  /** math/arithmetic **/                \
  __(min   , MIN , 2)                    \
  __(max   , MAX , 2)                    \
  __(sqrt  , SQRT, 1)                    \
  __(sin   , SIN , 1)                    \
  __(cos   , COS , 1)                    \
  __(tan   , TAN,  1)                    \
  __(abs   , ABS  ,1)                    \
  __(ceil  , CEIL, 1)                    \
  __(floor , FLOOR,1)                    \
  /** bit shifting **/                   \
  __(lshift,LSHIFT,2)                    \
  __(rshift,RSHIFT,2)                    \
  __(lror  ,LROR  ,2)                    \
  __(rror  ,RROR  ,2)                    \
  __(band  ,BAND  ,2)                    \
  __(bor   ,BOR   ,2)                    \
  __(bxor  ,BXOR  ,2)                    \
  /** type conversion **/                \
  __(int   ,INT   ,1)                    \
  __(real  ,REAL  ,1)                    \
  __(string,STRING,1)                    \
  __(boolean,BOOLEAN,1)                  \
  /** list function **/                  \
  __(push  ,PUSH,  2)                    \
  __(pop   ,POP ,  1)                    \
  /** object function **/                \
  __(add   ,ADD ,  3)                    \
  __(has   ,HAS ,  2)                    \
  __(update,UPDATE,3)                    \
  __(put   ,PUT ,  3)                    \
  __(remove,REMOVE,2)                    \
  /** attributes **/                     \
  __(clear ,CLEAR ,1)                    \
  __(type  ,TYPE  ,1)                    \
  __(len   ,LEN   ,1)                    \
  __(empty ,EMPTY ,1)


#endif // BUILTINS_H_
