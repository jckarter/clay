#include "clay.hpp"

vector<OverloadPtr> pointerOverloads;
vector<OverloadPtr> codePointerOverloads;
vector<OverloadPtr> cCodePointerOverloads;
vector<OverloadPtr> arrayOverloads;
vector<OverloadPtr> vecOverloads;
vector<OverloadPtr> tupleOverloads;
vector<OverloadPtr> unionOverloads;
vector<OverloadPtr> staticOverloads;

vector<OverloadPtr> typeOverloads;

bool isOverloadablePrimOp(ObjectPtr x)
{
    if (x->objKind != PRIM_OP)
        return false;
    PrimOpPtr y = (PrimOp *)x.ptr();
    switch (y->primOpCode) {
    case PRIM_Pointer :
    case PRIM_CodePointer :
    case PRIM_CCodePointer :
    case PRIM_Array :
    case PRIM_Vec :
    case PRIM_Tuple :
    case PRIM_Union :
    case PRIM_Static :
        return true;
    default :
        return false;
    }
}

vector<OverloadPtr> &primOpOverloads(PrimOpPtr x)
{
    vector<OverloadPtr> *ptr = NULL;
    switch (x->primOpCode) {
    case PRIM_Pointer :
        ptr = &pointerOverloads;
        break;
    case PRIM_CodePointer :
        ptr = &codePointerOverloads;
        break;
    case PRIM_CCodePointer :
        ptr = &cCodePointerOverloads;
        break;
    case PRIM_Array :
        ptr = &arrayOverloads;
        break;
    case PRIM_Vec :
        ptr = &vecOverloads;
        break;
    case PRIM_Tuple :
        ptr = &tupleOverloads;
        break;
    case PRIM_Union :
        ptr = &unionOverloads;
        break;
    case PRIM_Static :
        ptr = &staticOverloads;
        break;
    default :
        assert(false);
    }
    return *ptr;
}

void addPrimOpOverload(PrimOpPtr x, OverloadPtr overload)
{
    vector<OverloadPtr> &v = primOpOverloads(x);
    v.insert(v.begin(), overload);
}

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

    IndexingPtr retType = new Indexing(recName, new ExprList());
    retType->location = x->location;
    for (unsigned i = 0; i < x->params.size(); ++i) {
        ExprPtr typeArg = new NameRef(x->params[i]);
        typeArg->location = x->params[i]->location;
        retType->args->add(typeArg);
    }
    if (x->varParam.ptr()) {
        ExprPtr nameRef = new NameRef(x->varParam);
        nameRef->location = x->varParam->location;
        ExprPtr typeArg = new Unpack(nameRef);
        typeArg->location = nameRef->location;
        retType->args->add(typeArg);
    }

    CallPtr returnExpr = new Call(retType.ptr(), new ExprList());
    returnExpr->location = x->location;
    for (unsigned i = 0; i < y->fields.size(); ++i) {
        ExprPtr callArg = new NameRef(y->fields[i]->name);
        callArg->location = y->fields[i]->location;
        returnExpr->args->add(callArg);
    }

    code->body = new Return(RETURN_VALUE, new ExprList(returnExpr.ptr()));
    code->body->location = returnExpr->location;

    OverloadPtr defaultOverload = new Overload(recName, code, true);
    defaultOverload->location = x->location;
    defaultOverload->env = x->env;
    x->overloads.push_back(defaultOverload);
}
