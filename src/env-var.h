#ifndef ENV_VAR_H_
#define ENV_VAR_H_

#include <string>

namespace lavascript {

bool GetEnvVar( const char* name , std::string* );
bool GetEnvVar( const char* name , const char** );
bool GetEnvVar( const char* name , int* );
bool GetEnvVar( const char* name , bool* );

} // namespace lavascript

#endif // ENV_VAR_H_
