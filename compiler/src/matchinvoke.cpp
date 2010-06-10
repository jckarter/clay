#include "clay.hpp"

MatchResultPtr matchInvoke(CodePtr code, EnvPtr codeEnv,
                           const vector<TypePtr> &argsKey,
                           ExprPtr callableExpr, ObjectPtr callable)
{
    if (code->formalVarArg.ptr()) {
        if (argsKey.size() < code->formalArgs.size())
            return new MatchArityError();
    }
    else {
        if (code->formalArgs.size() != argsKey.size())
            return new MatchArityError();
    }
    vector<PatternCellPtr> cells;
    EnvPtr patternEnv = new Env(codeEnv);
    for (unsigned i = 0; i < code->patternVars.size(); ++i) {
        PatternCellPtr cell = new PatternCell(NULL);
        addLocal(patternEnv, code->patternVars[i], cell.ptr());
        cells.push_back(cell);
    }
    if (callableExpr.ptr()) {
        PatternPtr pattern = evaluateOnePattern(callableExpr, patternEnv);
        if (!unifyPatternObj(pattern, callable))
            return new MatchCallableError();
    }
    const vector<FormalArgPtr> &formalArgs = code->formalArgs;
    for (unsigned i = 0; i < formalArgs.size(); ++i) {
        FormalArgPtr x = formalArgs[i];
        if (x->type.ptr()) {
            PatternPtr pattern = evaluateOnePattern(x->type, patternEnv);
            if (!unifyPatternObj(pattern, argsKey[i].ptr()))
                return new MatchArgumentError(i);
        }
    }
    EnvPtr staticEnv = new Env(codeEnv);
    for (unsigned i = 0; i < cells.size(); ++i) {
        ObjectPtr v = derefDeep(cells[i].ptr());
        if (!v)
            error(code->patternVars[i], "unbound pattern variable");
        addLocal(staticEnv, code->patternVars[i], v);
    }
    if (code->predicate.ptr()) {
        if (!evaluateBool(code->predicate, staticEnv))
            return new MatchPredicateError();
    }
    MatchSuccessPtr result = new MatchSuccess(staticEnv);
    for (unsigned i = 0; i < formalArgs.size(); ++i) {
        FormalArgPtr x = formalArgs[i];
        result->fixedArgNames.push_back(x->name);
        result->fixedArgTypes.push_back(argsKey[i]);
    }

    if (code->formalVarArg.ptr()) {
        result->varArgName = code->formalVarArg;
        for (unsigned i = formalArgs.size(); i < argsKey.size(); ++i) {
            result->varArgTypes.push_back(argsKey[i]);
        }
    }
    return result.ptr();
}

void signalMatchError(MatchResultPtr result,
                      const vector<LocationPtr> &argLocations)
{
    switch (result->matchCode) {
    case MATCH_SUCCESS :
        assert(false);
        break;
    case MATCH_CALLABLE_ERROR : {
        error("callable mismatch");
        break;
    }
    case MATCH_ARITY_ERROR : {
        error("incorrect no. of arguments");
    }
    case MATCH_ARGUMENT_ERROR : {
        MatchArgumentError *e = (MatchArgumentError *)result.ptr();
        LocationPtr loc = argLocations[e->argIndex];
        if (loc.ptr())
            pushLocation(loc);
        error("argument mismatch");
        break;
    }
    case MATCH_PREDICATE_ERROR :
        error("invoke predicate failure");
    default :
        assert(false);
    }
}
