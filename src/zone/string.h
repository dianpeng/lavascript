#ifndef ZONE_STRING_H_
#define ZONE_STRING_H_

#include "zone.h"
#include <src/core/trace.h>

#include <string>
#include <cstring>


namespace lavascript {
namespace zone {

// A String literal lives on a specific ZoneObject. This string is
// immutable and permenent. So it only offers accessor for its internal
// states no mutator provided.
class String : public ZoneObject {
  /**
   * An empty/null string static instance. All null string's pointer should
   * point to this instance. The reason to make it private is that people
   * should not rely on the truth that a null string always points to this
   * instance since if user instantiate a value *string* with default constructor
   * then this null string will not be a instance of kNullString */
  static String kNullString;
 public:
  String() : ptr_("") , size_(0) {}
  inline String( Zone* zone , const char* string );
  inline String( Zone* zone , const char* string , size_t length );
  inline String( Zone* , const std::string& );
  inline String( Zone* , const String& );

  static String* New( Zone* ) { return &kNullString; }

  static String* New( Zone* zone , const char* str )
  { return ::new (zone->Malloc<String>()) String(zone,str); }

  static String* New( Zone* zone , const char* str , size_t length )
  { return ::new (zone->Malloc<String>()) String(zone,str,length); }

  static String* New( Zone* zone , const std::string& str )
  { return ::new (zone->Malloc<String>()) String(zone,str); }

  static String* New( Zone* zone , const String& str )
  { return ::new (zone->Malloc<String>()) String(zone,str); }

 public:
  bool operator == ( const String& that ) const
  { return strcmp(ptr_,that.ptr_) == 0; }
  bool operator == ( const char* that ) const
  { return strcmp(ptr_,that) == 0; }
  bool operator == ( const std::string& that ) const
  { return that == ptr_; }

  bool operator != ( const String& that ) const
  { return !(*this == that); }
  bool operator != ( const char* that ) const
  { return !(*this == that); }
  bool operator != ( const std::string& that ) const
  { return !(*this == that); }

  bool operator >  ( const String& that ) const
  { return strcmp(ptr_,that.ptr_) > 0; }
  bool operator >  ( const char* that ) const
  { return strcmp(ptr_,that) > 0; }
  bool operator >  ( const std::string& that ) const
  { return that <= ptr_; }

  bool operator >= ( const String& that ) const
  { return strcmp(ptr_,that.ptr_) >=0; }
  bool operator >= ( const char* that ) const
  { return strcmp(ptr_,that) >= 0; }
  bool operator >= ( const std::string& that ) const
  { return that < ptr_; }

  bool operator < ( const String& that ) const
  { return strcmp(ptr_,that.ptr_) < 0; }
  bool operator < ( const char* that ) const
  { return strcmp(ptr_,that) < 0; }
  bool operator < ( const std::string& that ) const
  { return that >= ptr_; }

  bool operator <=( const String& that ) const
  { return strcmp(ptr_,that.ptr_) <= 0; }
  bool operator <=( const char* that ) const
  { return strcmp(ptr_,that) <= 0; }
  bool operator <=( const std::string& that ) const
  { return that > ptr_; }

 public:
  const char* data() const { return ptr_; }
  size_t size() const { return size_; }
  bool empty() const { return size_ != 0; }

  char operator [] (int index) const {
    return Index(index);
  }

  char Index(int index) const {
    lava_assert(index < static_cast<int>(size_),"Index out of bound!");
    return ptr_[index];
  }

 private:
  const char* ptr_;
  size_t size_;
};

inline String::String( Zone* zone , const char* str ) :
  ptr_(NULL),
  size_(strlen(str))
{
  ptr_ = static_cast<const char*>(zone->Malloc(size_+1));
  memcpy((void*)ptr_,str,size_+1);
}

inline String::String( Zone* zone , const char* str , size_t length ):
  ptr_( static_cast<const char*>(zone->Malloc(length+1)) ),
  size_(length)
{ memcpy((void*)ptr_,str,length); ((char*)ptr_)[length] = 0; }

inline String::String( Zone* zone , const std::string& str ):
  ptr_( static_cast<const char*>(zone->Malloc(str.size()+1)) ),
  size_(str.size())
{ memcpy((void*)ptr_,str.c_str(),str.size()+1); }

inline String::String( Zone* zone , const String& str ):
  ptr_( static_cast<const char*>(zone->Malloc(str.size()+1)) ),
  size_(str.size())
{ memcpy((void*)ptr_,str.data(),str.size()+1); }


} // namespace zone
} // namespace lavascript

#endif // ZONE_STRING_H_
