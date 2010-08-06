#include "clay.hpp"

static void initializePatterns(OverloadPtr x)
{
    if (x->patternsInitialized)
        return;
    x->patternsInitialized = true;
    CodePtr code = x->code;
    const vector<PatternVar> &pvars = code->patternVars;
    x->patternEnv = new Env(x->env);
    for (unsigned i = 0; i < pvars.size(); ++i) {
        if (pvars[i].isMulti) {
            MultiPatternCellPtr multiCell = new MultiPatternCell(NULL);
            x->multiCells.push_back(multiCell);
            x->cells.push_back(NULL);
            addLocal(x->patternEnv, pvars[i].name, multiCell.ptr());
        }
        else {
            PatternCellPtr cell = new PatternCell(NULL);
            x->cells.push_back(cell);
            x->multiCells.push_back(NULL);
            addLocal(x->patternEnv, pvars[i].name, cell.ptr());
        }
    }

    assert(x->target.ptr());
    x->callablePattern = evaluateOnePattern(x->target, x->patternEnv);

    const vector<FormalArgPtr> &formalArgs = code->formalArgs;
    for (unsigned i = 0; i < formalArgs.size(); ++i) {
        FormalArgPtr y = formalArgs[i];
        PatternPtr pattern;
        if (y->type.ptr())
            pattern = evaluateOnePattern(y->type, x->patternEnv);
        x->argPatterns.push_back(pattern);
    }

    if (code->formalVarArg.ptr() && code->formalVarArg->type.ptr()) {
        ExprPtr unpack = new Unpack(code->formalVarArg->type.ptr());
        unpack->location = code->formalVarArg->type->location;
        vector<ExprPtr> exprs;
        exprs.push_back(unpack);
        x->varArgPattern = evaluateMultiPattern(exprs, x->patternEnv);
    }
}

static void resetPatterns(OverloadPtr x)
{
    assert(x->cells.size() == x->multiCells.size());
    for (unsigned i = 0; i < x->cells.size(); ++i) {
        if (x->cells[i].ptr()) {
            x->cells[i]->obj = NULL;
            assert(!x->multiCells[i]);
        }
        else {
            assert(x->multiCells[i].ptr());
            x->multiCells[i]->data = NULL;
        }
    }
}

struct PatternReseter {
    OverloadPtr x;
    PatternReseter(OverloadPtr x)
        : x(x) {}
    ~PatternReseter() {
        reset();
    }
    void reset() {
        if (!x)
            return;
        resetPatterns(x);
        x = NULL;
    }
};

MatchResultPtr matchInvoke(OverloadPtr overload,
                           ObjectPtr callable,
                           const vector<TypePtr> &argsKey)
{
    initializePatterns(overload);

    CodePtr code = overload->code;
    if (code->formalVarArg.ptr()) {
        if (argsKey.size() < code->formalArgs.size())
            return new MatchArityError();
    }
    else {
        if (code->formalArgs.size() != argsKey.size())
            return new MatchArityError();
    }

    PatternReseter reseter(overload);

    if (!unifyPatternObj(overload->callablePattern, callable))
        return new MatchCallableError();

    const vector<FormalArgPtr> &formalArgs = code->formalArgs;
    for (unsigned i = 0; i < formalArgs.size(); ++i) {
        FormalArgPtr x = formalArgs[i];
        if (x->type.ptr()) {
            PatternPtr pattern = overload->argPatterns[i];
            if (!unifyPatternObj(pattern, argsKey[i].ptr()))
                return new MatchArgumentError(i);
        }
    }
    if (code->formalVarArg.ptr() && code->formalVarArg->type.ptr()) {
        MultiStaticPtr types = new MultiStatic();
        for (unsigned i = formalArgs.size(); i < argsKey.size(); ++i)
            types->add(argsKey[i].ptr());
        if (!unifyMulti(overload->varArgPattern, types))
            return new MatchArgumentError(formalArgs.size());
    }

    EnvPtr staticEnv = new Env(overload->env);
    const vector<PatternVar> &pvars = code->patternVars;
    for (unsigned i = 0; i < pvars.size(); ++i) {
        if (pvars[i].isMulti) {
            MultiStaticPtr ms = derefDeep(overload->multiCells[i].ptr());
            if (!ms)
                error(pvars[i].name, "unbound pattern variable");
            addLocal(staticEnv, pvars[i].name, ms.ptr());
        }
        else {
            ObjectPtr v = derefDeep(overload->cells[i].ptr());
            if (!v)
                error(pvars[i].name, "unbound pattern variable");
            addLocal(staticEnv, pvars[i].name, v.ptr());
        }
    }

    reseter.reset();
    
    if (code->predicate.ptr()) {
        if (!evaluateBool(code->predicate, staticEnv))
            return new MatchPredicateError();
    }

    MatchSuccessPtr result = new MatchSuccess(overload->inlined,
                                              code, staticEnv,
                                              callable, argsKey);
    for (unsigned i = 0; i < formalArgs.size(); ++i) {
        FormalArgPtr x = formalArgs[i];
        result->fixedArgNames.push_back(x->name);
        result->fixedArgTypes.push_back(argsKey[i]);
    }
    if (code->formalVarArg.ptr()) {
        result->varArgName = code->formalVarArg->name;
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
