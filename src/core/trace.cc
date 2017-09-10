#include "trace.h"
#include "env-var.h"

#include <cstdlib>
#include <string>
#include <cstdio>
#include <cstring>
#include <sstream>

#include "src/os.h"

namespace lavascript {
namespace core {

namespace {

static const char* kInfoLogFile = "lavascript.trace.info.txt";
static const char* kWarnLogFile = "lavascript.trace.warn.txt";
static const char* kErrorLogFile = "lavascript.trace.error.txt";
static const char* kCrashLogFile = "lavascript.trace.crash.txt";

struct LogContext {
  FILE* info;
  FILE* warn;
  FILE* error;
  FILE* crash;
};

static LogContext kContext;

std::string FileNameWithPid( const char* filename ) {
  std::stringstream formatter;
  formatter<<filename<<'.'<<::lavascript::OS::GetPid();
  return formatter.str();
}

std::string FormatPath( const char* path , const char* filename ) {
  if(path) {
    std::string buf(path);
    buf.push_back('/');
    buf.append(FileNameWithPid(filename));
    return buf;
  } else {
    return std::string(FileNameWithPid(filename));
  }
}

FILE* CheckLogHandler( FILE* output , FILE* backup ) {
  bool crash = false;
  GetEnvVar("LAVA_LOG_IF_INIT_TRACE_FAIL",&crash);
  if(!output) {
    if(crash) std::abort();
    return backup;
  }
  return output;
}

void CloseContext() {
  if(kContext.info) { fclose(kContext.info); kContext.info = NULL; }
  if(kContext.warn) { fclose(kContext.warn); kContext.warn = NULL; }
  if(kContext.error){ fclose(kContext.error);kContext.error= NULL; }
  if(kContext.crash){ fclose(kContext.crash); kContext.crash=NULL; }
}

void PrintLog( FILE* output , const char* file , int line , const char* format ,
                                                            va_list vl ) {
  fprintf(output,"[WHERE:(%s:%d)]:",file,line);
  vfprintf(output,format,vl);
  fwrite("\n",1,1,output);
}

} // namespace

void InitTrace( const char* folder ) {
  if( folder == NULL ) {
    GetEnvVar( "LAVA_LOG_PATH" , &folder );
  }

  if(folder && std::strcmp(folder,"-") == 0) {
    kContext.info = stdout;
    kContext.warn = stderr;
    kContext.error=stderr;
    kContext.crash=stderr;
  } else {
    kContext.info = CheckLogHandler(
        std::fopen(FormatPath(folder,kInfoLogFile).c_str(),"w+"),stdout);
    kContext.warn = CheckLogHandler(
        std::fopen(FormatPath(folder,kWarnLogFile).c_str(),"w+"),stderr);
    kContext.error= CheckLogHandler(
        std::fopen(FormatPath(folder,kErrorLogFile).c_str(),"w+"),stderr);
    kContext.crash = CheckLogHandler(
        std::fopen(FormatPath(folder,kCrashLogFile).c_str(),"w+"),stderr);
  }

  // Call CloseContext when program exits
  std::atexit( CloseContext );
}

bool CrashV( const char* expression , const char* file , int line ,
                                                         const char* format,
                                                         va_list vl ) {
  fprintf(kContext.crash,"[CRASH:(%s)@(%s:%d)]:",expression,file,line);
  vfprintf(kContext.crash,format,vl);
  fwrite("\n",1,1,kContext.crash);
  fflush(kContext.crash);
  std::abort();
  CloseContext();
  return true;
}

void LogV( LogSeverity severity , const char* file , int line , const char* format ,
                                                                va_list vl ) {
  switch(severity) {
    case kLogInfo:
      PrintLog(kContext.info,file,line,format,vl);
      break;
    case kLogWarn:
      PrintLog(kContext.warn,file,line,format,vl);
      fflush(kContext.warn);
      break;
    default:
      PrintLog(kContext.error,file,line,format,vl);
      fflush(kContext.error);
      break;
  }
}

} // namespace core
} // namespace lavascript
