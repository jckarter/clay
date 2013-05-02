#pragma once


#include "clay.hpp"


namespace clay {


void codegenPrimOp(PrimOpPtr x,
                   MultiCValuePtr args,
                   CodegenContext* ctx,
                   MultiCValuePtr out);


}
