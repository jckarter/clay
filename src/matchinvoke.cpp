#include "clay.hpp"

MatchResultPtr matchInvoke(CodePtr code, EnvPtr codeEnv,
                           const vector<ObjectPtr> &argsKey,
                           ExprPtr callableExpr, ObjectPtr callable)
{
    if (code->formalArgs.size() != argsKey.size())
        return new MatchArityError();
    vector<PatternCellPtr> cells;
    EnvPtr patternEnv = new Env(codeEnv);
    for (unsigned i = 0; i < code->patternVars.size(); ++i) {
        PatternCellPtr cell = new PatternCell(code->patternVars[i], NULL);
        addLocal(patternEnv, code->patternVars[i], cell.ptr());
        cells.push_back(cell);
    }
    if (callableExpr.ptr()) {
        PatternPtr pattern = evaluatePattern(callableExpr, patternEnv);
        if (!unify(pattern, callable))
            return new MatchCallableError();
    }
    const vector<FormalArgPtr> &formalArgs = code->formalArgs;
    for (unsigned i = 0; i < formalArgs.size(); ++i) {
        FormalArgPtr farg = formalArgs[i];
        switch (farg->objKind) {
        case VALUE_ARG : {
            ValueArg *x = (ValueArg *)farg.ptr();
            if (x->type.ptr()) {
                PatternPtr pattern = evaluatePattern(x->type, patternEnv);
                if (!unify(pattern, argsKey[i]))
                    return new MatchArgumentError(i);
            }
            break;
        }
        case STATIC_ARG : {
            StaticArg *x = (StaticArg *)farg.ptr();
            PatternPtr pattern = evaluatePattern(x->pattern, patternEnv);
            if (!unify(pattern, argsKey[i]))
                return new MatchArgumentError(i);
            break;
        }
        default :
            assert(false);
        }
    }
    EnvPtr staticEnv = new Env(codeEnv);
    for (unsigned i = 0; i < cells.size(); ++i)
        addLocal(staticEnv, code->patternVars[i], derefCell(cells[i]));
    if (code->predicate.ptr()) {
        if (!evaluateBool(code->predicate, staticEnv))
            return new MatchPredicateError();
    }
    MatchSuccessPtr result = new MatchSuccess(staticEnv);
    for (unsigned i = 0; i < formalArgs.size(); ++i) {
        FormalArgPtr farg = formalArgs[i];
        switch (farg->objKind) {
        case VALUE_ARG : {
            ValueArg *x = (ValueArg *)farg.ptr();
            result->argNames.push_back(x->name);
            assert(argsKey[i]->objKind == TYPE);
            TypePtr y = (Type *)argsKey[i].ptr();
            result->argTypes.push_back(y);
            break;
        }
        case STATIC_ARG : {
            StaticArg *x = (StaticArg *)farg.ptr();
            ObjectPtr y = evaluateStatic(x->pattern, staticEnv);
            result->staticArgs.push_back(y);
            break;
        }
        default :
            assert(false);
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
