#ifndef ARCH_H_
#define ARCH_H_

/* Some common CPU related definition */

#ifdef __GNUC__

#if __x86_64__
#define LAVASCRIPT_ARCH_X64
#else
#define LAVASCRIPT_ARCH_X32
#endif // __x86_64__

#ifdef __ORDER_LITTLE_ENDIAN__
#define LAVA_LITTLE_ENDIAN
#else
#define LAVA_BIG_ENDIAN
#endif // __ORDER_LITTLE_ENDIAN__

#endif // __GNUC__

#endif // ARCH_H_
