#ifndef OS_H_
#define OS_H_

#ifdef __linux__
#include "os-linux.h"
#else
#error "Unsupported OS"
#endif // __linux__

#endif // OS_H_
