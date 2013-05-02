#pragma once


#include "clay.hpp"

namespace clay {

bool isOverloadablePrimOp(ObjectPtr x);
vector<OverloadPtr> &primOpOverloads(PrimOpPtr x);
vector<OverloadPtr> &getPatternOverloads();
void initBuiltinConstructor(RecordDeclPtr x);

}
