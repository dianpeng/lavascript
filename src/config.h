#ifndef CONFIG_H_
#define CONFIG_H_
#include <cstdint>
#include <string>

namespace lavascript {

/* ---------------------------------------------------------------
 *
 *  The following configuration value are *fixed* and cannot be
 *  mutated. The assumption is baked inside of how the code is
 *  designed
 *
 * ---------------------------------------------------------------*/
static const std::size_t kSSOMaxSize               = 32; // inclusive
static const std::size_t kDefaultListSize          = 4;
static const std::size_t kDefaultObjectSize        = 8;
static const std::size_t kMaxPrototypeSize         = 65536;
static const std::size_t kMaxPrototypeCount        = 65535;
static const std::size_t kMaxListEntryCount        = 256;
static const std::size_t kMaxObjectEntryCount      = 256;

// Interpreter related confugration options
namespace interpreter {

static const std::size_t kMaxIntrinsicCall         = 256;
static const std::size_t kMaxCodeLength            = 65536;
static const std::size_t kRegisterSize             = 256;
static const std::size_t kMaxFunctionArgumentCount = 256;
static const std::size_t kMaxLiteralSize           = 256;
static const std::size_t kMaxUpValueSize           = 256;
static const std::uint8_t kAccRegisterIndex        = 255;

} // namespace interpreter

namespace compiler {

static const std::size_t kHotCountArraySize = 256;

// type of hot count
typedef std::uint16_t hotcount_t;

} // namespace compiler


// Initialize the dynamic configuration via command line options
bool DConfigInit( int argc , char** argv , std::string* error );

namespace dconf {

std::int32_t GetInt32  ( const char* , const char* );
std::int64_t GetInt64  ( const char* , const char* );
double GetDouble       ( const char* , const char* );
std::string GetString  ( const char* , const char* );
bool GetBoolean        ( const char* , const char* );

void AddOption ( const char* , const char* , const char* , std::int32_t );
void AddOption ( const char* , const char* , const char* , std::int64_t );
void AddOption ( const char* , const char* , const char* , double );
void AddOption ( const char* , const char* , const char* , const char* );
void AddOption ( const char* , const char* , const char* , const std::string& );
void AddOption ( const char* , const char* , const char* , bool );

} // namespace dconf

#define _DCONF_REGCLS_NAME(SEC,KEY) DConfReg__##SEC##_##KEY
#define _DCONF_REGINS_NAME(SEC,KEY) kDConfReg__##SEC##_##KEY
#define _DCONF_GETTER_NAME(SEC,KEY) OptGet_##SEC##_##KEY

// Declaration macros -------------------------------------
#define LAVA_DECLARE_INT32(SEC,KEY) extern std::int32_t _DCONF_GETTER_NAME(SEC,KEY)()
#define LAVA_DECLARE_INT64(SEC,KEY) extern std::int64_t _DCONF_GETTER_NAME(SEC,KEY)()
#define LAVA_DECLARE_DOUBLE(SEC,KEY) extern double _DCONF_GETTER_NAME(SEC,KEY)()
#define LAVA_DECLARE_STRING(SEC,KEY) extern const std::string& _DCONF_GETTER_NAME(SEC,KEY)()
#define LAVA_DECLARE_BOOLEAN(SEC,KEY) extern bool _DCONF_GETTER_NAME(SEC,KEY)()

// Definition macros---------------------------------------
#define _LAVA_ADD_OPTION(SEC,KEY,CPPTYPE,COMMENT,DEFAULT)               \
  class _DCONF_REGCLS_NAME(SEC,KEY) {                                   \
   public:                                                              \
    _DCONF_REGCLS_NAME(SEC,KEY)() {                                     \
      dconf::AddOption(#SEC,#KEY,COMMENT,static_cast<CPPTYPE>(DEFAULT));\
    }                                                                   \
  }; static _DCONF_REGCLS_NAME(SEC,KEY) _DCONF_REGINS_NAME(SEC,KEY);

#define _LAVA_ADD_GETTER(SEC,KEY,SIGTYPE,CPPTYPE,DEFAULT) \
  CPPTYPE _DCONF_GETTER_NAME(SEC,KEY)() {                 \
    return dconf::Get##SIGTYPE(#SEC,#KEY);                \
  } static_assert(true)

#define _LAVA_DEFINE_XX(SEC,KEY,TYPE,SIGTYPE,CPPTYPE,COMMENT,DEFAULT) \
  _LAVA_ADD_OPTION(SEC,KEY,CPPTYPE,COMMENT,DEFAULT)                   \
  _LAVA_ADD_GETTER(SEC,KEY,SIGTYPE,CPPTYPE,DEFAULT)

#define LAVA_DEFINE_INT32(SEC,KEY,COMMENT,DEFAULT) \
  _LAVA_DEFINE_XX(SEC,KEY,INT32,Int32,std::int32_t,COMMENT,DEFAULT)

#define LAVA_DEFINE_INT64(SEC,KEY,COMMENT,DEFAULT) \
  _LAVA_DEFINE_XX(SEC,KEY,INT64,Int64,std::int64_t,COMMENT,DEFAULT)

#define LAVA_DEFINE_DOUBLE(SEC,KEY,COMMENT,DEFAULT) \
  _LAVA_DEFINE_XX(SEC,KEY,DOUBLE,Double,double,COMMENT,DEFAULT)

#define LAVA_DEFINE_STRING(SEC,KEY,COMMENT,DEFAULT) \
  _LAVA_DEFINE_XX(SEC,KEY,STRING,String,std::string,COMMENT,DEFAULT)

#define LAVA_DEFINE_BOOLEAN(SEC,KEY,COMMENT,DEFAULT) \
  _LAVA_DEFINE_XX(SEC,KEY,BOOLEAN,Boolean,bool,COMMENT,DEFAULT)

// Getter macros -------------------------------------------
//
// Avoid using this macro since it is kind of expensive , you
// need to store the result of this macro into some places and
// just use that value unless you want to accept reconfiguration
// of those options
#define LAVA_OPTION(SEC,KEY) _DCONF_GETTER_NAME(SEC,KEY)()

} // namespace lavascript

#endif // CONFIG_H_
