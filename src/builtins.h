#ifndef BUILTINS_H_
#define BUILTINS_H_

/** List of builtin functions for lavascript */
#define LAVASCRIPT_BUILTIN_FUNCTIONS(__) \
  /* name  , tag , argument-count */     \
  /** math/arithmetic **/                \
  __(min,MIN,Min,2,"function min needs 2 input arguments, both must be real number" )                    \
  __(max,MAX,Max,2,"function max needs 2 input arguments, both must be real number" )                    \
  __(sqrt,SQRT,Sqrt,1,"function sqrt needs 1 input argument and it must be real number")                 \
  __(sin,SIN,Sin,1,"function sin needs 1 input argument, and it must be real number")                    \
  __(cos,COS,Cos,1,"function cos needs 1 input argument, and it must be real number")                    \
  __(tan,TAN,Tan,1,"function tan needs 1 input argument, and it must be real number")                    \
  __(abs,ABS,Abs,1,"function ceil needs 1 input argument, and it must be real number")                   \
  __(ceil,CEIL,Ceil,1,"function ceil needs 1 input argument, and it must be real number")                \
  __(floor,FLOOR,Floor,1,"function floor needs 1 input argument, and it must be real number")            \
  /** bit shifting **/                                                                                   \
  __(lshift,LSHIFT,LShift,2,"function lshift needs 2 input arguments, both must be real number")         \
  __(rshift,RSHIFT,RShift,2,"function rshift needs 2 input arguments, both must be real number")         \
  __(lro,LRO,LRo,2,"function lro needs 2 input arguments, both must be real number")                     \
  __(rro,RRO,RRo,2,"function rro needs 2 input arguments, both must be real number")                     \
  __(band,BAND,BAnd,2,"function band needs 2 input arguments, both must be real number")                 \
  __(bor,BOR,BOr,2,"function bor  needs 2 input arguments, both must be real number")                    \
  __(bxor,BXOR,BXor,2,"function bxor needs 2 input arguments, both must be real number")                 \
  /** type conversion **/                                                                                \
  __(int,INT,Int,1,"function int needs 1 input argument and must be real/boolean/string")                \
  __(real,REAL,Real,1,"function real needs 1 input argument and must be real/boolean/string")            \
  __(string,STRING,String,1,"function string needs 1 input argument and must be real/boolean/string")    \
  __(boolean,BOOLEAN,Boolean,1,"function boolean needs 1 input argument and must be real/boolean/string")\
  /** list function **/                                                                                  \
  __(push,PUSH,Push,2,"function push needs 2 input arguments, 1st must be list")                         \
  __(pop,POP,Pop,1,"function push needs 1 input argument, and it must be list")                          \
  /** object function **/                                                                                \
  __(set,SET,Set,3,"function set needs 3 input arguments, 1st must be object and 2nd must be string")    \
  __(has,HAS,Has,2,"function has needs 2 input arguments, 1st must be object and 2nd must be string")    \
  __(get,GET,Get,2,"function get needs 2 input arguments, 1st must be object and 2nd must be string")    \
  __(update,UPDATE,Update,3,"function update needs 2 input arguments, 1st must be object and 2nd must be string") \
  __(put,PUT,Put,3,"function put needs 3 input arguments, 1st must be object and 2nd must be string")    \
  __(delete,DELETE,Delete,2,"function delete needs 2 input arguments, 1st must be object and 2nd must be string") \
  /** attributes **/                                                                                     \
  __(clear,CLEAR,Clear,1,"clear needs 1 input argument")                                                 \
  __(type,TYPE,Type,1,"type needs 1 input argument" )                                                    \
  __(len,LEN,Len,1,"len needs 1 input argument")                                                         \
  __(empty,EMPTY,Empty,1,"empty needs 1 input argument")                                                 \
  __(iter,ITER,Iter,1,"iter needs 1 input argument" )

#endif // BUILTINS_H_
