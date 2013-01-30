#include "clay.hpp"
#include "loader.hpp"
#include "patterns.hpp"
#include "analyzer.hpp"
#include "constructors.hpp"
#include "env.hpp"


namespace clay {

bool isOverloadablePrimOp(ObjectPtr x)
{
    if (x->objKind != PRIM_OP)
        return false;
    PrimOpPtr y = (PrimOp *)x.ptr();
    switch (y->primOpCode) {
    case PRIM_Pointer :
    case PRIM_CodePointer :
    case PRIM_ExternalCodePointer :
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

vector<OverloadPtr> &primOpOverloads(PrimOpPtr x,
                                     CompilerState* cst)
{
    vector<OverloadPtr> *ptr = NULL;
    switch (x->primOpCode) {
    case PRIM_Pointer :
        ptr = &cst->pointerOverloads;
        break;
    case PRIM_CodePointer :
        ptr = &cst->codePointerOverloads;
        break;
    case PRIM_ExternalCodePointer :
        ptr = &cst->cCodePointerOverloads;
        break;
    case PRIM_Array :
        ptr = &cst->arrayOverloads;
        break;
    case PRIM_Vec :
        ptr = &cst->vecOverloads;
        break;
    case PRIM_Tuple :
        ptr = &cst->tupleOverloads;
        break;
    case PRIM_Union :
        ptr = &cst->unionOverloads;
        break;
    case PRIM_Static :
        ptr = &cst->staticOverloads;
        break;
    default :
        assert(false);
    }
    return *ptr;
}

vector<OverloadPtr> &getPatternOverloads(CompilerState* cst)
{
    return cst->patternOverloads;
}

void initBuiltinConstructor(RecordDeclPtr x)
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
    code->hasVarArg = y->hasVarField;
    for (size_t i = 0; i < x->params.size(); ++i) {
        PatternVar pvar(false, x->params[i]);
        code->patternVars.push_back(pvar);
    }
    if (x->varParam.ptr()) {
        PatternVar pvar(true, x->varParam);
        code->patternVars.push_back(pvar);
    }

    for (size_t i = 0; i < y->fields.size(); ++i) {
        RecordFieldPtr f = y->fields[i];
        FormalArgPtr arg = new FormalArg(f->name, f->type, TEMPNESS_DONTCARE, f->varField);
        arg->location = f->location;
        code->formalArgs.push_back(arg.ptr());
    }

    IndexingPtr retType = new Indexing(recName, new ExprList());
    retType->location = x->location;
    for (size_t i = 0; i < x->params.size(); ++i) {
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
    for (size_t i = 0; i < y->fields.size(); ++i) {
        ExprPtr callArg = new NameRef(y->fields[i]->name);
        callArg->location = y->fields[i]->location;
        returnExpr->parenArgs->add(callArg);
    }

    code->body = new Return(RETURN_VALUE, new ExprList(returnExpr.ptr()));
    code->body->location = returnExpr->location;

    OverloadPtr defaultOverload = new Overload(x->module, recName, code, true, IGNORE);
    defaultOverload->location = x->location;
    defaultOverload->env = x->env;
    x->overloads.push_back(defaultOverload);
}

}
