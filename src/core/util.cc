#include "util.h"

#include <cstdio>

namespace lavascript {
namespace core {


void FormatV(std::string* buffer, const char* format, va_list vl) {
  va_list backup;
  size_t old_size = buffer->size();

  va_copy(backup, vl);
  buffer->resize(old_size + 128);

  int ret = vsnprintf(AsBuffer(buffer, old_size), 128, format, vl);
  if (ret >= 128) {
    buffer->resize(old_size + ret);
    ret = vsnprintf(AsBuffer(buffer, old_size), ret, format, backup);
  }
  buffer->resize(old_size + ret);
}

} // namespace core
} // namespace lavascript
