#include "clay.hpp"

MatchResultPtr matchInvoke(CodePtr code, EnvPtr codeEnv,
                           const vector<ObjectPtr> &argsKey)
{
    assert(code->formalArgs.size() == argsKey.size());
    vector<PatternCellPtr> cells;
    EnvPtr patternEnv = new Env(codeEnv);
    for (unsigned i = 0; i < code->patternVars.size(); ++i) {
        PatternCellPtr cell = new PatternCell(code->patternVars[i], NULL);
        addLocal(patternEnv, code->patternVars[i], cell.ptr());
        cells.push_back(cell);
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
    EnvPtr bodyEnv = new Env(codeEnv);
    for (unsigned i = 0; i < cells.size(); ++i)
        addLocal(bodyEnv, code->patternVars[i], derefCell(cells[i]));
    if (code->predicate.ptr()) {
        if (!evaluateBool(code->predicate, bodyEnv))
            return new MatchPredicateError();
    }
    return new MatchSuccess(bodyEnv);
}

void signalMatchError(MatchResultPtr result,
                      const vector<LocationPtr> &argLocations)
{
    switch (result->matchCode) {
    case MATCH_SUCCESS :
        assert(false);
        break;
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
    }
}
