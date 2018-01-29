#ifndef UTIL_INL_H_
#define UTIL_INL_H_

#include <string>
#include <cstdlib>

namespace lavascript {
namespace detail {

template< typename T > class LexicalCastImpl {
 public:
  template< typename CONV >
  bool operator () ( const char* data , T* output , CONV conv ) {
    try {
      auto r = conv(data,NULL);
      *output = static_cast<T>(r);
      return true;
    } catch( ... ) {
      // for any exception we catch during calling converter function, then
      // we just report we cannot convert the data into numeric representation
      return false;
    }
  }
};

} // namespace detail

// our assumptions
static_assert( sizeof(std::int32_t)  == sizeof(int));
static_assert( sizeof(std::uint64_t) == sizeof(long long int));

inline bool LexicalCast( const char* data , std::int32_t* output ) {
  return detail::LexicalCastImpl<std::int32_t>()(data,output,
           [](const char* data , std::size_t* idx ){ return std::stoi(data,idx); }
  );
}

inline bool LexicalCast( const char* data , std::uint32_t* output ) {
  return detail::LexicalCastImpl<std::uint32_t>()(data,output,
           [](const char* data ,std::size_t* idx ) { return std::stoul(data,idx); }
  );
}

inline bool LexicalCast( const char* data , std::int64_t* output ) {
  return detail::LexicalCastImpl<std::int64_t>()(data,output,
           [](const char* data,std::size_t* idx ) { return std::stoll(data,idx); }
  );
}

inline bool LexicalCast( const char* data , std::uint64_t* output ) {
  return detail::LexicalCastImpl<std::uint64_t>()(data,output,
           [](const char* data, std::size_t* idx) { return std::stoull(data,idx); }
  );
}

inline bool LexicalCast( const char* data , double* output ) {
  return detail::LexicalCastImpl<double>()(data,output,
           [](const char* data, std::size_t* idx) { return std::stod(data,idx); }
  );
}

inline bool LexicalCast( double real , std::string* output ) {
  *output = std::to_string(real);
  /**
   * Now we try to remove all the tailing zeros in the returned
   * std::string result value.
   */
  size_t npos = output->find_last_of('.');
  if(npos != std::string::npos) {
    npos = output->find_last_not_of('0');
    if(npos != std::string::npos) {
      output->erase(npos+1,std::string::npos);
    }

    // remove the trailing dot if we need to
    if(output->back() == '.') {
      output->pop_back();
    }
  }
  return true;
}

inline bool LexicalCast( bool bval , std::string* output ) {
  *output = bval ? "true" : "false";
  return true;
}

} // namespace lavascript

#endif // UTIL_INL_H_