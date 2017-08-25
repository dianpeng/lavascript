#ifndef CORE_TRACE_H_
#define CORE_TRACE_H_
#include <cstdarg>

namespace lavascript {
namespace core {

/**
 * Call this function at very start to initialize the trace functionality inside
 * of the code base
 */
void InitTrace( const char* path );

/**
 * Crash the process and generate needed information on different platforms.
 *
 * The most common effect of this call is that the code die at this point and
 * generate a core dump on system that supports core dump
 */

bool CrashV( const char* expression , const char* file , int line ,
                                                         const char* format,
                                                         va_list);

inline bool Crash ( const char* expression , const char* file , int line ,
                                             const char* format , ... ) {
  va_list vl;
  va_start(vl,format);
  return CrashV(expression,file,line,format,vl);
}


#define lava_assert( EXPRESSION , MESSAGE ) \
  (void)((EXPRESSION) || core::Crash(#EXPRESSION,__FILE__,__LINE__,MESSAGE))

#define lava_assertF( EXPRESSION , FORMAT , ... ) \
  (void)((EXPRESSION) || core::Crash(#EXPRESSION,__FILE__,__LINE__,FORMAT,__VA_ARGS__))

#define lava_unreach(MESSAGE) core::Crash("unreachable!!",__FILE__,__LINE__,MESSAGE)

/**
 * Logging routines. These logging helper functions are thread safe and context safe.
 * It can be called at anytime of the code. Avoid using the log function directly
 * but prefer to use the macro extended one since they typically exposes certain optional
 * build quality which we desire */

enum LogSeverity {
  kLogInfo,
  kLogWarn,
  kLogError
};

void LogV( LogSeverity severity , const char* file , int line , const char* format , va_list );

inline void Log ( LogSeverity severity , const char* file , int line ,
    const char* format , ... ) {
  va_list vl;;
  va_start(vl,format);
  LogV(severity,file,line,format,vl);
}

inline void LogD( LogSeverity severity , const char* file ,
    int line , const char* format , ... ) {
#ifdef LAVA_DEBUG_LOG
  va_list vl;
  va_start(vl,format);
  LogF(severity,file,line,format,vl);
#else
  (void)severity;
  (void)file;
  (void)line;
  (void)format;
#endif
}

#define lava_info( FMT , ... ) \
  core::Log(kLogInfo,__FILE__,__LINE__,FMT,__VA_ARGS__)

#define lava_warn( FMT , ... ) \
  core::Log(kLogWarn,__FILE__,__LINE__,FMT,__VA_ARGS__)

#define lava_error(FMT, ... ) \
  core::Log(kLogError,__FILE__,__LINE__,FMT,__VA_ARGS__)

#define lava_infoD( FMT , ... ) \
  core::LogD( kLogInfo , __FILE__ , __LINE__ , FMT , __VA_ARGS__ )

#define lava_warnD( FMT , ... ) \
  core::LogD( kLogWarn , __FILE__ , __LINE__ , FMT , __VA_ARGS__ )

#define lava_errorD( FMT , ... ) \
  core::LogD( kLogError , __FILE__ , __LINE__ , FMT , __VA_ARGS__ )

} // namespace core
} // namespace lavascript

#endif // CORE_TRACE_H_
