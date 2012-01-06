#include "clay.hpp"

namespace clay {

MultiEValuePtr evalMultiArgsAsRef(ExprListPtr exprs, EnvPtr env);
MultiEValuePtr evalArgExprAsRef(ExprPtr x, EnvPtr env);

EValuePtr evalForwardOneAsRef(ExprPtr expr, EnvPtr env);
MultiEValuePtr evalForwardMultiAsRef(ExprListPtr exprs, EnvPtr env);
MultiEValuePtr evalForwardExprAsRef(ExprPtr expr, EnvPtr env);

EValuePtr evalOneAsRef(ExprPtr expr, EnvPtr env);
MultiEValuePtr evalMultiAsRef(ExprListPtr exprs, EnvPtr env);
MultiEValuePtr evalExprAsRef(ExprPtr expr, EnvPtr env);

void evalOneInto(ExprPtr expr, EnvPtr env, EValuePtr out);
void evalMultiInto(ExprListPtr exprs, EnvPtr env, MultiEValuePtr out, unsigned wantCount);
void evalExprInto(ExprPtr expr, EnvPtr env, MultiEValuePtr out);

void evalMulti(ExprListPtr exprs, EnvPtr env, MultiEValuePtr out, unsigned wantCount);
void evalOne(ExprPtr expr, EnvPtr env, EValuePtr out);
void evalExpr(ExprPtr expr, EnvPtr env, MultiEValuePtr out);
void evalStaticObject(ObjectPtr x, MultiEValuePtr out);
void evalValueHolder(ValueHolderPtr x, MultiEValuePtr out);
void evalIndexingExpr(ExprPtr indexable,
                      ExprListPtr args,
                      EnvPtr env,
                      MultiEValuePtr out);
void evalAliasIndexing(GlobalAliasPtr x,
                       ExprListPtr args,
                       EnvPtr env,
                       MultiEValuePtr out);
void evalCallExpr(ExprPtr callable,
                  ExprListPtr args,
                  EnvPtr env,
                  MultiEValuePtr out);
void evalDispatch(ObjectPtr obj,
                  MultiEValuePtr args,
                  MultiPValuePtr pvArgs,
                  const vector<unsigned> &dispatchIndices,
                  MultiEValuePtr out);
void evalCallValue(EValuePtr callable,
                   MultiEValuePtr args,
                   MultiEValuePtr out);
void evalCallValue(EValuePtr callable,
                   MultiEValuePtr args,
                   MultiPValuePtr pvArgs,
                   MultiEValuePtr out);
void evalCallPointer(EValuePtr x,
                     MultiEValuePtr args,
                     MultiEValuePtr out);
void evalCallCode(InvokeEntryPtr entry,
                  MultiEValuePtr args,
                  MultiEValuePtr out);
void evalCallCompiledCode(InvokeEntryPtr entry,
                          MultiEValuePtr args,
                          MultiEValuePtr out);
void evalCallByName(InvokeEntryPtr entry,
                    ExprPtr callable,
                    ExprListPtr args,
                    EnvPtr env,
                    MultiEValuePtr out);

enum TerminationKind {
    TERMINATE_RETURN,
    TERMINATE_BREAK,
    TERMINATE_CONTINUE,
    TERMINATE_GOTO
};

struct Termination : public Object {
    TerminationKind terminationKind;
    LocationPtr location;
    Termination(TerminationKind terminationKind, LocationPtr location)
        : Object(DONT_CARE), terminationKind(terminationKind),
          location(location) {}
};
typedef Pointer<Termination> TerminationPtr;

struct TerminateReturn : Termination {
    TerminateReturn(LocationPtr location)
        : Termination(TERMINATE_RETURN, location) {}
};

struct TerminateBreak : Termination {
    TerminateBreak(LocationPtr location)
        : Termination(TERMINATE_BREAK, location) {}
};

struct TerminateContinue : Termination {
    TerminateContinue(LocationPtr location)
        : Termination(TERMINATE_CONTINUE, location) {}
};

struct TerminateGoto : Termination {
    IdentifierPtr targetLabel;
    TerminateGoto(IdentifierPtr targetLabel, LocationPtr location)
        : Termination(TERMINATE_GOTO, location),
          targetLabel(targetLabel) {}
};

struct LabelInfo {
    EnvPtr env;
    int stackMarker;
    int blockPosition;
    LabelInfo() {}
    LabelInfo(EnvPtr env, int stackMarker, int blockPosition)
        : env(env), stackMarker(stackMarker), blockPosition(blockPosition) {}
};

struct EReturn {
    bool byRef;
    TypePtr type;
    EValuePtr value;
    EReturn(bool byRef, TypePtr type, EValuePtr value)
        : byRef(byRef), type(type), value(value) {}
};

struct EvalContext : public Object {
    vector<EReturn> returns;
    EvalContext(const vector<EReturn> &returns)
        : Object(DONT_CARE), returns(returns) {}
};
typedef Pointer<EvalContext> EvalContextPtr;

TerminationPtr evalStatement(StatementPtr stmt,
                             EnvPtr env,
                             EvalContextPtr ctx);

void evalCollectLabels(const vector<StatementPtr> &statements,
                       unsigned startIndex,
                       EnvPtr env,
                       map<string, LabelInfo> &labels);
EnvPtr evalBinding(BindingPtr x, EnvPtr env);

void evalPrimOp(PrimOpPtr x, MultiEValuePtr args, MultiEValuePtr out);



//
// staticToType
//

bool staticToType(ObjectPtr x, TypePtr &out)
{
    if (x->objKind != TYPE)
        return false;
    out = (Type *)x.ptr();
    return true;
}

TypePtr staticToType(MultiStaticPtr x, unsigned index)
{
    TypePtr out;
    if (!staticToType(x->values[index], out))
        argumentError(index, "expecting a type");
    return out;
}



//
// evaluator wrappers
//

void evaluateReturnSpecs(const vector<ReturnSpecPtr> &returnSpecs,
                         ReturnSpecPtr varReturnSpec,
                         EnvPtr env,
                         vector<bool> &returnIsRef,
                         vector<TypePtr> &returnTypes)
{
    returnIsRef.clear();
    returnTypes.clear();
    for (unsigned i = 0; i < returnSpecs.size(); ++i) {
        ReturnSpecPtr x = returnSpecs[i];
        TypePtr t = evaluateType(x->type, env);
        bool isRef = unwrapByRef(t);
        returnIsRef.push_back(isRef);
        returnTypes.push_back(t);
    }
    if (varReturnSpec.ptr()) {
        LocationContext loc(varReturnSpec->type->location);
        MultiStaticPtr ms = evaluateExprStatic(varReturnSpec->type, env);
        for (unsigned i = 0; i < ms->size(); ++i) {
            TypePtr t = staticToType(ms, i);
            bool isRef = unwrapByRef(t);
            returnIsRef.push_back(isRef);
            returnTypes.push_back(t);
        }
    }
}

static void setSizeTEValue(EValuePtr v, size_t x) {
    switch (typeSize(cSizeTType)) {
    case 4 : *(size32_t *)v->addr = size32_t(x); break;
    case 8 : *(size64_t *)v->addr = size64_t(x); break;
    default : assert(false);
    }
}

/* not used yet
static void setPtrDiffTEValue(EValuePtr v, ptrdiff_t x) {
    switch (typeSize(cPtrDiffTType)) {
    case 4 : *(ptrdiff32_t *)v->addr = ptrdiff32_t(x); break;
    case 8 : *(ptrdiff64_t *)v->addr = ptrdiff64_t(x); break;
    default : assert(false);
    }
}
*/

static void setPtrEValue(EValuePtr v, void* x) {
    switch (typeSize(cPtrDiffTType)) {
    case 4 : *(size32_t *)v->addr = size32_t(size_t(x)); break;
    case 8 : *(size64_t *)v->addr = size64_t(x); break;
    default : assert(false);
    }
}

MultiStaticPtr evaluateExprStatic(ExprPtr expr, EnvPtr env)
{
    AnalysisCachingDisabler disabler;
    MultiPValuePtr mpv = safeAnalyzeExpr(expr, env);
    vector<ValueHolderPtr> valueHolders;
    MultiEValuePtr mev = new MultiEValue();
    bool allStatic = true;
    for (unsigned i = 0; i < mpv->size(); ++i) {
        TypePtr t = mpv->values[i]->type;
        if (!isStaticOrTupleOfStatics(t))
            allStatic = false;
        ValueHolderPtr vh = new ValueHolder(t);
        valueHolders.push_back(vh);
        EValuePtr ev = new EValue(t, vh->buf);
        mev->add(ev);
    }
    if (!allStatic) {
        int marker = evalMarkStack();
        evalExprInto(expr, env, mev);
        evalDestroyAndPopStack(marker);
    }
    MultiStaticPtr ms = new MultiStatic();
    for (unsigned i = 0; i < valueHolders.size(); ++i) {
        Type *t = valueHolders[i]->type.ptr();
        if (t->typeKind == STATIC_TYPE) {
            StaticType *st = (StaticType *)t;
            ms->add(st->obj);
        }
        else {
            ms->add(valueHolders[i].ptr());
        }
    }
    return ms;
}

ObjectPtr evaluateOneStatic(ExprPtr expr, EnvPtr env)
{
    MultiStaticPtr ms = evaluateExprStatic(expr, env);
    if (ms->size() != 1)
        arityError(expr, 1, ms->size());
    return ms->values[0];
}

MultiStaticPtr evaluateMultiStatic(ExprListPtr exprs, EnvPtr env)
{
    MultiStaticPtr out = new MultiStatic();
    for (unsigned i = 0; i < exprs->size(); ++i) {
        ExprPtr x = exprs->exprs[i];
        if (x->exprKind == UNPACK) {
            Unpack *y = (Unpack *)x.ptr();
            MultiStaticPtr z = evaluateExprStatic(y->expr, env);
            out->add(z);
        }
        else if (x->exprKind == PAREN) {
            MultiStaticPtr z = evaluateExprStatic(x, env);
            out->add(z);
        }
        else {
            ObjectPtr y = evaluateOneStatic(x, env);
            out->add(y);
        }
    }
    return out;
}

TypePtr evaluateType(ExprPtr expr, EnvPtr env)
{
    ObjectPtr v = evaluateOneStatic(expr, env);
    if (v->objKind != TYPE) {
        ostringstream sout;
        sout << "expecting a type but got ";
        printStaticName(sout, v);
        error(expr, sout.str());
    }
    return (Type *)v.ptr();
}

void evaluateMultiType(ExprListPtr exprs, EnvPtr env, vector<TypePtr> &out)
{
    MultiStaticPtr types = evaluateMultiStatic(exprs, env);
    for (unsigned i = 0; i < types->size(); ++i) {
        if (types->values[i]->objKind != TYPE) {
            ostringstream sout;
            sout << "expecting a type but got ";
            printStaticName(sout, types->values[i]);
            error(sout.str());
        }
        out.push_back((Type*)types->values[i].ptr());
    }
}

bool evaluateBool(ExprPtr expr, EnvPtr env)
{
    ObjectPtr v = evaluateOneStatic(expr, env);
    LocationContext loc(expr->location);
    if (v->objKind != VALUE_HOLDER)
        error(expr, "expecting a bool value");
    ValueHolderPtr vh = (ValueHolder *)v.ptr();
    if (vh->type != boolType)
        typeError(boolType, vh->type);
    return (*(char *)vh->buf) != 0;
}

void evaluateToplevelPredicate(const vector<PatternVar> &patternVars,
    ExprPtr predicate, EnvPtr env)
{
    for (unsigned i = 0; i < patternVars.size(); ++i) {
        if (lookupEnv(env, patternVars[i].name) == NULL)
            error(patternVars[i].name, "unbound pattern variable");
    }
    if (predicate != NULL) {
        if (!evaluateBool(predicate, env))
            error(predicate, "definition predicate failed");
    }
}

ValueHolderPtr intToValueHolder(int x)
{
    ValueHolderPtr v = new ValueHolder(cIntType);
    *(int *)v->buf = x;
    return v;
}

ValueHolderPtr sizeTToValueHolder(size_t x)
{
    ValueHolderPtr v = new ValueHolder(cSizeTType);
    switch (typeSize(cSizeTType)) {
    case 4 : *(size32_t *)v->buf = size32_t(x); break;
    case 8 : *(size64_t *)v->buf = size64_t(x); break;
    default : assert(false);
    }
    return v;
}

ValueHolderPtr ptrDiffTToValueHolder(ptrdiff_t x)
{
    ValueHolderPtr v = new ValueHolder(cPtrDiffTType);
    switch (typeSize(cPtrDiffTType)) {
    case 4 : *(ptrdiff32_t *)v->buf = ptrdiff32_t(x); break;
    case 8 : *(ptrdiff64_t *)v->buf = ptrdiff64_t(x); break;
    default : assert(false);
    }
    return v;
}

ValueHolderPtr boolToValueHolder(bool x)
{
    ValueHolderPtr v = new ValueHolder(boolType);
    *(char *)v->buf = x ? 1 : 0;
    return v;
}



//
// makeTupleValue
//

ObjectPtr makeTupleValue(const vector<ObjectPtr> &elements)
{
    bool allStatics = true;
    for (unsigned i = 0; i < elements.size(); ++i) {
        switch (elements[i]->objKind) {
        case TYPE :
        case PRIM_OP :
        case PROCEDURE :
        case RECORD :
        case VARIANT :
        case MODULE_HOLDER :
        case IDENTIFIER :
            break;
        default :
            allStatics = false;
            break;
        }
        if (!allStatics)
            break;
    }
    if (allStatics) {
        vector<TypePtr> elementTypes;
        for (unsigned i = 0; i < elements.size(); ++i)
            elementTypes.push_back(staticType(elements[i]));
        TypePtr t = tupleType(elementTypes);
        return new ValueHolder(t);
    }
    ExprListPtr elementExprs = new ExprList();
    for (unsigned i = 0; i < elements.size(); ++i)
        elementExprs->add(new ObjectExpr(elements[i]));
    ExprPtr tupleExpr = new Tuple(elementExprs);
    return evaluateOneStatic(tupleExpr, new Env());
}



//
// extractTupleElements
//

void extractTupleElements(EValuePtr ev,
                          vector<ObjectPtr> &elements)
{
    assert(ev->type->typeKind == TUPLE_TYPE);
    TupleType *tt = (TupleType *)ev->type.ptr();
    const llvm::StructLayout *layout = tupleTypeLayout(tt);
    for (unsigned i = 0; i < tt->elementTypes.size(); ++i) {
        char *addr = ev->addr + layout->getElementOffset(i);
        EValuePtr evElement = new EValue(tt->elementTypes[i], addr);
        elements.push_back(evalueToStatic(evElement));
    }
}



//
// evalueToStatic
//

ObjectPtr evalueToStatic(EValuePtr ev)
{
    if (ev->type->typeKind == STATIC_TYPE) {
        StaticType *st = (StaticType *)ev->type.ptr();
        return st->obj;
    }
    else {
        ValueHolderPtr vh = new ValueHolder(ev->type);
        EValuePtr dest = new EValue(vh->type, vh->buf);
        evalValueCopy(dest, ev);
        return vh.ptr();
    }
}



//
// evaluator
//

static vector<EValuePtr> stackEValues;



//
// utility procs
//

static EValuePtr staticEValue(ObjectPtr obj)
{
    TypePtr t = staticType(obj);
    return new EValue(t, NULL);
}

static EValuePtr derefValue(EValuePtr evPtr)
{
    assert(evPtr->type->typeKind == POINTER_TYPE);
    PointerType *pt = (PointerType *)evPtr->type.ptr();
    char *addr = *((char **)evPtr->addr);
    return new EValue(pt->pointeeType, addr);
}



//
// evaluate value ops
//

void evalValueInit(EValuePtr dest)
{
}

void evalValueDestroy(EValuePtr dest)
{
}

void evalValueCopy(EValuePtr dest, EValuePtr src)
{
    if (dest->type == src->type) {
        if (dest->type->typeKind != STATIC_TYPE)
            memcpy(dest->addr, src->addr, typeSize(dest->type));
        return;
    }
    evalCallValue(staticEValue(dest->type.ptr()),
                  new MultiEValue(src),
                  new MultiEValue(dest));
}

void evalValueMove(EValuePtr dest, EValuePtr src)
{
    if (dest->type == src->type) {
        if (dest->type->typeKind != STATIC_TYPE)
            memcpy(dest->addr, src->addr, typeSize(dest->type));
        return;
    }
    evalCallValue(staticEValue(prelude_move()),
                  new MultiEValue(src),
                  new MultiEValue(dest));
}

void evalValueAssign(EValuePtr dest, EValuePtr src)
{
    if (dest->type == src->type) {
        if (dest->type->typeKind != STATIC_TYPE)
            memcpy(dest->addr, src->addr, typeSize(dest->type));
        return;
    }
    MultiEValuePtr args = new MultiEValue(dest);
    args->add(src);
    evalCallValue(staticEValue(prelude_assign()),
                  args,
                  new MultiEValue());
}

void evalValueMoveAssign(EValuePtr dest, EValuePtr src)
{
    if (dest->type == src->type) {
        if (dest->type->typeKind != STATIC_TYPE)
            memcpy(dest->addr, src->addr, typeSize(dest->type));
        return;
    }
    MultiEValuePtr args = new MultiEValue(dest);
    args->add(src);
    MultiPValuePtr pvArgs = new MultiPValue(new PValue(dest->type, false));
    pvArgs->add(new PValue(src->type, true));
    evalCallValue(staticEValue(prelude_assign()),
                  args,
                  pvArgs,
                  new MultiEValue());
}

bool evalToBoolFlag(EValuePtr a)
{
    if (a->type != boolType)
        typeError(boolType, a->type);
    return *((char *)a->addr) != 0;
}



//
// evaluator temps
//

int evalMarkStack()
{
    return stackEValues.size();
}

void evalDestroyStack(int marker)
{
    int i = (int)stackEValues.size();
    assert(marker <= i);
    while (marker < i) {
        --i;
        evalValueDestroy(stackEValues[i]);
    }
}

void evalPopStack(int marker)
{
    assert(marker <= (int)stackEValues.size());
    while (marker < (int)stackEValues.size()) {
        free(stackEValues.back()->addr);
        stackEValues.pop_back();
    }
}

void evalDestroyAndPopStack(int marker)
{
    assert(marker <= (int)stackEValues.size());
    while (marker < (int)stackEValues.size()) {
        evalValueDestroy(stackEValues.back());
        free(stackEValues.back()->addr);
        stackEValues.pop_back();
    }
}

EValuePtr evalAllocValue(TypePtr t)
{
    char *buf = (char *)calloc(1, typeSize(t));
    EValuePtr ev = new EValue(t, buf);
    stackEValues.push_back(ev);
    return ev;
}



//
// evalMultiArgsAsRef, evalArgExprAsRef
//

MultiEValuePtr evalMultiArgsAsRef(ExprListPtr exprs, EnvPtr env)
{
    MultiEValuePtr out = new MultiEValue();
    for (unsigned i = 0; i < exprs->size(); ++i) {
        ExprPtr x = exprs->exprs[i];
        if (x->exprKind == UNPACK) {
            Unpack *y = (Unpack *)x.ptr();
            MultiEValuePtr mev = evalArgExprAsRef(y->expr, env);
            out->add(mev);
        }
        else if (x->exprKind == PAREN) {
            MultiEValuePtr mev = evalArgExprAsRef(x, env);
            out->add(mev);
        }
        else {
            MultiEValuePtr mev = evalArgExprAsRef(x, env);
            out->add(mev);
        }
    }
    return out;
}

MultiEValuePtr evalArgExprAsRef(ExprPtr x, EnvPtr env)
{
    if (x->exprKind == DISPATCH_EXPR) {
        DispatchExpr *y = (DispatchExpr *)x.ptr();
        return evalExprAsRef(y->expr, env);
    }
    return evalExprAsRef(x, env);
}



//
// evalForwardOneAsRef, evalForwardMultiAsRef, evalForwardExprAsRef
//

EValuePtr evalForwardOneAsRef(ExprPtr expr, EnvPtr env)
{
    MultiEValuePtr mev = evalForwardExprAsRef(expr, env);
    LocationContext loc(expr->location);
    ensureArity(mev, 1);
    return mev->values[0];
}

MultiEValuePtr evalForwardMultiAsRef(ExprListPtr exprs, EnvPtr env)
{
    MultiEValuePtr out = new MultiEValue();
    for (unsigned i = 0; i < exprs->size(); ++i) {
        ExprPtr x = exprs->exprs[i];
        if (x->exprKind == UNPACK) {
            Unpack *y = (Unpack *)x.ptr();
            MultiEValuePtr mev = evalForwardExprAsRef(y->expr, env);
            out->add(mev);
        }
        else {
            MultiEValuePtr mev = evalForwardExprAsRef(x, env);
            out->add(mev);
        }
    }
    return out;
}

MultiEValuePtr evalForwardExprAsRef(ExprPtr expr, EnvPtr env)
{
    MultiPValuePtr mpv = safeAnalyzeExpr(expr, env);
    MultiEValuePtr mev = new MultiEValue();
    for (unsigned i = 0; i < mpv->size(); ++i) {
        PValuePtr pv = mpv->values[i];
        if (pv->isTemp) {
            EValuePtr ev = evalAllocValue(pv->type);
            mev->add(ev);
        }
        else {
            EValuePtr evPtr = evalAllocValue(pointerType(pv->type));
            mev->add(evPtr);
        }
    }
    evalExpr(expr, env, mev);
    MultiEValuePtr out = new MultiEValue();
    for (unsigned i = 0; i < mpv->size(); ++i) {
        if (mpv->values[i]->isTemp) {
            EValuePtr ev = mev->values[i];
            EValuePtr ev2 = new EValue(ev->type, ev->addr);
            ev2->forwardedRValue = true;
            out->add(ev2);
        }
        else {
            out->add(derefValue(mev->values[i]));
        }
    }
    return out;
}



//
// evalOneAsRef, evalMultiAsRef, evalExprAsRef
//

EValuePtr evalOneAsRef(ExprPtr expr, EnvPtr env)
{
    MultiEValuePtr mev = evalExprAsRef(expr, env);
    LocationContext loc(expr->location);
    ensureArity(mev, 1);
    return mev->values[0];
}

MultiEValuePtr evalMultiAsRef(ExprListPtr exprs, EnvPtr env)
{
    MultiEValuePtr out = new MultiEValue();
    for (unsigned i = 0; i < exprs->size(); ++i) {
        ExprPtr x = exprs->exprs[i];
        if (x->exprKind == UNPACK) {
            Unpack *y = (Unpack *)x.ptr();
            MultiEValuePtr mev = evalExprAsRef(y->expr, env);
            out->add(mev);
        }
        else if (x->exprKind == PAREN) {
            MultiEValuePtr mev = evalExprAsRef(x, env);
            out->add(mev);
        }
        else {
            EValuePtr ev = evalOneAsRef(x, env);
            out->add(ev);
        }
    }
    return out;
}

MultiEValuePtr evalExprAsRef(ExprPtr expr, EnvPtr env)
{
    MultiPValuePtr mpv = safeAnalyzeExpr(expr, env);
    MultiEValuePtr mev = new MultiEValue();
    for (unsigned i = 0; i < mpv->size(); ++i) {
        PValuePtr pv = mpv->values[i];
        if (pv->isTemp) {
            EValuePtr ev = evalAllocValue(pv->type);
            mev->add(ev);
        }
        else {
            EValuePtr evPtr = evalAllocValue(pointerType(pv->type));
            mev->add(evPtr);
        }
    }
    evalExpr(expr, env, mev);
    MultiEValuePtr out = new MultiEValue();
    for (unsigned i = 0; i < mpv->size(); ++i) {
        if (mpv->values[i]->isTemp)
            out->add(mev->values[i]);
        else
            out->add(derefValue(mev->values[i]));
    }
    return out;
}



//
// evalOneInto, evalMultiInto, evalExprInto
//

void evalOneInto(ExprPtr expr, EnvPtr env, EValuePtr out)
{
    PValuePtr pv = safeAnalyzeOne(expr, env);
    if (pv->isTemp) {
        evalOne(expr, env, out);
    }
    else {
        EValuePtr evPtr = evalAllocValue(pointerType(pv->type));
        evalOne(expr, env, evPtr);
        evalValueCopy(out, derefValue(evPtr));
    }
}


void evalMultiInto(ExprListPtr exprs, EnvPtr env, MultiEValuePtr out, unsigned wantCount)
{
    unsigned j = 0;
    ExprPtr unpackExpr = implicitUnpackExpr(wantCount, exprs);
    if (unpackExpr != NULL) {
        MultiPValuePtr mpv = safeAnalyzeExpr(unpackExpr, env);
        assert(j + mpv->size() <= out->size());
        MultiEValuePtr out2 = new MultiEValue();
        for (unsigned k = 0; k < mpv->size(); ++k)
            out2->add(out->values[j + k]);
        evalExprInto(unpackExpr, env, out2);
        j += mpv->size();
    } else for (unsigned i = 0; i < exprs->size(); ++i) {
        ExprPtr x = exprs->exprs[i];
        if (x->exprKind == UNPACK) {
            Unpack *y = (Unpack *)x.ptr();
            MultiPValuePtr mpv = safeAnalyzeExpr(y->expr, env);
            assert(j + mpv->size() <= out->size());
            MultiEValuePtr out2 = new MultiEValue();
            for (unsigned k = 0; k < mpv->size(); ++k)
                out2->add(out->values[j + k]);
            evalExprInto(y->expr, env, out2);
            j += mpv->size();
        }
        else if (x->exprKind == PAREN) {
            MultiPValuePtr mpv = safeAnalyzeExpr(x, env);
            assert(j + mpv->size() <= out->size());
            MultiEValuePtr out2 = new MultiEValue();
            for (unsigned k = 0; k < mpv->size(); ++k)
                out2->add(out->values[j + k]);
            evalExprInto(x, env, out2);
            j += mpv->size();
        }
        else {
            MultiPValuePtr mpv = safeAnalyzeExpr(x, env);
            if (mpv->size() != 1)
                arityError(x, 1, mpv->size());
            assert(j < out->size());
            evalOneInto(x, env, out->values[j]);
            ++j;
        }
    }
    assert(j == out->size());
}

void evalExprInto(ExprPtr expr, EnvPtr env, MultiEValuePtr out)
{
    MultiPValuePtr mpv = safeAnalyzeExpr(expr, env);
    assert(out->size() == mpv->size());
    MultiEValuePtr mev = new MultiEValue();
    for (unsigned i = 0; i < mpv->size(); ++i) {
        PValuePtr pv = mpv->values[i];
        if (pv->isTemp) {
            mev->add(out->values[i]);
        }
        else {
            EValuePtr evPtr = evalAllocValue(pointerType(pv->type));
            mev->add(evPtr);
        }
    }
    evalExpr(expr, env, mev);
    for (unsigned i = 0; i < mpv->size(); ++i) {
        if (!mpv->values[i]->isTemp) {
            evalValueCopy(out->values[i], derefValue(mev->values[i]));
        }
    }
}



//
// evalMulti
//

void evalMulti(ExprListPtr exprs, EnvPtr env, MultiEValuePtr out, unsigned wantCount)
{
    unsigned j = 0;
    ExprPtr unpackExpr = implicitUnpackExpr(wantCount, exprs);
    if (unpackExpr != NULL) {
        MultiPValuePtr mpv = safeAnalyzeExpr(unpackExpr, env);
        assert(j + mpv->size() <= out->size());
        MultiEValuePtr out2 = new MultiEValue();
        for (unsigned k = 0; k < mpv->size(); ++k)
            out2->add(out->values[j + k]);
        evalExpr(unpackExpr, env, out2);
        j += mpv->size();
    } else for (unsigned i = 0; i < exprs->size(); ++i) {
        ExprPtr x = exprs->exprs[i];
        if (x->exprKind == UNPACK) {
            Unpack *y = (Unpack *)x.ptr();
            MultiPValuePtr mpv = safeAnalyzeExpr(y->expr, env);
            assert(j + mpv->size() <= out->size());
            MultiEValuePtr out2 = new MultiEValue();
            for (unsigned k = 0; k < mpv->size(); ++k)
                out2->add(out->values[j + k]);
            evalExpr(y->expr, env, out2);
            j += mpv->size();
        }
        else if (x->exprKind == PAREN) {
            MultiPValuePtr mpv = safeAnalyzeExpr(x, env);
            assert(j + mpv->size() <= out->size());
            MultiEValuePtr out2 = new MultiEValue();
            for (unsigned k = 0; k < mpv->size(); ++k)
                out2->add(out->values[j + k]);
            evalExpr(x, env, out2);
            j += mpv->size();
        }
        else {
            MultiPValuePtr mpv = safeAnalyzeExpr(x, env);
            if (mpv->size() != 1)
                arityError(x, 1, mpv->size());
            assert(j < out->size());
            evalOne(x, env, out->values[j]);
            ++j;
        }
    }
    assert(j == out->size());
}



//
// evalOne
//

void evalOne(ExprPtr expr, EnvPtr env, EValuePtr out)
{
    evalExpr(expr, env, new MultiEValue(out));
}



//
// evalExpr
//

void evalExpr(ExprPtr expr, EnvPtr env, MultiEValuePtr out)
{
    LocationContext loc(expr->location);

    switch (expr->exprKind) {

    case BOOL_LITERAL : {
        BoolLiteral *x = (BoolLiteral *)expr.ptr();
        ValueHolderPtr y = boolToValueHolder(x->value);
        evalValueHolder(y, out);
        break;
    }

    case INT_LITERAL : {
        IntLiteral *x = (IntLiteral *)expr.ptr();
        ValueHolderPtr y = parseIntLiteral(safeLookupModule(env), x);
        evalValueHolder(y, out);
        break;
    }

    case FLOAT_LITERAL : {
        FloatLiteral *x = (FloatLiteral *)expr.ptr();
        ValueHolderPtr y = parseFloatLiteral(safeLookupModule(env), x);
        evalValueHolder(y, out);
        break;
    }

    case CHAR_LITERAL : {
        CharLiteral *x = (CharLiteral *)expr.ptr();
        if (!x->desugared)
            x->desugared = desugarCharLiteral(x->value);
        evalExpr(x->desugared, env, out);
        break;
    }

    case STRING_LITERAL : {
        StringLiteral *x = (StringLiteral *)expr.ptr();
        char *str = const_cast<char*>(x->value.c_str());
        char *strEnd = str + x->value.size();
        TypePtr ptrInt8Type = pointerType(int8Type);
        EValuePtr evFirst = evalAllocValue(ptrInt8Type);
        *((char **)evFirst->addr) = str;
        EValuePtr evLast = evalAllocValue(ptrInt8Type);
        *((char **)evLast->addr) = strEnd;
        MultiEValuePtr args = new MultiEValue();
        args->add(evFirst);
        args->add(evLast);
        evalCallValue(staticEValue(prelude_StringConstant()), args, out);
        break;
    }

    case IDENTIFIER_LITERAL :
        break;

    case FILE_EXPR :
    case ARG_EXPR :
        break;

    case LINE_EXPR : {
        LocationPtr location = safeLookupCallByNameLocation(env);
        int line, column, tabColumn;
        getLineCol(location, line, column, tabColumn);

        ValueHolderPtr vh = sizeTToValueHolder(line+1);
        evalStaticObject(vh.ptr(), out);
        break;
    }
    case COLUMN_EXPR : {
        LocationPtr location = safeLookupCallByNameLocation(env);
        int line, column, tabColumn;
        getLineCol(location, line, column, tabColumn);

        ValueHolderPtr vh = sizeTToValueHolder(column);
        evalStaticObject(vh.ptr(), out);
        break;
    }

    case NAME_REF : {
        NameRef *x = (NameRef *)expr.ptr();
        ObjectPtr y = safeLookupEnv(env, x->name);
        if (y->objKind == EXPRESSION) {
            ExprPtr z = (Expr *)y.ptr();
            evalExpr(z, env, out);
        }
        else if (y->objKind == EXPR_LIST) {
            ExprListPtr z = (ExprList *)y.ptr();
            evalMulti(z, env, out, 0);
        }
        else {
            evalStaticObject(y, out);
        }
        break;
    }

    case TUPLE : {
        Tuple *x = (Tuple *)expr.ptr();
        evalCallExpr(prelude_expr_tupleLiteral(),
                     x->args,
                     env,
                     out);
        break;
    }

    case PAREN : {
        Paren *x = (Paren *)expr.ptr();
        evalMulti(x->args, env, out, 0);
        break;
    }

    case INDEXING : {
        Indexing *x = (Indexing *)expr.ptr();
        evalIndexingExpr(x->expr, x->args, env, out);
        break;
    }

    case CALL : {
        Call *x = (Call *)expr.ptr();
        evalCallExpr(x->expr, x->allArgs(), env, out);
        break;
    }

    case FIELD_REF : {
        FieldRef *x = (FieldRef *)expr.ptr();
        PValuePtr pv = safeAnalyzeOne(x->expr, env);
        if (pv->type->typeKind == STATIC_TYPE) {
            StaticType *st = (StaticType *)pv->type.ptr();
            if (st->obj->objKind == MODULE_HOLDER) {
                ModuleHolder *mh = (ModuleHolder *)st->obj.ptr();
                ObjectPtr obj = safeLookupModuleHolder(mh, x->name);
                evalStaticObject(obj, out);
                break;
            }
        }
        if (!x->desugared)
            x->desugared = desugarFieldRef(x);
        evalExpr(x->desugared, env, out);
        break;
    }

    case STATIC_INDEXING : {
        StaticIndexing *x = (StaticIndexing *)expr.ptr();
        if (!x->desugared)
            x->desugared = desugarStaticIndexing(x);
        evalExpr(x->desugared, env, out);
        break;
    }

    case UNARY_OP : {
        UnaryOp *x = (UnaryOp *)expr.ptr();
        if (x->op == ADDRESS_OF) {
            PValuePtr pv = safeAnalyzeOne(x->expr, env);
            if (pv->isTemp)
                error("can't take address of a temporary");
        }
        if (!x->desugared)
            x->desugared = desugarUnaryOp(x);
        evalExpr(x->desugared, env, out);
        break;
    }

    case BINARY_OP : {
        BinaryOp *x = (BinaryOp *)expr.ptr();
        if (!x->desugared)
            x->desugared = desugarBinaryOp(x);
        evalExpr(x->desugared, env, out);
        break;
    }

    case EVAL_EXPR : {
        EvalExpr *eval = (EvalExpr *)expr.ptr();
        // XXX compilation context
        ExprListPtr evaled = desugarEvalExpr(eval, env);
        evalMulti(evaled, env, out, 0);
        break;
    }

    case AND : {
        And *x = (And *)expr.ptr();
        EValuePtr ev1 = evalOneAsRef(x->expr1, env);
        bool result = false;
        if (evalToBoolFlag(ev1)) {
            EValuePtr ev2 = evalOneAsRef(x->expr2, env);
            result = evalToBoolFlag(ev2);
        }
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type == boolType);
        *((char *)out0->addr) = result ? 1 : 0;
        break;
    }

    case OR : {
        Or *x = (Or *)expr.ptr();
        EValuePtr ev1 = evalOneAsRef(x->expr1, env);
        bool result = true;
        if (!evalToBoolFlag(ev1)) {
            EValuePtr ev2 = evalOneAsRef(x->expr2, env);
            result = evalToBoolFlag(ev2);
        }
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type == boolType);
        *((char *)out0->addr) = result ? 1 : 0;
        break;
    }

    case IF_EXPR : {
        IfExpr *x = (IfExpr *)expr.ptr();
        if (!x->desugared)
            x->desugared = desugarIfExpr(x);
        evalExpr(x->desugared, env, out);
        break;
    }

    case LAMBDA : {
        Lambda *x = (Lambda *)expr.ptr();
        if (!x->initialized)
            initializeLambda(x, env);
        evalExpr(x->converted, env, out);
        break;
    }

    case UNPACK : {
        error("incorrect usage of unpack operator");
        break;
    }

    case STATIC_EXPR : {
        StaticExpr *x = (StaticExpr *)expr.ptr();
        if (!x->desugared)
            x->desugared = desugarStaticExpr(x);
        evalExpr(x->desugared, env, out);
        break;
    }

    case DISPATCH_EXPR : {
        error("incorrect usage of dispatch operator");
        break;
    }

    case FOREIGN_EXPR : {
        ForeignExpr *x = (ForeignExpr *)expr.ptr();
        evalExpr(x->expr, x->getEnv(), out);
        break;
    }

    case OBJECT_EXPR : {
        ObjectExpr *x = (ObjectExpr *)expr.ptr();
        evalStaticObject(x->obj, out);
        break;
    }

    default :
        assert(false);

    }
}



//
// evalStaticObject
//

void evalStaticObject(ObjectPtr x, MultiEValuePtr out)
{
    switch (x->objKind) {

    case ENUM_MEMBER : {
        EnumMember *y = (EnumMember *)x.ptr();
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type == y->type);
        assert(out0->type->typeKind == ENUM_TYPE);
        initializeEnumType((EnumType*)out0->type.ptr());
        *((int *)out0->addr) = y->index;
        break;
    }

    case GLOBAL_VARIABLE : {
        GlobalVariable *y = (GlobalVariable *)x.ptr();
        if (y->hasParams()) {
            assert(out->size() == 1);
            assert(out->values[0]->type == staticType(x));
        }
        else {
            GVarInstancePtr z = defaultGVarInstance(y);
            if (!z->llGlobal)
                codegenGVarInstance(z);
            void *ptr = llvmEngine->getPointerToGlobal(z->llGlobal);
            assert(out->size() == 1);
            EValuePtr out0 = out->values[0];
            assert(out0->type == pointerType(z->type));
            *((void **)out0->addr) = ptr;
        }
        break;
    }

    case EXTERNAL_VARIABLE : {
        error("compile-time access to C global variables not supported");
        break;
    }

    case EXTERNAL_PROCEDURE : {
        error("compile-time access to C functions not supported");
        break;
    }

    case GLOBAL_ALIAS : {
        GlobalAlias *y = (GlobalAlias *)x.ptr();
        if (y->hasParams()) {
            assert(out->size() == 1);
            assert(out->values[0]->type == staticType(x));
        }
        else {
            evalExpr(y->expr, y->env, out);
        }
        break;
    }

    case VALUE_HOLDER : {
        ValueHolder *y = (ValueHolder *)x.ptr();
        evalValueHolder(y, out);
        break;
    }

    case MULTI_STATIC : {
        MultiStatic *y = (MultiStatic *)x.ptr();
        assert(y->size() == out->size());
        for (unsigned i = 0; i < y->size(); ++i)
            evalStaticObject(y->values[i], new MultiEValue(out->values[i]));
        break;
    }

    case RECORD : {
        Record *y = (Record *)x.ptr();
        ObjectPtr z;
        if (y->params.empty() && !y->varParam)
            z = recordType(y, vector<ObjectPtr>()).ptr();
        else
            z = y;
        assert(out->size() == 1);
        assert(out->values[0]->type == staticType(z));
        break;
    }

    case VARIANT : {
        Variant *y = (Variant *)x.ptr();
        ObjectPtr z;
        if (y->params.empty() && !y->varParam)
            z = variantType(y, vector<ObjectPtr>()).ptr();
        else
            z = y;
        assert(out->size() == 1);
        assert(out->values[0]->type == staticType(z));
        break;
    }

    case TYPE :
    case PRIM_OP :
    case PROCEDURE :
    case MODULE_HOLDER :
    case IDENTIFIER : {
        assert(out->size() == 1);
        assert(out->values[0]->type == staticType(x));
        break;
    }

    case EVALUE : {
        EValue *y = (EValue *)x.ptr();
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        if (y->forwardedRValue) {
            assert(out0->type == y->type);
            evalValueMove(out0, y);
        }
        else {
            assert(out0->type == pointerType(y->type));
            *((void **)out0->addr) = (void *)y->addr;
        }
        break;
    }

    case MULTI_EVALUE : {
        MultiEValue *y = (MultiEValue *)x.ptr();
        assert(out->size() == y->size());
        for (unsigned i = 0; i < y->size(); ++i) {
            EValuePtr ev = y->values[i];
            EValuePtr outi = out->values[i];
            if (ev->forwardedRValue) {
                assert(outi->type == ev->type);
                evalValueMove(outi, ev);
            }
            else {
                assert(outi->type == pointerType(ev->type));
                *((void **)outi->addr) = (void *)ev->addr;
            }
        }
        break;
    }

    case CVALUE : {
        // allow values of static type
        CValue *y = (CValue *)x.ptr();
        if (y->type->typeKind != STATIC_TYPE)
            invalidStaticObjectError(x);
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        if (y->forwardedRValue)
            assert(out0->type == y->type);
        else
            assert(out0->type == pointerType(y->type));
        break;
    }

    case MULTI_CVALUE : {
        // allow values of static type
        MultiCValue *y = (MultiCValue *)x.ptr();
        assert(out->size() == y->size());
        for (unsigned i = 0; i < y->size(); ++i) {
            CValuePtr cv = y->values[i];
            if (cv->type->typeKind != STATIC_TYPE)
                argumentInvalidStaticObjectError(i, cv.ptr());
            EValuePtr outi = out->values[i];
            if (cv->forwardedRValue)
                assert(outi->type == cv->type);
            else
                assert(outi->type == pointerType(cv->type));
        }
        break;
    }

    case PVALUE : {
        // allow values of static type
        PValue *y = (PValue *)x.ptr();
        if (y->type->typeKind != STATIC_TYPE)
            invalidStaticObjectError(y);
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        if (y->isTemp)
            assert(out0->type == y->type);
        else
            assert(out0->type == pointerType(y->type));
        break;
    }

    case MULTI_PVALUE : {
        // allow values of static type
        MultiPValue *y = (MultiPValue *)x.ptr();
        assert(out->size() == y->size());
        for (unsigned i = 0; i < y->size(); ++i) {
            PValuePtr pv = y->values[i];
            if (pv->type->typeKind != STATIC_TYPE)
                argumentInvalidStaticObjectError(i, pv.ptr());
            EValuePtr outi = out->values[i];
            if (pv->isTemp)
                assert(outi->type == pv->type);
            else
                assert(outi->type == pointerType(pv->type));
        }
        break;
    }

    case PATTERN :
        error("pattern variable cannot be used as value");
        break;

    default :
        invalidStaticObjectError(x);
        break;
    }
}



//
// evalValueHolder
//

void evalValueHolder(ValueHolderPtr x, MultiEValuePtr out)
{
    assert(out->size() == 1);
    EValuePtr out0 = out->values[0];
    assert(out0->type == x->type);

    switch (x->type->typeKind) {

    case BOOL_TYPE :
    case INTEGER_TYPE :
    case FLOAT_TYPE :
    case COMPLEX_TYPE :
        memcpy(out0->addr, x->buf, typeSize(x->type));
        break;

    case STATIC_TYPE :
        break;

    default :
        evalValueCopy(out0, new EValue(x->type, x->buf));
        break;

    }
}



//
// evalIndexingExpr
//

void evalIndexingExpr(ExprPtr indexable,
                      ExprListPtr args,
                      EnvPtr env,
                      MultiEValuePtr out)
{
    MultiPValuePtr mpv = safeAnalyzeIndexingExpr(indexable, args, env);
    assert(mpv->size() == out->size());
    bool allTempStatics = true;
    for (unsigned i = 0; i < mpv->size(); ++i) {
        PValuePtr pv = mpv->values[i];
        if (pv->isTemp)
            assert(out->values[i]->type == pv->type);
        else
            assert(out->values[i]->type == pointerType(pv->type));
        if ((pv->type->typeKind != STATIC_TYPE) || !pv->isTemp) {
            allTempStatics = false;
        }
    }
    if (allTempStatics) {
        // takes care of type constructors
        return;
    }
    PValuePtr pv = safeAnalyzeOne(indexable, env);
    if (pv->type->typeKind == STATIC_TYPE) {
        StaticType *st = (StaticType *)pv->type.ptr();
        ObjectPtr obj = st->obj;
        if (obj->objKind == GLOBAL_ALIAS) {
            GlobalAlias *x = (GlobalAlias *)obj.ptr();
            evalAliasIndexing(x, args, env, out);
            return;
        }
        if (obj->objKind == GLOBAL_VARIABLE) {
            GlobalVariable *x = (GlobalVariable *)obj.ptr();
            GVarInstancePtr y = analyzeGVarIndexing(x, args, env);
            if (!y->llGlobal)
                codegenGVarInstance(y);
            void *ptr = llvmEngine->getPointerToGlobal(y->llGlobal);
            assert(out->size() == 1);
            EValuePtr out0 = out->values[0];
            assert(out0->type == pointerType(y->type));
            *((void **)out0->addr) = ptr;
            return;
        }
    }
    ExprListPtr args2 = new ExprList(indexable);
    args2->add(args);
    evalCallExpr(prelude_expr_index(), args2, env, out);
}



//
// evalAliasIndexing
//

void evalAliasIndexing(GlobalAliasPtr x,
                       ExprListPtr args,
                       EnvPtr env,
                       MultiEValuePtr out)
{
    assert(x->hasParams());
    MultiStaticPtr params = evaluateMultiStatic(args, env);
    if (x->varParam.ptr()) {
        if (params->size() < x->params.size())
            arityError2(x->params.size(), params->size());
    }
    else {
        ensureArity(params, x->params.size());
    }
    EnvPtr bodyEnv = new Env(x->env);
    for (unsigned i = 0; i < x->params.size(); ++i) {
        addLocal(bodyEnv, x->params[i], params->values[i]);
    }
    if (x->varParam.ptr()) {
        MultiStaticPtr varParams = new MultiStatic();
        for (unsigned i = x->params.size(); i < params->size(); ++i)
            varParams->add(params->values[i]);
        addLocal(bodyEnv, x->varParam, varParams.ptr());
    }
    evalExpr(x->expr, bodyEnv, out);
}



//
// evalCallExpr
//

static bool isMemoizable(ObjectPtr callable) {
    // UGLY HACK: memoize if procedure name ends with '?'
    if (callable->objKind != PROCEDURE)
        return false;
    Procedure *x = (Procedure *)callable.ptr();
    const string &s = x->name->str;
    return s[s.size()-1] == '?';
}

void evalCallExpr(ExprPtr callable,
                  ExprListPtr args,
                  EnvPtr env,
                  MultiEValuePtr out)
{
    PValuePtr pv = safeAnalyzeOne(callable, env);

    switch (pv->type->typeKind) {
    case CODE_POINTER_TYPE :
        EValuePtr ev = evalOneAsRef(callable, env);
        MultiEValuePtr mev = evalMultiAsRef(args, env);
        evalCallPointer(ev, mev, out);
        return;
    }

    if (pv->type->typeKind != STATIC_TYPE) {
        ExprListPtr args2 = new ExprList(callable);
        args2->add(args);
        evalCallExpr(prelude_expr_call(), args2, env, out);
        return;
    }

    StaticType *st = (StaticType *)pv->type.ptr();
    ObjectPtr obj = st->obj;

    switch (obj->objKind) {

    case TYPE :
    case RECORD :
    case VARIANT :
    case PROCEDURE :
    case PRIM_OP : {
        if ((obj->objKind == PRIM_OP) && !isOverloadablePrimOp(obj)) {
            PrimOpPtr x = (PrimOp *)obj.ptr();
            MultiEValuePtr mev = evalMultiAsRef(args, env);
            evalPrimOp(x, mev, out);
            break;
        }
        vector<unsigned> dispatchIndices;
        MultiPValuePtr mpv = safeAnalyzeMultiArgs(args, env, dispatchIndices);
        if (dispatchIndices.size() > 0) {
            MultiEValuePtr mev = evalMultiArgsAsRef(args, env);
            evalDispatch(obj, mev, mpv, dispatchIndices, out);
            break;
        }
        vector<TypePtr> argsKey;
        vector<ValueTempness> argsTempness;
        computeArgsKey(mpv, argsKey, argsTempness);
        CompileContextPusher pusher(obj, argsKey);
        InvokeEntryPtr entry = safeAnalyzeCallable(obj, argsKey, argsTempness);
        if (entry->callByName) {
            evalCallByName(entry, callable, args, env, out);
        }
        else {
            assert(entry->analyzed);
            MultiEValuePtr mev = evalMultiAsRef(args, env);
            if (isMemoizable(obj)) {
                assert(obj->objKind == PROCEDURE);
                Procedure *x = (Procedure *)obj.ptr();
                vector<ObjectPtr> args2;
                for (unsigned i = 0; i < mev->size(); ++i)
                    args2.push_back(evalueToStatic(mev->values[i]));
                if (!x->evaluatorCache)
                    x->evaluatorCache = new ObjectTable();
                ObjectPtr result = x->evaluatorCache->lookup(args2);
                if (!result) {
                    evalCallCode(entry, mev, out);
                    MultiStaticPtr ms = new MultiStatic();
                    for (unsigned i = 0; i < out->size(); ++i)
                        ms->add(evalueToStatic(out->values[i]));
                    x->evaluatorCache->lookup(args2) = ms.ptr();
                }
                else {
                    evalStaticObject(result, out);
                }
            }
            else {
                evalCallCode(entry, mev, out);
            }
        }
        break;
    }

    default :
        error("invalid call expression");
        break;

    }
}



//
// evalDispatch
//

int evalVariantTag(EValuePtr ev)
{
    int tag = -1;
    EValuePtr etag = new EValue(cIntType, (char *)&tag);
    evalCallValue(staticEValue(prelude_variantTag()),
                  new MultiEValue(ev),
                  new MultiEValue(etag));
    return tag;
}

EValuePtr evalVariantIndex(EValuePtr ev, int tag)
{
    assert(ev->type->typeKind == VARIANT_TYPE);
    VariantType *vt = (VariantType *)ev->type.ptr();
    const vector<TypePtr> &memberTypes = variantMemberTypes(vt);
    assert((tag >= 0) && (unsigned(tag) < memberTypes.size()));

    MultiEValuePtr args = new MultiEValue();
    args->add(ev);
    ValueHolderPtr vh = intToValueHolder(tag);
    args->add(staticEValue(vh.ptr()));

    EValuePtr evPtr = evalAllocValue(pointerType(memberTypes[tag]));
    MultiEValuePtr out = new MultiEValue(evPtr);

    evalCallValue(staticEValue(prelude_unsafeVariantIndex()), args, out);
    return new EValue(memberTypes[tag], *((char **)ev->addr));
}

void evalDispatch(ObjectPtr obj,
                  MultiEValuePtr args,
                  MultiPValuePtr pvArgs,
                  const vector<unsigned> &dispatchIndices,
                  MultiEValuePtr out)
{
    if (dispatchIndices.empty()) {
        evalCallValue(staticEValue(obj), args, pvArgs, out);
        return;
    }

    vector<TypePtr> argsKey;
    vector<ValueTempness> argsTempness;
    computeArgsKey(pvArgs, argsKey, argsTempness);
    CompileContextPusher pusher(obj, argsKey);

    unsigned index = dispatchIndices[0];
    vector<unsigned> dispatchIndices2(dispatchIndices.begin() + 1,
                                      dispatchIndices.end());

    MultiEValuePtr prefix = new MultiEValue();
    MultiPValuePtr pvPrefix = new MultiPValue();
    for (unsigned i = 0; i < index; ++i) {
        prefix->add(args->values[i]);
        pvPrefix->add(pvArgs->values[i]);
    }
    MultiEValuePtr suffix = new MultiEValue();
    MultiPValuePtr pvSuffix = new MultiPValue();
    for (unsigned i = index+1; i < args->size(); ++i) {
        suffix->add(args->values[i]);
        pvSuffix->add(pvArgs->values[i]);
    }
    EValuePtr evDispatch = args->values[index];
    PValuePtr pvDispatch = pvArgs->values[index];
    if (pvDispatch->type->typeKind != VARIANT_TYPE) {
        argumentTypeError(index,
                          "variant for dispatch operator",
                          pvDispatch->type);
    }
    VariantTypePtr t = (VariantType *)pvDispatch->type.ptr();
    const vector<TypePtr> &memberTypes = variantMemberTypes(t);
    if (memberTypes.empty())
        argumentError(index, "variant has no member types");

    int tag = evalVariantTag(evDispatch);
    if ((tag < 0) || (tag >= (int)memberTypes.size()))
        argumentError(index, "invalid variant value");

    MultiEValuePtr args2 = new MultiEValue();
    args2->add(prefix);
    args2->add(evalVariantIndex(evDispatch, tag));
    args2->add(suffix);
    MultiPValuePtr pvArgs2 = new MultiPValue();
    pvArgs2->add(pvPrefix);
    pvArgs2->add(new PValue(memberTypes[tag], pvDispatch->isTemp));
    pvArgs2->add(pvSuffix);

    evalDispatch(obj, args2, pvArgs2, dispatchIndices2, out);
}



//
// evalCallValue
//

void evalCallValue(EValuePtr callable,
                   MultiEValuePtr args,
                   MultiEValuePtr out)
{
    MultiPValuePtr pvArgs = new MultiPValue();
    for (unsigned i = 0; i < args->size(); ++i)
        pvArgs->add(new PValue(args->values[i]->type, false));
    evalCallValue(callable, args, pvArgs, out);
}

void evalCallValue(EValuePtr callable,
                   MultiEValuePtr args,
                   MultiPValuePtr pvArgs,
                   MultiEValuePtr out)
{
    switch (callable->type->typeKind) {
    case CODE_POINTER_TYPE :
        evalCallPointer(callable, args, out);
        return;
    }

    if (callable->type->typeKind != STATIC_TYPE) {
        MultiEValuePtr args2 = new MultiEValue(callable);
        args2->add(args);
        MultiPValuePtr pvArgs2 =
            new MultiPValue(new PValue(callable->type, false));
        pvArgs2->add(pvArgs);
        evalCallValue(staticEValue(prelude_call()), args2, pvArgs2, out);
        return;
    }

    StaticType *st = (StaticType *)callable->type.ptr();
    ObjectPtr obj = st->obj;

    switch (obj->objKind) {

    case TYPE :
    case RECORD :
    case VARIANT :
    case PROCEDURE :
    case PRIM_OP : {
        if ((obj->objKind == PRIM_OP) && !isOverloadablePrimOp(obj)) {
            PrimOpPtr x = (PrimOp *)obj.ptr();
            evalPrimOp(x, args, out);
            break;
        }
        vector<TypePtr> argsKey;
        vector<ValueTempness> argsTempness;
        computeArgsKey(pvArgs, argsKey, argsTempness);
        CompileContextPusher pusher(obj, argsKey);
        InvokeEntryPtr entry = safeAnalyzeCallable(obj, argsKey, argsTempness);
        if (entry->callByName) {
            ExprListPtr objectExprs = new ExprList();
            for (vector<EValuePtr>::const_iterator i = args->values.begin();
                 i != args->values.end();
                 ++i)
            {
                objectExprs->add(new ObjectExpr(i->ptr()));
            }
            ExprPtr callableObject = new ObjectExpr(callable.ptr());
            callableObject->location = topLocation();
            evalCallByName(entry, callableObject, objectExprs, new Env(), out);
        } else {
            assert(entry->analyzed);
            evalCallCode(entry, args, out);
        }
        break;
    }

    default :
        error("invalid call operation");
        break;

    }
}



//
// evalCallPointer
//

void evalCallPointer(EValuePtr /*x*/,
                     MultiEValuePtr /*args*/,
                     MultiEValuePtr /*out*/)
{
    error("invoking a code pointer not yet supported in evaluator");
}



//
// evalCallCode
//

void evalCallCode(InvokeEntryPtr entry,
                  MultiEValuePtr args,
                  MultiEValuePtr out)
{
    assert(!entry->callByName);
    assert(entry->analyzed);
    if (entry->code->isLLVMBody()) {
        evalCallCompiledCode(entry, args, out);
        return;
    }

    ensureArity(args, entry->argsKey.size());

    EnvPtr env = new Env(entry->env);

    for (unsigned i = 0; i < entry->fixedArgNames.size(); ++i) {
        EValuePtr ev = args->values[i];
        EValuePtr earg = new EValue(ev->type, ev->addr);
        earg->forwardedRValue = entry->forwardedRValueFlags[i];
        addLocal(env, entry->fixedArgNames[i], earg.ptr());
    }

    if (entry->varArgName.ptr()) {
        unsigned nFixed = entry->fixedArgNames.size();
        MultiEValuePtr varArgs = new MultiEValue();
        for (unsigned i = 0; i < entry->varArgTypes.size(); ++i) {
            EValuePtr ev = args->values[i + nFixed];
            EValuePtr earg = new EValue(ev->type, ev->addr);
            earg->forwardedRValue = entry->forwardedRValueFlags[i + nFixed];
            varArgs->add(earg);
        }
        addLocal(env, entry->varArgName, varArgs.ptr());
    }

    assert(out->size() == entry->returnTypes.size());
    vector<EReturn> returns;
    for (unsigned i = 0; i < entry->returnTypes.size(); ++i) {
        bool isRef = entry->returnIsRef[i];
        TypePtr rt = entry->returnTypes[i];
        EValuePtr ev = out->values[i];
        returns.push_back(EReturn(isRef, rt, ev));
    }

    if (entry->code->hasReturnSpecs()) {
        const vector<ReturnSpecPtr> &returnSpecs = entry->code->returnSpecs;
        unsigned i = 0;
        for (; i < returnSpecs.size(); ++i) {
            ReturnSpecPtr rspec = returnSpecs[i];
            if (rspec->name.ptr())
                addLocal(env, rspec->name, returns[i].value.ptr());
        }
        ReturnSpecPtr varReturnSpec = entry->code->varReturnSpec;
        if (!varReturnSpec)
            assert(i == entry->returnTypes.size());
        if (varReturnSpec.ptr() && varReturnSpec->name.ptr()) {
            MultiEValuePtr mev = new MultiEValue();
            for (; i < entry->returnTypes.size(); ++i)
                mev->add(returns[i].value);
            addLocal(env, varReturnSpec->name, mev.ptr());
        }
    }

    EvalContextPtr ctx = new EvalContext(returns);

    assert(entry->code->body.ptr());
    TerminationPtr term = evalStatement(entry->code->body, env, ctx);
    if (term.ptr()) {
        switch (term->terminationKind) {
        case TERMINATE_RETURN :
            break;
        case TERMINATE_BREAK :
            error(term, "invalid 'break' statement");
        case TERMINATE_CONTINUE:
            error(term, "invalid 'continue' statement");
        case TERMINATE_GOTO :
            error(term, "invalid 'goto' statement");
        default :
            assert(false);
        }
    }
}



//
// evalCallCompiledCode
//

void evalCallCompiledCode(InvokeEntryPtr entry,
                          MultiEValuePtr args,
                          MultiEValuePtr out)
{
    if (!entry->llvmFunc)
        codegenCodeBody(entry);
    assert(entry->llvmFunc);

    vector<llvm::GenericValue> gvArgs;

    for (unsigned i = 0; i < args->size(); ++i) {
        EValuePtr earg = args->values[i];
        assert(earg->type == entry->argsKey[i]);
        gvArgs.push_back(llvm::GenericValue(earg->addr));
    }

    assert(out->size() == entry->returnTypes.size());
    for (unsigned i = 0; i < entry->returnTypes.size(); ++i) {
        TypePtr t = entry->returnTypes[i];
        EValuePtr ev = out->values[i];
        if (entry->returnIsRef[i]) {
            assert(ev->type == pointerType(t));
            gvArgs.push_back(llvm::GenericValue(out->values[i]->addr));
        }
        else {
            assert(ev->type == t);
            gvArgs.push_back(llvm::GenericValue(out->values[i]->addr));
        }
    }
    error("calling compiled code is not supported in the evaluator");
}



//
// evalCallByName
//

void evalCallByName(InvokeEntryPtr entry,
                    ExprPtr callable,
                    ExprListPtr args,
                    EnvPtr env,
                    MultiEValuePtr out)
{
    assert(entry->callByName);
    if (entry->varArgName.ptr())
        assert(args->size() >= entry->fixedArgNames.size());
    else
        assert(args->size() == entry->fixedArgNames.size());

    EnvPtr bodyEnv = new Env(entry->env);
    bodyEnv->callByNameExprHead = callable;

    for (unsigned i = 0; i < entry->fixedArgNames.size(); ++i) {
        ExprPtr expr = foreignExpr(env, args->exprs[i]);
        addLocal(bodyEnv, entry->fixedArgNames[i], expr.ptr());
    }

    if (entry->varArgName.ptr()) {
        ExprListPtr varArgs = new ExprList();
        for (unsigned i = entry->fixedArgNames.size(); i < args->size(); ++i) {
            ExprPtr expr = foreignExpr(env, args->exprs[i]);
            varArgs->add(expr);
        }
        addLocal(bodyEnv, entry->varArgName, varArgs.ptr());
    }

    MultiPValuePtr mpv = safeAnalyzeCallByName(entry, callable, args, env);
    assert(mpv->size() == out->size());

    vector<EReturn> returns;
    for (unsigned i = 0; i < mpv->size(); ++i) {
        PValuePtr pv = mpv->values[i];
        EValuePtr ev = out->values[i];
        if (pv->isTemp)
            assert(ev->type == pv->type);
        else
            assert(ev->type == pointerType(pv->type));
        returns.push_back(EReturn(!pv->isTemp, pv->type, ev));
    }

    if (entry->code->hasReturnSpecs()) {
        const vector<ReturnSpecPtr> &returnSpecs = entry->code->returnSpecs;
        unsigned i = 0;
        for (; i < returnSpecs.size(); ++i) {
            ReturnSpecPtr rspec = returnSpecs[i];
            if (rspec->name.ptr())
                addLocal(bodyEnv, rspec->name, returns[i].value.ptr());
        }
        ReturnSpecPtr varReturnSpec = entry->code->varReturnSpec;
        if (!varReturnSpec)
            assert(i == mpv->size());
        if (varReturnSpec.ptr() && varReturnSpec->name.ptr()) {
            MultiEValuePtr mev = new MultiEValue();
            for (; i < mpv->size(); ++i)
                mev->add(returns[i].value);
            addLocal(bodyEnv, varReturnSpec->name, mev.ptr());
        }
    }

    EvalContextPtr ctx = new EvalContext(returns);

    assert(entry->code->body.ptr());
    TerminationPtr term = evalStatement(entry->code->body, bodyEnv, ctx);
    if (term.ptr()) {
        switch (term->terminationKind) {
        case TERMINATE_RETURN :
            break;
        case TERMINATE_BREAK :
            error(term, "invalid 'break' statement");
        case TERMINATE_CONTINUE :
            error(term, "invalid 'continue' statement");
        case TERMINATE_GOTO :
            error(term, "invalid 'goto' statement");
        default :
            assert(false);
        }
    }
}



//
// evalStatement
//

TerminationPtr evalStatement(StatementPtr stmt,
                             EnvPtr env,
                             EvalContextPtr ctx)
{
    LocationContext loc(stmt->location);

    switch (stmt->stmtKind) {

    case BLOCK : {
        Block *x = (Block *)stmt.ptr();
        int blockMarker = evalMarkStack();
        map<string,LabelInfo> labels;
        evalCollectLabels(x->statements, 0, env, labels);
        TerminationPtr termination;
        unsigned pos = 0;
        while (pos < x->statements.size()) {
            StatementPtr y = x->statements[pos];
            if (y->stmtKind == LABEL) {
            }
            else if (y->stmtKind == BINDING) {
                env = evalBinding((Binding *)y.ptr(), env);
                evalCollectLabels(x->statements, pos+1, env, labels);
            }
            else {
                termination = evalStatement(y, env, ctx);
                if (termination.ptr()) {
                    if (termination->terminationKind == TERMINATE_GOTO) {
                        TerminateGoto *z = (TerminateGoto *)termination.ptr();
                        if (labels.count(z->targetLabel->str)) {
                            LabelInfo &info = labels[z->targetLabel->str];
                            env = info.env;
                            evalDestroyAndPopStack(info.stackMarker);
                            pos = info.blockPosition;
                            termination = NULL;
                            continue;
                        }
                    }
                    break;
                }
            }
            ++pos;
        }
        evalDestroyAndPopStack(blockMarker);
        return termination;
    }

    case LABEL :
        error("invalid label. labels can only appear within blocks");
        return NULL;

    case BINDING :
        error("invalid binding. bindings can only appear within blocks");
        return NULL;

    case ASSIGNMENT : {
        Assignment *x = (Assignment *)stmt.ptr();
        MultiPValuePtr mpvLeft = safeAnalyzeMulti(x->left, env, 0);
        MultiPValuePtr mpvRight = safeAnalyzeMulti(x->right, env, mpvLeft->size());
        if (mpvLeft->size() != mpvRight->size())
            arityMismatchError(mpvLeft->size(), mpvRight->size());
        for (unsigned i = 0; i < mpvLeft->size(); ++i) {
            if (mpvLeft->values[i]->isTemp)
                argumentError(i, "cannot assign to a temporary");
        }
        int marker = evalMarkStack();
        if (mpvLeft->size() == 1) {
            ExprListPtr args = new ExprList();
            args->add(x->left);
            args->add(x->right);
            ExprPtr assignCall = new Call(prelude_expr_assign(), args);
            evalExprAsRef(assignCall, env);
        }
        else {
            MultiEValuePtr mevRight = new MultiEValue();
            for (unsigned i = 0; i < mpvRight->size(); ++i) {
                PValuePtr pv = mpvRight->values[i];
                EValuePtr ev = evalAllocValue(pv->type);
                mevRight->add(ev);
            }
            evalMultiInto(x->right, env, mevRight, mpvLeft->size());
            MultiEValuePtr mevLeft = evalMultiAsRef(x->left, env);
            assert(mevLeft->size() == mevRight->size());
            for (unsigned i = 0; i < mevLeft->size(); ++i) {
                EValuePtr evLeft = mevLeft->values[i];
                EValuePtr evRight = mevRight->values[i];
                evalValueMoveAssign(evLeft, evRight);
            }
        }
        evalDestroyAndPopStack(marker);
        return NULL;
    }

    case INIT_ASSIGNMENT : {
        InitAssignment *x = (InitAssignment *)stmt.ptr();
        MultiPValuePtr mpvLeft = safeAnalyzeMulti(x->left, env, 0);
        MultiPValuePtr mpvRight = safeAnalyzeMulti(x->right, env, mpvLeft->size());
        if (mpvLeft->size() != mpvRight->size())
            arityMismatchError(mpvLeft->size(), mpvRight->size());
        for (unsigned i = 0; i < mpvLeft->size(); ++i) {
            if (mpvLeft->values[i]->isTemp)
                argumentError(i, "cannot assign to a temporary");
            if (mpvLeft->values[i]->type != mpvRight->values[i]->type) {
                argumentTypeError(i,
                                  mpvLeft->values[i]->type,
                                  mpvRight->values[i]->type);
            }
        }
        int marker = evalMarkStack();
        MultiEValuePtr mevLeft = evalMultiAsRef(x->left, env);
        evalMultiInto(x->right, env, mevLeft, mpvLeft->size());
        evalDestroyAndPopStack(marker);
        return NULL;
    }

    case UPDATE_ASSIGNMENT : {
        UpdateAssignment *x = (UpdateAssignment *)stmt.ptr();
        PValuePtr pvLeft = safeAnalyzeOne(x->left, env);
        if (pvLeft->isTemp)
            error(x->left, "cannot assign to a temporary");
        CallPtr call = new Call(prelude_expr_updateAssign(), new ExprList());
        call->parenArgs->add(updateOperatorExpr(x->op));
        call->parenArgs->add(x->left);
        call->parenArgs->add(x->right);
        return evalStatement(new ExprStatement(call.ptr()), env, ctx);
    }

    case GOTO : {
        Goto *x = (Goto *)stmt.ptr();
        return new TerminateGoto(x->labelName, x->location);
    }

    case RETURN : {
        Return *x = (Return *)stmt.ptr();
        MultiPValuePtr mpv = safeAnalyzeMulti(x->values, env, 0);
        MultiEValuePtr mev = new MultiEValue();
        ensureArity(mpv, ctx->returns.size());
        for (unsigned i = 0; i < mpv->size(); ++i) {
            PValuePtr pv = mpv->values[i];
            bool byRef = returnKindToByRef(x->returnKind, pv);
            EReturn &y = ctx->returns[i];
            if (y.type != pv->type)
                argumentTypeError(i, y.type, pv->type);
            if (byRef != y.byRef)
                argumentError(i, "mismatching by-ref and by-value returns");
            if (byRef && pv->isTemp)
                argumentError(i, "cannot return a temporary by reference");
            mev->add(y.value);
        }
        int marker = evalMarkStack();
        switch (x->returnKind) {
        case RETURN_VALUE :
            evalMultiInto(x->values, env, mev, 0);
            break;
        case RETURN_REF : {
            MultiEValuePtr mevRef = evalMultiAsRef(x->values, env);
            assert(mev->size() == mevRef->size());
            for (unsigned i = 0; i < mev->size(); ++i) {
                EValuePtr evPtr = mev->values[i];
                EValuePtr evRef = mevRef->values[i];
                *((void **)evPtr->addr) = (void *)evRef->addr;
            }
            break;
        }
        case RETURN_FORWARD :
            evalMulti(x->values, env, mev, 0);
            break;
        default :
            assert(false);
        }
        evalDestroyAndPopStack(marker);
        return new TerminateReturn(x->location);
    }

    case IF : {
        If *x = (If *)stmt.ptr();
        int marker = evalMarkStack();
        EValuePtr ev = evalOneAsRef(x->condition, env);
        bool flag = evalToBoolFlag(ev);
        evalDestroyAndPopStack(marker);
        if (flag)
            return evalStatement(x->thenPart, env, ctx);
        if (x->elsePart.ptr())
            return evalStatement(x->elsePart, env, ctx);
        return NULL;
    }

    case SWITCH : {
        Switch *x = (Switch *)stmt.ptr();
        if (!x->desugared)
            x->desugared = desugarSwitchStatement(x);
        return evalStatement(x->desugared, env, ctx);
    }

    case EVAL_STATEMENT : {
        EvalStatement *eval = (EvalStatement*)stmt.ptr();
        vector<StatementPtr> const &evaled = desugarEvalStatement(eval, env);
        TerminationPtr termination;
        for (unsigned i = 0; i < evaled.size(); ++i) {
            termination = evalStatement(evaled[i], env, ctx);
            if (termination != NULL)
                return termination;
        }
        return NULL;
    }

    case EXPR_STATEMENT : {
        ExprStatement *x = (ExprStatement *)stmt.ptr();
        int marker = evalMarkStack();
        evalExprAsRef(x->expr, env);
        evalDestroyAndPopStack(marker);
        return NULL;
    }

    case WHILE : {
        While *x = (While *)stmt.ptr();
        while (true) {
            int marker = evalMarkStack();
            EValuePtr ev = evalOneAsRef(x->condition, env);
            bool flag = evalToBoolFlag(ev);
            evalDestroyAndPopStack(marker);
            if (!flag) break;
            TerminationPtr term = evalStatement(x->body, env, ctx);
            if (term.ptr()) {
                if (term->terminationKind == TERMINATE_BREAK)
                    break;
                if (term->terminationKind == TERMINATE_CONTINUE)
                    continue;
                return term;
            }
        }
        return NULL;
    }

    case BREAK : {
        return new TerminateBreak(stmt->location);
    }

    case CONTINUE : {
        return new TerminateContinue(stmt->location);
    }

    case FOR : {
        For *x = (For *)stmt.ptr();
        if (!x->desugared)
            x->desugared = desugarForStatement(x);
        return evalStatement(x->desugared, env, ctx);
    }

    case FOREIGN_STATEMENT : {
        ForeignStatement *x = (ForeignStatement *)stmt.ptr();
        return evalStatement(x->statement, x->getEnv(), ctx);
    }

    case TRY : {
        Try *x = (Try *)stmt.ptr();
        // exception handling not supported in the evaluator.
        return evalStatement(x->tryBlock, env, ctx);
    }

    case THROW : {
        error("throw statement not supported in the evaluator");
        return NULL;
    }

    case STATIC_FOR : {
        StaticFor *x = (StaticFor *)stmt.ptr();
        MultiEValuePtr mev = evalForwardMultiAsRef(x->values, env);
        initializeStaticForClones(x, mev->size());
        for (unsigned i = 0; i < mev->size(); ++i) {
            EnvPtr env2 = new Env(env);
            addLocal(env2, x->variable, mev->values[i].ptr());
            TerminationPtr termination =
                evalStatement(x->clonedBodies[i], env2, ctx);
            if (termination.ptr())
                return termination;
        }
        return NULL;
    }

    case ONERROR :
    case FINALLY :
        // exception handling not supported in the evaluator.
        return NULL;

    case UNREACHABLE : {
        error("unreachable statement");
        return NULL;
    }

    default :
        assert(false);
        return NULL;

    }
}



//
// evalCollectLabels
//

void evalCollectLabels(const vector<StatementPtr> &statements,
                       unsigned startIndex,
                       EnvPtr env,
                       map<string, LabelInfo> &labels)
{
    for (unsigned i = startIndex; i < statements.size(); ++i) {
        StatementPtr x = statements[i];
        switch (x->stmtKind) {
        case LABEL : {
            Label *y = (Label *)x.ptr();
            if (labels.count(y->name->str))
                error(x, "label redefined");
            labels[y->name->str] = LabelInfo(env, evalMarkStack(), i+1);
            break;
        }
        case BINDING :
            return;
        default :
            break;
        }
    }
}



//
// evalBinding
//

EnvPtr evalBinding(BindingPtr x, EnvPtr env)
{
    LocationContext loc(x->location);

    switch (x->bindingKind) {

    case VAR : {
        MultiPValuePtr mpv = safeAnalyzeMulti(x->values, env, x->names.size());
        if (mpv->size() != x->names.size())
            arityError(x->names.size(), mpv->size());
        MultiEValuePtr mev = new MultiEValue();
        for (unsigned i = 0; i < x->names.size(); ++i) {
            EValuePtr ev = evalAllocValue(mpv->values[i]->type);
            mev->add(ev);
        }
        int marker = evalMarkStack();
        evalMultiInto(x->values, env, mev, x->names.size());
        evalDestroyAndPopStack(marker);
        EnvPtr env2 = new Env(env);
        for (unsigned i = 0; i < x->names.size(); ++i)
            addLocal(env2, x->names[i], mev->values[i].ptr());
        return env2;
    }

    case REF : {
        MultiPValuePtr mpv = safeAnalyzeMulti(x->values, env, x->names.size());
        if (mpv->size() != x->names.size())
            arityError(x->names.size(), mpv->size());
        MultiEValuePtr mev = new MultiEValue();
        for (unsigned i = 0; i < x->names.size(); ++i) {
            PValuePtr pv = mpv->values[i];
            if (pv->isTemp)
                argumentError(i, "ref can only bind to an rvalue");
            EValuePtr evPtr = evalAllocValue(pointerType(pv->type));
            mev->add(evPtr);
        }
        int marker = evalMarkStack();
        evalMulti(x->values, env, mev, x->names.size());
        evalDestroyAndPopStack(marker);
        EnvPtr env2 = new Env(env);
        for (unsigned i = 0; i < x->names.size(); ++i) {
            EValuePtr evPtr = mev->values[i];
            addLocal(env2, x->names[i], derefValue(evPtr).ptr());
        }
        return env2;
    }

    case FORWARD : {
        MultiPValuePtr mpv = safeAnalyzeMulti(x->values, env, x->names.size());
        if (mpv->size() != x->names.size())
            arityError(x->names.size(), mpv->size());
        MultiEValuePtr mev = new MultiEValue();
        for (unsigned i = 0; i < x->names.size(); ++i) {
            PValuePtr pv = mpv->values[i];
            if (pv->isTemp) {
                EValuePtr ev = evalAllocValue(pv->type);
                mev->add(ev);
            }
            else {
                EValuePtr evPtr = evalAllocValue(pointerType(pv->type));
                mev->add(evPtr);
            }
        }
        int marker = evalMarkStack();
        evalMulti(x->values, env, mev, x->names.size());
        evalDestroyAndPopStack(marker);
        EnvPtr env2 = new Env(env);
        for (unsigned i = 0; i < x->names.size(); ++i) {
            if (mpv->values[i]->isTemp) {
                EValuePtr ev = mev->values[i];
                addLocal(env2, x->names[i], ev.ptr());
            }
            else {
                EValuePtr evPtr = mev->values[i];
                addLocal(env2, x->names[i], derefValue(evPtr).ptr());
            }
        }
        return env2;
    }

    case ALIAS : {
        ensureArity(x->names, 1);
        ensureArity(x->values->exprs, 1);
        EnvPtr env2 = new Env(env);
        ExprPtr y = foreignExpr(env, x->values->exprs[0]);
        addLocal(env2, x->names[0], y.ptr());
        return env2;
    }

    default : {
        assert(false);
        return NULL;
    }

    }

}



//
// valueToStatic, valueToStaticSizeTOrInt
// valueToType, valueToNumericType, valueToIntegerType,
// valueToPointerLikeType, valueToTupleType, valueToRecordType,
// valueToVariantType, valueToEnumType, valueToIdentifier
//

static ObjectPtr valueToStatic(EValuePtr ev)
{
    if (ev->type->typeKind != STATIC_TYPE)
        return NULL;
    StaticType *st = (StaticType *)ev->type.ptr();
    return st->obj;
}

static ObjectPtr valueToStatic(MultiEValuePtr args, unsigned index)
{
    ObjectPtr obj = valueToStatic(args->values[index]);
    if (!obj)
        argumentError(index, "expecting a static value");
    return obj;
}

static size_t valueToStaticSizeTOrInt(MultiEValuePtr args, unsigned index)
{
    ObjectPtr obj = valueToStatic(args->values[index]);
    if (!obj || (obj->objKind != VALUE_HOLDER))
        argumentError(index, "expecting a static SizeT or Int value");
    ValueHolder *vh = (ValueHolder *)obj.ptr();
    if (vh->type == cSizeTType) {
        return *((size_t *)vh->buf);
    }
    else if (vh->type == cIntType) {
        int i = *((int *)vh->buf);
        return size_t(i);
    }
    else {
        argumentError(index, "expecting a static SizeT or Int value");
        return 0;
    }
}

static TypePtr valueToType(MultiEValuePtr args, unsigned index)
{
    ObjectPtr obj = valueToStatic(args->values[index]);
    if (!obj || (obj->objKind != TYPE))
        argumentError(index, "expecting a type");
    return (Type *)obj.ptr();
}

static TypePtr valueToNumericType(MultiEValuePtr args, unsigned index)
{
    TypePtr t = valueToType(args, index);
    switch (t->typeKind) {
    case INTEGER_TYPE :
    case FLOAT_TYPE :
        return t;
    default :
        argumentTypeError(index, "numeric type", t);
        return NULL;
    }
}

static ComplexTypePtr valueToComplexType(MultiEValuePtr args, unsigned index)
{
    TypePtr t = valueToType(args, index);
    if (t->typeKind != COMPLEX_TYPE)
        argumentTypeError(index, "complex type", t);
    return (ComplexType *)t.ptr();
}

static IntegerTypePtr valueToIntegerType(MultiEValuePtr args, unsigned index)
{
    TypePtr t = valueToType(args, index);
    if (t->typeKind != INTEGER_TYPE)
        argumentTypeError(index, "integer type", t);
    return (IntegerType *)t.ptr();
}

static TypePtr valueToPointerLikeType(MultiEValuePtr args, unsigned index)
{
    TypePtr t = valueToType(args, index);
    if (!isPointerOrCodePointerType(t))
        argumentTypeError(index, "pointer or code-pointer type", t);
    return t;
}

static TupleTypePtr valueToTupleType(MultiEValuePtr args, unsigned index)
{
    TypePtr t = valueToType(args, index);
    if (t->typeKind != TUPLE_TYPE)
        argumentTypeError(index, "tuple type", t);
    return (TupleType *)t.ptr();
}

static UnionTypePtr valueToUnionType(MultiEValuePtr args, unsigned index)
{
    TypePtr t = valueToType(args, index);
    if (t->typeKind != UNION_TYPE)
        argumentTypeError(index, "union type", t);
    return (UnionType *)t.ptr();
}

static RecordTypePtr valueToRecordType(MultiEValuePtr args, unsigned index)
{
    TypePtr t = valueToType(args, index);
    if (t->typeKind != RECORD_TYPE)
        argumentTypeError(index, "record type", t);
    return (RecordType *)t.ptr();
}

static VariantTypePtr valueToVariantType(MultiEValuePtr args, unsigned index)
{
    TypePtr t = valueToType(args, index);
    if (t->typeKind != VARIANT_TYPE)
        argumentTypeError(index, "variant type", t);
    return (VariantType *)t.ptr();
}

static EnumTypePtr valueToEnumType(MultiEValuePtr args, unsigned index)
{
    TypePtr t = valueToType(args, index);
    if (t->typeKind != ENUM_TYPE)
        argumentTypeError(index, "enum type", t);
    initializeEnumType((EnumType*)t.ptr());
    return (EnumType *)t.ptr();
}

static IdentifierPtr valueToIdentifier(MultiEValuePtr args, unsigned index)
{
    ObjectPtr obj = valueToStatic(args->values[index]);
    if (!obj || (obj->objKind != IDENTIFIER))
        argumentError(index, "expecting identifier value");
    return (Identifier *)obj.ptr();
}



//
// numericValue, integerValue, pointerValue, pointerLikeValue,
// arrayValue, tupleValue, recordValue, variantValue, enumValue
//

static EValuePtr numericValue(MultiEValuePtr args, unsigned index,
                              TypePtr &type)
{
    EValuePtr ev = args->values[index];
    if (type.ptr()) {
        if (ev->type != type)
            argumentTypeError(index, type, ev->type);
    }
    else {
        switch (ev->type->typeKind) {
        case INTEGER_TYPE :
        case FLOAT_TYPE :
            break;
        default :
            argumentTypeError(index, "numeric type", ev->type);
        }
        type = ev->type;
    }
    return ev;
}

static EValuePtr complexValue(MultiEValuePtr args, unsigned index,
                            ComplexTypePtr &type)
{
    EValuePtr ev = args->values[index];
    if (type.ptr()) {
        if (ev->type != (Type *)type.ptr())
            argumentTypeError(index, type.ptr(), ev->type);
    }
    else {
        if (ev->type->typeKind != COMPLEX_TYPE)
            argumentTypeError(index, "complex type", ev->type);
        type = (ComplexType *)ev->type.ptr();
    }
    return ev;
}

static EValuePtr integerValue(MultiEValuePtr args, unsigned index,
                              IntegerTypePtr &type)
{
    EValuePtr ev = args->values[index];
    if (type.ptr()) {
        if (ev->type != type.ptr())
            argumentTypeError(index, type.ptr(), ev->type);
    }
    else {
        if (ev->type->typeKind != INTEGER_TYPE)
            argumentTypeError(index, "integer type", ev->type);
        type = (IntegerType *)ev->type.ptr();
    }
    return ev;
}

static EValuePtr pointerValue(MultiEValuePtr args, unsigned index,
                              PointerTypePtr &type)
{
    EValuePtr ev = args->values[index];
    if (type.ptr()) {
        if (ev->type != (Type *)type.ptr())
            argumentTypeError(index, type.ptr(), ev->type);
    }
    else {
        if (ev->type->typeKind != POINTER_TYPE)
            argumentTypeError(index, "pointer type", ev->type);
        type = (PointerType *)ev->type.ptr();
    }
    return ev;
}

static EValuePtr pointerLikeValue(MultiEValuePtr args, unsigned index,
                                  TypePtr &type)
{
    EValuePtr ev = args->values[index];
    if (type.ptr()) {
        if (ev->type != type)
            argumentTypeError(index, type, ev->type);
    }
    else {
        if (!isPointerOrCodePointerType(ev->type))
            argumentTypeError(index,
                              "pointer or code-pointer type",
                              ev->type);
        type = ev->type;
    }
    return ev;
}

static EValuePtr arrayValue(MultiEValuePtr args, unsigned index,
                            ArrayTypePtr &type)
{
    EValuePtr ev = args->values[index];
    if (type.ptr()) {
        if (ev->type != (Type *)type.ptr())
            argumentTypeError(index, type.ptr(), ev->type);
    }
    else {
        if (ev->type->typeKind != ARRAY_TYPE)
            argumentTypeError(index, "array type", ev->type);
        type = (ArrayType *)ev->type.ptr();
    }
    return ev;
}

static EValuePtr tupleValue(MultiEValuePtr args, unsigned index,
                            TupleTypePtr &type)
{
    EValuePtr ev = args->values[index];
    if (type.ptr()) {
        if (ev->type != (Type *)type.ptr())
            argumentTypeError(index, type.ptr(), ev->type);
    }
    else {
        if (ev->type->typeKind != TUPLE_TYPE)
            argumentTypeError(index, "tuple type", ev->type);
        type = (TupleType *)ev->type.ptr();
    }
    return ev;
}

static EValuePtr recordValue(MultiEValuePtr args, unsigned index,
                            RecordTypePtr &type)
{
    EValuePtr ev = args->values[index];
    if (type.ptr()) {
        if (ev->type != (Type *)type.ptr())
            argumentTypeError(index, type.ptr(), ev->type);
    }
    else {
        if (ev->type->typeKind != RECORD_TYPE)
            argumentTypeError(index, "record type", ev->type);
        type = (RecordType *)ev->type.ptr();
    }
    return ev;
}

static EValuePtr variantValue(MultiEValuePtr args, unsigned index,
                              VariantTypePtr &type)
{
    EValuePtr ev = args->values[index];
    if (type.ptr()) {
        if (ev->type != (Type *)type.ptr())
            argumentTypeError(index, type.ptr(), ev->type);
    }
    else {
        if (ev->type->typeKind != VARIANT_TYPE)
            argumentTypeError(index, "variant type", ev->type);
        type = (VariantType *)ev->type.ptr();
    }
    return ev;
}

static EValuePtr enumValue(MultiEValuePtr args, unsigned index,
                           EnumTypePtr &type)
{
    EValuePtr ev = args->values[index];
    if (type.ptr()) {
        if (ev->type != (Type *)type.ptr())
            argumentTypeError(index, type.ptr(), ev->type);
    }
    else {
        if (ev->type->typeKind != ENUM_TYPE)
            argumentTypeError(index, "enum type", ev->type);
        type = (EnumType *)ev->type.ptr();
    }
    return ev;
}



//
// binaryNumericOp
//

template <template<typename> class T>
static void binaryNumericOp(EValuePtr a, EValuePtr b, EValuePtr out)
{
    assert(a->type == b->type);
    switch (a->type->typeKind) {

    case INTEGER_TYPE : {
        IntegerType *t = (IntegerType *)a->type.ptr();
        if (t->isSigned) {
            switch (t->bits) {
            case 8 :  T<char>().eval(a, b, out); break;
            case 16 : T<short>().eval(a, b, out); break;
            case 32 : T<int>().eval(a, b, out); break;
            case 64 : T<long long>().eval(a, b, out); break;
            case 128 : T<clay_int128>().eval(a, b, out); break;
            default : assert(false);
            }
        }
        else {
            switch (t->bits) {
            case 8 :  T<unsigned char>().eval(a, b, out); break;
            case 16 : T<unsigned short>().eval(a, b, out); break;
            case 32 : T<unsigned int>().eval(a, b, out); break;
            case 64 : T<unsigned long long>().eval(a, b, out); break;
            case 128 : T<clay_uint128>().eval(a, b, out); break;
            default : assert(false);
            }
        }
        break;
    }

    case FLOAT_TYPE : {
        FloatType *t = (FloatType *)a->type.ptr();
        switch (t->bits) {
        case 32 : T<float>().eval(a, b, out); break;
        case 64 : T<double>().eval(a, b, out); break;
        case 80 : T<long double>().eval(a, b, out); break;
        default : assert(false);
        }
        break;
    }

    default :
        assert(false);
    }
}

template<typename T>
static void overflowError(const char *op, T a, T b) {
    ostringstream sout;
    sout << "integer overflow: " << a << " " << op << " " << b;
    error(sout.str());
}

template<typename T>
static void invalidShiftError(const char *op, T a, T b) {
    ostringstream sout;
    sout << "invalid shift: " << a << " " << op << " " << b;
    error(sout.str());
}

template<typename T>
static void overflowError(const char *op, T a) {
    ostringstream sout;
    sout << "integer overflow: " << op << a;
    error(sout.str());
}

static void divideByZeroError() {
    error("integer division by zero");
}

template <typename T>
class BinaryOpHelper {
public :
    void eval(EValuePtr a, EValuePtr b, EValuePtr out) {
        perform(*((T *)a->addr), *((T *)b->addr), out->addr);
    }
    virtual void perform(T &a, T &b, void *out) = 0;
};

template <typename T>
class Op_numericEqualsP : public BinaryOpHelper<T> {
public :
    virtual void perform(T &a, T &b, void *out) {
        *((char *)out) = (a == b) ? 1 : 0;
    }
};

template <typename T>
class Op_numericLesserP : public BinaryOpHelper<T> {
public :
    virtual void perform(T &a, T &b, void *out) {
        *((char *)out) = (a < b) ? 1 : 0;
    }
};

template <typename T>
class Op_numericAdd : public BinaryOpHelper<T> {
public :
    virtual void perform(T &a, T &b, void *out) {
        *((T *)out) = a + b;
    }
};

template <typename T>
class Op_numericSubtract : public BinaryOpHelper<T> {
public :
    virtual void perform(T &a, T &b, void *out) {
        *((T *)out) = a - b;
    }
};

template <typename T>
class Op_numericMultiply : public BinaryOpHelper<T> {
public :
    virtual void perform(T &a, T &b, void *out) {
        *((T *)out) = a * b;
    }
};

template <typename T>
class Op_numericDivide : public BinaryOpHelper<T> {
public :
    virtual void perform(T &a, T &b, void *out) {
        *((T *)out) = a / b;
    }
};


//
// unaryNumericOp
//

template <template<typename> class T>
static void unaryNumericOp(EValuePtr a, EValuePtr out)
{
    switch (a->type->typeKind) {

    case INTEGER_TYPE : {
        IntegerType *t = (IntegerType *)a->type.ptr();
        if (t->isSigned) {
            switch (t->bits) {
            case 8 :  T<char>().eval(a, out); break;
            case 16 : T<short>().eval(a, out); break;
            case 32 : T<int>().eval(a, out); break;
            case 64 : T<long long>().eval(a, out); break;
            case 128 : T<clay_int128>().eval(a, out); break;
            default : assert(false);
            }
        }
        else {
            switch (t->bits) {
            case 8 :  T<unsigned char>().eval(a, out); break;
            case 16 : T<unsigned short>().eval(a, out); break;
            case 32 : T<unsigned int>().eval(a, out); break;
            case 64 : T<unsigned long long>().eval(a, out); break;
            case 128 : T<clay_uint128>().eval(a, out); break;
            default : assert(false);
            }
        }
        break;
    }

    case FLOAT_TYPE : {
        FloatType *t = (FloatType *)a->type.ptr();
        switch (t->bits) {
        case 32 : T<float>().eval(a, out); break;
        case 64 : T<double>().eval(a, out); break;
        case 80 : T<long double>().eval(a, out); break;
        default : assert(false);
        }
        break;
    }

    default :
        assert(false);
    }
}

template <typename T>
class UnaryOpHelper {
public :
    void eval(EValuePtr a, EValuePtr out) {
        perform(*((T *)a->addr), out->addr);
    }
    virtual void perform(T &a, void *out) = 0;
};

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4146)
#endif

template <typename T>
class Op_numericNegate : public UnaryOpHelper<T> {
public :
    virtual void perform(T &a, void *out) {
        *((T *)out) = -a;
    }
};

#ifdef _MSC_VER
#pragma warning(pop)
#endif


//
// binaryIntegerOp
//

template <template<typename> class T>
static void binaryIntegerOp(EValuePtr a, EValuePtr b, EValuePtr out)
{
    assert(a->type == b->type);
    assert(a->type->typeKind == INTEGER_TYPE);
    IntegerType *t = (IntegerType *)a->type.ptr();
    if (t->isSigned) {
        switch (t->bits) {
        case 8 :  T<char>().eval(a, b, out); break;
        case 16 : T<short>().eval(a, b, out); break;
        case 32 : T<int>().eval(a, b, out); break;
        case 64 : T<long long>().eval(a, b, out); break;
        case 128 : T<clay_int128>().eval(a, b, out); break;
        default : assert(false);
        }
    }
    else {
        switch (t->bits) {
        case 8 :  T<unsigned char>().eval(a, b, out); break;
        case 16 : T<unsigned short>().eval(a, b, out); break;
        case 32 : T<unsigned int>().eval(a, b, out); break;
        case 64 : T<unsigned long long>().eval(a, b, out); break;
        case 128 : T<clay_uint128>().eval(a, b, out); break;
        default : assert(false);
        }
    }
}

template <typename T>
class Op_integerRemainder : public BinaryOpHelper<T> {
    virtual void perform(T &a, T &b, void *out) {
        *((T *)out) = a % b;
    }
};

template <typename T>
class Op_integerShiftLeft : public BinaryOpHelper<T> {
    virtual void perform(T &a, T &b, void *out) {
        *((T *)out) = a << b;
    }
};

template <typename T>
class Op_integerShiftRight : public BinaryOpHelper<T> {
    virtual void perform(T &a, T &b, void *out) {
        *((T *)out) = a >> b;
    }
};

template <typename T>
class Op_integerBitwiseAnd : public BinaryOpHelper<T> {
    virtual void perform(T &a, T &b, void *out) {
        *((T *)out) = a & b;
    }
};

template <typename T>
class Op_integerBitwiseOr : public BinaryOpHelper<T> {
    virtual void perform(T &a, T &b, void *out) {
        *((T *)out) = a | b;
    }
};

template <typename T>
class Op_integerBitwiseXor : public BinaryOpHelper<T> {
    virtual void perform(T &a, T &b, void *out) {
        *((T *)out) = a ^ b;
    }
};

#if defined(__clang__)
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wtautological-compare"
#elif defined(__GCC__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wtype-limits"
#endif

template <typename T>
class Op_integerAddChecked : public BinaryOpHelper<T> {
public :
    virtual void perform(T &a, T &b, void *out) {
        if ((b < T(0) && a < std::numeric_limits<T>::min() - b)
            || (b > T(0) && a > std::numeric_limits<T>::max() - b))
                overflowError("+", a, b);
        else
            *((T *)out) = a + b;
    }
};

template <typename T>
class Op_integerSubtractChecked : public BinaryOpHelper<T> {
public :
    virtual void perform(T &a, T &b, void *out) {
        if ((b < T(0) && a > std::numeric_limits<T>::max() + b)
            || (b > T(0) && a < std::numeric_limits<T>::min() + b))
                overflowError("-", a, b);
        else
            *((T *)out) = a - b;
    }
};

template <typename T>
class Op_integerShiftLeftChecked : public BinaryOpHelper<T> {
    virtual void perform(T &a, T &b, void *out) {
        if (b < 0)
            invalidShiftError("bitshl", a, b);

        if (b == 0) {
            *((T *)out) = a;
            return;
        }

        if (std::numeric_limits<T>::min() != 0) {
            // signed
            T testa = a < 0 ? ~a : a;
            if (b > sizeof(T)*8 - 1 || (testa >> (sizeof(T)*8 - b - 1)) != 0)
                overflowError("bitshl", a, b);
        } else {
            // unsigned
            if (b > sizeof(T)*8 || (a >> (sizeof(T)*8 - b)) != 0)
                overflowError("bitshl", a, b);
        }
        *((T *)out) = a << b;
    }
};

#if defined(__clang__)
#  pragma clang diagnostic pop
#elif defined(__GCC__)
#  pragma GCC diagnostic pop
#endif

template <typename T>
class Op_integerRemainderChecked : public BinaryOpHelper<T> {
    virtual void perform(T &a, T &b, void *out) {
        if (b == 0)
            divideByZeroError();
        *((T *)out) = a % b;
    }
};

template<typename T> struct next_larger_type;
template<> struct next_larger_type<char> { typedef short type; };
template<> struct next_larger_type<short> { typedef int type; };
template<> struct next_larger_type<int> { typedef ptrdiff64_t type; };
template<> struct next_larger_type<ptrdiff64_t> { typedef clay_int128 type; };
template<> struct next_larger_type<clay_int128> { typedef clay_int128 type; };
template<> struct next_larger_type<unsigned char> { typedef unsigned short type; };
template<> struct next_larger_type<unsigned short> { typedef unsigned type; };
template<> struct next_larger_type<unsigned> { typedef size64_t type; };
template<> struct next_larger_type<size64_t> { typedef clay_uint128 type; };
template<> struct next_larger_type<clay_uint128> { typedef clay_uint128 type; };

template <typename T>
class Op_integerMultiplyChecked : public BinaryOpHelper<T> {
public :
    typedef typename next_larger_type<T>::type BigT;
    virtual void perform(T &a, T &b, void *out) {
        // XXX this won't work for 128-bit types or 64-bit types without
        // compiler int128 support
        BigT bigout = BigT(a) * BigT(b);
        if (b < std::numeric_limits<T>::min() || std::numeric_limits<T>::max() < b)
            overflowError("*", a, b);
        *((T *)out) = a * b;
    }
};

template <typename T>
class Op_integerDivideChecked : public BinaryOpHelper<T> {
public :
    virtual void perform(T &a, T &b, void *out) {
        if (std::numeric_limits<T>::min() != 0
            && a == std::numeric_limits<T>::min()
            && b == -1)
            overflowError("/", a, b);
        if (b == 0)
            divideByZeroError();
        *((T *)out) = a / b;
    }
};


//
// unaryIntegerOp
//

template <template<typename> class T>
static void unaryIntegerOp(EValuePtr a, EValuePtr out)
{
    assert(a->type->typeKind == INTEGER_TYPE);
    IntegerType *t = (IntegerType *)a->type.ptr();
    if (t->isSigned) {
        switch (t->bits) {
        case 8 :  T<char>().eval(a, out); break;
        case 16 : T<short>().eval(a, out); break;
        case 32 : T<int>().eval(a, out); break;
        case 64 : T<long long>().eval(a, out); break;
        case 128 : T<clay_int128>().eval(a, out); break;
        default : assert(false);
        }
    }
    else {
        switch (t->bits) {
        case 8 :  T<unsigned char>().eval(a, out); break;
        case 16 : T<unsigned short>().eval(a, out); break;
        case 32 : T<unsigned int>().eval(a, out); break;
        case 64 : T<unsigned long long>().eval(a, out); break;
        case 128 : T<clay_uint128>().eval(a, out); break;
        default : assert(false);
        }
    }
}


template <typename T>
class Op_integerBitwiseNot : public UnaryOpHelper<T> {
public :
    virtual void perform(T &a, void *out) {
        *((T *)out) = ~a;
    }
};

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4146)
#endif

template <typename T>
class Op_integerNegateChecked : public UnaryOpHelper<T> {
public :
    virtual void perform(T &a, void *out) {
        if (std::numeric_limits<T>::min() != 0
            && a == std::numeric_limits<T>::min())
            overflowError("-", a);
        *((T *)out) = -a;
    }
};

#ifdef _MSC_VER
#pragma warning(pop)
#endif


//
// op_numericConvert
//

template <typename DEST, typename SRC, bool CHECK>
struct op_numericConvert3;

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4244)
#endif
template <typename DEST, typename SRC>
struct op_numericConvert3<DEST, SRC, false> {
    static void perform(EValuePtr dest, EValuePtr src)
    {
        SRC value = *((SRC *)src->addr);
        *((DEST *)dest->addr) = DEST(value);
    }
};

template <typename DEST, typename SRC>
struct op_numericConvert3<DEST, SRC, true> {
    static void perform(EValuePtr dest, EValuePtr src)
    {
        SRC value = *((SRC *)src->addr);
        // ???
        *((DEST *)dest->addr) = DEST(value);
    }
};
#ifdef _MSC_VER
#pragma warning(pop)
#endif

template <typename D, bool CHECK>
static void op_numericConvert2(EValuePtr dest, EValuePtr src)
{
    switch (src->type->typeKind) {
    case INTEGER_TYPE : {
        IntegerType *t = (IntegerType *)src->type.ptr();
        if (t->isSigned) {
            switch (t->bits) {
            case 8 : op_numericConvert3<D,char,CHECK>::perform(dest, src); break;
            case 16 : op_numericConvert3<D,short,CHECK>::perform(dest, src); break;
            case 32 : op_numericConvert3<D,int,CHECK>::perform(dest, src); break;
            case 64 : op_numericConvert3<D,long long,CHECK>::perform(dest, src); break;
            case 128 : op_numericConvert3<D,clay_int128,CHECK>::perform(dest, src); break;
            default : assert(false);
            }
        }
        else {
            switch (t->bits) {
            case 8 : op_numericConvert3<D,unsigned char,CHECK>::perform(dest, src); break;
            case 16 : op_numericConvert3<D,unsigned short,CHECK>::perform(dest, src); break;
            case 32 : op_numericConvert3<D,unsigned int,CHECK>::perform(dest, src); break;
            case 64 : op_numericConvert3<D,unsigned long long,CHECK>::perform(dest, src); break;
            case 128 : op_numericConvert3<D,clay_uint128,CHECK>::perform(dest, src); break;
            default : assert(false);
            }
        }
        break;
    }
    case FLOAT_TYPE : {
        FloatType *t = (FloatType *)src->type.ptr();
        switch (t->bits) {
        case 32 : op_numericConvert3<D,float,CHECK>::perform(dest, src); break;
        case 64 : op_numericConvert3<D,double,CHECK>::perform(dest, src); break;
        case 80 : op_numericConvert3<D,long double,CHECK>::perform(dest, src); break;
        default : assert(false);
        }
        break;
    }

    default :
        assert(false);
    }
}

template <bool CHECK>
static void op_numericConvert(EValuePtr dest, EValuePtr src)
{
    switch (dest->type->typeKind) {
    case INTEGER_TYPE : {
        IntegerType *t = (IntegerType *)dest->type.ptr();
        if (t->isSigned) {
            switch (t->bits) {
            case 8 : op_numericConvert2<char,CHECK>(dest, src); break;
            case 16 : op_numericConvert2<short,CHECK>(dest, src); break;
            case 32 : op_numericConvert2<int,CHECK>(dest, src); break;
            case 64 : op_numericConvert2<long long,CHECK>(dest, src); break;
            case 128 : op_numericConvert2<clay_int128,CHECK>(dest, src); break;
            default : assert(false);
            }
        }
        else {
            switch (t->bits) {
            case 8 : op_numericConvert2<unsigned char,CHECK>(dest, src); break;
            case 16 : op_numericConvert2<unsigned short,CHECK>(dest, src); break;
            case 32 : op_numericConvert2<unsigned int,CHECK>(dest, src); break;
            case 64 : op_numericConvert2<unsigned long long,CHECK>(dest, src); break;
            case 128 : op_numericConvert2<clay_uint128,CHECK>(dest, src); break;
            default : assert(false);
            }
        }
        break;
    }
    case FLOAT_TYPE : {
        FloatType *t = (FloatType *)dest->type.ptr();
        switch (t->bits) {
        case 32 : op_numericConvert2<float,false>(dest, src); break;
        case 64 : op_numericConvert2<double,false>(dest, src); break;
        case 80 : op_numericConvert2<long double,false>(dest, src); break;
        default : assert(false);
        }
        break;
    }

    default :
        assert(false);
    }
}



//
// op_intToPtrInt
//

template <typename T>
static ptrdiff_t op_intToPtrInt2(EValuePtr a)
{
    return ptrdiff_t(*((T *)a->addr));
}

static ptrdiff_t op_intToPtrInt(EValuePtr a)
{
    assert(a->type->typeKind == INTEGER_TYPE);
    IntegerType *t = (IntegerType *)a->type.ptr();
    if (t->isSigned) {
        switch (t->bits) {
        case 8 : return op_intToPtrInt2<char>(a);
        case 16 : return op_intToPtrInt2<short>(a);
        case 32 : return op_intToPtrInt2<int>(a);
        case 64 : return op_intToPtrInt2<long long>(a);
        case 128 : return op_intToPtrInt2<clay_int128>(a);
        default : assert(false);
        }
    }
    else {
        switch (t->bits) {
        case 8 : return op_intToPtrInt2<unsigned char>(a);
        case 16 : return op_intToPtrInt2<unsigned short>(a);
        case 32 : return op_intToPtrInt2<unsigned int>(a);
        case 64 : return op_intToPtrInt2<unsigned long long>(a);
        case 128 : return op_intToPtrInt2<clay_uint128>(a);
        default : assert(false);
        }
    }
    return 0;
}



//
// op_pointerToInt
//

template <typename T>
static void op_pointerToInt2(EValuePtr dest, void *ptr)
{
    *((T *)dest->addr) = T(size_t(ptr));
}

static void op_pointerToInt(EValuePtr dest, void *ptr)
{
    assert(dest->type->typeKind == INTEGER_TYPE);
    IntegerType *t = (IntegerType *)dest->type.ptr();
    if (t->isSigned) {
        switch (t->bits) {
        case 8 : op_pointerToInt2<char>(dest, ptr); break;
        case 16 : op_pointerToInt2<short>(dest, ptr); break;
        case 32 : op_pointerToInt2<int>(dest, ptr); break;
        case 64 : op_pointerToInt2<long long>(dest, ptr); break;
        case 128 : op_pointerToInt2<clay_int128>(dest, ptr); break;
        default : assert(false);
        }
    }
    else {
        switch (t->bits) {
        case 8 : op_pointerToInt2<unsigned char>(dest, ptr); break;
        case 16 : op_pointerToInt2<unsigned short>(dest, ptr); break;
        case 32 : op_pointerToInt2<unsigned int>(dest, ptr); break;
        case 64 : op_pointerToInt2<unsigned long long>(dest, ptr); break;
        case 128 : op_pointerToInt2<clay_uint128>(dest, ptr); break;
        default : assert(false);
        }
    }
}



//
// evalPrimOp
//

void evalPrimOp(PrimOpPtr x, MultiEValuePtr args, MultiEValuePtr out)
{
    switch (x->primOpCode) {

    case PRIM_TypeP : {
        ensureArity(args, 1);
        ObjectPtr obj = valueToStatic(args->values[0]);
        bool isType = (obj.ptr() && (obj->objKind == TYPE));
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type == boolType);
        *((char *)out0->addr) = isType ? 1 : 0;
        break;
    }

    case PRIM_TypeSize : {
        ensureArity(args, 1);
        TypePtr t = valueToType(args, 0);
        ValueHolderPtr vh = sizeTToValueHolder(typeSize(t));
        evalStaticObject(vh.ptr(), out);
        break;
    }

    case PRIM_TypeAlignment : {
        ensureArity(args, 1);
        TypePtr t = valueToType(args, 0);
        ValueHolderPtr vh = sizeTToValueHolder(typeAlignment(t));
        evalStaticObject(vh.ptr(), out);
        break;
    }

    case PRIM_CallDefinedP : {
        if (args->size() < 1)
            arityError2(1, args->size());
        ObjectPtr callable = valueToStatic(args->values[0]);
        if (!callable) {
            EValuePtr evCall = staticEValue(prelude_call());
            MultiEValuePtr args2 = new MultiEValue(evCall);
            args2->add(staticEValue(args->values[0]->type.ptr()));
            for (unsigned i = 1; i < args->size(); ++i)
                args2->add(args->values[i]);
            evalPrimOp(x, args2, out);
            break;
        }
        switch (callable->objKind) {
        case TYPE :
        case RECORD :
        case VARIANT :
        case PROCEDURE :
            break;
        default :
            argumentError(0, "invalid callable");
        }
        vector<TypePtr> argsKey;
        vector<ValueTempness> argsTempness;
        for (unsigned i = 1; i < args->size(); ++i) {
            TypePtr t = valueToType(args, i);
            argsKey.push_back(t);
            argsTempness.push_back(TEMPNESS_LVALUE);
        }
        CompileContextPusher pusher(callable, argsKey);
        bool isDefined = analyzeIsDefined(callable, argsKey, argsTempness);
        ValueHolderPtr vh = boolToValueHolder(isDefined);
        evalStaticObject(vh.ptr(), out);
        break;
    }

    case PRIM_primitiveCopy : {
        ensureArity(args, 2);
        EValuePtr ev0 = args->values[0];
        EValuePtr ev1 = args->values[1];
        if (!isPrimitiveType(ev0->type))
            argumentTypeError(0, "primitive type", ev0->type);
        if (ev0->type != ev1->type)
            argumentTypeError(1, ev0->type, ev1->type);
        memcpy(ev0->addr, ev1->addr, typeSize(ev0->type));
        assert(out->size() == 0);
        break;
    }

    case PRIM_boolNot : {
        ensureArity(args, 1);
        EValuePtr ev = args->values[0];
        if (ev->type != boolType)
            argumentTypeError(0, boolType, ev->type);
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type == boolType);
        char *p = (char *)ev->addr;
        *((char *)out0->addr) = (*p == 0);
        break;
    }

    case PRIM_numericEqualsP : {
        ensureArity(args, 2);
        TypePtr t;
        EValuePtr ev0 = numericValue(args, 0, t);
        EValuePtr ev1 = numericValue(args, 1, t);
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type == boolType);
        binaryNumericOp<Op_numericEqualsP>(ev0, ev1, out0);
        break;
    }

    case PRIM_numericLesserP : {
        ensureArity(args, 2);
        TypePtr t;
        EValuePtr ev0 = numericValue(args, 0, t);
        EValuePtr ev1 = numericValue(args, 1, t);
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type == boolType);
        binaryNumericOp<Op_numericLesserP>(ev0, ev1, out0);
        break;
    }

    case PRIM_numericAdd : {
        ensureArity(args, 2);
        TypePtr t;
        EValuePtr ev0 = numericValue(args, 0, t);
        EValuePtr ev1 = numericValue(args, 1, t);
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type == t);
        binaryNumericOp<Op_numericAdd>(ev0, ev1, out0);
        break;
    }

    case PRIM_numericSubtract : {
        ensureArity(args, 2);
        TypePtr t;
        EValuePtr ev0 = numericValue(args, 0, t);
        EValuePtr ev1 = numericValue(args, 1, t);
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type == t);
        binaryNumericOp<Op_numericSubtract>(ev0, ev1, out0);
        break;
    }

    case PRIM_numericMultiply : {
        ensureArity(args, 2);
        TypePtr t;
        EValuePtr ev0 = numericValue(args, 0, t);
        EValuePtr ev1 = numericValue(args, 1, t);
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type == t);
        binaryNumericOp<Op_numericMultiply>(ev0, ev1, out0);
        break;
    }

    case PRIM_numericDivide : {
        ensureArity(args, 2);
        TypePtr t;
        EValuePtr ev0 = numericValue(args, 0, t);
        EValuePtr ev1 = numericValue(args, 1, t);
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type == t);
        binaryNumericOp<Op_numericDivide>(ev0, ev1, out0);
        break;
    }

    case PRIM_numericNegate : {
        ensureArity(args, 1);
        TypePtr t;
        EValuePtr ev = numericValue(args, 0, t);
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type == t);
        unaryNumericOp<Op_numericNegate>(ev, out0);
        break;
    }

    case PRIM_integerRemainder : {
        ensureArity(args, 2);
        IntegerTypePtr t;
        EValuePtr ev0 = integerValue(args, 0, t);
        EValuePtr ev1 = integerValue(args, 1, t);
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type.ptr() == t.ptr());
        binaryIntegerOp<Op_integerRemainder>(ev0, ev1, out0);
        break;
    }

    case PRIM_integerAddChecked : {
        ensureArity(args, 2);
        IntegerTypePtr t;
        EValuePtr ev0 = integerValue(args, 0, t);
        EValuePtr ev1 = integerValue(args, 1, t);
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type.ptr() == t.ptr());
        binaryIntegerOp<Op_integerAddChecked>(ev0, ev1, out0);
        break;
    }

    case PRIM_integerSubtractChecked : {
        ensureArity(args, 2);
        IntegerTypePtr t;
        EValuePtr ev0 = integerValue(args, 0, t);
        EValuePtr ev1 = integerValue(args, 1, t);
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type.ptr() == t.ptr());
        binaryIntegerOp<Op_integerSubtractChecked>(ev0, ev1, out0);
        break;
    }

    case PRIM_integerMultiplyChecked : {
        ensureArity(args, 2);
        IntegerTypePtr t;
        EValuePtr ev0 = integerValue(args, 0, t);
        EValuePtr ev1 = integerValue(args, 1, t);
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type.ptr() == t.ptr());
        binaryIntegerOp<Op_integerMultiplyChecked>(ev0, ev1, out0);
        break;
    }

    case PRIM_integerDivideChecked : {
        ensureArity(args, 2);
        IntegerTypePtr t;
        EValuePtr ev0 = integerValue(args, 0, t);
        EValuePtr ev1 = integerValue(args, 1, t);
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type.ptr() == t.ptr());
        binaryIntegerOp<Op_integerDivideChecked>(ev0, ev1, out0);
        break;
    }

    case PRIM_integerRemainderChecked : {
        ensureArity(args, 2);
        IntegerTypePtr t;
        EValuePtr ev0 = integerValue(args, 0, t);
        EValuePtr ev1 = integerValue(args, 1, t);
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type.ptr() == t.ptr());
        binaryIntegerOp<Op_integerRemainderChecked>(ev0, ev1, out0);
        break;
    }

    case PRIM_integerNegateChecked : {
        ensureArity(args, 1);
        IntegerTypePtr t;
        EValuePtr ev = integerValue(args, 0, t);
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type == t.ptr());
        unaryIntegerOp<Op_integerNegateChecked>(ev, out0);
        break;
    }

    case PRIM_integerShiftLeft : {
        ensureArity(args, 2);
        IntegerTypePtr t;
        EValuePtr ev0 = integerValue(args, 0, t);
        EValuePtr ev1 = integerValue(args, 1, t);
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type == t.ptr());
        binaryIntegerOp<Op_integerShiftLeft>(ev0, ev1, out0);
        break;
    }

    case PRIM_integerShiftRight : {
        ensureArity(args, 2);
        IntegerTypePtr t;
        EValuePtr ev0 = integerValue(args, 0, t);
        EValuePtr ev1 = integerValue(args, 1, t);
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type == t.ptr());
        binaryIntegerOp<Op_integerShiftRight>(ev0, ev1, out0);
        break;
    }

    case PRIM_integerBitwiseAnd : {
        ensureArity(args, 2);
        IntegerTypePtr t;
        EValuePtr ev0 = integerValue(args, 0, t);
        EValuePtr ev1 = integerValue(args, 1, t);
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type == t.ptr());
        binaryIntegerOp<Op_integerBitwiseAnd>(ev0, ev1, out0);
        break;
    }

    case PRIM_integerBitwiseOr : {
        ensureArity(args, 2);
        IntegerTypePtr t;
        EValuePtr ev0 = integerValue(args, 0, t);
        EValuePtr ev1 = integerValue(args, 1, t);
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type == t.ptr());
        binaryIntegerOp<Op_integerBitwiseOr>(ev0, ev1, out0);
        break;
    }

    case PRIM_integerBitwiseXor : {
        ensureArity(args, 2);
        IntegerTypePtr t;
        EValuePtr ev0 = integerValue(args, 0, t);
        EValuePtr ev1 = integerValue(args, 1, t);
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type == t.ptr());
        binaryIntegerOp<Op_integerBitwiseXor>(ev0, ev1, out0);
        break;
    }

    case PRIM_integerBitwiseNot : {
        ensureArity(args, 1);
        IntegerTypePtr t;
        EValuePtr ev = integerValue(args, 0, t);
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type == t.ptr());
        unaryIntegerOp<Op_integerBitwiseNot>(ev, out0);
        break;
    }

    case PRIM_numericConvert : {
        ensureArity(args, 2);
        TypePtr dest = valueToNumericType(args, 0);
        TypePtr src;
        EValuePtr ev = numericValue(args, 1, src);
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type == dest);
        op_numericConvert<false>(out0, ev);
        break;
    }

    case PRIM_integerConvertChecked : {
        ensureArity(args, 2);
        TypePtr dest = valueToIntegerType(args, 0).ptr();
        TypePtr src;
        EValuePtr ev = numericValue(args, 1, src);
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type == dest);
        op_numericConvert<true>(out0, ev);
        break;
    }

    case PRIM_integerShiftLeftChecked : {
        ensureArity(args, 2);
        IntegerTypePtr t;
        EValuePtr ev0 = integerValue(args, 0, t);
        EValuePtr ev1 = integerValue(args, 1, t);
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type == t.ptr());
        binaryIntegerOp<Op_integerShiftLeftChecked>(ev0, ev1, out0);
        break;
    }

    case PRIM_Pointer :
        error("Pointer type constructor cannot be called");

    case PRIM_addressOf : {
        ensureArity(args, 1);
        EValuePtr ev = args->values[0];
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type == pointerType(ev->type));
        *((void **)out0->addr) = (void *)ev->addr;
        break;
    }

    case PRIM_pointerDereference : {
        ensureArity(args, 1);
        PointerTypePtr t;
        EValuePtr ev = pointerValue(args, 0, t);
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type == t.ptr());
        *((void **)out0->addr) = *((void **)ev->addr);
        break;
    }

    case PRIM_pointerEqualsP : {
        ensureArity(args, 2);
        PointerTypePtr t;
        EValuePtr ev0 = pointerValue(args, 0, t);
        EValuePtr ev1 = pointerValue(args, 1, t);
        bool flag = *((void **)ev0->addr) == *((void **)ev1->addr);
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type == boolType);
        *((char *)out0->addr) = flag ? 1 : 0;
        break;
    }

    case PRIM_pointerLesserP : {
        ensureArity(args, 2);
        PointerTypePtr t;
        EValuePtr ev0 = pointerValue(args, 0, t);
        EValuePtr ev1 = pointerValue(args, 1, t);
        bool flag = *((void **)ev0->addr) < *((void **)ev1->addr);
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type == boolType);
        *((char *)out0->addr) = flag ? 1 : 0;
        break;
    }

    case PRIM_pointerOffset : {
        ensureArity(args, 2);
        PointerTypePtr t;
        EValuePtr v0 = pointerValue(args, 0, t);
        IntegerTypePtr offsetT;
        EValuePtr v1 = integerValue(args, 1, offsetT);
        ptrdiff_t offset = op_intToPtrInt(v1);
        char *ptr = *((char **)v0->addr);
        ptr += offset*typeSize(t->pointeeType);
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type == t.ptr());
        *((void **)out0->addr) = (void *)ptr;
        break;
    }

    case PRIM_pointerToInt : {
        ensureArity(args, 2);
        IntegerTypePtr dest = valueToIntegerType(args, 0);
        PointerTypePtr pt;
        EValuePtr ev = pointerValue(args, 1, pt);
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type == dest.ptr());
        void *ptr = *((void **)ev->addr);
        op_pointerToInt(out0, ptr);
        break;
    }

    case PRIM_intToPointer : {
        ensureArity(args, 2);
        TypePtr pointeeType = valueToType(args, 0);
        TypePtr dest = pointerType(pointeeType);
        IntegerTypePtr t;
        EValuePtr ev = integerValue(args, 1, t);
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type == dest);
        ptrdiff_t ptrInt = op_intToPtrInt(ev);
        *((void **)out0->addr) = (void *)ptrInt;
        break;
    }

    case PRIM_CodePointer :
        error("CodePointer type constructor cannot be called");

    case PRIM_makeCodePointer : {
        error("code pointers cannot be taken at compile time");
        break;
    }

    case PRIM_CCodePointerP : {
        ensureArity(args, 1);
        bool isCCodePointerType = false;
        ObjectPtr obj = valueToStatic(args->values[0]);
        if (obj.ptr() && (obj->objKind == TYPE)) {
            Type *t = (Type *)obj.ptr();
            if (t->typeKind == CCODE_POINTER_TYPE)
                isCCodePointerType = true;
        }
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type == boolType);
        *((char *)out0->addr) = isCCodePointerType ? 1 : 0;
        break;
    }

    case PRIM_CCodePointer :
        error("CCodePointer type constructor cannot be called");

    case PRIM_VarArgsCCodePointer :
        error("VarArgsCCodePointer type constructor cannot be called");

    case PRIM_StdCallCodePointer :
        error("StdCallCodePointer type constructor cannot be called");

    case PRIM_FastCallCodePointer :
        error("FastCallCodePointer type constructor cannot be called");

    case PRIM_ThisCallCodePointer :
        error("ThisCallCodePointer type constructor cannot be called");

    case PRIM_LLVMCodePointer :
        error("LLVMCodePointer type constructor cannot be called");

    case PRIM_makeCCodePointer : {
        error("code pointers cannot be created at compile time");
        break;
    }

    case PRIM_callCCodePointer : {
        error("invoking a code pointer not yet supported in evaluator");
        break;
    }

    case PRIM_pointerCast : {
        ensureArity(args, 2);
        TypePtr dest = valueToPointerLikeType(args, 0);
        TypePtr src;
        EValuePtr ev = pointerLikeValue(args, 1, src);
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type == dest);
        *((void **)out0->addr) = *((void **)ev->addr);
        break;
    }

    case PRIM_Array :
        error("Array type constructor cannot be called");

    case PRIM_Vec :
        error("Vec type constructor cannot be called");

    case PRIM_arrayRef : {
        ensureArity(args, 2);
        ArrayTypePtr at;
        EValuePtr earray = arrayValue(args, 0, at);
        IntegerTypePtr indexType;
        EValuePtr iv = integerValue(args, 1, indexType);
        ptrdiff_t i = op_intToPtrInt(iv);
        char *ptr = earray->addr + i*typeSize(at->elementType);
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type == pointerType(at->elementType));
        *((void **)out0->addr) = (void *)ptr;
        break;
    }

    case PRIM_arrayElements : {
        ensureArity(args, 1);
        ArrayTypePtr at;
        EValuePtr earray = arrayValue(args, 0, at);
        assert(out->size() == (unsigned)at->size);
        for (unsigned i = 0; i < (unsigned)at->size; ++i) {
            EValuePtr outi = out->values[i];
            assert(outi->type == pointerType(at->elementType));
            char *ptr = earray->addr + i*typeSize(at->elementType);
            *((void **)outi->addr) = (void *)ptr;
        }
        break;
    }

    case PRIM_Tuple :
        error("Tuple type constructor cannot be called");

    case PRIM_TupleElementCount : {
        ensureArity(args, 1);
        TupleTypePtr t = valueToTupleType(args, 0);
        ValueHolderPtr vh = sizeTToValueHolder(t->elementTypes.size());
        evalStaticObject(vh.ptr(), out);
        break;
    }

    case PRIM_tupleRef : {
        ensureArity(args, 2);
        TupleTypePtr tt;
        EValuePtr etuple = tupleValue(args, 0, tt);
        size_t i = valueToStaticSizeTOrInt(args, 1);
        if (i >= tt->elementTypes.size())
            argumentIndexRangeError(1, "tuple element index",
                                    i, tt->elementTypes.size());
        const llvm::StructLayout *layout = tupleTypeLayout(tt.ptr());
        char *ptr = etuple->addr + layout->getElementOffset(i);
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type == pointerType(tt->elementTypes[i]));
        *((void **)out0->addr) = (void *)ptr;
        break;
    }

    case PRIM_tupleElements : {
        ensureArity(args, 1);
        TupleTypePtr tt;
        EValuePtr etuple = tupleValue(args, 0, tt);
        assert(out->size() == tt->elementTypes.size());
        const llvm::StructLayout *layout = tupleTypeLayout(tt.ptr());
        for (unsigned i = 0; i < tt->elementTypes.size(); ++i) {
            EValuePtr outi = out->values[i];
            assert(outi->type == pointerType(tt->elementTypes[i]));
            char *ptr = etuple->addr + layout->getElementOffset(i);
            *((void **)outi->addr) = (void *)ptr;
        }
        break;
    }

    case PRIM_Union :
        error("Union type constructor cannot be called");

    case PRIM_UnionMemberCount : {
        ensureArity(args, 1);
        UnionTypePtr t = valueToUnionType(args, 0);
        ValueHolderPtr vh = sizeTToValueHolder(t->memberTypes.size());
        evalStaticObject(vh.ptr(), out);
        break;
    }

    case PRIM_RecordP : {
        ensureArity(args, 1);
        bool isRecordType = false;
        ObjectPtr obj = valueToStatic(args->values[0]);
        if (obj.ptr() && (obj->objKind == TYPE)) {
            Type *t = (Type *)obj.ptr();
            if (t->typeKind == RECORD_TYPE)
                isRecordType = true;
        }
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type == boolType);
        *((char *)out0->addr) = isRecordType ? 1 : 0;
        break;
    }

    case PRIM_RecordFieldCount : {
        ensureArity(args, 1);
        RecordTypePtr rt = valueToRecordType(args, 0);
        const vector<TypePtr> &fieldTypes = recordFieldTypes(rt);
        ValueHolderPtr vh = sizeTToValueHolder(fieldTypes.size());
        evalStaticObject(vh.ptr(), out);
        break;
    }

    case PRIM_RecordFieldName : {
        ensureArity(args, 2);
        RecordTypePtr rt = valueToRecordType(args, 0);
        size_t i = valueToStaticSizeTOrInt(args, 1);
        const vector<IdentifierPtr> &fieldNames = recordFieldNames(rt);
        if (i >= fieldNames.size())
            argumentIndexRangeError(1, "record field index",
                                    i, fieldNames.size());
        evalStaticObject(fieldNames[i].ptr(), out);
        break;
    }

    case PRIM_RecordWithFieldP : {
        ensureArity(args, 2);
        bool result = false;
        ObjectPtr obj = valueToStatic(args->values[0]);
        IdentifierPtr fname = valueToIdentifier(args, 1);
        if (obj.ptr() && (obj->objKind == TYPE)) {
            Type *t = (Type *)obj.ptr();
            if (t->typeKind == RECORD_TYPE) {
                RecordType *rt = (RecordType *)t;
                const map<string, size_t> &fieldIndexMap =
                    recordFieldIndexMap(rt);
                result = (fieldIndexMap.find(fname->str)
                          != fieldIndexMap.end());
            }
        }
        ValueHolderPtr vh = boolToValueHolder(result);
        evalStaticObject(vh.ptr(), out);
        break;
    }

    case PRIM_recordFieldRef : {
        ensureArity(args, 2);
        RecordTypePtr rt;
        EValuePtr erec = recordValue(args, 0, rt);
        size_t i = valueToStaticSizeTOrInt(args, 1);
        const vector<TypePtr> &fieldTypes = recordFieldTypes(rt);
        if (i >= fieldTypes.size())
            argumentIndexRangeError(1, "record field index",
                                    i, fieldTypes.size());
        const llvm::StructLayout *layout = recordTypeLayout(rt.ptr());
        char *ptr = erec->addr + layout->getElementOffset(i);
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type == pointerType(fieldTypes[i]));
        *((void **)out0->addr) = (void *)ptr;
        break;
    }

    case PRIM_recordFieldRefByName : {
        ensureArity(args, 2);
        RecordTypePtr rt;
        EValuePtr erec = recordValue(args, 0, rt);
        IdentifierPtr fname = valueToIdentifier(args, 1);
        const map<string, size_t> &fieldIndexMap = recordFieldIndexMap(rt);
        map<string,size_t>::const_iterator fi =
            fieldIndexMap.find(fname->str);
        if (fi == fieldIndexMap.end()) {
            ostringstream sout;
            sout << "field not found: " << fname->str;
            argumentError(1, sout.str());
        }
        size_t index = fi->second;
        const vector<TypePtr> &fieldTypes = recordFieldTypes(rt);
        const llvm::StructLayout *layout = recordTypeLayout(rt.ptr());
        char *ptr = erec->addr + layout->getElementOffset(index);
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type == pointerType(fieldTypes[index]));
        *((void **)out0->addr) = (void *)ptr;
        break;
    }

    case PRIM_recordFields : {
        ensureArity(args, 1);
        RecordTypePtr rt;
        EValuePtr erec = recordValue(args, 0, rt);
        const vector<TypePtr> &fieldTypes = recordFieldTypes(rt);
        assert(out->size() == fieldTypes.size());
        const llvm::StructLayout *layout = recordTypeLayout(rt.ptr());
        for (unsigned i = 0; i < fieldTypes.size(); ++i) {
            EValuePtr outi = out->values[i];
            assert(outi->type == pointerType(fieldTypes[i]));
            char *ptr = erec->addr + layout->getElementOffset(i);
            *((void **)outi->addr) = (void *)ptr;
        }
        break;
    }

    case PRIM_VariantP : {
        ensureArity(args, 1);
        bool isVariantType = false;
        ObjectPtr obj = valueToStatic(args->values[0]);
        if (obj.ptr() && (obj->objKind == TYPE)) {
            Type *t = (Type *)obj.ptr();
            if (t->typeKind == VARIANT_TYPE)
                isVariantType = true;
        }
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type == boolType);
        *((char *)out0->addr) = isVariantType ? 1 : 0;
        break;
    }

    case PRIM_VariantMemberIndex : {
        ensureArity(args, 2);
        VariantTypePtr vt = valueToVariantType(args, 0);
        TypePtr mt = valueToType(args, 1);
        const vector<TypePtr> &memberTypes = variantMemberTypes(vt);
        size_t index = (size_t)-1;
        for (unsigned i = 0; i < memberTypes.size(); ++i) {
            if (memberTypes[i] == mt) {
                index = i;
                break;
            }
        }
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type == cSizeTType);
        setSizeTEValue(out0, index);
        break;
    }

    case PRIM_VariantMemberCount : {
        ensureArity(args, 1);
        VariantTypePtr vt = valueToVariantType(args, 0);
        const vector<TypePtr> &memberTypes = variantMemberTypes(vt);
        size_t size = memberTypes.size();
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type == cSizeTType);
        setSizeTEValue(out0, size);
        break;
    }

    case PRIM_variantRepr : {
        ensureArity(args, 1);
        VariantTypePtr vt;
        EValuePtr evar = variantValue(args, 0, vt);
        TypePtr reprType = variantReprType(vt);
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type == pointerType(reprType));
        setPtrEValue(out0, (void *)evar->addr);
        break;
    }

    case PRIM_Static :
        error("Static type constructor cannot be called");

    case PRIM_ModuleName : {
        ensureArity(args, 1);
        ObjectPtr obj = valueToStatic(args, 0);
        ModulePtr m = staticModule(obj);
        if (!m)
            argumentError(0, "value has no associated module");
        ExprPtr z = new StringLiteral(m->moduleName);
        evalExpr(z, new Env(), out);
        break;
    }

    case PRIM_StaticName : {
        ensureArity(args, 1);
        ObjectPtr obj = valueToStatic(args, 0);
        ostringstream sout;
        printStaticName(sout, obj);
        ExprPtr z = new StringLiteral(sout.str());
        evalExpr(z, new Env(), out);
        break;
    }

    case PRIM_staticIntegers : {
        ensureArity(args, 1);
        ObjectPtr obj = valueToStatic(args, 0);
        if (obj->objKind != VALUE_HOLDER)
            argumentError(0, "expecting a static SizeT or Int value");
        ValueHolder *vh = (ValueHolder *)obj.ptr();
        if (vh->type == cIntType) {
            int count = *((int *)vh->buf);
            if (count < 0)
                argumentError(0, "negative values are not allowed");
            assert(out->size() == (size_t)count);
            for (int i = 0; i < count; ++i) {
                ValueHolderPtr vhi = intToValueHolder(i);
                EValuePtr outi = out->values[i];
                assert(outi->type == staticType(vhi.ptr()));
            }
        }
        else if (vh->type == cSizeTType) {
            size_t count = *((size_t *)vh->buf);
            assert(out->size() == count);
            for (size_t i = 0; i < count; ++i) {
                ValueHolderPtr vhi = sizeTToValueHolder(i);
                EValuePtr outi = out->values[i];
                assert(outi->type == staticType(vhi.ptr()));
            }
        }
        else {
            argumentError(0, "expecting a static SizeT or Int value");
        }
        break;
    }

    case PRIM_staticFieldRef : {
        ensureArity(args, 2);
        ObjectPtr moduleObj = valueToStatic(args, 0);
        if (moduleObj->objKind != MODULE_HOLDER)
            argumentError(0, "expecting a module");
        ModuleHolder *module = (ModuleHolder *)moduleObj.ptr();
        IdentifierPtr ident = valueToIdentifier(args, 1);
        ObjectPtr obj = safeLookupModuleHolder(module, ident);
        evalStaticObject(obj, out);
        break;
    }

    case PRIM_EnumP : {
        ensureArity(args, 1);
        bool isEnumType = false;
        ObjectPtr obj = valueToStatic(args->values[0]);
        if (obj.ptr() && (obj->objKind == TYPE)) {
            Type *t = (Type *)obj.ptr();
            if (t->typeKind == ENUM_TYPE)
                isEnumType = true;
        }
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type == boolType);
        *((char *)out0->addr) = isEnumType ? 1 : 0;
        break;
    }

    case PRIM_EnumMemberCount : {
        ensureArity(args, 1);
        EnumTypePtr et = valueToEnumType(args, 0);
        size_t n = et->enumeration->members.size();
        ValueHolderPtr vh = sizeTToValueHolder(n);
        evalStaticObject(vh.ptr(), out);
        break;
    }

    case PRIM_EnumMemberName : {
        ensureArity(args, 2);
        EnumTypePtr et = valueToEnumType(args, 0);
        size_t i = valueToStaticSizeTOrInt(args, 1);
        EnumerationPtr e = et->enumeration;
        if (i >= e->members.size())
            argumentIndexRangeError(1, "enum member index",
                                    i, e->members.size());
        ExprPtr str = new StringLiteral(e->members[i]->name->str);
        evalExpr(str, new Env(), out);
        break;
    }

    case PRIM_enumToInt : {
        ensureArity(args, 1);
        EnumTypePtr et;
        EValuePtr ev = enumValue(args, 0, et);
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type == cIntType);
        *((int *)out0->addr) = *((int *)ev->addr);
        break;
    }

    case PRIM_intToEnum : {
        ensureArity(args, 2);
        EnumTypePtr et = valueToEnumType(args, 0);
        IntegerTypePtr it = (IntegerType *)cIntType.ptr();
        EValuePtr ev = integerValue(args, 1, it);
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type == et.ptr());
        *((int *)out0->addr) = *((int *)ev->addr);
        break;
    }

    case PRIM_IdentifierP : {
        ensureArity(args, 1);
        bool result = false;
        EValuePtr ev0 = args->values[0];
        if (ev0->type->typeKind == STATIC_TYPE) {
            StaticType *st = (StaticType *)ev0->type.ptr();
            result = (st->obj->objKind == IDENTIFIER);
        }
        ValueHolderPtr vh = boolToValueHolder(result);
        evalStaticObject(vh.ptr(), out);
        break;
    }

    case PRIM_IdentifierSize : {
        ensureArity(args, 1);
        IdentifierPtr ident = valueToIdentifier(args, 0);
        ValueHolderPtr vh = sizeTToValueHolder(ident->str.size());
        evalStaticObject(vh.ptr(), out);
        break;
    }

    case PRIM_IdentifierConcat : {
        string result;
        for (unsigned i = 0; i < args->size(); ++i) {
            IdentifierPtr ident = valueToIdentifier(args, i);
            result.append(ident->str);
        }
        evalStaticObject(new Identifier(result), out);
        break;
    }

    case PRIM_IdentifierSlice : {
        ensureArity(args, 3);
        IdentifierPtr ident = valueToIdentifier(args, 0);
        size_t begin = valueToStaticSizeTOrInt(args, 1);
        size_t end = valueToStaticSizeTOrInt(args, 2);
        if (end > ident->str.size()) {
            argumentIndexRangeError(2, "ending index",
                                    end, ident->str.size());
        }
        if (begin > end)
            argumentIndexRangeError(1, "starting index",
                                    begin, end);
        string result = ident->str.substr(begin, end-begin);
        evalStaticObject(new Identifier(result), out);
        break;
    }

    case PRIM_IdentifierModuleName : {
        ensureArity(args, 1);
        ObjectPtr obj = valueToStatic(args, 0);
        ModulePtr m = staticModule(obj);
        if (!m)
            argumentError(0, "value has no associated module");
        evalStaticObject(new Identifier(m->moduleName), out);
        break;
    }

    case PRIM_IdentifierStaticName : {
        ensureArity(args, 1);
        ObjectPtr obj = valueToStatic(args, 0);
        ostringstream sout;
        printStaticName(sout, obj);
        evalStaticObject(new Identifier(sout.str()), out);
        break;
    }

    case PRIM_FlagP : {
        ensureArity(args, 1);
        ObjectPtr obj = unwrapStaticType(args->values[0]->type);
        if (obj != NULL && obj->objKind == IDENTIFIER) {
            Identifier *ident = (Identifier*)obj.ptr();

            map<string, string>::const_iterator flag = globalFlags.find(ident->str);
            assert(out->size() == 1);
            EValuePtr out0 = out->values[0];
            assert(out0->type == boolType);
            *((char *)out0->addr) = flag != globalFlags.end();
        } else
            argumentTypeError(0, "identifier", args->values[0]->type);
        break;
    }

    case PRIM_Flag :
        break;

    case PRIM_atomicFence:
    case PRIM_atomicRMW:
    case PRIM_atomicLoad:
    case PRIM_atomicStore:
    case PRIM_atomicCompareExchange:
        error("atomic operations not supported in evaluator");
        break;

    case PRIM_activeException:
        error("exceptions not supported in evaluator");
        break;

    default :
        assert(false);

    }
}

}
