#ifndef __MATCHIVOKE_HPP
#define __MATCHIVOKE_HPP

#include "clay.hpp"

namespace clay {

//
// match invoke
//

enum MatchCode {
    MATCH_SUCCESS,
    MATCH_CALLABLE_ERROR,
    MATCH_ARITY_ERROR,
    MATCH_ARGUMENT_ERROR,
    MATCH_MULTI_ARGUMENT_ERROR,
    MATCH_PREDICATE_ERROR,
    MATCH_BINDING_ERROR,
    MATCH_MULTI_BINDING_ERROR
};

struct MatchResult : public Object {
    int matchCode;
    MatchResult(int matchCode)
        : Object(DONT_CARE), matchCode(matchCode) {}
};

struct MatchSuccess : public MatchResult {
    bool callByName;
    InlineAttribute isInline;
    CodePtr code;
    EnvPtr env;

    ObjectPtr callable;
    vector<TypePtr> argsKey;

    vector<TypePtr> fixedArgTypes;
    vector<IdentifierPtr> fixedArgNames;
    IdentifierPtr varArgName;
    vector<TypePtr> varArgTypes;
    unsigned varArgPosition;

    MatchSuccess(bool callByName, InlineAttribute isInline, CodePtr code, EnvPtr env,
                 ObjectPtr callable, const vector<TypePtr> &argsKey)
        : MatchResult(MATCH_SUCCESS), callByName(callByName),
          isInline(isInline), code(code), env(env), callable(callable),
          argsKey(argsKey), varArgPosition(0) {}
};
typedef Pointer<MatchSuccess> MatchSuccessPtr;

struct MatchCallableError : public MatchResult {
    ExprPtr patternExpr;
    ObjectPtr callable;
    MatchCallableError(ExprPtr patternExpr, ObjectPtr callable)
        : MatchResult(MATCH_CALLABLE_ERROR), patternExpr(patternExpr), callable(callable) {}
};

struct MatchArityError : public MatchResult {
    unsigned expectedArgs;
    unsigned gotArgs;
    bool variadic;
    MatchArityError(unsigned expectedArgs, unsigned gotArgs, bool variadic)
        : MatchResult(MATCH_ARITY_ERROR),
          expectedArgs(expectedArgs),
          gotArgs(gotArgs),
          variadic(variadic) {}
};

struct MatchArgumentError : public MatchResult {
    unsigned argIndex;
    TypePtr type;
    FormalArgPtr arg;
    MatchArgumentError(unsigned argIndex, TypePtr type, FormalArgPtr arg)
        : MatchResult(MATCH_ARGUMENT_ERROR), argIndex(argIndex),
            type(type), arg(arg) {}
};

struct MatchMultiArgumentError : public MatchResult {
    unsigned argIndex;
    MultiStaticPtr types;
    FormalArgPtr varArg;
    MatchMultiArgumentError(unsigned argIndex, MultiStaticPtr types, FormalArgPtr varArg)
        : MatchResult(MATCH_MULTI_ARGUMENT_ERROR),
            argIndex(argIndex), types(types), varArg(varArg) {}
};

struct MatchPredicateError : public MatchResult {
    ExprPtr predicateExpr;
    MatchPredicateError(ExprPtr predicateExpr)
        : MatchResult(MATCH_PREDICATE_ERROR), predicateExpr(predicateExpr) {}
};

struct MatchBindingError : public MatchResult {
    unsigned argIndex;
    TypePtr type;
    FormalArgPtr arg;
    MatchBindingError(unsigned argIndex, TypePtr type, FormalArgPtr arg)
        : MatchResult(MATCH_BINDING_ERROR), argIndex(argIndex),
            type(type), arg(arg) {}
};

struct MatchMultiBindingError : public MatchResult {
    unsigned argIndex;
    MultiStaticPtr types;
    FormalArgPtr varArg;
    MatchMultiBindingError(unsigned argIndex, MultiStaticPtr types, FormalArgPtr varArg)
        : MatchResult(MATCH_MULTI_BINDING_ERROR),
            argIndex(argIndex), types(types), varArg(varArg) {}
};

void initializePatternEnv(EnvPtr patternEnv, const vector<PatternVar> &pvars, vector<PatternCellPtr> &cells, vector<MultiPatternCellPtr> &multiCells);

MatchResultPtr matchInvoke(OverloadPtr overload,
                           ObjectPtr callable,
                           const vector<TypePtr> &argsKey);

void printMatchError(llvm::raw_ostream &os, const MatchResultPtr& result);

}

#endif // __MATCHIVOKE_HPP
