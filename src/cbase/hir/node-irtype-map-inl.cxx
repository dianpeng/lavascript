
template<>
struct MapIRClassToIRType<MemoryNode> {
  static bool Test( IRType type ) {
    return type == HIR_ARG ||
           type == HIR_LIST||
           type == HIR_OBJECT;
  }
};

template<>
struct MapIRClassToIRType<EffectBarrier> {
  static bool Test ( IRType type ) {
    return type == HIR_ARITHMETIC ||
           type == HIR_COMPARE    ||
           type == HIR_PGET       ||
           type == HIR_PSET       ||
           type == HIR_IGET       ||
           type == HIR_ISET;
  }
};

template<>
struct MapIRClassToIRType<WriteEffect> {
  static bool Test( IRType type ) {
    return type == HIR_GSET                              ||
           type == HIR_USET                              ||
           MapIRClassToIRType<EffectBarrier>::Test(type) ||
           type == HIR_EFFECT_PHI;
  }
};

template<>
struct MapIRClassToIRType<ReadEffect> {
  static bool Test( IRType type ) {
    return type == HIR_GGET       ||
           type == HIR_UGET;
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
