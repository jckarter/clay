#include "clay.hpp"

InvokeEntryPtr findMatchingInvoke(const vector<OverloadPtr> &overloads,
                                  unsigned &overloadIndex,
                                  ObjectPtr callable,
                                  const vector<TypePtr> &argsKey)
{
    while (overloadIndex < overloads.size()) {
        OverloadPtr x = overloads[overloadIndex++];
        MatchResultPtr result = matchInvoke(x, callable, argsKey);
        if (result->matchCode == MATCH_SUCCESS) {
            InvokeEntryPtr entry = new InvokeEntry(callable, argsKey);
            entry->code = clone(x->code);
            MatchSuccess *y = (MatchSuccess *)result.ptr();
            entry->env = y->env;
            entry->fixedArgTypes = y->fixedArgTypes;
            entry->fixedArgNames = y->fixedArgNames;
            entry->varArgName = y->varArgName;
            entry->varArgTypes = y->varArgTypes;
            entry->inlined = x->inlined;
            return entry;
        }
    }
    return NULL;
}

MatchResultPtr matchInvoke(OverloadPtr overload,
                           ObjectPtr callable,
                           const vector<TypePtr> &argsKey)
{
    CodePtr code = overload->code;
    if (code->formalVarArg.ptr()) {
        if (argsKey.size() < code->formalArgs.size())
            return new MatchArityError();
    }
    else {
        if (code->formalArgs.size() != argsKey.size())
            return new MatchArityError();
    }
    vector<PatternCellPtr> cells;
    vector<MultiPatternCellPtr> multiCells;
    const vector<PatternVar> &pvars = code->patternVars;
    EnvPtr patternEnv = new Env(overload->env);
    for (unsigned i = 0; i < pvars.size(); ++i) {
        if (pvars[i].isMulti) {
            MultiPatternCellPtr multiCell = new MultiPatternCell(NULL);
            multiCells.push_back(multiCell);
            cells.push_back(NULL);
            addLocal(patternEnv, pvars[i].name, multiCell.ptr());
        }
        else {
            PatternCellPtr cell = new PatternCell(NULL);
            cells.push_back(cell);
            multiCells.push_back(NULL);
            addLocal(patternEnv, pvars[i].name, cell.ptr());
        }
    }

    assert(overload->target.ptr());
    PatternPtr callablePattern =
        evaluateOnePattern(overload->target, patternEnv);
    if (!unifyPatternObj(callablePattern, callable))
        return new MatchCallableError();

    const vector<FormalArgPtr> &formalArgs = code->formalArgs;
    for (unsigned i = 0; i < formalArgs.size(); ++i) {
        FormalArgPtr x = formalArgs[i];
        if (x->type.ptr()) {
            PatternPtr pattern = evaluateOnePattern(x->type, patternEnv);
            if (!unifyPatternObj(pattern, argsKey[i].ptr()))
                return new MatchArgumentError(i);
        }
    }
    if (code->formalVarArg.ptr() && code->formalVarArg->type.ptr()) {
        ExprPtr unpack = new Unpack(code->formalVarArg->type.ptr());
        unpack->location = code->formalVarArg->type->location;
        vector<ExprPtr> exprs;
        exprs.push_back(unpack);
        MultiPatternPtr pattern = evaluateMultiPattern(exprs, patternEnv);
        MultiStaticPtr types = new MultiStatic();
        for (unsigned i = formalArgs.size(); i < argsKey.size(); ++i)
            types->add(argsKey[i].ptr());
        if (!unifyMulti(pattern, types))
            return new MatchArgumentError(formalArgs.size());
    }
    EnvPtr staticEnv = new Env(overload->env);
    for (unsigned i = 0; i < pvars.size(); ++i) {
        if (pvars[i].isMulti) {
            MultiStaticPtr ms = derefDeep(multiCells[i].ptr());
            if (!ms)
                error(pvars[i].name, "unbound pattern variable");
            addLocal(staticEnv, pvars[i].name, ms.ptr());
        }
        else {
            ObjectPtr v = derefDeep(cells[i].ptr());
            if (!v)
                error(pvars[i].name, "unbound pattern variable");
            addLocal(staticEnv, pvars[i].name, v.ptr());
        }
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
