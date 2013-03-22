#ifndef __CLAY_CODEGEN_OP_HPP
#define __CLAY_CODEGEN_OP_HPP


#include "clay.hpp"


namespace clay {


void codegenPrimOp(PrimOpPtr x,
                   MultiCValuePtr args,
                   CodegenContext* ctx,
                   MultiCValuePtr out);


}

#endif
