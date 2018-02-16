#ifndef PARSER_OPTIMIZER_H_
#define PARSER_OPTIMIZER_H_

#include <string>

namespace lavascript {
namespace zone { struct Zone; }
namespace parser {
namespace ast { struct Node; }


/**
 * Simple optimization pass while doing parsing , it only perform simple ConstantFold
 * that doesn't need a IR consturction.
 */

ast::Node* Optimize( ::lavascript::zone::Zone* , const char* source , ast::Node* ,
                                                                      std::string* );

} // namespace parser
} // namespace lavascript

#endif // PARSER_OPTIMIZER_H_
