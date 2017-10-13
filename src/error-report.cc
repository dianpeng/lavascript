#include "error-report.h"
#include "util.h"

#include <cctype>

namespace lavascript {
namespace {

size_t RemoveTailingSpaces( const char* source , size_t start , size_t end ) {
  if(end == start) return end;

  for( --end ; end > start ; --end ) {
    if(!std::isspace(source[end])) {
      return end + 1;
    }
  }
  return end+1;
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
  Format(buffer,"around line:%d andd position:%d ,source code ...  %s  ...",
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
  FormatV(&message,format,vl);
  Format(buffer,"Error in %s happened at %s:\n%s\n",where,snippet.c_str(),
                                                                message.c_str());
}

std::string GetSourceSnippetInOneLine( const std::string& source , size_t start, size_t end ) {
  if(end > start) {
    // NOTES: the source code coordinate is [start,end]
    end = RemoveTailingSpaces(source.c_str(),start,end);
    std::string source_code(source.substr(start,(end-start)));
    std::string ret; ret.reserve(source_code.size());

    for( auto &e : source_code ) {
      switch(e) {
        case '\n': ret.append("\\n"); break;
        case '\t': ret.append("\\t"); break;
        case '\v': ret.append("\\v"); break;
        case '\r': ret.append("\\r"); break;
        case '\b': ret.append("\\b"); break;
        default: ret.push_back(e); break;
      }
    }
    return ret;
  } else {
    return std::string();
  }
}

} // lavascript
