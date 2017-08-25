#ifndef CORE_ENV_VAR_H_
#define CORE_ENV_VAR_H_

#include <string>

namespace lavascript {
namespace core {

bool GetEnvVar( const char* name , std::string* );
bool GetEnvVar( const char* name , const char** );
bool GetEnvVar( const char* name , int* );
bool GetEnvVar( const char* name , bool* );

} // namespace core
} // namespace lavascript

#endif // CORE_ENV_VAR_H_
