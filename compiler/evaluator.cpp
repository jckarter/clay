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
#include "evaluator_op.hpp"


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

void evalValueForward(EValuePtr dest, EValuePtr src)
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


}
