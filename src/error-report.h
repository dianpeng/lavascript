#ifndef ERROR_REPORT_H_
#define ERROR_REPORT_H_

#include <cstdarg>
#include <string>

/**
 * Report error while processing (lexing,parsing,interepting,jitting) the script.
 * It should show 1) where the error happened ; 2) code snippets if needed.
 */

namespace lavascript {


void ReportErrorV( std::string* buffer , const char* where , const char* source ,
                                                             size_t start,
                                                             size_t end  ,
                                                             const char* format ,
                                                             va_list );

inline
std::string ReportErrorV( const char* where , const char* source , size_t start ,
                                                                   size_t end   ,
                                                                   const char* format ,
                                                                   va_list vl ) {
  std::string buf;
  ReportErrorV(&buf,where,source,start,end,format,vl);
  return buf;
}

inline
void ReportError( std::string* buffer , const char* where, const char* source,
                                                           size_t start ,
                                                           size_t end ,
                                                           const char* format ,
                                                           ... ) {
  va_list vl;
  va_start(vl,format);
  ReportErrorV(buffer,where,source,start,end,format,vl);
}

inline
std::string ReportError( const char* where, const char* source, size_t start,
                                                                size_t end,
                                                                const char* format ,
                                                                ... ) {
  va_list vl;
  va_start(vl,format);
  return ReportErrorV(where,source,start,end,format,vl);
}

} // namespace lavascript

#endif // ERROR_REPORT_H_
