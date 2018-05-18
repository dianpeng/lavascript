#ifndef MACRO_H_
#define MACRO_H_

#define LAVA_DISALLOW_COPY_AND_ASSIGN(X) \
  void operator = (const X&) = delete;   \
  X(const X&) = delete;

#define LAVA_DISALLOW_ASSIGN(X)          \
  void operator = (const X&) = delete;

#ifdef __GNUG__
#define LAVA_ALWAYS_INLINE inline __attribute__((always_inline))
#define LAVA_NOT_INLINE    __attribute__((noinline))
#else
#error "compiler not support currently!!"
#endif // __GNUG__

#endif // MACRO_H_
