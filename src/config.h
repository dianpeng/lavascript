#ifndef CONFIG_H_
#define CONFIG_H_
#include <cstdint>

namespace lavascript {

static const std::size_t kSSOMaxSize = 32;

static const std::size_t kDefaultListSize = 4;

static const std::size_t kDefaultObjectSize = 8;

static const std::size_t kMaxLiteralCount = 65536;

static const std::size_t kMaxPrototypeCont = 65535;

} // namespace lavascript

#endif // CONFIG_H_
