#ifndef BYTECODE_GENERATOR_H_
#define BYTECODE_GENERATOR_H_

namespace lavascript {
class Context;
class Script;

namespace parser {
namespace ast {
struct Root;
} // namespace ast
} // namespace parser

namespace interpreter {

/**
 * Generate bytecode from a AST into an Script objects ; if failed then return
 * false and put the error description inside of the error buffer
 */
bool GenerateBytecode( Context* , const ::lavascript::parser::ast::Root& ,
                                  Script* ,
                                  std::string* );


} // namespace interpreter
} // namespace lavascript

#endif // BYTECODE_GENERATOR_H_