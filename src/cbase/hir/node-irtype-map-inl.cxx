
template<>
struct MapIRClassToIRType<MemoryNode> {
  static bool Test( IRType type ) {
    return type == HIR_ARG ||
           type == HIR_LIST||
           type == HIR_OBJECT;
  }
};

template<>
struct MapIRClassToIRType<MemoryWrite> {
  static bool Test( IRType type ) {
    return type == HIR_ISET ||
           type == HIR_PSET ||
           type == HIR_OBJECT_SET ||
           type == HIR_LIST_SET   ||
           type == HIR_GSET       ||
           type == HIR_USET;
  }
};

template<>
struct MapIRClassToIRType<MemoryRead> {
  static bool Test( IRType type ) {
    return type == HIR_IGET ||
           type == HIR_PGET ||
           type == HIR_OBJECT_GET ||
           type == HIR_LIST_GET   ||
           type == HIR_GGET       ||
           type == HIR_UGET;
  }
};

template<>
struct MapIRClassToIRType<WriteBarrier> {
  static bool Test ( IRType type ) {
    return type == HIR_ARITHMETIC ||
           type == HIR_COMPARE;
  }
};

template<>
struct MapIRClassToIRType<WriteEffect> {
  static bool Test( IRType type ) {
    return MapIRClassToIRType<MemoryWrite>::Test(type)   ||
           MapIRClassToIRType<WriteBarrier>::Test(type) ||
           type == HIR_WRITE_EFFECT_PHI                  ||
           type == HIR_NO_WRITE_EFFECT;
  }
};

template<>
struct MapIRClassToIRType<ReadEffect> {
  static bool Test( IRType type ) {
    return MapIRClassToIRType<MemoryRead>::Test(type)  ||
           type == HIR_NO_READ_EFFECT;
  }
};

template<> struct MapIRClassToIRType<Expr> {
  static bool Test( IRType type ) {
#define __(A,B,...) case HIR_##B: return true;
    switch(type) { CBASE_HIR_EXPRESSION(__) default: return false; }
#undef __ // __
  }
};

template<> struct MapIRClassToIRType<ControlFlow> {
  static bool Test( IRType type ) {
#define __(A,B,...) case HIR_##B: return true;
    switch(type) { CBASE_HIR_CONTROL_FLOW(__) default: return false; }
#undef __ // __
  }
};

template<> struct MapIRClassToIRType<Test> {
  static bool Test( IRType type ) {
#define __(A,B,...) case HIR_##B: return true;
    switch(type) { CBASE_HIR_TEST(__) default: return false; }
#undef __ // __
  }
};
