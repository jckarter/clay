#ifndef __CONSTRUCTORS_HPP
#define __CONSTRUCTORS_HPP

#include "clay.hpp"

namespace clay {

bool isOverloadablePrimOp(ObjectPtr x);
vector<OverloadPtr> &primOpOverloads(PrimOpPtr x);
void addPrimOpOverload(PrimOpPtr x, OverloadPtr overload);
void addPatternOverload(OverloadPtr x);
void initTypeOverloads(TypePtr t);
void initBuiltinConstructor(RecordPtr x);

}

#endif // __CONSTRUCTORS_HPP
