#ifndef TRACE_H_
#define TRACE_H_
#include <cstdarg>
#include <cstdint>

#include "os.h"

namespace lavascript {

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
  (void)((EXPRESSION) || ::lavascript::Crash(#EXPRESSION,__FILE__,__LINE__,MESSAGE))

#define lava_assertF( EXPRESSION , FORMAT , ... ) \
  (void)((EXPRESSION) || ::lavascript::Crash(#EXPRESSION,__FILE__,__LINE__,FORMAT,__VA_ARGS__))

#define lava_unreach(MESSAGE) ::lavascript::Crash("unreachable!!",__FILE__,__LINE__,MESSAGE)

#define lava_unreachF( FORMAT , ... ) \
  ::lavascript::Crash("unreachable!!",__FILE__,__LINE__,FORMAT,__VA_ARGS__)

#define lava_die() ::lavascript::Crash("die!!",__FILE__,__LINE__,"")

#define lava_verify( EXPRESSION ) lava_assert( EXPRESSION , "" )

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

namespace detail {

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

} // namespace detail

#define lava_info( FMT , ... ) \
  ::lavascript::detail::Log(kLogInfo,__FILE__,__LINE__,FMT,__VA_ARGS__)

#define lava_warn( FMT , ... ) \
  ::lavascript::detail::Log(kLogWarn,__FILE__,__LINE__,FMT,__VA_ARGS__)

#define lava_error(FMT, ... ) \
  ::lavascript::detail::Log(kLogError,__FILE__,__LINE__,FMT,__VA_ARGS__)

#define lava_infoD( FMT , ... ) \
  ::lavascript::detail::LogD( kLogInfo , __FILE__ , __LINE__ , FMT , __VA_ARGS__ )

#define lava_warnD( FMT , ... ) \
  ::lavascript::detail::LogD( kLogWarn , __FILE__ , __LINE__ , FMT , __VA_ARGS__ )

#define lava_errorD( FMT , ... ) \
  ::lavascript::detail::LogD( kLogError , __FILE__ , __LINE__ , FMT , __VA_ARGS__ )

/**
 * Helper for debugging code or other verification when certain compiler
 * level is reached.
 */

#define lava_debug( LEVEL , ... ) \
  _LAVA_DEBUG_##LEVEL( __VA_ARGS__ )

#if LAVASCRIPT_DEBUG_LEVEL == 0
#define _LAVA_DEBUG_NORMAL( ... ) (void)(NULL)
#define _LAVA_DEBUG_VERBOSE(... ) (void)(NULL)
#define _LAVA_DEBUG_CRAZY ( ... ) (void)(NULL)
#elif LAVASCRIPT_DEBUG_LEVE == 1
#define _LAVA_DEBUG_NORMAL( ... ) do { __VA_ARGS__ } while(false)
#define _LAVA_DEBUG_VERBOSE( ... ) (void)(NULL)
#define _LAVA_DEBUG_CRAZY ( ... ) (void)(NULL)
#elif LAVASCRIPT_DEBUG_LEVEL == 2
#define _LAVA_DEBUG_NORMAL( ... ) do { __VA_ARGS__ } while(false)
#define _LAVA_DEBUG_VERBOSE( ... ) do { __VA_ARGS__ } while(false)
#define _LAVA_DEBUG_CRAZY ( ... ) (void)(NULL)
#elif LAVASCRIPT_DEBUG_LEVEL == 2
#define _LAVA_DEBUG_NORMAL( ... ) do { __VA_ARGS__ } while(false)
#define _LAVA_DEBUG_VERBOSE( ... ) do { __VA_ARGS__ } while(false)
#define _LAVA_DEBUG_CRAZY ( ... ) do ( __VA_ARGS__ } while(false)
#endif // LAVASCRIPT_DEBUG_LEVEL


/**
 * Helper function for tracking the time spent in certain lexical scope
 */

namespace detail {

class LexicalScopeBenchmark {
 public:
  LexicalScopeBenchmark( const char* message , const char* file , int line ) :
    timestamp_ ( ::lavascript::OS::NowInMicroSeconds() ),
    message_   ( message ) ,
    file_      ( file )    ,
    line_      ( line )
  {}

  ~LexicalScopeBenchmark() {
    detail::Log(kLogInfo,file_,line_,"Benchmark(%lld):%s",
        static_cast<std::uint64_t>(::lavascript::OS::NowInMicroSeconds() - timestamp_),
        message_);
  }

 private:
  std::uint64_t timestamp_;
  const char* message_;
  const char* file_;
  int line_;
};

} // namespace detail


/**
 * Put the lava_bench on top of the lexical scope you want to profile or benchmark
 *
 * { lava_bench("Bench my shit");
 *
 *   Your code goes here
 * }
 */
#ifdef LAVASCRIPT_BENCH
#define lava_bench(MESSAGE) ::lavascript::detail::LexicalScopeBenchmark(MESSAGE,__FILE__,__LINE__)
#else
#define lava_bench(MESSAGE) (void)(MESSAGE)
#endif // LAVASCRIPT_BENCH

} // namespace lavascript

#endif // CORE_TRACE_H_
