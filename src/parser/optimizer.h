#ifndef OPTIMIZER_H_
#define OPTIMIZER_H_

#include <string>

namespace lavascript {
namespace zone { struct Zone; }
namespace parser {
namespace ast { struct Node; }


/**
 * Simple optimization pass while doing parsing , it only perform following AST
 * leve optimization:
 *
 * 1. trivial constant folding
 * 2. trivial strength reduction
 *
 * By trivial I mean is expression level , no control flow graph is built
 *
 */

ast::Node* Optimize( ::lavascript::zone::Zone* , const char* source , ast::Node* ,
                                                                      std::string* );

} // namespace parser
} // namespace lavascript

#endif // OPTIMIZER_H_
