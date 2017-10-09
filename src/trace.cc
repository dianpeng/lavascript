#include "trace.h"
#include "util.h"
#include "env-var.h"
#include "os.h"
#include "common.h"

#include <cstdlib>
#include <string>
#include <cstdio>
#include <cstring>
#include <sstream>

#include "src/os.h"

namespace lavascript {

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

void PrintLog( FILE* output , const char* file , int line , const char* message ) {
  fprintf(output,"[WHERE:(%s:%d)]:%s",file,line,message);
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

void LogV( LogSeverity severity , const char* file , int line , const char* message ) {
  switch(severity) {
    case kLogInfo: PrintLog(kContext.info,file,line,message); break;
    case kLogWarn: PrintLog(kContext.warn,file,line,message); fflush(kContext.warn); break;
    default: PrintLog(kContext.error,file,line,message); fflush(kContext.error); break;
  }
}

DumpWriter::DumpWriter( const char* filename ):
  file_(filename,std::ios_base::in|std::ios_base::trunc),
  use_file_(true)
{
  use_file_ = file_ ? true : false;
}

void DumpWriter::Write( const char* fmt , ... ) {
  va_list vl;
  va_start(vl,fmt);

  if(use_file_) {
    file_ << Format(fmt,vl);
  } else {
    LogV(kLogInfo,__FILE__,__LINE__,fmt,vl);
  }
}

void DumpWriter::WriteL( const char* fmt , ... ) {
  va_list vl;
  va_start(vl,fmt);
  if(use_file_) {
    file_ << Format(fmt,vl) << '\n';
  } else {
    LogV(kLogInfo,__FILE__,__LINE__,fmt,vl);
  }
}

namespace {
static const char* kSeparator = "------------------------------------------------";
} // namespace

namespace detail {

LexicalScopeBenchmark::LexicalScopeBenchmark( const char* message ,
                                              const char* file ,
                                              int line ) :
  timestamp_ ( ::lavascript::OS::NowInMicroSeconds() ),
  message_   ( message ) ,
  file_      ( file )    ,
  line_      ( line )
{}

LexicalScopeBenchmark::~LexicalScopeBenchmark() {
  detail::Log(kLogInfo,file_,line_,"Benchmark(%lld):%s",
      static_cast<std::uint64_t>(::lavascript::OS::NowInMicroSeconds() - timestamp_),
      message_);
}

} // namespace detail


DumpWriter::Section::Section(DumpWriter* writer) : writer_(writer) {
  writer->WriteL(kSeparator);
}

DumpWriter::Section::Section(DumpWriter* writer, const char* format, ... ) : writer_(writer) {
  writer->WriteL(kSeparator);
  va_list vl;
  va_start(vl,format);
  LogV( kLogInfo , __FILE__ , __LINE__ , FormatV(format,vl).c_str() );
}

DumpWriter::Section::~Section() {
  writer_->WriteL(kSeparator);
}

} // namespace lavascript
