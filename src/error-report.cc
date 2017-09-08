#include "error-report.h"
#include "core/util.h"

#include <cctype>

namespace lavascript {
namespace {

size_t RemoveTailingSpaces( const char* source , size_t start , size_t end ) {
  for( ; end > start ; --end ) {
    if(!std::isspace(source[end])) break;
  }
  return end + 1;
}

void GetCoordinate( const char* source , size_t pos , size_t* line ,
                                                      size_t* ccount ) {
  size_t l , c;
  l = 1; c = 1;

  for( size_t i = 0 ; i < pos ; ++i ) {
    if(source[i] == '\n') {
      ++l; c= 1;
    } else {
      ++c;
    }
  }

  *line = l; *ccount = c;
}

/** Show code sample at certain point in the source code **/
void GetCodeSnippets( std::string* buffer , size_t start , size_t end ,
                                                           const char* source ) {
  size_t line,ccount;

  // remove the tailing spaces for the end since we may have them
  end = RemoveTailingSpaces(source,start,end);

  // get the coordinate position for a specific token or string
  GetCoordinate(source,start,&line,&ccount);

  std::string snippet( source + start , (end-start) );

  core::Format(buffer,"around line:%d andd position:%d ,source code ...  %s  ...",
                      static_cast<int>(line),
                      static_cast<int>(ccount),
                      snippet.c_str());
}

} // namespace

void ReportErrorV( std::string* buffer , const char* where , const char* source ,
                                                             size_t start,
                                                             size_t end ,
                                                             const char* format,
                                                             va_list vl ) {
  std::string snippet;
  std::string message;
  GetCodeSnippets(&snippet,start,end,source);
  core::FormatV(&message,format,vl);
  core::Format(buffer,"Error in %s happened at %s:\n%s\n",where,snippet.c_str(),message.c_str());
}

} // lavascript
