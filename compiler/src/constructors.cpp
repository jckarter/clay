#include "clay.hpp"

vector<OverloadPtr> typeOverloads;

void addTypeOverload(OverloadPtr x)
{
    typeOverloads.insert(typeOverloads.begin(), x);
}

void initTypeOverloads(TypePtr t)
{
    assert(!t->overloadsInitialized);

    for (unsigned i = 0; i < typeOverloads.size(); ++i) {
        OverloadPtr x = typeOverloads[i];
        EnvPtr env = new Env(x->env);
        const vector<PatternVar> &pvars = x->code->patternVars;
        for (unsigned i = 0; i < pvars.size(); ++i) {
            if (pvars[i].isMulti) {
                MultiPatternCellPtr cell = new MultiPatternCell(NULL);
                addLocal(env, pvars[i].name, cell.ptr());
            }
            else {
                PatternCellPtr cell = new PatternCell(NULL);
                addLocal(env, pvars[i].name, cell.ptr());
            }
        }
        PatternPtr pattern = evaluateOnePattern(x->target, env);
        if (unifyPatternObj(pattern, t.ptr()))
            t->overloads.push_back(x);
    }

    t->overloadsInitialized = true;
}

void initBuiltinConstructor(RecordPtr x)
{
    assert(!(x->builtinOverloadInitialized));
    x->builtinOverloadInitialized = true;

    assert(x->patternVars.size() > 0);

    ExprPtr recName = new NameRef(x->name);
    recName->location = x->name->location;

    CodePtr code = new Code();
    code->location = x->location;
    for (unsigned i = 0; i < x->patternVars.size(); ++i) {
        PatternVar pvar(false, x->patternVars[i]);
        code->patternVars.push_back(pvar);
    }

    for (unsigned i = 0; i < x->fields.size(); ++i) {
        RecordFieldPtr f = x->fields[i];
        FormalArgPtr arg = new FormalArg(f->name, f->type);
        arg->location = f->location;
        code->formalArgs.push_back(arg.ptr());
    }

    IndexingPtr retType = new Indexing(recName);
    retType->location = x->location;
    for (unsigned i = 0; i < x->patternVars.size(); ++i) {
        ExprPtr typeArg = new NameRef(x->patternVars[i]);
        typeArg->location = x->patternVars[i]->location;
        retType->args.push_back(typeArg);
    }

    CallPtr returnExpr = new Call(retType.ptr());
    returnExpr->location = x->location;
    for (unsigned i = 0; i < x->fields.size(); ++i) {
        ExprPtr callArg = new NameRef(x->fields[i]->name);
        callArg->location = x->fields[i]->location;
        returnExpr->args.push_back(callArg);
    }

    vector<ExprPtr> exprs; exprs.push_back(returnExpr.ptr());
    code->body = new Return(RETURN_VALUE, exprs);
    code->body->location = returnExpr->location;

    OverloadPtr defaultOverload = new Overload(recName, code, true);
    defaultOverload->location = x->location;
    defaultOverload->env = x->env;
    x->overloads.push_back(defaultOverload);
}
