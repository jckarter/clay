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

    RecordBodyPtr y =  x->body;
    if (y->isComputed)
        return;

    assert((x->params.size() > 0) || x->varParam.ptr());

    ExprPtr recName = new NameRef(x->name);
    recName->location = x->name->location;

    CodePtr code = new Code();
    code->location = x->location;
    for (unsigned i = 0; i < x->params.size(); ++i) {
        PatternVar pvar(false, x->params[i]);
        code->patternVars.push_back(pvar);
    }
    if (x->varParam.ptr()) {
        PatternVar pvar(true, x->varParam);
        code->patternVars.push_back(pvar);
    }

    for (unsigned i = 0; i < y->fields.size(); ++i) {
        RecordFieldPtr f = y->fields[i];
        FormalArgPtr arg = new FormalArg(f->name, f->type);
        arg->location = f->location;
        code->formalArgs.push_back(arg.ptr());
    }

    IndexingPtr retType = new Indexing(recName);
    retType->location = x->location;
    for (unsigned i = 0; i < x->params.size(); ++i) {
        ExprPtr typeArg = new NameRef(x->params[i]);
        typeArg->location = x->params[i]->location;
        retType->args.push_back(typeArg);
    }
    if (x->varParam.ptr()) {
        ExprPtr nameRef = new NameRef(x->varParam);
        nameRef->location = x->varParam->location;
        ExprPtr typeArg = new Unpack(nameRef);
        typeArg->location = nameRef->location;
        retType->args.push_back(typeArg);
    }

    CallPtr returnExpr = new Call(retType.ptr());
    returnExpr->location = x->location;
    for (unsigned i = 0; i < y->fields.size(); ++i) {
        ExprPtr callArg = new NameRef(y->fields[i]->name);
        callArg->location = y->fields[i]->location;
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
