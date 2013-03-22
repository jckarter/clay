#include "clay.hpp"
#include "evaluator.hpp"
#include "codegen.hpp"
#include "loader.hpp"
#include "operators.hpp"
#include "lambdas.hpp"
#include "analyzer.hpp"
#include "invoketables.hpp"
#include "literals.hpp"
#include "desugar.hpp"
#include "constructors.hpp"
#include "env.hpp"
#include "objects.hpp"
#include "error.hpp"
#include "checkedcast.hpp"


#pragma clang diagnostic ignored "-Wcovered-switch-default"


namespace clay {

MultiEValuePtr evalMultiArgsAsRef(ExprListPtr exprs, EnvPtr env);
MultiEValuePtr evalArgExprAsRef(ExprPtr x, EnvPtr env);

EValuePtr evalForwardOneAsRef(ExprPtr expr, EnvPtr env);
MultiEValuePtr evalForwardMultiAsRef(ExprListPtr exprs, EnvPtr env);
MultiEValuePtr evalForwardExprAsRef(ExprPtr expr, EnvPtr env);

MultiEValuePtr evalMultiAsRef(ExprListPtr exprs, EnvPtr env);
MultiEValuePtr evalExprAsRef(ExprPtr expr, EnvPtr env);

void evalOneInto(ExprPtr expr, EnvPtr env, EValuePtr out);
void evalMultiInto(ExprListPtr exprs, EnvPtr env, MultiEValuePtr out, size_t wantCount);
void evalExprInto(ExprPtr expr, EnvPtr env, MultiEValuePtr out);

void evalMulti(ExprListPtr exprs, EnvPtr env, MultiEValuePtr out, size_t wantCount);
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
                  llvm::ArrayRef<unsigned> dispatchIndices,
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
void evalCallCode(InvokeEntry* entry,
                  MultiEValuePtr args,
                  MultiEValuePtr out);
void evalCallCompiledCode(InvokeEntry* entry,
                          MultiEValuePtr args,
                          MultiEValuePtr out);
void evalCallByName(InvokeEntry* entry,
                    ExprPtr callable,
                    ExprListPtr args,
                    EnvPtr env,
                    MultiEValuePtr out);

static llvm::StringMap<const void*> staticStringTableConstants;

enum TerminationKind {
    TERMINATE_RETURN,
    TERMINATE_BREAK,
    TERMINATE_CONTINUE,
    TERMINATE_GOTO
};

struct Termination : public RefCounted {
    TerminationKind terminationKind;
    Location location;
    Termination(TerminationKind terminationKind, Location const & location)
        : terminationKind(terminationKind),
          location(location) {}
};
typedef Pointer<Termination> TerminationPtr;

struct TerminateReturn : Termination {
    TerminateReturn(Location const & location)
        : Termination(TERMINATE_RETURN, location) {}
};

struct TerminateBreak : Termination {
    TerminateBreak(Location const & location)
        : Termination(TERMINATE_BREAK, location) {}
};

struct TerminateContinue : Termination {
    TerminateContinue(Location const & location)
        : Termination(TERMINATE_CONTINUE, location) {}
};

struct TerminateGoto : Termination {
    IdentifierPtr targetLabel;
    TerminateGoto(IdentifierPtr targetLabel, Location const & location)
        : Termination(TERMINATE_GOTO, location),
          targetLabel(targetLabel) {}
};

struct LabelInfo {
    EnvPtr env;
    unsigned stackMarker;
    unsigned blockPosition;
    LabelInfo() {}
    LabelInfo(EnvPtr env, unsigned stackMarker, unsigned blockPosition)
        : env(env), stackMarker(stackMarker), blockPosition(blockPosition) {}
};

struct EReturn {
    bool byRef;
    TypePtr type;
    EValuePtr value;
    EReturn(bool byRef, TypePtr type, EValuePtr value)
        : byRef(byRef), type(type), value(value) {}
};

struct EvalContext : public RefCounted {
    vector<EReturn> returns;
    EvalContext(llvm::ArrayRef<EReturn> returns)
        : returns(returns) {}
};
typedef Pointer<EvalContext> EvalContextPtr;

TerminationPtr evalStatement(StatementPtr stmt,
                             EnvPtr env,
                             EvalContextPtr ctx);

void evalCollectLabels(llvm::ArrayRef<StatementPtr> statements,
                       unsigned startIndex,
                       EnvPtr env,
                       llvm::StringMap<LabelInfo> &labels);
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

TypePtr staticToType(MultiStaticPtr x, size_t index)
{
    TypePtr out;
    if (!staticToType(x->values[index], out))
        argumentError(index, "expecting a type");
    return out;
}



//
// evaluator wrappers
//

void evaluateReturnSpecs(llvm::ArrayRef<ReturnSpecPtr> returnSpecs,
                         ReturnSpecPtr varReturnSpec,
                         EnvPtr env,
                         vector<uint8_t> &returnIsRef,
                         vector<TypePtr> &returnTypes)
{
    returnIsRef.clear();
    returnTypes.clear();
    for (size_t i = 0; i < returnSpecs.size(); ++i) {
        ReturnSpecPtr x = returnSpecs[i];
        TypePtr t = evaluateType(x->type, env);
        bool isRef = unwrapByRef(t);
        returnIsRef.push_back(isRef);
        returnTypes.push_back(t);
    }
    if (varReturnSpec.ptr()) {
        LocationContext loc(varReturnSpec->type->location);
        MultiStaticPtr ms = evaluateExprStatic(varReturnSpec->type, env);
        for (size_t i = 0; i < ms->size(); ++i) {
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

MultiStaticPtr evaluateExprStatic(ExprPtr expr, EnvPtr env)
{
    AnalysisCachingDisabler disabler;
    MultiPValuePtr mpv = safeAnalyzeExpr(expr, env);
    vector<ValueHolderPtr> valueHolders;
    MultiEValuePtr mev = new MultiEValue();
    bool allStatic = true;
    for (unsigned i = 0; i < mpv->size(); ++i) {
        TypePtr t = mpv->values[i].type;
        if (!isStaticOrTupleOfStatics(t))
            allStatic = false;
        ValueHolderPtr vh = new ValueHolder(t);
        valueHolders.push_back(vh);
        EValuePtr ev = new EValue(t, vh->buf);
        mev->add(ev);
    }
    if (!allStatic) {
        unsigned marker = evalMarkStack();
        evalExprInto(expr, env, mev);
        evalDestroyAndPopStack(marker);
    }
    MultiStaticPtr ms = new MultiStatic();
    for (size_t i = 0; i < valueHolders.size(); ++i) {
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
    for (size_t i = 0; i < exprs->size(); ++i) {
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
        string buf;
        llvm::raw_string_ostream sout(buf);
        sout << "expecting a type but got ";
        printStaticName(sout, v);
        error(expr, sout.str());
    }
    return (Type *)v.ptr();
}

void evaluateMultiType(ExprListPtr exprs, EnvPtr env, vector<TypePtr> &out)
{
    MultiStaticPtr types = evaluateMultiStatic(exprs, env);
    for (size_t i = 0; i < types->size(); ++i) {
        if (types->values[i]->objKind != TYPE) {
            string buf;
            llvm::raw_string_ostream sout(buf);
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
    return vh->as<bool>();
}

void evaluatePredicate(llvm::ArrayRef<PatternVar> patternVars,
    ExprPtr predicate, EnvPtr env)
{
    for (size_t i = 0; i < patternVars.size(); ++i) {
        if (lookupEnv(env, patternVars[i].name) == NULL)
            error(patternVars[i].name, "unbound pattern variable");
    }
    if (predicate != NULL) {
        if (!evaluateBool(predicate, env))
            error(predicate, "definition predicate failed");
    }
}

void evaluateStaticAssert(Location const& location,
        const ExprPtr& cond, const ExprListPtr& message, EnvPtr env)
{
    bool r = evaluateBool(cond, env);

    // Note, message is evaluated even if assert does not fail.
    // It is slower, but results is better code.
    MultiStaticPtr str = evaluateMultiStatic(message, env);

    if (!r) {
        std::string s;
        llvm::raw_string_ostream ss(s);

        for (size_t i = 0; i < str->size(); ++i) {
            printStaticName(ss, str->values[i]);
        }

        ss.flush();

        if (!s.empty()) {
            error(location, "static assert failed: " + s);
        } else {
            error(location, "static assert failed");
        }

    }
}

ValueHolderPtr intToValueHolder(int x)
{
    ValueHolderPtr v = new ValueHolder(cIntType);
    v->as<int>() = x;
    return v;
}

ValueHolderPtr sizeTToValueHolder(size_t x)
{
    ValueHolderPtr v = new ValueHolder(cSizeTType);
    switch (typeSize(cSizeTType)) {
    case 4 : v->as<size32_t>() = size32_t(x); break;
    case 8 : v->as<size64_t>() = size64_t(x); break;
    default : assert(false);
    }
    return v;
}

ValueHolderPtr ptrDiffTToValueHolder(ptrdiff_t x)
{
    ValueHolderPtr v = new ValueHolder(cPtrDiffTType);
    switch (typeSize(cPtrDiffTType)) {
    case 4 : v->as<ptrdiff32_t>() = ptrdiff32_t(x); break;
    case 8 : v->as<ptrdiff64_t>() = ptrdiff64_t(x); break;
    default : assert(false);
    }
    return v;
}

ValueHolderPtr boolToValueHolder(bool x)
{
    ValueHolderPtr v = new ValueHolder(boolType);
    v->as<bool>() = x;
    return v;
}



//
// makeTupleValue
//

ObjectPtr makeTupleValue(llvm::ArrayRef<ObjectPtr> elements)
{
    bool allStatics = true;
    for (size_t i = 0; i < elements.size(); ++i) {
        switch (elements[i]->objKind) {
        case TYPE :
        case PRIM_OP :
        case PROCEDURE :
        case INTRINSIC :
        case GLOBAL_ALIAS :
        case RECORD_DECL :
        case VARIANT_DECL :
        case MODULE :
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
        for (size_t i = 0; i < elements.size(); ++i)
            elementTypes.push_back(staticType(elements[i]));
        TypePtr t = tupleType(elementTypes);
        return new ValueHolder(t);
    }
    ExprListPtr elementExprs = new ExprList();
    for (size_t i = 0; i < elements.size(); ++i)
        elementExprs->add(new ObjectExpr(elements[i]));
    ExprPtr tupleExpr = new Tuple(elementExprs);
    return evaluateOneStatic(tupleExpr, new Env());
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
    char *addr = evPtr->as<char *>();
    return new EValue(pt->pointeeType, addr);
}

static EValuePtr derefValueForPValue(EValuePtr ev, PVData const &pv)
{
    if (pv.isTemp) {
        return ev;
    } else {
        assert(ev->type == pointerType(pv.type));
        return derefValue(ev);
    }
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
    evalCallValue(staticEValue(operator_copy()),
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
    evalCallValue(staticEValue(operator_move()),
                  new MultiEValue(src),
                  new MultiEValue(dest));
}

static void evalValueForward(EValuePtr dest, EValuePtr src)
{
    if (dest->type == src->type) {
        evalValueMove(dest, src);
    }
    else {
        assert(dest->type == pointerType(src->type));
        dest->as<void *>() = (void *)src->addr;
    }
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
    evalCallValue(staticEValue(operator_assign()),
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
    MultiPValuePtr pvArgs = new MultiPValue(PVData(dest->type, false));
    pvArgs->add(PVData(src->type, true));
    evalCallValue(staticEValue(operator_assign()),
                  args,
                  pvArgs,
                  new MultiEValue());
}

bool evalToBoolFlag(EValuePtr a, bool acceptStatics)
{
    BoolKind boolKind = typeBoolKind(a->type);

    if (boolKind == BOOL_EXPR) {
        return a->as<bool>();
    } else {
        if (!acceptStatics) {
            typeError(boolType, a->type);
        }
        return boolKind == BOOL_STATIC_TRUE;
    }
    return a->as<bool>();
}



//
// evaluator temps
//

unsigned evalMarkStack()
{
    return unsigned(stackEValues.size());
}

void evalDestroyStack(unsigned marker)
{
    size_t i = stackEValues.size();
    assert(marker <= i);
    while (marker < i) {
        --i;
        evalValueDestroy(stackEValues[i]);
    }
}

void evalPopStack(unsigned marker)
{
    assert(marker <= stackEValues.size());
    while (marker < stackEValues.size()) {
        free(stackEValues.back()->addr);
        stackEValues.pop_back();
    }
}

void evalDestroyAndPopStack(unsigned marker)
{
    assert(marker <= stackEValues.size());
    while (marker < stackEValues.size()) {
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

EValuePtr evalAllocValueForPValue(PVData const &pv)
{
    if (pv.isTemp)
        return evalAllocValue(pv.type);
    else
        return evalAllocValue(pointerType(pv.type));
}


//
// evalMultiArgsAsRef, evalArgExprAsRef
//

MultiEValuePtr evalMultiArgsAsRef(ExprListPtr exprs, EnvPtr env)
{
    MultiEValuePtr out = new MultiEValue();
    for (size_t i = 0; i < exprs->size(); ++i) {
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
    for (size_t i = 0; i < exprs->size(); ++i) {
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
        PVData const &pv = mpv->values[i];
        mev->add(evalAllocValueForPValue(pv));
    }
    evalExpr(expr, env, mev);
    MultiEValuePtr out = new MultiEValue();
    for (unsigned i = 0; i < mpv->size(); ++i) {
        if (mpv->values[i].isTemp) {
            mev->values[i]->forwardedRValue = true;
            out->add(mev->values[i]);
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
    for (size_t i = 0; i < exprs->size(); ++i) {
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
        PVData const &pv = mpv->values[i];
        mev->add(evalAllocValueForPValue(pv));
    }
    evalExpr(expr, env, mev);
    MultiEValuePtr out = new MultiEValue();
    for (unsigned i = 0; i < mpv->size(); ++i) {
        if (mpv->values[i].isTemp)
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
    PVData pv = safeAnalyzeOne(expr, env);
    if (pv.isTemp) {
        evalOne(expr, env, out);
    }
    else {
        EValuePtr evPtr = evalAllocValue(pointerType(pv.type));
        evalOne(expr, env, evPtr);
        evalValueCopy(out, derefValue(evPtr));
    }
}


void evalMultiInto(ExprListPtr exprs, EnvPtr env, MultiEValuePtr out, size_t wantCount)
{
    size_t j = 0;
    ExprPtr unpackExpr = implicitUnpackExpr(wantCount, exprs);
    if (unpackExpr != NULL) {
        MultiPValuePtr mpv = safeAnalyzeExpr(unpackExpr, env);
        assert(j + mpv->size() <= out->size());
        MultiEValuePtr out2 = new MultiEValue();
        for (size_t k = 0; k < mpv->size(); ++k)
            out2->add(out->values[j + k]);
        evalExprInto(unpackExpr, env, out2);
        j += mpv->size();
    } else for (size_t i = 0; i < exprs->size(); ++i) {
        ExprPtr x = exprs->exprs[i];
        if (x->exprKind == UNPACK) {
            Unpack *y = (Unpack *)x.ptr();
            MultiPValuePtr mpv = safeAnalyzeExpr(y->expr, env);
            assert(j + mpv->size() <= out->size());
            MultiEValuePtr out2 = new MultiEValue();
            for (size_t k = 0; k < mpv->size(); ++k)
                out2->add(out->values[j + k]);
            evalExprInto(y->expr, env, out2);
            j += mpv->size();
        }
        else if (x->exprKind == PAREN) {
            MultiPValuePtr mpv = safeAnalyzeExpr(x, env);
            assert(j + mpv->size() <= out->size());
            MultiEValuePtr out2 = new MultiEValue();
            for (size_t k = 0; k < mpv->size(); ++k)
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
        PVData const &pv = mpv->values[i];
        if (pv.isTemp) {
            mev->add(out->values[i]);
        }
        else {
            EValuePtr evPtr = evalAllocValue(pointerType(pv.type));
            mev->add(evPtr);
        }
    }
    evalExpr(expr, env, mev);
    for (unsigned i = 0; i < mpv->size(); ++i) {
        if (!mpv->values[i].isTemp) {
            evalValueCopy(out->values[i], derefValue(mev->values[i]));
        }
    }
}



//
// evalMulti
//

void evalMulti(ExprListPtr exprs, EnvPtr env, MultiEValuePtr out, size_t wantCount)
{
    size_t j = 0;
    ExprPtr unpackExpr = implicitUnpackExpr(wantCount, exprs);
    if (unpackExpr != NULL) {
        MultiPValuePtr mpv = safeAnalyzeExpr(unpackExpr, env);
        assert(j + mpv->size() <= out->size());
        MultiEValuePtr out2 = new MultiEValue();
        for (size_t k = 0; k < mpv->size(); ++k)
            out2->add(out->values[j + k]);
        evalExpr(unpackExpr, env, out2);
        j += mpv->size();
    } else for (size_t i = 0; i < exprs->size(); ++i) {
        ExprPtr x = exprs->exprs[i];
        if (x->exprKind == UNPACK) {
            Unpack *y = (Unpack *)x.ptr();
            MultiPValuePtr mpv = safeAnalyzeExpr(y->expr, env);
            assert(j + mpv->size() <= out->size());
            MultiEValuePtr out2 = new MultiEValue();
            for (size_t k = 0; k < mpv->size(); ++k)
                out2->add(out->values[j + k]);
            evalExpr(y->expr, env, out2);
            j += mpv->size();
        }
        else if (x->exprKind == PAREN) {
            MultiPValuePtr mpv = safeAnalyzeExpr(x, env);
            assert(j + mpv->size() <= out->size());
            MultiEValuePtr out2 = new MultiEValue();
            for (size_t k = 0; k < mpv->size(); ++k)
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

    case STRING_LITERAL :
        break;

    case FILE_EXPR :
    case ARG_EXPR :
        break;

    case LINE_EXPR : {
        Location location = safeLookupCallByNameLocation(env);
        unsigned line, column, tabColumn;
        getLineCol(location, line, column, tabColumn);

        ValueHolderPtr vh = sizeTToValueHolder(line+1);
        evalStaticObject(vh.ptr(), out);
        break;
    }
    case COLUMN_EXPR : {
        Location location = safeLookupCallByNameLocation(env);
        unsigned line, column, tabColumn;
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
        evalCallExpr(operator_expr_tupleLiteral(),
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
        if (!x->desugared)
            desugarFieldRef(x, safeLookupModule(env));
        if (x->isDottedModuleName) {
            evalExpr(x->desugared, env, out);
            break;
        }
        PVData pv = safeAnalyzeOne(x->expr, env);
        if (pv.type->typeKind == STATIC_TYPE) {
            StaticType *st = (StaticType *)pv.type.ptr();
            if (st->obj->objKind == MODULE) {
                Module *m = (Module *)st->obj.ptr();
                ObjectPtr obj = safeLookupPublic(m, x->name);
                evalStaticObject(obj, out);
                break;
            }
        }
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

    case VARIADIC_OP : {
        VariadicOp *x = (VariadicOp *)expr.ptr();
        if (x->op == ADDRESS_OF) {
            PVData pv = safeAnalyzeOne(x->exprs->exprs.front(), env);
            if (pv.isTemp)
                error("can't take address of a temporary");
        }
        if (!x->desugared)
            x->desugared = desugarVariadicOp(x);
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
        if (evalToBoolFlag(ev1, false)) {
            EValuePtr ev2 = evalOneAsRef(x->expr2, env);
            result = evalToBoolFlag(ev2, false);
        }
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type == boolType);
        out0->as<bool>() = result;
        break;
    }

    case OR : {
        Or *x = (Or *)expr.ptr();
        EValuePtr ev1 = evalOneAsRef(x->expr1, env);
        bool result = true;
        if (!evalToBoolFlag(ev1, false)) {
            EValuePtr ev2 = evalOneAsRef(x->expr2, env);
            result = evalToBoolFlag(ev2, false);
        }
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type == boolType);
        out0->as<bool>() = result;
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
        Unpack *unpack = (Unpack *)expr.ptr();
        if (unpack->expr->exprKind != FOREIGN_EXPR)
            error("incorrect usage of unpack operator");
        evalExpr(unpack->expr, env, out);
        break;
    }

    case STATIC_EXPR : {
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
        out0->as<int>() = y->index;
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
            if (z->staticGlobal == NULL)
                codegenGVarInstance(z);
            assert(z->staticGlobal != NULL);
            assert(out->size() == 1);
            EValuePtr out0 = out->values[0];
            assert(out0->type == pointerType(z->type));
            out0->as<void *>() = (void*)z->staticGlobal->buf;
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
        for (size_t i = 0; i < y->size(); ++i)
            evalStaticObject(y->values[i], new MultiEValue(out->values[i]));
        break;
    }

    case RECORD_DECL : {
        RecordDecl *y = (RecordDecl *)x.ptr();
        ObjectPtr z;
        if (y->params.empty() && !y->varParam)
            z = recordType(y, vector<ObjectPtr>()).ptr();
        else
            z = y;
        assert(out->size() == 1);
        assert(out->values[0]->type == staticType(z));
        break;
    }

    case VARIANT_DECL : {
        VariantDecl *y = (VariantDecl *)x.ptr();
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
    case MODULE :
    case INTRINSIC :
    case IDENTIFIER : {
        assert(out->size() == 1);
        assert(out->values[0]->type == staticType(x));
        break;
    }

    case EVALUE : {
        EValue *y = (EValue *)x.ptr();
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        evalValueForward(out0, y);
        break;
    }

    case MULTI_EVALUE : {
        MultiEValue *y = (MultiEValue *)x.ptr();
        assert(out->size() == y->size());
        for (size_t i = 0; i < y->size(); ++i) {
            EValuePtr ev = y->values[i];
            EValuePtr outi = out->values[i];
            evalValueForward(outi, ev);
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
        PValue *yv = (PValue *)x.ptr();
        PVData const &y = yv->data;
        if (y.type->typeKind != STATIC_TYPE)
            invalidStaticObjectError(new PValue(y));
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        if (y.isTemp)
            assert(out0->type == y.type);
        else
            assert(out0->type == pointerType(y.type));
        break;
    }

    case MULTI_PVALUE : {
        // allow values of static type
        MultiPValue *y = (MultiPValue *)x.ptr();
        assert(out->size() == y->size());
        for (unsigned i = 0; i < y->size(); ++i) {
            PVData const &pv = y->values[i];
            if (pv.type->typeKind != STATIC_TYPE)
                argumentInvalidStaticObjectError(i, new PValue(pv));
            EValuePtr outi = out->values[i];
            if (pv.isTemp)
                assert(outi->type == pv.type);
            else
                assert(outi->type == pointerType(pv.type));
        }
        break;
    }

    case PATTERN :
    case MULTI_PATTERN :
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
    case ENUM_TYPE :
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
        PVData const &pv = mpv->values[i];
        if (pv.isTemp)
            assert(out->values[i]->type == pv.type);
        else
            assert(out->values[i]->type == pointerType(pv.type));
        if ((pv.type->typeKind != STATIC_TYPE) || !pv.isTemp) {
            allTempStatics = false;
        }
    }
    if (allTempStatics) {
        // takes care of type constructors
        return;
    }
    PVData pv = safeAnalyzeOne(indexable, env);
    if (pv.type->typeKind == STATIC_TYPE) {
        StaticType *st = (StaticType *)pv.type.ptr();
        ObjectPtr obj = st->obj;
        if (obj->objKind == GLOBAL_ALIAS) {
            GlobalAlias *x = (GlobalAlias *)obj.ptr();
            evalAliasIndexing(x, args, env, out);
            return;
        }
        if (obj->objKind == GLOBAL_VARIABLE) {
            GlobalVariable *x = (GlobalVariable *)obj.ptr();
            GVarInstancePtr y = analyzeGVarIndexing(x, args, env);
            if (y->staticGlobal == NULL)
                codegenGVarInstance(y);
            assert(y->staticGlobal != NULL);
            assert(out->size() == 1);
            EValuePtr out0 = out->values[0];
            assert(out0->type == pointerType(y->type));
            out0->as<void *>() = (void*)y->staticGlobal->buf;
            return;
        }
    }
    ExprListPtr args2 = new ExprList(indexable);
    args2->add(args);
    evalCallExpr(operator_expr_index(), args2, env, out);
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
    for (size_t i = 0; i < x->params.size(); ++i) {
        addLocal(bodyEnv, x->params[i], params->values[i]);
    }
    if (x->varParam.ptr()) {
        MultiStaticPtr varParams = new MultiStatic();
        for (size_t i = x->params.size(); i < params->size(); ++i)
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
    llvm::StringRef s = x->name->str;
    return s[s.size()-1] == '?';
}

void evalCallExpr(ExprPtr callable,
                  ExprListPtr args,
                  EnvPtr env,
                  MultiEValuePtr out)
{
    PVData pv = safeAnalyzeOne(callable, env);

    switch (pv.type->typeKind) {
    case CODE_POINTER_TYPE : {
        EValuePtr ev = evalOneAsRef(callable, env);
        MultiEValuePtr mev = evalMultiAsRef(args, env);
        evalCallPointer(ev, mev, out);
        return;
    }
    default:
        break;
    }

    if (pv.type->typeKind != STATIC_TYPE) {
        ExprListPtr args2 = new ExprList(callable);
        args2->add(args);
        evalCallExpr(operator_expr_call(), args2, env, out);
        return;
    }

    StaticType *st = (StaticType *)pv.type.ptr();
    ObjectPtr obj = st->obj;

    switch (obj->objKind) {

    case TYPE :
    case RECORD_DECL :
    case VARIANT_DECL :
    case PROCEDURE :
    case GLOBAL_ALIAS :
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
        InvokeEntry* entry = safeAnalyzeCallable(obj, argsKey, argsTempness);
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
                for (size_t i = 0; i < mev->size(); ++i)
                    args2.push_back(evalueToStatic(mev->values[i]));
                if (!x->evaluatorCache)
                    x->evaluatorCache = new ObjectTable();
                Pointer<RefCounted> result = x->evaluatorCache->lookup(args2);
                if (!result) {
                    evalCallCode(entry, mev, out);
                    MultiStaticPtr ms = new MultiStatic();
                    for (size_t i = 0; i < out->size(); ++i)
                        ms->add(evalueToStatic(out->values[i]));
                    x->evaluatorCache->lookup(args2) = ms.ptr();
                }
                else {
                    evalStaticObject(checked_cast<MultiStatic*>(result.ptr()), out);
                }
            }
            else {
                evalCallCode(entry, mev, out);
            }
        }
        break;
    }
            
    case INTRINSIC : {
        error(callable, "compile-time calls to LLVM intrinsics are not supported");
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

unsigned evalDispatchTag(EValuePtr ev)
{
    unsigned tag;
    EValuePtr etag = new EValue(cIntType, (char *)&tag);
    evalCallValue(staticEValue(operator_dispatchTag()),
                  new MultiEValue(ev),
                  new MultiEValue(etag));
    if (!tag)
        error("dispatch tag uninitialized");
    return tag;
}

EValuePtr evalDispatchIndex(EValuePtr ev, PVData const &pvOut, unsigned tag)
{
    MultiEValuePtr args = new MultiEValue();
    args->add(ev);
    ValueHolderPtr vh = intToValueHolder((int)tag);
    args->add(staticEValue(vh.ptr()));

    EValuePtr evOut = evalAllocValueForPValue(pvOut);
    MultiEValuePtr out = new MultiEValue(evOut);
    evalCallValue(staticEValue(operator_dispatchIndex()), args, out);
    return derefValueForPValue(evOut, pvOut);
}

void evalDispatch(ObjectPtr obj,
                  MultiEValuePtr args,
                  MultiPValuePtr pvArgs,
                  llvm::ArrayRef<unsigned> dispatchIndices,
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
    PVData const &pvDispatch = pvArgs->values[index];

    unsigned tag = evalDispatchTag(evDispatch);
    unsigned tagCount = dispatchTagCount(evDispatch->type);
    if (tag >= tagCount)
        argumentError(index, "invalid variant value");

    MultiEValuePtr args2 = new MultiEValue();
    MultiPValuePtr pvArgs2 = new MultiPValue();
    pvArgs2->add(pvPrefix);
    PVData pvDispatch2 = analyzeDispatchIndex(pvDispatch, tag);
    pvArgs2->add(pvDispatch2);
    pvArgs2->add(pvSuffix);

    args2->add(prefix);
    args2->add(evalDispatchIndex(evDispatch, pvDispatch2, tag));
    args2->add(suffix);

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
    for (size_t i = 0; i < args->size(); ++i)
        pvArgs->add(PVData(args->values[i]->type, false));
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
    default:
        break;
    }

    if (callable->type->typeKind != STATIC_TYPE) {
        MultiEValuePtr args2 = new MultiEValue(callable);
        args2->add(args);
        MultiPValuePtr pvArgs2 =
            new MultiPValue(PVData(callable->type, false));
        pvArgs2->add(pvArgs);
        evalCallValue(staticEValue(operator_call()), args2, pvArgs2, out);
        return;
    }

    StaticType *st = (StaticType *)callable->type.ptr();
    ObjectPtr obj = st->obj;

    switch (obj->objKind) {

    case TYPE :
    case RECORD_DECL :
    case VARIANT_DECL :
    case PROCEDURE :
    case GLOBAL_ALIAS :
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
        InvokeEntry* entry = safeAnalyzeCallable(obj, argsKey, argsTempness);
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
    
    case INTRINSIC : {
        error("compile-time calls to LLVM intrinsics are not supported");
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

void evalCallCode(InvokeEntry* entry,
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
    
    unsigned k = 0;
    for (; k < entry->varArgPosition; ++k) {
        EValuePtr ev = args->values[k];
        EValuePtr earg = new EValue(ev->type, ev->addr);
        earg->forwardedRValue = entry->forwardedRValueFlags[k];
        addLocal(env, entry->fixedArgNames[k], earg.ptr());
    }
    if (entry->varArgName.ptr()) {
        MultiEValuePtr varArgs = new MultiEValue();
        unsigned j = 0;
        for (; j < entry->varArgTypes.size(); ++j) {
            EValuePtr ev = args->values[k + j];
            EValuePtr earg = new EValue(ev->type, ev->addr);
            earg->forwardedRValue = entry->forwardedRValueFlags[k+j];
            varArgs->add(earg);
        }
        addLocal(env, entry->varArgName, varArgs.ptr());
        for (; k < entry->fixedArgNames.size(); ++k) {
            EValuePtr ev = args->values[k+j];
            EValuePtr earg = new EValue(ev->type, ev->addr);
            earg->forwardedRValue = entry->forwardedRValueFlags[k+j];
            addLocal(env, entry->fixedArgNames[k], earg.ptr());
        }
    }
    
    assert(out->size() == entry->returnTypes.size());
    vector<EReturn> returns;
    for (size_t i = 0; i < entry->returnTypes.size(); ++i) {
        bool isRef = entry->returnIsRef[i];
        TypePtr rt = entry->returnTypes[i];
        EValuePtr ev = out->values[i];
        returns.push_back(EReturn(isRef, rt, ev));
    }

    if (entry->code->hasReturnSpecs()) {
        llvm::ArrayRef<ReturnSpecPtr> returnSpecs = entry->code->returnSpecs;
        size_t i = 0;
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

void evalCallCompiledCode(InvokeEntry* entry,
                          MultiEValuePtr args,
                          MultiEValuePtr out)
{
    if (!entry->llvmFunc)
        codegenCodeBody(entry);
    assert(entry->llvmFunc);

    vector<llvm::GenericValue> gvArgs;

    for (size_t i = 0; i < args->size(); ++i) {
        EValuePtr earg = args->values[i];
        assert(earg->type == entry->argsKey[i]);
        gvArgs.push_back(llvm::GenericValue(earg->addr));
    }

    assert(out->size() == entry->returnTypes.size());
    for (size_t i = 0; i < entry->returnTypes.size(); ++i) {
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

void evalCallByName(InvokeEntry* entry,
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
 
    unsigned i = 0, j = 0;
    for (; i < entry->varArgPosition; ++i) {
        ExprPtr expr = foreignExpr(env, args->exprs[i]);
        addLocal(bodyEnv, entry->fixedArgNames[i], expr.ptr());
    }
    if (entry->varArgName.ptr()) {
        ExprListPtr varArgs = new ExprList();
        for (; j < args->size() - entry->fixedArgNames.size(); ++j) {
            ExprPtr expr = foreignExpr(env, args->exprs[i+j]);
            varArgs->add(expr);
        }
        addLocal(bodyEnv, entry->varArgName, varArgs.ptr());
        for (; i < entry->fixedArgNames.size(); ++i) {
            ExprPtr expr = foreignExpr(env, args->exprs[i+j]);
            addLocal(bodyEnv, entry->fixedArgNames[i], expr.ptr());
        }
    }

    MultiPValuePtr mpv = safeAnalyzeCallByName(entry, callable, args, env);
    assert(mpv->size() == out->size());

    vector<EReturn> returns;
    for (unsigned i = 0; i < mpv->size(); ++i) {
        PVData const &pv = mpv->values[i];
        EValuePtr ev = out->values[i];
        if (pv.isTemp)
            assert(ev->type == pv.type);
        else
            assert(ev->type == pointerType(pv.type));
        returns.push_back(EReturn(!pv.isTemp, pv.type, ev));
    }

    if (entry->code->hasReturnSpecs()) {
        llvm::ArrayRef<ReturnSpecPtr> returnSpecs = entry->code->returnSpecs;
        size_t i = 0;
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

static EnvPtr evalStatementExpressionStatements(llvm::ArrayRef<StatementPtr> stmts,
    EnvPtr env,
    EvalContextPtr ctx)
{
    EnvPtr env2 = env;

    for (StatementPtr const *i = stmts.begin(), *end = stmts.end();
         i != end;
         ++i)
    {
        switch ((*i)->stmtKind) {
        case BINDING: {
            env2 = evalBinding((Binding*)i->ptr(), env2);
            break;
        }

        case ASSIGNMENT:
        case VARIADIC_ASSIGNMENT:
        case INIT_ASSIGNMENT:
        case EXPR_STATEMENT: {
            TerminationPtr term = evalStatement(*i, env2, ctx);
            assert(term == NULL);
            break;
        }

        default:
            assert(false);
            return NULL;
        }
    }
    return env2;
}


TerminationPtr evalStatement(StatementPtr stmt,
                             EnvPtr env,
                             EvalContextPtr ctx)
{
    LocationContext loc(stmt->location);

    switch (stmt->stmtKind) {

    case BLOCK : {
        Block *x = (Block *)stmt.ptr();
        unsigned blockMarker = evalMarkStack();
        llvm::StringMap<LabelInfo> labels;
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
            arityMismatchError(mpvLeft->size(), mpvRight->size(), /*hasVarArg=*/false);
        for (unsigned i = 0; i < mpvLeft->size(); ++i) {
            if (mpvLeft->values[i].isTemp)
                argumentError(i, "cannot assign to a temporary");
        }
        unsigned marker = evalMarkStack();
        if (mpvLeft->size() == 1) {
            ExprListPtr args = new ExprList();
            args->add(x->left);
            args->add(x->right);
            ExprPtr assignCall = new Call(operator_expr_assign(), args);
            evalExprAsRef(assignCall, env);
        }
        else {
            MultiEValuePtr mevRight = new MultiEValue();
            for (unsigned i = 0; i < mpvRight->size(); ++i) {
                PVData const &pv = mpvRight->values[i];
                EValuePtr ev = evalAllocValue(pv.type);
                mevRight->add(ev);
            }
            evalMultiInto(x->right, env, mevRight, mpvLeft->size());
            MultiEValuePtr mevLeft = evalMultiAsRef(x->left, env);
            assert(mevLeft->size() == mevRight->size());
            for (size_t i = 0; i < mevLeft->size(); ++i) {
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
            arityMismatchError(mpvLeft->size(), mpvRight->size(), /*hasVarArg=*/false);
        for (unsigned i = 0; i < mpvLeft->size(); ++i) {
            if (mpvLeft->values[i].isTemp)
                argumentError(i, "cannot assign to a temporary");
            if (mpvLeft->values[i].type != mpvRight->values[i].type) {
                argumentTypeError(i,
                                  mpvLeft->values[i].type,
                                  mpvRight->values[i].type);
            }
        }
        unsigned marker = evalMarkStack();
        MultiEValuePtr mevLeft = evalMultiAsRef(x->left, env);
        evalMultiInto(x->right, env, mevLeft, mpvLeft->size());
        evalDestroyAndPopStack(marker);
        return NULL;
    }

    case VARIADIC_ASSIGNMENT : {
        VariadicAssignment *x = (VariadicAssignment *)stmt.ptr();
        PVData pvLeft = safeAnalyzeOne(x->exprs->exprs[1], env);
        if (pvLeft.isTemp)
            error(x->exprs->exprs[1], "cannot assign to a temporary");
        CallPtr call;
        if (x->op == PREFIX_OP)
            call = new Call(operator_expr_prefixUpdateAssign(), new ExprList());
        else
            call = new Call(operator_expr_updateAssign(), new ExprList());        
        call->parenArgs->add(x->exprs);
        return evalStatement(new ExprStatement(call.ptr()), env, ctx);
    }

    case GOTO : {
        Goto *x = (Goto *)stmt.ptr();
        return new TerminateGoto(x->labelName, x->location);
    }

    case RETURN : {
        Return *x = (Return *)stmt.ptr();
        size_t wantCount = x->isReturnSpecs ? 1 : 0;
        MultiPValuePtr mpv = safeAnalyzeMulti(x->values, env, wantCount);
        MultiEValuePtr mev = new MultiEValue();
        ensureArity(mpv, ctx->returns.size());
        for (unsigned i = 0; i < mpv->size(); ++i) {
            PVData const &pv = mpv->values[i];
            bool byRef = returnKindToByRef(x->returnKind, pv);
            EReturn &y = ctx->returns[i];
            if (y.type != pv.type)
                argumentTypeError(i, y.type, pv.type);
            if (byRef != y.byRef)
                argumentError(i, "mismatching by-ref and by-value returns");
            if (byRef && pv.isTemp)
                argumentError(i, "cannot return a temporary by reference");
            mev->add(y.value);
        }
        unsigned marker = evalMarkStack();
        switch (x->returnKind) {
        case RETURN_VALUE :
            evalMultiInto(x->values, env, mev, wantCount);
            break;
        case RETURN_REF : {
            MultiEValuePtr mevRef = evalMultiAsRef(x->values, env);
            assert(mev->size() == mevRef->size());
            for (size_t i = 0; i < mev->size(); ++i) {
                EValuePtr evPtr = mev->values[i];
                EValuePtr evRef = mevRef->values[i];
                evPtr->as<void *>() = (void *)evRef->addr;
            }
            break;
        }
        case RETURN_FORWARD :
            evalMulti(x->values, env, mev, wantCount);
            break;
        default :
            assert(false);
        }
        evalDestroyAndPopStack(marker);
        return new TerminateReturn(x->location);
    }

    case IF : {
        If *x = (If *)stmt.ptr();

        unsigned scopeMarker = evalMarkStack();

        EnvPtr env2 = evalStatementExpressionStatements(x->conditionStatements, env, ctx);

        unsigned tempMarker = evalMarkStack();
        EValuePtr ev = evalOneAsRef(x->condition, env2);
        bool flag = evalToBoolFlag(ev, true);
        evalDestroyAndPopStack(tempMarker);
        if (flag)
            return evalStatement(x->thenPart, env2, ctx);
        if (x->elsePart.ptr())
            return evalStatement(x->elsePart, env2, ctx);

        evalDestroyAndPopStack(scopeMarker);

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
        llvm::ArrayRef<StatementPtr> evaled = desugarEvalStatement(eval, env);
        TerminationPtr termination;
        for (size_t i = 0; i < evaled.size(); ++i) {
            termination = evalStatement(evaled[i], env, ctx);
            if (termination != NULL)
                return termination;
        }
        return NULL;
    }

    case EXPR_STATEMENT : {
        ExprStatement *x = (ExprStatement *)stmt.ptr();
        unsigned marker = evalMarkStack();
        evalExprAsRef(x->expr, env);
        evalDestroyAndPopStack(marker);
        return NULL;
    }

    case WHILE : {
        While *x = (While *)stmt.ptr();

        unsigned scopeMarker = evalMarkStack();

        while (true) {
            EnvPtr env2 = evalStatementExpressionStatements(x->conditionStatements, env, ctx);

            unsigned tempMarker = evalMarkStack();
            EValuePtr ev = evalOneAsRef(x->condition, env2);
            bool flag = evalToBoolFlag(ev, false);
            evalDestroyAndPopStack(tempMarker);

            if (!flag) break;
            TerminationPtr term = evalStatement(x->body, env2, ctx);
            if (term.ptr()) {
                if (term->terminationKind == TERMINATE_BREAK)
                    break;
                if (term->terminationKind == TERMINATE_CONTINUE)
                    goto whileContinue;
                return term;
            }
whileContinue:
            evalDestroyStack(scopeMarker);
        }
        evalDestroyAndPopStack(scopeMarker);

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
        for (size_t i = 0; i < mev->size(); ++i) {
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

    case STATIC_ASSERT_STATEMENT : {
        StaticAssertStatement *x = (StaticAssertStatement *)stmt.ptr();
        evaluateStaticAssert(x->location, x->cond, x->message, env);
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

void evalCollectLabels(llvm::ArrayRef<StatementPtr> statements,
                       unsigned startIndex,
                       EnvPtr env,
                       llvm::StringMap<LabelInfo> &labels)
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
        MultiPValuePtr mpv = safeAnalyzeMulti(x->values, env, x->args.size());
        if(x->hasVarArg){
            if (mpv->values.size() < x->args.size()-1)
                arityMismatchError(x->args.size()-1, mpv->values.size(), /*hasVarArg=*/true);
        } else
            if (mpv->values.size() != x->args.size())
                arityMismatchError(x->args.size(), mpv->values.size(), /*hasVarArg=*/false);
        MultiEValuePtr mev = new MultiEValue();
        for (unsigned i = 0; i < x->args.size(); ++i) {
            EValuePtr ev = evalAllocValue(mpv->values[i].type);
            mev->add(ev);
        }
        unsigned marker = evalMarkStack();
        evalMultiInto(x->values, env, mev, x->args.size());
        evalDestroyAndPopStack(marker);
        EnvPtr env2 = new Env(env);
        for (size_t i = 0; i < x->patternVars.size(); ++i)
            addLocal(env2, x->patternVars[i].name, x->patternTypes[i]);
        size_t varArgSize = mev->values.size()-x->args.size()+1;
        for (size_t i = 0, j = 0; i < x->args.size(); ++i) {
            if (x->args[i]->varArg) {
                MultiEValuePtr varArgs = new MultiEValue();
                for (; j < varArgSize; ++j) {
                    EValuePtr ev = mev->values[i+j];
                    varArgs->add(ev);
                }
                --j;
                addLocal(env2, x->args[i]->name, varArgs.ptr());  
            } else
                addLocal(env2, x->args[i]->name, mev->values[i+j].ptr());
        }
        return env2;
    }

    case REF : {
        MultiPValuePtr mpv = safeAnalyzeMulti(x->values, env, x->args.size());
        if(x->hasVarArg){
            if (mpv->values.size() < x->args.size()-1)
                arityMismatchError(x->args.size()-1, mpv->values.size(), /*hasVarArg=*/true);
        } else
            if (mpv->values.size() != x->args.size())
                arityMismatchError(x->args.size(), mpv->values.size(), /*hasVarArg=*/false);
        MultiEValuePtr mev = new MultiEValue();
        for (unsigned i = 0; i < x->args.size(); ++i) {
            PVData const &pv = mpv->values[i];
            if (pv.isTemp)
                argumentError(i, "ref can only bind to an rvalue");
            EValuePtr evPtr = evalAllocValue(pointerType(pv.type));
            mev->add(evPtr);
        }
        unsigned marker = evalMarkStack();
        evalMulti(x->values, env, mev, x->args.size());
        evalDestroyAndPopStack(marker);
        EnvPtr env2 = new Env(env);
        for (size_t i = 0; i < x->patternVars.size(); ++i)
            addLocal(env2, x->patternVars[i].name, x->patternTypes[i]);
        size_t varArgSize = mev->values.size()-x->args.size()+1;
        for (size_t i = 0, j = 0; i < x->args.size(); ++i) {
            if (x->args[i]->varArg) {
                MultiEValuePtr varArgs = new MultiEValue();
                for (; j < varArgSize; ++j) {
                    EValuePtr ev = derefValue(mev->values[i+j]);
                    varArgs->add(ev);
                }
                --j;
                addLocal(env2, x->args[i]->name, varArgs.ptr());  
            } else {        
                EValuePtr evPtr = mev->values[i+j];
                addLocal(env2, x->args[i]->name, derefValue(evPtr).ptr());
            }
        }
        
        return env2;
    }

    case FORWARD : {
        MultiPValuePtr mpv = safeAnalyzeMulti(x->values, env, x->args.size());
        if(x->hasVarArg){
            if (mpv->values.size() < x->args.size()-1)
                arityMismatchError(x->args.size()-1, mpv->values.size(), /*hasVarArg=*/true);
        } else
            if (mpv->values.size() != x->args.size())
                arityMismatchError(x->args.size(), mpv->values.size(), /*hasVarArg=*/false);
        MultiEValuePtr mev = new MultiEValue();
        for (unsigned i = 0; i < x->args.size(); ++i) {
            PVData const &pv = mpv->values[i];
            mev->add(evalAllocValueForPValue(pv));
        }
        unsigned marker = evalMarkStack();
        evalMulti(x->values, env, mev, x->args.size());
        evalDestroyAndPopStack(marker);
        EnvPtr env2 = new Env(env);
        for (size_t i = 0; i < x->patternVars.size(); ++i)
            addLocal(env2, x->patternVars[i].name, x->patternTypes[i]);
        size_t varArgSize = mev->values.size()-x->args.size()+1;
        for (unsigned i = 0, j = 0; i < x->args.size(); ++i) {
            if (x->args[i]->varArg) {
                MultiEValuePtr varArgs = new MultiEValue();
                for (; j < varArgSize; ++j) {
                    EValuePtr rev, ev;
                    rev = mev->values[i+j];
                    if (mpv->values[i+j].isTemp) {
                        ev = rev;
                    }
                    else {
                        ev = derefValue(rev);
                    }
                    varArgs->add(ev);
                }
                --j;
                addLocal(env2, x->args[i]->name, varArgs.ptr());  
            } else {
                if (mpv->values[i+j].isTemp) {
                    EValuePtr ev = mev->values[i+j];
                    addLocal(env2, x->args[i]->name, ev.ptr());
                }
                else {
                    EValuePtr evPtr = mev->values[i+j];
                    addLocal(env2, x->args[i]->name, derefValue(evPtr).ptr());
                }
            }
        }
        
        return env2;
    }

    case ALIAS : {
        ensureArity(x->args, 1);
        ensureArity(x->values->exprs, 1);
        EnvPtr env2 = new Env(env);
        ExprPtr y = foreignExpr(env, x->values->exprs[0]);
        addLocal(env2, x->args[0]->name, y.ptr());
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

size_t valueHolderToSizeT(ValueHolderPtr vh)
{
    switch (typeSize(cSizeTType)) {
    case 4 : return vh->as<size32_t>();
    case 8 : return (size_t)vh->as<size64_t>();
    default : llvm_unreachable("unexpected pointer size");
    }
}

static size_t valueToStaticSizeTOrInt(MultiEValuePtr args, unsigned index)
{
    ObjectPtr obj = valueToStatic(args->values[index]);
    if (!obj || (obj->objKind != VALUE_HOLDER))
        argumentError(index, "expecting a static SizeT or Int value");
    ValueHolder *vh = (ValueHolder *)obj.ptr();
    if (vh->type == cSizeTType) {
        return valueHolderToSizeT(vh);
    }
    else if (vh->type == cIntType) {
        return size_t(vh->as<int>());
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

static EValuePtr floatValue(MultiEValuePtr args, unsigned index,
                            FloatTypePtr &type)
{
    EValuePtr ev = args->values[index];
    if (type.ptr()) {
        if (ev->type.ptr() != type.ptr())
            argumentTypeError(index, type.ptr(), ev->type);
    }
    else {
        switch (ev->type->typeKind) {
        case FLOAT_TYPE :
            break;
        default :
            argumentTypeError(index, "float type", ev->type);
        }
        type = (FloatType*)ev->type.ptr();
    }
    return ev;
}

static EValuePtr integerOrPointerLikeValue(MultiEValuePtr args, unsigned index,
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
        case POINTER_TYPE :
        case CODE_POINTER_TYPE :
        case CCODE_POINTER_TYPE :
            break;
        default :
            argumentTypeError(index, "integer, pointer, or code pointer type", ev->type);
        }
        type = ev->type;
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

    case POINTER_TYPE :
    case CODE_POINTER_TYPE :
    case CCODE_POINTER_TYPE : {
        switch (typeSize(a->type)) {
        case 2 : T<unsigned short>().eval(a, b, out); break;
        case 4 : T<size32_t>().eval(a, b, out); break;
        case 8 : T<size64_t>().eval(a, b, out); break;
        default : error("unsupported pointer size");
        }
        break;
    }

    default :
        assert(false);
    }
}

template<typename T>
static void overflowError(const char *op, T a, T b) {
    string buf;
    llvm::raw_string_ostream sout(buf);
    sout << "integer overflow: " << a << " " << op << " " << b;
    error(sout.str());
}

template<typename T>
static void invalidShiftError(const char *op, T a, T b) {
    string buf;
    llvm::raw_string_ostream sout(buf);
    sout << "invalid shift: " << a << " " << op << " " << b;
    error(sout.str());
}

template<typename T>
static void overflowError(const char *op, T a) {
    string buf;
    llvm::raw_string_ostream sout(buf);
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
        perform(a->as<T>(), b->as<T>(), out->addr);
    }
    virtual void perform(T &a, T &b, void *out) = 0;
};

#define BINARY_OP(name, type, expr) \
    template <typename T>                               \
    class name : public BinaryOpHelper<T> {             \
    public :                                            \
        virtual void perform(T &a, T &b, void *out) {   \
            *((type *)out) = (expr);                    \
        }                                               \
    }

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wtautological-compare"

BINARY_OP(Op_orderedEqualsP, bool, a == b);
BINARY_OP(Op_orderedLesserP, bool, a < b);
BINARY_OP(Op_orderedLesserEqualsP, bool, a <= b);
BINARY_OP(Op_orderedGreaterP, bool, a > b);
BINARY_OP(Op_orderedGreaterEqualsP, bool, a >= b);
BINARY_OP(Op_orderedNotEqualsP, bool, a != b && a == a && b == b);
BINARY_OP(Op_orderedP, bool, a == a && b == b);
BINARY_OP(Op_unorderedEqualsP, bool, a == b || a != a || b != b);
BINARY_OP(Op_unorderedLesserP, bool, !(a >= b));
BINARY_OP(Op_unorderedLesserEqualsP, bool, !(a > b));
BINARY_OP(Op_unorderedGreaterP, bool, !(a <= b));
BINARY_OP(Op_unorderedGreaterEqualsP, bool, !(a < b));
BINARY_OP(Op_unorderedNotEqualsP, bool, a != b);
BINARY_OP(Op_unorderedP, bool, a != a || b != b);
BINARY_OP(Op_numericAdd, T, a + b);
BINARY_OP(Op_numericSubtract, T, a - b);
BINARY_OP(Op_numericMultiply, T, a * b);
BINARY_OP(Op_floatDivide, T, a / b);
BINARY_OP(Op_integerQuotient, T, a / b);
BINARY_OP(Op_integerRemainder, T, a % b);
BINARY_OP(Op_integerShiftLeft, T, a << b);
BINARY_OP(Op_integerShiftRight, T, a >> b);
BINARY_OP(Op_integerBitwiseAnd, T, a & b);
BINARY_OP(Op_integerBitwiseOr, T, a | b);
BINARY_OP(Op_integerBitwiseXor, T, a ^ b);

#pragma clang diagnostic pop

#undef BINARY_OP


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
        perform(a->as<T>(), out->addr);
    }
    virtual void perform(T &a, void *out) = 0;
};

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4146)
#endif

#define UNARY_OP(name, type, expr) \
    template <typename T>                       \
    class name : public UnaryOpHelper<T> {      \
    public :                                    \
        virtual void perform(T &a, void *out) { \
            *((type *)out) = (expr);            \
        }                                       \
    };

UNARY_OP(Op_numericNegate, T, -a)
UNARY_OP(Op_integerBitwiseNot, T, ~a)

#undef UNARY_OP

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

#if defined(__clang__)
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wtautological-compare"
#elif defined(__GNUC__)
#  if __GNUC__ < 4 || __GNUC__ == 4 && __GNUC_MINOR__ < 6
#    warning "--- The following warnings are expected ---"
#  else
#    pragma GCC diagnostic push
#    pragma GCC diagnostic ignored "-Wtype-limits"
#  endif
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
            if (b > T(sizeof(T)*8 - 1) || (testa >> (sizeof(T)*8 - (size_t)b - 1)) != 0)
                overflowError("bitshl", a, b);
        } else {
            // unsigned
            if (b > T(sizeof(T)*8) || (a >> (sizeof(T)*8 - (size_t)b)) != 0)
                overflowError("bitshl", a, b);
        }
        *((T *)out) = a << b;
    }
};

#if defined(__clang__)
#  pragma clang diagnostic pop
#elif defined(__GNUC__)
#  if __GNUC__ > 4 || __GNUC__ == 4 && __GNUC_MINOR__ >= 6
#    pragma GCC diagnostic pop
#  endif
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
        if (bigout < std::numeric_limits<T>::min() || std::numeric_limits<T>::max() < bigout)
            overflowError("*", a, b);
        *((T *)out) = a * b;
    }
};

template <typename T>
class Op_integerQuotientChecked : public BinaryOpHelper<T> {
public :
    virtual void perform(T &a, T &b, void *out) {
        if (std::numeric_limits<T>::min() != 0
            && a == std::numeric_limits<T>::min()
            && b == (-1))
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
        SRC value = src->as<SRC>();
        dest->as<DEST>() = DEST(value);
    }
};

template <typename DEST, typename SRC>
struct op_numericConvert3<DEST, SRC, true> {
    static void perform(EValuePtr dest, EValuePtr src)
    {
        SRC value = src->as<SRC>();
        // ???
        dest->as<DEST>() = DEST(value);
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

static ptrdiff_t op_intToPtrInt(EValuePtr a)
{
    assert(a->type->typeKind == INTEGER_TYPE);
    IntegerType *t = (IntegerType *)a->type.ptr();
    if (t->isSigned) {
        switch (t->bits) {
        case 8 : return ptrdiff_t(a->as<char>());
        case 16 : return ptrdiff_t(a->as<short>());
        case 32 : return ptrdiff_t(a->as<int>());
        case 64 : return ptrdiff_t(a->as<long long>());
        case 128 : return ptrdiff_t(a->as<clay_int128>());
        default : assert(false);
        }
    }
    else {
        switch (t->bits) {
        case 8 : return ptrdiff_t(a->as<unsigned char>());
        case 16 : return ptrdiff_t(a->as<unsigned short>());
        case 32 : return ptrdiff_t(a->as<unsigned int>());
        case 64 : return ptrdiff_t(a->as<unsigned long long>());
        case 128 : return ptrdiff_t(a->as<clay_uint128>());
        default : assert(false);
        }
    }
    return 0;
}


//
// op_intToSizeT
//

static size_t op_intToSizeT(EValuePtr a)
{
    assert(a->type->typeKind == INTEGER_TYPE);
    IntegerType *t = (IntegerType *)a->type.ptr();
    if (t->isSigned) {
        switch (t->bits) {
        case 8 : return size_t(a->as<char>());
        case 16 : return size_t(a->as<short>());
        case 32 : return size_t(a->as<int>());
        case 64 : return size_t(a->as<long long>());
        case 128 : return size_t(a->as<clay_int128>());
        default : assert(false);
        }
    }
    else {
        switch (t->bits) {
        case 8 : return size_t(a->as<unsigned char>());
        case 16 : return size_t(a->as<unsigned short>());
        case 32 : return size_t(a->as<unsigned int>());
        case 64 : return size_t(a->as<unsigned long long>());
        case 128 : return size_t(a->as<clay_uint128>());
        default : assert(false);
        }
    }
    return 0;
}




//
// op_pointerToInt
//

static void op_pointerToInt(EValuePtr dest, EValuePtr ev)
{
    assert(dest->type->typeKind == INTEGER_TYPE);
    IntegerType *t = (IntegerType *)dest->type.ptr();
    if (t->isSigned) {
        switch (t->bits) {
        case 8 : dest->as<char>() = ev->as<char>(); break;
        case 16 : dest->as<short>() = ev->as<short>(); break;
        case 32 : dest->as<int>() = ev->as<int>(); break;
        case 64 : dest->as<long long>() = ev->as<long long>(); break;
        case 128 : dest->as<clay_int128>() = ev->as<clay_int128>(); break;
        default : assert(false);
        }
    }
    else {
        switch (t->bits) {
        case 8 : dest->as<unsigned char>() = ev->as<unsigned char>(); break;
        case 16 : dest->as<unsigned short>() = ev->as<unsigned short>(); break;
        case 32 : dest->as<unsigned int>() = ev->as<unsigned int>(); break;
        case 64 : dest->as<unsigned long long>() = ev->as<unsigned long long>(); break;
        case 128 : dest->as<clay_uint128>() = ev->as<clay_uint128>(); break;
        default : assert(false);
        }
    }
}



//
// evalPrimOp
//
static const void *evalStringTableConstant(llvm::StringRef s)
{
    llvm::StringMap<const void*>::const_iterator oldConstant = staticStringTableConstants.find(s);
    if (oldConstant != staticStringTableConstants.end())
        return oldConstant->second;
    size_t bits = typeSize(cSizeTType);
    size_t size = s.size();
    void *buf = malloc(size + bits + 1);
    switch (bits) {
    case 4:
        *(size32_t*)buf = (size32_t)size;
        break;
    case 8:
        *(size64_t*)buf = size;
        break;
    default:
        error("unsupported pointer width");
    }
    memcpy((void*)((char*)buf + bits), (const void*)s.begin(), s.size());
    ((char*)buf)[s.size()] = 0;
    staticStringTableConstants[s] = (const void*)buf;
    return buf;
}

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
        out0->as<bool>() = isType;
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

    case PRIM_OperatorP : {
        ensureArity(args, 1);
        ObjectPtr obj = valueToStatic(args->values[0]);
        bool isOperator = false; 
        if (!!obj.ptr() && (obj->objKind==PROCEDURE || obj->objKind==GLOBAL_ALIAS)) {
            TopLevelItem *item = (TopLevelItem *)obj.ptr();
            isOperator = item->name->isOperator;
        }
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type == boolType);
        out0->as<bool>() = isOperator;
        break;
    }

    case PRIM_SymbolP : {
        ensureArity(args, 1);
        ObjectPtr obj = valueToStatic(args->values[0]);
        bool isSymbol = false; 
        if (obj.ptr() != NULL) {
            switch (obj->objKind) {
            case TYPE :
            case RECORD_DECL :
            case VARIANT_DECL :
            case PROCEDURE :
            case INTRINSIC :
            case GLOBAL_ALIAS:
                isSymbol = true;
                break;
            default :
                break;
            }
        }
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type == boolType);
        out0->as<bool>() = isSymbol;
        break;
    }

    case PRIM_StaticCallDefinedP : {
        if (args->size() < 1)
            arityError2(1, args->size());
        ObjectPtr callable = valueToStatic(args->values[0]);
        if (callable == NULL) {
            argumentError(0, "static callable expected");
        }
        switch (callable->objKind) {
        case TYPE :
        case RECORD_DECL :
        case VARIANT_DECL :
        case PROCEDURE :
        case INTRINSIC :
        case GLOBAL_ALIAS :
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

    case PRIM_StaticCallOutputTypes :
        break;

    case PRIM_StaticMonoP : {
        ensureArity(args, 1);

        bool isMono;

        ObjectPtr callable = valueToStatic(args->values[0]);
        if (callable != NULL && callable->objKind == PROCEDURE) {
            Procedure *p = (Procedure*)callable.ptr();
            isMono = p->mono.monoState == Procedure_MonoOverload;
        } else
            isMono = false;


        ValueHolderPtr vh = boolToValueHolder(isMono);
        evalStaticObject(vh.ptr(), out);
        break;
    }

    case PRIM_StaticMonoInputTypes :
        break;

    case PRIM_bitcopy : {
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
        out0->as<bool>() = !ev->as<bool>();
        break;
    }

    case PRIM_integerEqualsP :
    case PRIM_integerLesserP : {
        ensureArity(args, 2);
        TypePtr t;
        EValuePtr ev0 = integerOrPointerLikeValue(args, 0, t);
        EValuePtr ev1 = integerOrPointerLikeValue(args, 1, t);
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type == boolType);
        switch (x->primOpCode) {
        case PRIM_integerEqualsP:
            binaryNumericOp<Op_orderedEqualsP>(ev0, ev1, out0);
            break;
        case PRIM_integerLesserP:
            binaryNumericOp<Op_orderedLesserP>(ev0, ev1, out0);
            break;
        default:
            assert(false);
        }
        break;
    }

    case PRIM_floatOrderedEqualsP :
    case PRIM_floatOrderedLesserP :
    case PRIM_floatOrderedLesserEqualsP :
    case PRIM_floatOrderedGreaterP :
    case PRIM_floatOrderedGreaterEqualsP :
    case PRIM_floatOrderedNotEqualsP :
    case PRIM_floatOrderedP :
    case PRIM_floatUnorderedEqualsP :
    case PRIM_floatUnorderedLesserP :
    case PRIM_floatUnorderedLesserEqualsP :
    case PRIM_floatUnorderedGreaterP :
    case PRIM_floatUnorderedGreaterEqualsP :
    case PRIM_floatUnorderedNotEqualsP :
    case PRIM_floatUnorderedP : {
        ensureArity(args, 2);
        FloatTypePtr t;
        EValuePtr ev0 = floatValue(args, 0, t);
        EValuePtr ev1 = floatValue(args, 1, t);
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type == boolType);
        switch (x->primOpCode) {
        case PRIM_floatOrderedEqualsP :
            binaryNumericOp<Op_orderedEqualsP>(ev0, ev1, out0);
            break;
        case PRIM_floatOrderedLesserP :
            binaryNumericOp<Op_orderedLesserP>(ev0, ev1, out0);
            break;
        case PRIM_floatOrderedLesserEqualsP :
            binaryNumericOp<Op_orderedLesserEqualsP>(ev0, ev1, out0);
            break;
        case PRIM_floatOrderedGreaterP :
            binaryNumericOp<Op_orderedGreaterP>(ev0, ev1, out0);
            break;
        case PRIM_floatOrderedGreaterEqualsP :
            binaryNumericOp<Op_orderedGreaterEqualsP>(ev0, ev1, out0);
            break;
        case PRIM_floatOrderedNotEqualsP :
            binaryNumericOp<Op_orderedNotEqualsP>(ev0, ev1, out0);
            break;
        case PRIM_floatOrderedP :
            binaryNumericOp<Op_orderedP>(ev0, ev1, out0);
            break;
        case PRIM_floatUnorderedEqualsP :
            binaryNumericOp<Op_unorderedEqualsP>(ev0, ev1, out0);
            break;
        case PRIM_floatUnorderedLesserP :
            binaryNumericOp<Op_unorderedLesserP>(ev0, ev1, out0);
            break;
        case PRIM_floatUnorderedLesserEqualsP :
            binaryNumericOp<Op_unorderedLesserEqualsP>(ev0, ev1, out0);
            break;
        case PRIM_floatUnorderedGreaterP :
            binaryNumericOp<Op_unorderedGreaterP>(ev0, ev1, out0);
            break;
        case PRIM_floatUnorderedGreaterEqualsP :
            binaryNumericOp<Op_unorderedGreaterEqualsP>(ev0, ev1, out0);
            break;
        case PRIM_floatUnorderedNotEqualsP :
            binaryNumericOp<Op_unorderedNotEqualsP>(ev0, ev1, out0);
            break;
        case PRIM_floatUnorderedP :
            binaryNumericOp<Op_unorderedP>(ev0, ev1, out0);
            break;
        default:
            assert(false);
        }
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

    case PRIM_floatDivide : {
        ensureArity(args, 2);
        FloatTypePtr t;
        EValuePtr ev0 = floatValue(args, 0, t);
        EValuePtr ev1 = floatValue(args, 1, t);
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type == t.ptr());
        binaryNumericOp<Op_floatDivide>(ev0, ev1, out0);
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

    case PRIM_integerQuotient : {
        ensureArity(args, 2);
        IntegerTypePtr t;
        EValuePtr ev0 = integerValue(args, 0, t);
        EValuePtr ev1 = integerValue(args, 1, t);
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type.ptr() == t.ptr());
        binaryIntegerOp<Op_integerQuotient>(ev0, ev1, out0);
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

    case PRIM_integerQuotientChecked : {
        ensureArity(args, 2);
        IntegerTypePtr t;
        EValuePtr ev0 = integerValue(args, 0, t);
        EValuePtr ev1 = integerValue(args, 1, t);
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type.ptr() == t.ptr());
        binaryIntegerOp<Op_integerQuotientChecked>(ev0, ev1, out0);
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
        out0->as<void *>() = (void *)ev->addr;
        break;
    }

    case PRIM_pointerDereference : {
        ensureArity(args, 1);
        PointerTypePtr t;
        EValuePtr ev = pointerValue(args, 0, t);
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type == t.ptr());
        out0->as<void*>() = ev->as<void*>();
        break;
    }

    case PRIM_pointerOffset : {
        ensureArity(args, 2);
        PointerTypePtr t;
        EValuePtr v0 = pointerValue(args, 0, t);
        IntegerTypePtr offsetT;
        EValuePtr v1 = integerValue(args, 1, offsetT);
        ptrdiff_t offset = op_intToPtrInt(v1);
        char *ptr = v0->as<char *>();
        ptr += offset*(ptrdiff_t)typeSize(t->pointeeType);
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type == t.ptr());
        out0->as<void*>() = (void*)ptr;
        break;
    }

    case PRIM_pointerToInt : {
        ensureArity(args, 2);
        IntegerTypePtr dest = valueToIntegerType(args, 0);
        TypePtr pt;
        EValuePtr ev = pointerLikeValue(args, 1, pt);
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type == dest.ptr());
        op_pointerToInt(out0, ev);
        break;
    }

    case PRIM_intToPointer : {
        ensureArity(args, 2);
        TypePtr dest = valueToPointerLikeType(args, 0);
        IntegerTypePtr t;
        EValuePtr ev = integerValue(args, 1, t);
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type == dest);
        ptrdiff_t ptrInt = op_intToPtrInt(ev);
        out0->as<void *>() = (void *)ptrInt;
        break;
    }

    case PRIM_nullPointer : {
        ensureArity(args, 1);
        TypePtr dest = valueToPointerLikeType(args, 0);
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type == dest);
        out0->as<void *>() = 0;
        break;
    }

    case PRIM_CodePointer :
        error("CodePointer type constructor cannot be called");

    case PRIM_makeCodePointer : {
        error("code pointers cannot be taken at compile time");
        break;
    }

    case PRIM_ExternalCodePointer :
        error("ExternalCodePointer type constructor cannot be called");

    case PRIM_makeExternalCodePointer : {
        error("code pointers cannot be created at compile time");
        break;
    }

    case PRIM_callExternalCodePointer : {
        error("invoking a code pointer not yet supported in evaluator");
        break;
    }

    case PRIM_bitcast : {
        ensureArity(args, 2);
        TypePtr dest = valueToType(args, 0);
        EValuePtr ev = args->values[1];
        if (typeSize(dest) > typeSize(ev->type))
            error("destination type for bitcast is larger than source type");
        if (typeAlignment(dest) > typeAlignment(ev->type))
            error("destination type for bitcast has stricter alignment than source type");
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type == pointerType(dest));
        out0->as<void*>() = (void*)ev->addr;
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
        char *ptr = earray->addr + i*(ptrdiff_t)typeSize(at->elementType);
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type == pointerType(at->elementType));
        out0->as<void*>() = (void *)ptr;
        break;
    }

    case PRIM_arrayElements : {
        ensureArity(args, 1);
        ArrayTypePtr at;
        EValuePtr earray = arrayValue(args, 0, at);
        assert(out->size() == (size_t)at->size);
        for (size_t i = 0; i < (size_t)at->size; ++i) {
            EValuePtr outi = out->values[i];
            assert(outi->type == pointerType(at->elementType));
            char *ptr = earray->addr + i*typeSize(at->elementType);
            outi->as<void*>() = (void *)ptr;
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
        char *ptr = etuple->addr + layout->getElementOffset(unsigned(i));
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type == pointerType(tt->elementTypes[i]));
        out0->as<void*>() = (void *)ptr;
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
            outi->as<void*>() = (void *)ptr;
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
        out0->as<bool>() = isRecordType;
        break;
    }

    case PRIM_RecordFieldCount : {
        ensureArity(args, 1);
        RecordTypePtr rt = valueToRecordType(args, 0);
        ValueHolderPtr vh = sizeTToValueHolder(rt->fieldCount());
        evalStaticObject(vh.ptr(), out);
        break;
    }

    case PRIM_RecordFieldName : {
        ensureArity(args, 2);
        RecordTypePtr rt = valueToRecordType(args, 0);
        size_t i = valueToStaticSizeTOrInt(args, 1);
        llvm::ArrayRef<IdentifierPtr> fieldNames = recordFieldNames(rt);
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
                const llvm::StringMap<size_t> &fieldIndexMap =
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
        unsigned i = unsigned(valueToStaticSizeTOrInt(args, 1));
        llvm::ArrayRef<TypePtr> fieldTypes = recordFieldTypes(rt);
        if (i >= rt->fieldCount())
            argumentIndexRangeError(1, "record field index",
                                    i, rt->fieldCount());
        const llvm::StructLayout *layout = recordTypeLayout(rt.ptr());
        
        if (rt->hasVarField && i >= rt->varFieldPosition) {
            if (i == rt->varFieldPosition) {
                assert(out->size() == rt->varFieldSize());
                for (unsigned j = 0; j < rt->varFieldSize(); ++j) {
                    char *ptr = erec->addr + layout->getElementOffset(i+j);
                    EValuePtr out0 = out->values[j];
                    assert(out0->type == pointerType(fieldTypes[i+j]));
                    out0->as<void *>() = (void *)ptr;
                }
            } else {
                unsigned k = i+unsigned(rt->varFieldSize()-1);
                char *ptr = erec->addr + layout->getElementOffset(k);
                assert(out->size() == 1);
                EValuePtr out0 = out->values[0];
                assert(out0->type == pointerType(fieldTypes[k]));
                out0->as<void *>() = (void *)ptr;
            }
        } else {
            char *ptr = erec->addr + layout->getElementOffset(i);
            assert(out->size() == 1);
            EValuePtr out0 = out->values[0];
            assert(out0->type == pointerType(fieldTypes[i]));
            out0->as<void *>() = (void *)ptr;
        }
        break;
    }

    case PRIM_recordFieldRefByName : {
        ensureArity(args, 2);
        RecordTypePtr rt;
        EValuePtr erec = recordValue(args, 0, rt);
        IdentifierPtr fname = valueToIdentifier(args, 1);
        const llvm::StringMap<size_t> &fieldIndexMap = recordFieldIndexMap(rt);
        llvm::StringMap<size_t>::const_iterator fi =
            fieldIndexMap.find(fname->str);
        if (fi == fieldIndexMap.end()) {
            string buf;
            llvm::raw_string_ostream sout(buf);
            sout << "field not found: " << fname->str;
            argumentError(1, sout.str());
        }
        unsigned i = unsigned(fi->second);
        llvm::ArrayRef<TypePtr> fieldTypes = recordFieldTypes(rt);
        const llvm::StructLayout *layout = recordTypeLayout(rt.ptr());
        
        if (rt->hasVarField && i >= rt->varFieldPosition) {
            if (i == rt->varFieldPosition) {
                assert(out->size() == rt->varFieldSize());
                for (unsigned j = 0; j < rt->varFieldSize(); ++j) {
                    char *ptr = erec->addr + layout->getElementOffset(i+j);
                    EValuePtr out0 = out->values[j];
                    assert(out0->type == pointerType(fieldTypes[i+j]));
                    out0->as<void *>() = (void *)ptr;
                }
            } else {
                unsigned k = i+unsigned(rt->varFieldSize())-1;
                char *ptr = erec->addr + layout->getElementOffset(k);
                assert(out->size() == 1);
                EValuePtr out0 = out->values[0];
                assert(out0->type == pointerType(fieldTypes[k]));
                out0->as<void *>() = (void *)ptr;
            }
        } else {
            char *ptr = erec->addr + layout->getElementOffset(i);
            assert(out->size() == 1);
            EValuePtr out0 = out->values[0];
            assert(out0->type == pointerType(fieldTypes[i]));
            out0->as<void *>() = (void *)ptr;
        }
        break;
    }

    case PRIM_recordFields : {
        ensureArity(args, 1);
        RecordTypePtr rt;
        EValuePtr erec = recordValue(args, 0, rt);
        llvm::ArrayRef<TypePtr> fieldTypes = recordFieldTypes(rt);
        assert(out->size() == fieldTypes.size());
        const llvm::StructLayout *layout = recordTypeLayout(rt.ptr());
        for (unsigned i = 0; i < fieldTypes.size(); ++i) {
            EValuePtr outi = out->values[i];
            assert(outi->type == pointerType(fieldTypes[i]));
            char *ptr = erec->addr + layout->getElementOffset(i);
            outi->as<void *>() = (void *)ptr;
        }
        break;
    }

    case PRIM_recordVariadicField : {
        ensureArity(args, 1);
        RecordTypePtr rt;
        EValuePtr erec = recordValue(args, 0, rt);
        llvm::ArrayRef<TypePtr> fieldTypes = recordFieldTypes(rt);
        if (!rt->hasVarField)
            argumentError(0, "expecting a record with a variadic field");
        assert(out->size() == fieldTypes.size());
        const llvm::StructLayout *layout = recordTypeLayout(rt.ptr());
        assert(out->size() == rt->varFieldSize());
        for (unsigned i = 0; i < rt->varFieldSize(); ++i) {
            unsigned k = unsigned(rt->varFieldPosition) + i;
            EValuePtr outi = out->values[i];
            assert(outi->type == pointerType(fieldTypes[k]));
            char *ptr = erec->addr + layout->getElementOffset(k);
            outi->as<void *>() = (void *)ptr;
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
        out0->as<bool>() = isVariantType;
        break;
    }

    case PRIM_VariantMemberIndex : {
        ensureArity(args, 2);
        VariantTypePtr vt = valueToVariantType(args, 0);
        TypePtr mt = valueToType(args, 1);
        llvm::ArrayRef<TypePtr> memberTypes = variantMemberTypes(vt);
        size_t index = (size_t)-1;
        for (size_t i = 0; i < memberTypes.size(); ++i) {
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
        llvm::ArrayRef<TypePtr> memberTypes = variantMemberTypes(vt);
        size_t size = memberTypes.size();
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type == cSizeTType);
        setSizeTEValue(out0, size);
        break;
    }

    case PRIM_VariantMembers :
        break;

    case PRIM_BaseType :
        break;
        
    case PRIM_Static :
        error("Static type constructor cannot be called");

    case PRIM_staticIntegers :
        break;

    case PRIM_integers : {
        ensureArity(args, 1);
        ObjectPtr obj = valueToStatic(args, 0);
        if (obj->objKind != VALUE_HOLDER)
            argumentError(0, "expecting a static SizeT or Int value");
        ValueHolder *vh = (ValueHolder *)obj.ptr();
        if (vh->type == cIntType) {
            int count = vh->as<int>();
            if (count < 0)
                argumentError(0, "negative values are not allowed");
            assert(out->size() == (size_t)count);
            for (int i = 0; i < count; ++i) {
                ValueHolderPtr vhi = intToValueHolder(i);
                EValuePtr outi = out->values[size_t(i)];
                assert(outi->type == cIntType);
                outi->as<int>() = i;
            }
        }
        else if (vh->type == cSizeTType) {
            size_t count = valueHolderToSizeT(vh);
            assert(out->size() == count);
            for (size_t i = 0; i < count; ++i) {
                ValueHolderPtr vhi = sizeTToValueHolder(i);
                EValuePtr outi = out->values[i];
                assert(outi->type == cSizeTType);
                outi->as<size_t>() = i;
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
        if (moduleObj->objKind != MODULE)
            argumentError(0, "expecting a module");
        Module *module = (Module *)moduleObj.ptr();
        IdentifierPtr ident = valueToIdentifier(args, 1);
        ObjectPtr obj = safeLookupPublic(module, ident);
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
        out0->as<bool>() = isEnumType;
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

    case PRIM_EnumMemberName :
        break;

    case PRIM_enumToInt : {
        ensureArity(args, 1);
        EnumTypePtr et;
        EValuePtr ev = enumValue(args, 0, et);
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type == cIntType);
        out0->as<int>() = ev->as<int>();
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
        out0->as<int>() = ev->as<int>();
        break;
    }

    case PRIM_StringLiteralP : {
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

    case PRIM_stringLiteralByteIndex : {
        ensureArity(args, 2);
        IdentifierPtr ident = valueToIdentifier(args, 0);
        size_t n = valueToStaticSizeTOrInt(args, 1);
        if (n >= ident->str.size())
            argumentError(1, "string literal index out of bounds");

        assert(out->size() == 1);
        EValuePtr outi = out->values[0];
        assert(outi->type == cIntType);
        outi->as<int>() = ident->str[unsigned(n)];
        break;
    }

    case PRIM_stringLiteralBytes : {
        ensureArity(args, 1);
        IdentifierPtr ident = valueToIdentifier(args, 0);
        assert(out->size() == ident->str.size());
        for (unsigned i = 0, sz = unsigned(ident->str.size()); i < sz; ++i) {
            EValuePtr outi = out->values[i];
            assert(outi->type == cIntType);
            outi->as<int>() = ident->str[i];
        }
        break;
    }

    case PRIM_stringLiteralByteSize : {
        ensureArity(args, 1);
        IdentifierPtr ident = valueToIdentifier(args, 0);
        ValueHolderPtr vh = sizeTToValueHolder(ident->str.size());
        evalStaticObject(vh.ptr(), out);
        break;
    }

    case PRIM_stringLiteralByteSlice :
    case PRIM_stringLiteralConcat :
    case PRIM_stringLiteralFromBytes :
        break;

    case PRIM_stringTableConstant : {
        ensureArity(args, 1);
        IdentifierPtr ident = valueToIdentifier(args, 0);
        const void *value = evalStringTableConstant(ident->str);

        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type == pointerType(cSizeTType));
        out0->as<const void*>() = value;
        break;
    }

    case PRIM_StaticName :
    case PRIM_MainModule :
    case PRIM_StaticModule :
    case PRIM_ModuleName :
    case PRIM_ModuleMemberNames :
        break;

    case PRIM_FlagP : {
        ensureArity(args, 1);
        IdentifierPtr ident = valueToIdentifier(args, 0);
        llvm::StringMap<string>::const_iterator flag = globalFlags.find(ident->str);
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type == boolType);
        out0->as<bool>() = flag != globalFlags.end();
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

    case PRIM_memcpy :
    case PRIM_memmove : {
        ensureArity(args, 3);
        PointerTypePtr topt;
        PointerTypePtr frompt;
        EValuePtr tov = pointerValue(args, 0, topt);
        EValuePtr fromv = pointerValue(args, 1, frompt);
        IntegerTypePtr it;
        EValuePtr countv = integerValue(args, 2, it);

        void *to = tov->as<void*>();
        void *from = fromv->as<void*>();
        size_t size = op_intToSizeT(countv);

        if (x->primOpCode == PRIM_memcpy)
            memcpy(to, from, size);
        else
            memmove(to, from, size);

        break;
    }

    case PRIM_countValues : {
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type == cIntType);
        out0->as<int>() = int(args->size());
        break;
    }

    case PRIM_nthValue : {
        if (args->size() < 1)
            arityError2(1, args->size());
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];

        size_t i = valueToStaticSizeTOrInt(args, 0);
        if (i+1 >= args->size())
            argumentError(0, "nthValue argument out of bounds");

        evalValueForward(out0, args->values[i+1]);
        break;
    }

    case PRIM_withoutNthValue : {
        if (args->size() < 1)
            arityError2(1, args->size());
        assert(out->size() == args->size() - 2);

        size_t i = valueToStaticSizeTOrInt(args, 0);
        if (i+1 >= args->size())
            argumentError(0, "withoutNthValue argument out of bounds");

        for (size_t argi = 1, outi = 0; argi < args->size(); ++argi) {
            if (argi == i+1)
                continue;
            assert(outi < out->size());
            evalValueForward(out->values[outi], args->values[argi]);
            ++outi;
        }
        break;
    }

    case PRIM_takeValues : {
        if (args->size() < 1)
            arityError2(1, args->size());

        size_t i = valueToStaticSizeTOrInt(args, 0);
        if (i+1 >= args->size())
            i = args->size() - 1;

        assert(out->size() == i);
        for (size_t argi = 1, outi = 0; argi < i+1; ++argi, ++outi) {
            assert(outi < out->size());
            evalValueForward(out->values[outi], args->values[argi]);
        }
        break;
    }

    case PRIM_dropValues : {
        if (args->size() < 1)
            arityError2(1, args->size());

        size_t i = valueToStaticSizeTOrInt(args, 0);
        if (i+1 >= args->size())
            i = args->size() - 1;

        assert(out->size() == args->size() - i - 1);
        for (size_t argi = i+1, outi = 0; argi < args->size(); ++argi, ++outi) {
            assert(outi < out->size());
            evalValueForward(out->values[outi], args->values[argi]);
        }
        break;
    }

    case PRIM_LambdaRecordP : {
        ensureArity(args, 1);

        bool isLambda;

        ObjectPtr callable = valueToStatic(args->values[0]);
        if (callable != NULL && callable->objKind == TYPE) {
            Type *t = (Type*)callable.ptr();
            if (t->typeKind == RECORD_TYPE) {
                RecordType *r = (RecordType*)t;
                isLambda = r->record->lambda != NULL;
            } else
                isLambda = false;
        } else {
            isLambda = false;
        }

        ValueHolderPtr vh = boolToValueHolder(isLambda);
        evalStaticObject(vh.ptr(), out);
        break;
    }

    case PRIM_LambdaSymbolP : {
        ensureArity(args, 1);

        bool isLambda;

        ObjectPtr callable = valueToStatic(args->values[0]);
        if (callable != NULL && callable->objKind == PROCEDURE) {
            Procedure *p = (Procedure*)callable.ptr();
            isLambda = p->lambda != NULL;
        } else
            isLambda = false;

        ValueHolderPtr vh = boolToValueHolder(isLambda);
        evalStaticObject(vh.ptr(), out);
        break;
    }

    case PRIM_LambdaMonoP : {
        ensureArity(args, 1);

        bool isMono;

        ObjectPtr callable = valueToStatic(args->values[0]);
        if (callable != NULL && callable->objKind == TYPE) {
            Type *t = (Type*)callable.ptr();
            if (t->typeKind == RECORD_TYPE) {
                RecordType *r = (RecordType*)t;
                isMono = r->record->lambda->mono.monoState == Procedure_MonoOverload;
            } else
                isMono = false;
        } else {
            isMono = false;
        }

        ValueHolderPtr vh = boolToValueHolder(isMono);
        evalStaticObject(vh.ptr(), out);
        break;
    }

    case PRIM_LambdaMonoInputTypes :
        break;

    case PRIM_GetOverload :
        break;

    case PRIM_usuallyEquals : {
        ensureArity(args, 2);
        EValuePtr ev0 = args->values[0];
        if (!isPrimitiveType(ev0->type))
            argumentTypeError(0, "primitive type", ev0->type);

        assert(out->values.size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type == ev0->type);
        memcpy(out0->addr, ev0->addr, typeSize(ev0->type));
        break;
    }

    default :
        assert(false);

    }
}

}
