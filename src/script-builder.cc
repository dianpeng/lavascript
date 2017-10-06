#include "script-builder.h"
#include "interpreter/bytecode.h"

namespace lavascript {

void ScriptBuilder::Dump( DumpWriter* writer ) const {
  {
    DumpWriter::Section section(writer,"__main__");
    main_->Dump(writer);
  }

  for( auto &e : function_table_ ) {
    DumpWriter::Section section(writer,e.name ? e.name->ToStdString().c_str() : "");
    e.prototype->Dump(writer);
  }
}

} // namespace lavascript
