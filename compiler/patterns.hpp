#ifndef __PATTERNS_HPP
#define __PATTERNS_HPP

#include "clay.hpp"

namespace clay {

ObjectPtr derefDeep(PatternPtr x, CompilerState* cst);
MultiStaticPtr derefDeep(MultiPatternPtr x, CompilerState* cst);

bool unifyObjObj(ObjectPtr a, ObjectPtr b, CompilerState* cst);
bool unifyObjPattern(ObjectPtr a, PatternPtr b, CompilerState* cst);
bool unifyPatternObj(PatternPtr a, ObjectPtr b, CompilerState* cst);
bool unify(PatternPtr a, PatternPtr b, CompilerState* cst);
bool unifyMulti(MultiPatternPtr a, MultiStaticPtr b, CompilerState* cst);
bool unifyMulti(MultiPatternPtr a, MultiPatternPtr b, CompilerState* cst);
bool unifyMulti(MultiPatternListPtr a, unsigned indexA,
                MultiPatternPtr b,
                CompilerState* cst);
bool unifyMulti(MultiPatternPtr a,
                MultiPatternListPtr b, unsigned indexB,
                CompilerState* cst);
bool unifyMulti(MultiPatternListPtr a, unsigned indexA,
                MultiPatternListPtr b, unsigned indexB,
                CompilerState* cst);
bool unifyEmpty(MultiPatternListPtr x, unsigned index);
bool unifyEmpty(MultiPatternPtr x);

PatternPtr evaluateOnePattern(ExprPtr expr, EnvPtr env, CompilerState* cst);
PatternPtr evaluateAliasPattern(GlobalAliasPtr x, MultiPatternPtr params,
                                CompilerState* cst);
MultiPatternPtr evaluateMultiPattern(ExprListPtr exprs, EnvPtr env,
                                     CompilerState* cst);

void patternPrint(llvm::raw_ostream &out, PatternPtr x);
void patternPrint(llvm::raw_ostream &out, MultiPatternPtr x);

}

#endif // __PATTERNS_HPP
