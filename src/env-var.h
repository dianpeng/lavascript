#ifndef ENV_VAR_H_
#define ENV_VAR_H_
#include <cstdint>
#include <string>

namespace lavascript {

bool GetEnvVar( const char* name , std::string* );
bool GetEnvVar( const char* name , const char** );
bool GetEnvVar( const char* name , double* );
bool GetEnvVar( const char* name , std::int32_t* );
bool GetEnvVar( const char* name , std::int64_t* );
bool GetEnvVar( const char* name , bool* );

} // namespace lavascript

#endif // ENV_VAR_H_
