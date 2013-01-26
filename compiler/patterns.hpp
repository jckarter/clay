#ifndef __PATTERNS_HPP
#define __PATTERNS_HPP

#include "clay.hpp"

namespace clay {

ObjectPtr derefDeep(PatternPtr x, CompilerStatePtr cst);
MultiStaticPtr derefDeep(MultiPatternPtr x, CompilerStatePtr cst);

bool unifyObjObj(ObjectPtr a, ObjectPtr b, CompilerStatePtr cst);
bool unifyObjPattern(ObjectPtr a, PatternPtr b, CompilerStatePtr cst);
bool unifyPatternObj(PatternPtr a, ObjectPtr b, CompilerStatePtr cst);
bool unify(PatternPtr a, PatternPtr b, CompilerStatePtr cst);
bool unifyMulti(MultiPatternPtr a, MultiStaticPtr b, CompilerStatePtr cst);
bool unifyMulti(MultiPatternPtr a, MultiPatternPtr b, CompilerStatePtr cst);
bool unifyMulti(MultiPatternListPtr a, unsigned indexA,
                MultiPatternPtr b,
                CompilerStatePtr cst);
bool unifyMulti(MultiPatternPtr a,
                MultiPatternListPtr b, unsigned indexB,
                CompilerStatePtr cst);
bool unifyMulti(MultiPatternListPtr a, unsigned indexA,
                MultiPatternListPtr b, unsigned indexB,
                CompilerStatePtr cst);
bool unifyEmpty(MultiPatternListPtr x, unsigned index);
bool unifyEmpty(MultiPatternPtr x);

PatternPtr evaluateOnePattern(ExprPtr expr, EnvPtr env, CompilerStatePtr cst);
PatternPtr evaluateAliasPattern(GlobalAliasPtr x, MultiPatternPtr params,
                                CompilerStatePtr cst);
MultiPatternPtr evaluateMultiPattern(ExprListPtr exprs, EnvPtr env,
                                     CompilerStatePtr cst);

void patternPrint(llvm::raw_ostream &out, PatternPtr x);
void patternPrint(llvm::raw_ostream &out, MultiPatternPtr x);

}

#endif // __PATTERNS_HPP
