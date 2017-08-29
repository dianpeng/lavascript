#include "parser.h"

#include <src/ast/ast.h>
#include <src/zone/zone.h>


namespace lavascript {
namespace parser {

ast::Node* Parser::ParseAtomic() {
  ast::Node* ret;
  switch(lexer_.current().token) {
  }
}
