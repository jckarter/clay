#include "clay.hpp"

static void initializePatterns(OverloadPtr x)
{
    if (x->patternsInitializedState == 1)
        return;
    if (x->patternsInitializedState == -1)
        error("unholy recursion detected");
    assert(x->patternsInitializedState == 0);
    x->patternsInitializedState = -1;

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
        x->varArgPattern = evaluateMultiPattern(new ExprList(unpack),
                                                x->patternEnv);
    }

    x->patternsInitializedState = 1;
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

    PatternReseter reseter(overload);

    if (!unifyPatternObj(overload->callablePattern, callable))
        return new MatchCallableError(overload->target, callable);

    CodePtr code = overload->code;
    if (code->formalVarArg.ptr()) {
        if (argsKey.size() < code->formalArgs.size())
            return new MatchArityError(argsKey.size(), code->formalArgs.size(), true);
    }
    else {
        if (code->formalArgs.size() != argsKey.size())
            return new MatchArityError(argsKey.size(), code->formalArgs.size(), false);
    }

    const vector<FormalArgPtr> &formalArgs = code->formalArgs;
    for (unsigned i = 0; i < formalArgs.size(); ++i) {
        FormalArgPtr x = formalArgs[i];
        if (x->type.ptr()) {
            PatternPtr pattern = overload->argPatterns[i];
            if (!unifyPatternObj(pattern, argsKey[i].ptr()))
                return new MatchArgumentError(i, argsKey[i], x);
        }
    }
    if (code->formalVarArg.ptr() && code->formalVarArg->type.ptr()) {
        MultiStaticPtr types = new MultiStatic();
        for (unsigned i = formalArgs.size(); i < argsKey.size(); ++i)
            types->add(argsKey[i].ptr());
        if (!unifyMulti(overload->varArgPattern, types))
            return new MatchMultiArgumentError(formalArgs.size(), types, code->formalVarArg);
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
            return new MatchPredicateError(code->predicate);
    }

    MatchSuccessPtr result = new MatchSuccess(
        overload->callByName, overload->isInline, code, staticEnv,
        callable, argsKey
    );
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

void printMatchError(ostream &os, MatchResultPtr result)
{
    switch (result->matchCode) {
    case MATCH_SUCCESS :
        assert(false);
        break;
    case MATCH_CALLABLE_ERROR : {
        MatchCallableError *e = (MatchCallableError*)result.ptr();
        os << "callable pattern \"" << shortString(e->patternExpr->asString());
        os << "\" did not match \"";
        printStaticName(os, e->callable);
        os << "\"";
        break;
    }
    case MATCH_ARITY_ERROR : {
        MatchArityError *e = (MatchArityError*)result.ptr();
        os << "incorrect number of arguments: expected ";
        if (e->variadic)
            os << "at least ";
        os << e->expectedArgs << " arguments, got " << e->gotArgs << " arguments";
        break;
    }
    case MATCH_ARGUMENT_ERROR : {
        MatchArgumentError *e = (MatchArgumentError *)result.ptr();
        os << "pattern \"" << shortString(e->arg->type->asString());
        os << "\" did not match type \"";
        printStaticName(os, e->type.ptr());
        os << "\" of argument " << e->argIndex + 1;
        break;
    }
    case MATCH_MULTI_ARGUMENT_ERROR : {
        MatchMultiArgumentError *e = (MatchMultiArgumentError *)result.ptr();
        os << "variadic argument type pattern did not match types of arguments ";
        os << "starting at " << e->argIndex + 1;
        break;
    }
    case MATCH_PREDICATE_ERROR : {
        MatchPredicateError *e = (MatchPredicateError *)result.ptr();
        os << "predicate \"" << shortString(e->predicateExpr->asString()) << "\" failed";
        break;
    }
    default :
        assert(false);
        break;
    }
}
