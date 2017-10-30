#ifndef CONFIG_H_
#define CONFIG_H_
#include <cstdint>

namespace lavascript {

/* ---------------------------------------------------------------
 *
 *  The following configuration value are *fixed* and cannot be
 *  mutated. The assumption is baked inside of how the code is
 *  designed
 *
 * ---------------------------------------------------------------*/

static const std::size_t kSSOMaxSize = 32;

static const std::size_t kDefaultListSize = 4;

static const std::size_t kDefaultObjectSize = 8;

static const std::size_t kMaxLiteralCount = 65536;

static const std::size_t kMaxPrototypeCont = 65535;

static const std::size_t kMaxFunctionArgumentCount = 256;

static const std::size_t kMaxListEntryCount = 65536;

static const std::size_t kMaxObjectEntryCount = 65536;

static const std::size_t kMaxPrototypeSize = 65536;

static const std::size_t kInterpreterInitStackSize = 1024;

static const std::size_t kInterpreterMaxStackSize  = 1024*640;

static const std::size_t kInterpreterMaxCallSize   = 1024*20;

} // namespace lavascript

#endif // CONFIG_H_
