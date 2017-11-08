#include "config.h"
#include "trace.h"
#include "util.h"
#include "env-var.h"

#include <cstdlib>
#include <cerrno>
#include <map>
#include <algorithm>
#include <cctype>

namespace lavascript {
namespace dconf {
namespace {

// An option value -------------------------------------------
struct OptionValue {
  enum {
    OPTVAL_UNDEFINED,
    OPTVAL_INT32,
    OPTVAL_INT64,
    OPTVAL_REAL,
    OPTVAL_STRING,
    OPTVAL_BOOLEAN
  };

  int type;

  union {
    std::int32_t int32_value;
    std::int64_t int64_value;
    double real_value;
    bool boolean_value;
  };
  std::string string_value;

 public:
  void set_string_value( const char* str ) {
    type = OPTVAL_STRING;
    string_value = str;
  }

  void set_int32( std::int32_t val ) {
    type = OPTVAL_INT32;
    int32_value = val;
  }

  void set_int64( std::int64_t val ) {
    type = OPTVAL_INT64;
    int64_value = val;
  }

  void set_double( double val ) {
    type = OPTVAL_REAL;
    real_value = val;
  }

  void set_boolean( bool val ) {
    type = OPTVAL_BOOLEAN;
    boolean_value = val;
  }

 public:
  OptionValue( std::int32_t ival ):
    type(OPTVAL_INT32),
    string_value()
  { int32_value = ival; }

  OptionValue( std::int64_t ival ):
    type(OPTVAL_INT64),
    string_value()
  { int64_value = ival; }

  OptionValue( double rval ):
    type(OPTVAL_REAL),
    string_value()
  { real_value = rval; }

  OptionValue( bool bval ):
    type(OPTVAL_BOOLEAN),
    string_value()
  { boolean_value = bval; }

  OptionValue( const std::string& sval ):
    type(OPTVAL_STRING),
    string_value(sval)
  {}

  OptionValue( const char* sval ):
    type(OPTVAL_STRING),
    string_value(sval)
  {}

  OptionValue():
    type(OPTVAL_UNDEFINED),
    string_value()
  {}
};

std::string MakeFullname( const std::string& section ,
    const std::string& key ) {
  std::string ret(section);
  ret.reserve( key.size() + section.size() + 1 );
  ret.push_back('.'); ret.append(key);
  return ret;
}

std::string MakeFullname( const char* section ,
                          const char* key ) {
  std::string ret(section);
  ret.push_back('.');
  ret.append(key);
  return ret;
}

std::string MakeEnvName ( const std::string& section,
                          const std::string& key ) {
  std::string ret("LAVASCRIPT_OPTION_");
  ret.append(section);
  ret.push_back('_');
  ret.append(key);
  std::transform(ret.begin(),ret.end(),ret.begin(),::toupper);
  return ret;
}

struct OptionItem {

  std::string section;
  std::string key;
  std::string fullname;
  std::string comment;
  OptionValue default_value;
  OptionValue command_value;

  bool operator < ( const OptionItem& that ) const {
    return fullname < that.fullname;
  }

  OptionItem( const std::string& sec ,
              const std::string& k   ,
              const std::string& cmt ,
              std::int32_t val ) :
    section  (sec),
    key      (k),
    fullname (MakeFullname(sec,k)),
    comment  (cmt),
    default_value (val),
    command_value ()
  {}

  OptionItem( const std::string& sec ,
              const std::string& k   ,
              const std::string& cmt ,
              std::int64_t       val ):
    section  (sec),
    key      (k)  ,
    fullname (MakeFullname(sec,k)),
    comment  (cmt),
    default_value(val),
    command_value()
  {}

  OptionItem( const std::string& sec ,
              const std::string& k   ,
              const std::string& cmt ,
              double             val ):
    section(sec),
    key    (k),
    fullname( MakeFullname(sec,k) ),
    comment (cmt),
    default_value(val),
    command_value()
  {}

  OptionItem( const std::string& sec ,
              const std::string& k ,
              const std::string& cmt ,
              bool bval ):
    section(sec),
    key    (k),
    fullname( MakeFullname(sec,k) ),
    comment (cmt),
    default_value(bval),
    command_value()
  {}

  OptionItem( const std::string& sec ,
              const std::string& k   ,
              const std::string& cmt ,
              const std::string& val ):
    section(sec),
    key    (k),
    fullname(MakeFullname(sec,k)),
    comment (cmt),
    default_value(val),
    command_value()
  {}

  OptionItem( const std::string& sec ,
              const std::string& k,
              const std::string& cmt,
              const char* val ):
    section(sec),
    key    (k),
    fullname(MakeFullname(sec,k)),
    comment(cmt),
    default_value(val),
    command_value()
  {}

};

typedef std::map<std::string,OptionItem> OptionMap;

// ----------------------------------------------------------------
// Get a global option map object. Local static variable is defined
// properly under C++11
OptionMap* GetOptionMap() {
  static OptionMap kMap;
  return &kMap;
}

// -------------------------------------------------
// Parser for our command line options
class CommandLineParser {
 public:
  CommandLineParser( int nargs , char** argv , OptionMap* opt_map ,
                                               std::string* error ):
    opt_map_(opt_map),
    cursor_ (1),
    nargs_  (nargs),
    argv_   (argv) ,
    error_  (error)
  {}
 public:
  bool Parse();

 private:
  void OnError( const char* format , ... );
  void GenHelp( std::string* buffer );

  bool SetInt32   ( const char* , OptionValue* , const char* );
  bool SetInt64   ( const char* , OptionValue* , const char* );
  bool SetReal    ( const char* , OptionValue* , const char* );
  bool SetBoolean ( const char* , OptionValue* , const char* );
  bool SetString  ( const char* , OptionValue* , const char* );

 private:
  OptionMap*  opt_map_;
  std::size_t cursor_;
  std::size_t nargs_ ;
  char**      argv_ ;
  std::string* error_;
};

void CommandLineParser::OnError( const char* format , ... ) {
  va_list vl;
  va_start(vl,format);
  Format(error_,format,vl);
  GenHelp(error_);
}

void CommandLineParser::GenHelp( std::string* buffer ) {
  buffer->push_back('\n');
  buffer->append("-------------------------------------\n");
  buffer->append("Help\n");
  buffer->append("-------------------------------------\n");

  for( auto &e : *opt_map_ ) {
    buffer->append(e.second.fullname);
    buffer->push_back(':');
    buffer->append(e.second.comment);
    buffer->push_back('.');
    buffer->push_back('(');

    switch(e.second.default_value.type) {
      case OptionValue::OPTVAL_INT32:
        buffer->append(Format("int32,%d)",e.second.default_value.int32_value));
        break;
      case OptionValue::OPTVAL_INT64:
        // Stupid int64_t format specifier
        buffer->append(
            Format("int64,%lld)",
              static_cast<long long int>(e.second.default_value.int64_value)));
        break;
      case OptionValue::OPTVAL_REAL:
        buffer->append(
            Format("double,%f)",e.second.default_value.real_value));
        break;
      case OptionValue::OPTVAL_STRING:
        buffer->append(
            Format("str,%s)",e.second.default_value.string_value.c_str()));
        break;
      case OptionValue::OPTVAL_BOOLEAN:
        buffer->append(
            Format("boolean,%s)",e.second.default_value.boolean_value ?
                                 "true" : "false" ));
        break;
      default: lava_unreach("");
    }
    buffer->push_back('\n');
  }
}

bool CommandLineParser::SetInt32( const char* opt , OptionValue* output ,
                                                    const char* str ) {
  if(!str) {
    if(cursor_ + 1 == nargs_) {
      OnError("option %s doesn't have a value, expect a int32 value!",opt);
      return false;
    }
    str = argv_[cursor_+1];
    ++cursor_;
  }

  char* pend;
  long int val = std::strtol(str,&pend,10);
  if(*pend || errno) {
    OnError("cannot convert %s to int32, option %s requires int32 value!",
        str,opt);
    return false;
  }
  output->set_int32(static_cast<std::int32_t>(val));
  return true;
}

bool CommandLineParser::SetInt64( const char* opt , OptionValue* output ,
                                                    const char* str ) {
  if(!str) {
    if(cursor_ + 1 == nargs_) {
      OnError("option %s doesn't have a value, expect a int64 value!",opt);
      return false;
    }
    str = argv_[cursor_+1];
    ++cursor_;
  }

  char* pend;
  long long int val = std::strtoll(str,&pend,10);
  if(*pend || errno) {
    OnError("cannot convert %s to int64, option %s requires int64 value!",
        str,opt);
    return false;
  }

  output->set_int64(static_cast<std::int64_t>(val));
  return true;
}

bool CommandLineParser::SetReal( const char* opt, OptionValue* output ,
                                                  const char* str ) {
  if(!str) {
    if(cursor_ + 1 == nargs_) {
      OnError("option %s doesn't have a value, expect a real value!",opt);
      return false;
    }
    str = argv_[cursor_+1];
    ++cursor_;
  }

  char* pend;
  double val = std::strtod(str,&pend);
  if(*pend || errno) {
    OnError("cannot convert %s to double, option %s requires double value!",
        str,opt);
    return false;
  }
  output->set_double(val);
  return true;
}

bool CommandLineParser::SetBoolean( const char* opt, OptionValue* output ,
                                                     const char* str ) {
  if(!str) {
    if(cursor_ + 1 < nargs_) {
      const char* nval = argv_[cursor_+1];
      if(strcmp(nval,"true") == 0 ||
         strcmp(nval,"false")== 0) {
        str = nval;
        ++cursor_;
      }
    }
  }

  if(str) {
    if(strcmp(str,"true") == 0) {
      output->set_boolean(true);
    } else if(strcmp(str,"false") == 0) {
      output->set_boolean(false);
    } else {
      OnError("cannot convert %s to boolean, option %s requires boolean value!",
          str,opt);
      return false;
    }
  } else {
    output->set_boolean(true);
  }
  return true;
}

bool CommandLineParser::SetString( const char* opt, OptionValue* output,
                                                    const char* str ) {
  if(!str) {
    if(cursor_ + 1 < nargs_) {
      OnError("option %s doesn't have a value, expect a string value!",opt);
      return false;
    }
    str = argv_[cursor_ + 1];
    ++cursor_;
  }
  output->set_string_value(str);
  return true;
}

bool CommandLineParser::Parse() {
  for ( ; cursor_ < nargs_ ; cursor_++ ) {
    char* opt = argv_[cursor_];
    if(opt[0] != '-' || opt[1] != '-') {
      OnError("unknown option %s!",argv_[cursor_]);
      return false;
    }
    opt += 2;
    char* pos = strchr(opt,'=');
    if(pos) *pos = '\0';

    // check option existed or not
    OptionMap::iterator itr = opt_map_->find(opt);
    if(itr == opt_map_->end()) {
      if(strcmp(opt,"help") == 0) {
        GenHelp(error_);
        return true;
      } else {
        OnError("unknown option %s!",opt);
        return false;
      }
    }

    // normal command line options
    OptionValue* output = &(itr->second.command_value);

    if(pos == NULL) {
      switch(itr->second.default_value.type) {
        case OptionValue::OPTVAL_INT32:
          if(!SetInt32(opt,output,NULL)) {
            return false;
          }
          break;
        case OptionValue::OPTVAL_INT64:
          if(!SetInt64(opt,output,NULL)) {
            return false;
          }
          break;
        case OptionValue::OPTVAL_REAL:
          if(!SetReal(opt,output,NULL)) {
            return false;
          }
          break;
        case OptionValue::OPTVAL_STRING:
          if(!SetString(opt,output,NULL)) {
            return false;
          }
          break;
        case OptionValue::OPTVAL_BOOLEAN:
          if(!SetBoolean(opt,output,NULL)) {
            return false;
          }
          break;
        default: lava_unreach("");
      }

    } else {
      switch(itr->second.default_value.type) {
        case OptionValue::OPTVAL_INT32:
          if(!SetInt32(opt,output,pos+1)) {
            return false;
          }
          break;
        case OptionValue::OPTVAL_INT64:
          if(!SetInt64(opt,output,pos+1)) {
            return false;
          }
          break;
        case OptionValue::OPTVAL_REAL:
          if(!SetReal (opt,output,pos+1)) {
            return false;
          }
          break;
        case OptionValue::OPTVAL_STRING:
          if(!SetString(opt,output,pos+1)) {
            return false;
          }
          break;
        case OptionValue::OPTVAL_BOOLEAN:
          if(!SetBoolean(opt,output,pos+1)) {
            return false;
          }
          break;
        default: lava_unreach("");
      }
    }
  }

  // for all *undefined* variables try to use environment variable
  for( auto &e : *opt_map_ ) {
    OptionItem& opt = e.second;
    if(opt.command_value.type == OptionValue::OPTVAL_UNDEFINED) {
      std::string name = MakeEnvName( opt.section , opt.key );
      switch(opt.default_value.type) {
        case OptionValue::OPTVAL_INT32:
          {
            std::int32_t val;
            if(GetEnvVar(name.c_str(),&val)) {
              opt.command_value.set_int32(val);
            }
          }
          break;
        case OptionValue::OPTVAL_INT64:
          {
            std::int64_t val;
            if(GetEnvVar(name.c_str(),&val)) {
              opt.command_value.set_int64(val);
            }
          }
          break;
        case OptionValue::OPTVAL_REAL:
          {
            double val;
            if(GetEnvVar(name.c_str(),&val)) {
              opt.command_value.set_double(val);
            }
          }
          break;
        case OptionValue::OPTVAL_STRING:
          {
            const char* str;
            if(GetEnvVar(name.c_str(),&str)) {
              opt.command_value.set_string_value(str);
            }
          }
          break;
        case OptionValue::OPTVAL_BOOLEAN:
          {
            bool val;
            if(GetEnvVar(name.c_str(),&val)) {
              opt.command_value.set_boolean(val);
            }
          }
          break;
        default:
          lava_unreach("");
      }
    }
  }
  return true;
}

} // namespace

std::int32_t GetInt32( const char* section , const char* key ) {
  OptionMap* opt_map = GetOptionMap();
  OptionMap::iterator itr = (opt_map->find(MakeFullname(section,key)));
  lava_verify(itr != opt_map->end());
  if(itr->second.command_value.type == OptionValue::OPTVAL_UNDEFINED) {
    return itr->second.default_value.int32_value;
  }
  lava_verify(itr->second.command_value.type == OptionValue::OPTVAL_INT32);
  return itr->second.command_value.int32_value;
}

std::int64_t GetInt64( const char* section , const char* key ) {
  OptionMap* opt_map = GetOptionMap();
  OptionMap::iterator itr = (opt_map->find(MakeFullname(section,key)));
  lava_verify(itr != opt_map->end());
  if(itr->second.command_value.type == OptionValue::OPTVAL_UNDEFINED) {
    return itr->second.default_value.int64_value;
  }
  lava_verify(itr->second.command_value.type == OptionValue::OPTVAL_INT64);
  return itr->second.command_value.int64_value;
}

double GetDouble( const char* section , const char* key ) {
  OptionMap* opt_map = GetOptionMap();
  OptionMap::iterator itr = (opt_map->find(MakeFullname(section,key)));
  lava_verify(itr != opt_map->end());
  if(itr->second.command_value.type == OptionValue::OPTVAL_UNDEFINED) {
    return itr->second.default_value.real_value;
  }
  lava_verify(itr->second.command_value.type == OptionValue::OPTVAL_REAL);
  return itr->second.command_value.real_value;
}

bool GetBoolean( const char* section , const char* key ) {
  OptionMap* opt_map = GetOptionMap();
  OptionMap::iterator itr = (opt_map->find(MakeFullname(section,key)));
  lava_verify(itr != opt_map->end());
  if(itr->second.command_value.type == OptionValue::OPTVAL_UNDEFINED) {
    return itr->second.default_value.boolean_value;
  }
  lava_verify(itr->second.command_value.type == OptionValue::OPTVAL_BOOLEAN);
  return itr->second.command_value.boolean_value;
}

std::string GetString ( const char* section , const char* key ) {
  OptionMap* opt_map = GetOptionMap();
  OptionMap::iterator itr = (opt_map->find(MakeFullname(section,key)));
  lava_verify(itr != opt_map->end());
  if(itr->second.command_value.type == OptionValue::OPTVAL_UNDEFINED) {
    return itr->second.default_value.string_value;
  }
  lava_verify(itr->second.command_value.type == OptionValue::OPTVAL_STRING);
  return itr->second.command_value.string_value;
}

void AddOption( const char* section , const char* key , const char* cmt ,
                                                        std::int32_t value ) {
  OptionMap* opt_map = GetOptionMap();
  lava_verify( opt_map->insert(std::make_pair(MakeFullname(section,key),
                               OptionItem(section,key,cmt,value))).second );
}

void AddOption( const char* section , const char* key , const char* cmt ,
                                                        std::int64_t value ) {
  OptionMap* opt_map = GetOptionMap();
  lava_verify( opt_map->insert(std::make_pair(MakeFullname(section,key),
                               OptionItem(section,key,cmt,value))).second );
}

void AddOption( const char* section , const char* key , const char* cmt ,
                                                        double value ) {
  OptionMap* opt_map = GetOptionMap();
  lava_verify( opt_map->insert(std::make_pair(MakeFullname(section,key),
                               OptionItem(section,key,cmt,value))).second );
}

void AddOption( const char* section , const char* key , const char* cmt ,
                                                        bool value ) {
  OptionMap* opt_map = GetOptionMap();
  lava_verify( opt_map->insert(std::make_pair(MakeFullname(section,key),
                               OptionItem(section,key,cmt,value))).second );
}

void AddOption( const char* section , const char* key , const char* cmt ,
                                                        const std::string& value ) {
  OptionMap* opt_map = GetOptionMap();
  lava_verify( opt_map->insert(std::make_pair(MakeFullname(section,key),
                               OptionItem(section,key,cmt,value))).second );
}

void AddOption( const char* section , const char* key , const char* cmt ,
                                                        const char* value ) {
  OptionMap* opt_map = GetOptionMap();
  lava_verify( opt_map->insert(std::make_pair(MakeFullname(section,key),
                               OptionItem(section,key,cmt,value))).second );
}

} // namespace dconf

bool DConfigInit( int argc, char** argv , std::string* error ) {
  dconf::CommandLineParser parser(argc,argv,dconf::GetOptionMap(),error);
  return parser.Parse();
}

} // namespace lavascript
