
#define  define_ir_class_type_map(NAME,...)      \
  template<> struct MapIRClassToIRType<NAME> {   \
    static bool Test( IRType type ) __VA_ARGS__  \
  };                                             \

define_ir_class_type_map(MemoryNode, {
  return type == HIR_ARG ||
         type == HIR_LIST||
         type == HIR_OBJECT;
})

define_ir_class_type_map(SoftBarrier,{
  return type == HIR_EFFECT_PHI      ||
         type == HIR_LOOP_EFFECT_PHI ||
         type == HIR_INIT_BARRIER    ||
         type == HIR_EMPTY_BARRIER   ||
         type == HIR_OBJECT_UPDATE   ||
         type == HIR_OBJECT_INSERT   ||
         type == HIR_LIST_INSERT;
})

define_ir_class_type_map(DynamicBinary,{
  return type == HIR_ARITHMETIC ||
         type == HIR_COMPARE;
})

define_ir_class_type_map(HardBarrier,{
  return type == HIR_PGET  ||
         type == HIR_PSET  ||
         type == HIR_IGET  ||
         type == HIR_ISET  ||
         MapIRClassToIRType<DynamicBinary>::Test(type);
})

define_ir_class_type_map(EffectBarrier,{
  return MapIRClassToIRType<SoftBarrier>::Test(type) ||
         MapIRClassToIRType<HardBarrier>::Test(type);
})

define_ir_class_type_map(WriteEffect,{
  return MapIRClassToIRType<EffectBarrier>::Test(type) ||
         type == HIR_GSET                              ||
         type == HIR_USET;
})

define_ir_class_type_map(ReadEffect,{
  return type == HIR_OBJECT_FIND ||
         type == HIR_LIST_INDEX  ||
         type == HIR_GGET        ||
         type == HIR_UGET;
})

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
