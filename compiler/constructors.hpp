#ifndef __CONSTRUCTORS_HPP
#define __CONSTRUCTORS_HPP

#include "clay.hpp"

namespace clay {

bool isOverloadablePrimOp(ObjectPtr x);
vector<OverloadPtr> &primOpOverloads(PrimOpPtr x,
                                     CompilerState* cst);
vector<OverloadPtr> &getPatternOverloads(CompilerState* cst);
void initBuiltinConstructor(RecordDeclPtr x);

}

#endif // __CONSTRUCTORS_HPP
