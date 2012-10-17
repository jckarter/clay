#ifndef __PATTERNS_HPP
#define __PATTERNS_HPP

#include "clay.hpp"

namespace clay {

ObjectPtr derefDeep(PatternPtr x);
MultiStaticPtr derefDeep(MultiPatternPtr x);

bool unifyObjObj(ObjectPtr a, ObjectPtr b);
bool unifyObjPattern(ObjectPtr a, PatternPtr b);
bool unifyPatternObj(PatternPtr a, ObjectPtr b);
bool unify(PatternPtr a, PatternPtr b);
bool unifyMulti(MultiPatternPtr a, MultiStaticPtr b);
bool unifyMulti(MultiPatternPtr a, MultiPatternPtr b);
bool unifyMulti(MultiPatternListPtr a, unsigned indexA,
                MultiPatternPtr b);
bool unifyMulti(MultiPatternPtr a,
                MultiPatternListPtr b, unsigned indexB);
bool unifyMulti(MultiPatternListPtr a, unsigned indexA,
                MultiPatternListPtr b, unsigned indexB);
bool unifyEmpty(MultiPatternListPtr x, unsigned index);
bool unifyEmpty(MultiPatternPtr x);

PatternPtr evaluateOnePattern(ExprPtr expr, EnvPtr env);
PatternPtr evaluateAliasPattern(GlobalAliasPtr x, MultiPatternPtr params);
MultiPatternPtr evaluateMultiPattern(ExprListPtr exprs, EnvPtr env);

void patternPrint(llvm::raw_ostream &out, PatternPtr x);
void patternPrint(llvm::raw_ostream &out, MultiPatternPtr x);

}

#endif // __PATTERNS_HPP
