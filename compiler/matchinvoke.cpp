#include "clay.hpp"
#include "patterns.hpp"
#include "matchinvoke.hpp"
#include "evaluator.hpp"
#include "env.hpp"


namespace clay {


void initializePatternEnv(EnvPtr patternEnv, llvm::ArrayRef<PatternVar> pvars, 
    vector<PatternCellPtr> &cells, vector<MultiPatternCellPtr> &multiCells)
{
    
    for (size_t i = 0; i < pvars.size(); ++i) {
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
}

static void initializePatterns(OverloadPtr x, CompilerState* cst)
{
    if (x->patternsInitializedState == 1)
        return;
    if (x->patternsInitializedState == -1)
        error("unholy recursion detected");
    assert(x->patternsInitializedState == 0);
    x->patternsInitializedState = -1;

    CodePtr code = x->code;
    llvm::ArrayRef<PatternVar> pvars = code->patternVars;
    EnvPtr patternEnv = new Env(x->env);
    initializePatternEnv(patternEnv, pvars, x->cells, x->multiCells);
    assert(x->target.ptr());
    x->callablePattern = evaluateOnePattern(x->target, patternEnv, cst);

    llvm::ArrayRef<FormalArgPtr> formalArgs = code->formalArgs;
    for (size_t i = 0; i < formalArgs.size(); ++i) {
        FormalArgPtr y = formalArgs[i];
        PatternPtr pattern;
        if (y->type.ptr()) {
            if (y->varArg) {
                ExprPtr unpack = new Unpack(y->type.ptr());
                unpack->location = y->type->location;
                x->varArgPattern = evaluateMultiPattern(new ExprList(unpack), 
                                                        patternEnv, cst);
                pattern = NULL;
            } else {
                pattern = evaluateOnePattern(y->type, patternEnv, cst);
            }
        }
        x->argPatterns.push_back(pattern);
    }

    x->patternsInitializedState = 1;
}

static void resetPatterns(OverloadPtr x)
{
    assert(x->cells.size() == x->multiCells.size());
    for (size_t i = 0; i < x->cells.size(); ++i) {
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
                           llvm::ArrayRef<TypePtr> argsKey)
{
    CompilerState* cst = overload->env->cst;
    initializePatterns(overload, cst);

    PatternReseter reseter(overload);

    if (!unifyPatternObj(overload->callablePattern, callable, cst))
        return new MatchCallableError(overload->target, callable);

    CodePtr code = overload->code;
    if (code->hasVarArg) {
        if (argsKey.size() < code->formalArgs.size()-1)
            return new MatchArityError(unsigned(code->formalArgs.size()), unsigned(argsKey.size()), true);
    }
    else {
        if (code->formalArgs.size() != argsKey.size())
            return new MatchArityError(unsigned(code->formalArgs.size()), unsigned(argsKey.size()), false);
    }
    llvm::ArrayRef<FormalArgPtr> formalArgs = code->formalArgs;
    unsigned varArgSize = unsigned(argsKey.size()-formalArgs.size()+1);
    for (unsigned i = 0, j = 0; i < formalArgs.size(); ++i) {
        FormalArgPtr x = formalArgs[i];
        if (x->varArg) {
            if (x->type.ptr()) {
                MultiStaticPtr types = new MultiStatic();
                for (; j < varArgSize; ++j)
                    types->add(argsKey[i+j].ptr());
                --j;
                MultiPatternPtr pattern = overload->varArgPattern;
                if (!unifyMulti(pattern, types, cst))
                    return new MatchMultiArgumentError(unsigned(formalArgs.size()), types, x);                
            } else {
                j = varArgSize-1;
            }
        } else {
            if (x->type.ptr()) {
                PatternPtr pattern = overload->argPatterns[i];
                if (!unifyPatternObj(pattern, argsKey[i+j].ptr(), cst))
                    return new MatchArgumentError(i+j, argsKey[i+j], x);
            }
        }
    }
    
    EnvPtr staticEnv = new Env(overload->env);
    llvm::ArrayRef<PatternVar> pvars = code->patternVars;
    for (size_t i = 0; i < pvars.size(); ++i) {
        if (pvars[i].isMulti) {
            MultiStaticPtr ms = derefDeep(overload->multiCells[i].ptr(), cst);
            if (!ms)
                error(pvars[i].name, "unbound pattern variable");
            addLocal(staticEnv, pvars[i].name, ms.ptr());
        }
        else {
            ObjectPtr v = derefDeep(overload->cells[i].ptr(), cst);
            if (!v)
                error(pvars[i].name, "unbound pattern variable");
            addLocal(staticEnv, pvars[i].name, v.ptr());
        }
    }

    reseter.reset();
    
    if (code->predicate.ptr())
        if (!evaluateBool(code->predicate, staticEnv, cst))
            return new MatchPredicateError(code->predicate);

    MatchSuccessPtr result = new MatchSuccess(
        overload, staticEnv, callable, argsKey
    );

    for (unsigned i = 0, j = 0; i < formalArgs.size(); ++i) {
        FormalArgPtr x = formalArgs[i];
        if (x->varArg) {
            result->varArgName = x->name;
            result->varArgPosition = i;
            for (; j < varArgSize; ++j) {
                result->varArgTypes.push_back(argsKey[i+j]);
            }
            --j;
        } else {
            result->fixedArgNames.push_back(x->name);
            result->fixedArgTypes.push_back(argsKey[i+j]);
        }
    }
    if(!code->hasVarArg) result->varArgPosition = unsigned(result->fixedArgNames.size());
    return result.ptr();
}

void printMatchError(llvm::raw_ostream &os, const MatchResultPtr& result)
{
    switch (result->matchCode) {
    case MATCH_SUCCESS :
        os << "matched";
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
    case MATCH_BINDING_ERROR : {
        MatchBindingError *e = (MatchBindingError *)result.ptr();
        os << "pattern \"" << shortString(e->arg->type->asString());
        os << "\" did not match type \"";
        printStaticName(os, e->type.ptr());
        os << "\" of binding " << e->argIndex + 1;
        break;
    }
    case MATCH_MULTI_BINDING_ERROR : {
        MatchMultiBindingError *e = (MatchMultiBindingError *)result.ptr();
        os << "variadic binding type pattern did not match types of values ";
        os << "starting at " << e->argIndex + 1;
        break;
    }
    default :
        assert(false);
        break;
    }
}

}
