#ifndef ARCH_H_
#define ARCH_H_


/**
 * Some common CPU related definition
 */

#ifdef __GNUC__
#if __x86_64__
#define LAVASCRIPT_ARCH_X64
#else
#define LAVASCRIPT_ARCH_X32
#endif // __x86_64__
#endif // __GNUC__


#endif // ARCH_H_
