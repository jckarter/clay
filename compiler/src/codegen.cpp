#include "clay.hpp"

namespace clay {

llvm::Module *llvmModule = NULL;
llvm::DIBuilder *llvmDIBuilder = NULL;
llvm::ExecutionEngine *llvmEngine;
const llvm::TargetData *llvmTargetData;

static vector<CValuePtr> initializedGlobals;
static llvm::StringMap<llvm::Constant*> stringTableConstants;
static CodegenContext *constructorsCtx = NULL;
static CodegenContext *destructorsCtx = NULL;

static bool isMsvcTarget() {
    llvm::Triple target(llvmModule->getTargetTriple());
    return (target.getOS() == llvm::Triple::Win32);
}

llvm::PointerType *exceptionReturnType() {
    return llvmPointerType(int8Type);
}
llvm::Value *noExceptionReturnValue() {
    return llvm::ConstantPointerNull::get(exceptionReturnType());
}

void codegenValueInit(CValuePtr dest, CodegenContext* ctx);
void codegenValueDestroy(CValuePtr dest, CodegenContext* ctx);
void codegenStackEntryDestroy(ValueStackEntry const &entry,
    CodegenContext* ctx,
    bool exception);
void codegenValueCopy(CValuePtr dest, CValuePtr src, CodegenContext* ctx);
void codegenValueMove(CValuePtr dest, CValuePtr src, CodegenContext* ctx);
void codegenValueAssign(CValuePtr dest, CValuePtr src, CodegenContext* ctx);
void codegenValueMoveAssign(CValuePtr dest,
                            CValuePtr src,
                            CodegenContext* ctx);
llvm::Value *codegenToBoolFlag(CValuePtr a, CodegenContext* ctx);

int cgMarkStack(CodegenContext* ctx);
void cgDestroyStack(int marker, CodegenContext* ctx, bool exception);
void cgPopStack(int marker, CodegenContext* ctx);
void cgDestroyAndPopStack(int marker, CodegenContext* ctx, bool exception);
void cgPushStackValue(CValuePtr cv, CodegenContext* ctx);
void cgPushStackStatement(ValueStackEntryType type,
    EnvPtr env,
    StatementPtr statement,
    CodegenContext* ctx);
CValuePtr codegenAllocValue(TypePtr t, CodegenContext* ctx);
CValuePtr codegenAllocNewValue(TypePtr t, CodegenContext* ctx);
CValuePtr codegenAllocValueForPValue(PVData const &pv, CodegenContext* ctx);

MultiCValuePtr codegenMultiArgsAsRef(ExprListPtr exprs,
                                     EnvPtr env,
                                     CodegenContext* ctx);
MultiCValuePtr codegenArgExprAsRef(ExprPtr x,
                                   EnvPtr env,
                                   CodegenContext* ctx);

CValuePtr codegenForwardOneAsRef(ExprPtr expr,
                                 EnvPtr env,
                                 CodegenContext* ctx);
MultiCValuePtr codegenForwardMultiAsRef(ExprListPtr exprs,
                                        EnvPtr env,
                                        CodegenContext* ctx);
MultiCValuePtr codegenForwardExprAsRef(ExprPtr expr,
                                       EnvPtr env,
                                       CodegenContext* ctx);

CValuePtr codegenOneAsRef(ExprPtr expr,
                          EnvPtr env,
                          CodegenContext* ctx);
MultiCValuePtr codegenMultiAsRef(ExprListPtr exprs,
                                 EnvPtr env,
                                 CodegenContext* ctx);
MultiCValuePtr codegenExprAsRef(ExprPtr expr,
                                EnvPtr env,
                                CodegenContext* ctx);

void codegenOneInto(ExprPtr expr,
                    EnvPtr env,
                    CodegenContext* ctx,
                    CValuePtr out);
void codegenMultiInto(ExprListPtr exprs,
                      EnvPtr env,
                      CodegenContext* ctx,
                      MultiCValuePtr out,
                      unsigned count);
void codegenExprInto(ExprPtr expr,
                     EnvPtr env,
                     CodegenContext* ctx,
                     MultiCValuePtr out);

void codegenMulti(ExprListPtr exprs,
                  EnvPtr env,
                  CodegenContext* ctx,
                  MultiCValuePtr out,
                  unsigned count);
void codegenOne(ExprPtr expr,
                EnvPtr env,
                CodegenContext* ctx,
                CValuePtr out);
void codegenExpr(ExprPtr expr,
                 EnvPtr env,
                 CodegenContext* ctx,
                 MultiCValuePtr out);

void codegenStaticObject(ObjectPtr x,
                         CodegenContext* ctx,
                         MultiCValuePtr out);

void codegenGVarInstance(GVarInstancePtr x);
void codegenExternalVariable(ExternalVariablePtr x);
void codegenExternalProcedure(ExternalProcedurePtr x, bool codegenBody);

void codegenValueHolder(ValueHolderPtr x,
                        CodegenContext* ctx,
                        MultiCValuePtr out);
void codegenCompileTimeValue(EValuePtr ev,
                             CodegenContext* ctx,
                             MultiCValuePtr out);
llvm::Value *codegenSimpleConstant(EValuePtr ev);

void codegenIndexingExpr(ExprPtr indexable,
                         ExprListPtr args,
                         EnvPtr env,
                         CodegenContext* ctx,
                         MultiCValuePtr out);
void codegenAliasIndexing(GlobalAliasPtr x,
                          ExprListPtr args,
                          EnvPtr env,
                          CodegenContext* ctx,
                          MultiCValuePtr out);
void codegenCallExpr(ExprPtr callable,
                     ExprListPtr args,
                     EnvPtr env,
                     CodegenContext* ctx,
                     MultiCValuePtr out);
void codegenDispatch(ObjectPtr obj,
                     MultiCValuePtr args,
                     MultiPValuePtr pvArgs,
                     const vector<unsigned> &dispatchIndices,
                     CodegenContext* ctx,
                     MultiCValuePtr out);
void codegenCallValue(CValuePtr callable,
                      MultiCValuePtr args,
                      CodegenContext* ctx,
                      MultiCValuePtr out);
void codegenCallValue(CValuePtr callable,
                      MultiCValuePtr args,
                      MultiPValuePtr pvArgs,
                      CodegenContext* ctx,
                      MultiCValuePtr out);
bool codegenShortcut(ObjectPtr callable,
                     MultiPValuePtr pvArgs,
                     ExprListPtr args,
                     EnvPtr env,
                     CodegenContext* ctx,
                     MultiCValuePtr out);
void codegenCallPointer(CValuePtr x,
                        MultiCValuePtr args,
                        CodegenContext* ctx,
                        MultiCValuePtr out);
void codegenCallCCode(CCodePointerTypePtr type,
                      llvm::Value *llCallable,
                      MultiCValuePtr args,
                      CodegenContext* ctx,
                      MultiCValuePtr out);
void codegenCallCCodePointer(CValuePtr x,
                             MultiCValuePtr args,
                             CodegenContext* ctx,
                             MultiCValuePtr out);
void codegenCallCode(InvokeEntry* entry,
                     MultiCValuePtr args,
                     CodegenContext* ctx,
                     MultiCValuePtr out);
void codegenLowlevelCall(llvm::Value *llCallable,
                         vector<llvm::Value *>::iterator argBegin,
                         vector<llvm::Value *>::iterator argEnd,
                         CodegenContext* ctx);
void codegenCallInline(InvokeEntry* entry,
                       MultiCValuePtr args,
                       CodegenContext* ctx,
                       MultiCValuePtr out);
void codegenCallByName(InvokeEntry* entry,
                       ExprPtr callable,
                       ExprListPtr args,
                       EnvPtr env,
                       CodegenContext* ctx,
                       MultiCValuePtr out);

void codegenCodeBody(InvokeEntry* entry);

void codegenCWrapper(InvokeEntry* entry, CallingConv cc);

bool codegenStatement(StatementPtr stmt,
                      EnvPtr env,
                      CodegenContext* ctx);

void codegenCollectLabels(const vector<StatementPtr> &statements,
                          unsigned startIndex,
                          CodegenContext* ctx);
EnvPtr codegenBinding(BindingPtr x, EnvPtr env, CodegenContext* ctx);

void codegenExprAssign(ExprPtr left,
                       CValuePtr cvRight,
                       PVData const &pvRight,
                       EnvPtr env,
                       CodegenContext* ctx);

void codegenMultiExprAssign(ExprListPtr left,
                            MultiCValuePtr mcvRight,
                            MultiPValuePtr mpvRight,
                            EnvPtr env,
                            CodegenContext* ctx);

void codegenPrimOp(PrimOpPtr x,
                   MultiCValuePtr args,
                   CodegenContext* ctx,
                   MultiCValuePtr out);



//
// flags for inline/exceptions
//

static bool _inlineEnabled = true;
static bool _exceptionsEnabled = true;

bool inlineEnabled()
{
    return _inlineEnabled;
}

void setInlineEnabled(bool enabled)
{
    _inlineEnabled = enabled;
}

bool exceptionsEnabled()
{
    return _exceptionsEnabled;
}

void setExceptionsEnabled(bool enabled)
{
    _exceptionsEnabled = enabled;
}



//
// utility procs
//


llvm::BasicBlock *newBasicBlock(llvm::StringRef name, CodegenContext* ctx)
{
    return llvm::BasicBlock::Create(llvm::getGlobalContext(),
                                    name,
                                    ctx->llvmFunc);
}

static CValuePtr staticCValue(ObjectPtr obj, CodegenContext* ctx)
{
    TypePtr t = staticType(obj);
    if (ctx->valueForStatics == NULL)
        ctx->valueForStatics = ctx->initBuilder->CreateAlloca(llvmType(t));
    return new CValue(t, ctx->valueForStatics);
}

static CValuePtr derefValue(CValuePtr cvPtr, CodegenContext* ctx)
{
    assert(cvPtr->type->typeKind == POINTER_TYPE);
    PointerType *pt = (PointerType *)cvPtr->type.ptr();
    llvm::Value *ptrValue = ctx->builder->CreateLoad(cvPtr->llValue);
    return new CValue(pt->pointeeType, ptrValue);
}

static CValuePtr derefValueForPValue(CValuePtr cv, PVData const &pv, CodegenContext* ctx)
{
    if (pv.isTemp) {
        return cv;
    } else {
        assert(cv->type == pointerType(pv.type));
        return derefValue(cv, ctx);
    }
}



//
// codegen value ops
//

void codegenValueInit(CValuePtr dest, CodegenContext* ctx)
{
    if (isPrimitiveAggregateType(dest->type))
        return;
    codegenCallValue(staticCValue(dest->type.ptr(), ctx),
                     new MultiCValue(),
                     ctx,
                     new MultiCValue(dest));
}

void codegenValueDestroy(CValuePtr dest, CodegenContext* ctx)
{
    if (isPrimitiveAggregateType(dest->type))
        return;
    bool savedCheckExceptions = ctx->checkExceptions;
    ctx->checkExceptions = false;
    codegenCallValue(staticCValue(operator_destroy(), ctx),
                     new MultiCValue(dest),
                     ctx,
                     new MultiCValue());
    ctx->checkExceptions = savedCheckExceptions;
}

void codegenStackEntryDestroy(ValueStackEntry const &entry,
    CodegenContext* ctx,
    bool exception)
{
    switch (entry.type) {
    case LOCAL_VALUE:
        codegenValueDestroy(entry.value, ctx);
        break;
    case FINALLY_STATEMENT: {
            bool savedCheckExceptions = ctx->checkExceptions;
            ctx->checkExceptions = false;
            codegenStatement(entry.statement, entry.statementEnv, ctx);
            ctx->checkExceptions = savedCheckExceptions;
        }
        break;
    case ONERROR_STATEMENT: {
            if (exception) {
                bool savedCheckExceptions = ctx->checkExceptions;
                ctx->checkExceptions = false;
                codegenStatement(entry.statement, entry.statementEnv, ctx);
                ctx->checkExceptions = savedCheckExceptions;
            }
        }
        break;
    }
}

void codegenValueCopy(CValuePtr dest, CValuePtr src, CodegenContext* ctx)
{
    if (isPrimitiveAggregateType(dest->type)
        && (dest->type == src->type)
        && (!isPrimitiveAggregateTooLarge(dest->type)))
    {
        if (dest->type->typeKind != STATIC_TYPE) {
            llvm::Value *v = ctx->builder->CreateLoad(src->llValue);
            ctx->builder->CreateStore(v, dest->llValue);
        }
        return;
    }
    codegenCallValue(staticCValue(operator_copy(), ctx),
                     new MultiCValue(src),
                     ctx,
                     new MultiCValue(dest));
}

void codegenValueMove(CValuePtr dest, CValuePtr src, CodegenContext* ctx)
{
    if (isPrimitiveAggregateType(dest->type)
        && (dest->type == src->type)
        && (!isPrimitiveAggregateTooLarge(dest->type)))
    {
        if (dest->type->typeKind != STATIC_TYPE) {
            llvm::Value *v = ctx->builder->CreateLoad(src->llValue);
            ctx->builder->CreateStore(v, dest->llValue);
        }
        return;
    }
    codegenCallValue(staticCValue(operator_move(), ctx),
                     new MultiCValue(src),
                     ctx,
                     new MultiCValue(dest));
}

static void codegenValueForward(CValuePtr dest, CValuePtr src, CodegenContext* ctx)
{
    if (src->type == dest->type) {
        codegenValueMove(dest, src, ctx);
    }
    else {
        assert(dest->type == pointerType(src->type));
        ctx->builder->CreateStore(src->llValue, dest->llValue);
    }
}

static void codegenValueAssign(
    PVData const &pdest,
    CValuePtr dest,
    PVData const &psrc,
    CValuePtr src,
    CodegenContext* ctx)
{
    if (isPrimitiveAggregateType(dest->type)
        && !pdest.isTemp
        && (dest->type == src->type)
        && (!isPrimitiveAggregateTooLarge(dest->type)))
    {
        if (dest->type->typeKind != STATIC_TYPE) {
            llvm::Value *v = ctx->builder->CreateLoad(src->llValue);
            ctx->builder->CreateStore(v, dest->llValue);
        }
        return;
    }
    MultiCValuePtr args = new MultiCValue(dest);
    args->add(src);
    MultiPValuePtr pvArgs = new MultiPValue(pdest);
    pvArgs->add(psrc);
    codegenCallValue(staticCValue(operator_assign(), ctx),
                     args,
                     pvArgs,
                     ctx,
                     new MultiCValue());
}

llvm::Value *codegenToBoolFlag(CValuePtr a, CodegenContext* ctx)
{
    if (a->type != boolType)
        typeError(boolType, a->type);
    llvm::Value *flag = ctx->builder->CreateLoad(a->llValue);
    assert(flag->getType() == llvmIntType(1));
    return flag;
}



//
// temps
//

static llvm::Value *allocTemp(llvm::Type *llType, CodegenContext* ctx)
{
    llvm::Value *llv = NULL;
    for (unsigned i = ctx->discardedSlots.size(); i > 0; --i) {
        if (ctx->discardedSlots[i-1].llType == llType) {
            llv = ctx->discardedSlots[i-1].llValue;
            ctx->discardedSlots.erase(ctx->discardedSlots.begin() + i-1);
            break;
        }
    }
    if (!llv)
        llv = ctx->initBuilder->CreateAlloca(llType);
    ctx->allocatedSlots.push_back(StackSlot(llType, llv));
    return llv;
}

static int markTemps(CodegenContext* ctx) {
    return ctx->allocatedSlots.size();
}

static void clearTemps(int marker, CodegenContext* ctx) {
    while (marker < (int)ctx->allocatedSlots.size()) {
        ctx->discardedSlots.push_back(ctx->allocatedSlots.back());
        ctx->allocatedSlots.pop_back();
    }
}



//
// codegen value stack
//

int cgMarkStack(CodegenContext* ctx)
{
    return ctx->valueStack.size();
}

void cgDestroyStack(int marker, CodegenContext* ctx, bool exception)
{
    int i = (int)ctx->valueStack.size();
    assert(marker <= i);
    while (marker < i) {
        --i;
        codegenStackEntryDestroy(ctx->valueStack[i], ctx, exception);
    }
}

void cgPopStack(int marker, CodegenContext* ctx)
{
    assert(marker <= (int)ctx->valueStack.size());
    while (marker < (int)ctx->valueStack.size())
        ctx->valueStack.pop_back();
}

void cgDestroyAndPopStack(int marker, CodegenContext* ctx, bool exception)
{
    assert(marker <= (int)ctx->valueStack.size());
    while (marker < (int)ctx->valueStack.size()) {
        ValueStackEntry entry = ctx->valueStack.back();
        ctx->valueStack.pop_back();
        codegenStackEntryDestroy(entry, ctx, exception);
    }
}

void cgPushStackValue(CValuePtr cv, CodegenContext* ctx)
{
    ctx->valueStack.push_back(ValueStackEntry(cv));
}

void cgPushStackStatement(ValueStackEntryType type,
    EnvPtr env,
    StatementPtr statement,
    CodegenContext* ctx)
{
    ctx->valueStack.push_back(ValueStackEntry(type, env, statement));
}

CValuePtr codegenAllocValue(TypePtr t, CodegenContext* ctx)
{
    llvm::Value *llv = allocTemp(llvmType(t), ctx);
    return new CValue(t, llv);
}

CValuePtr codegenAllocValueForPValue(PVData const &pv, CodegenContext* ctx)
{
    if (pv.isTemp)
        return codegenAllocValue(pv.type, ctx);
    else
        return codegenAllocValue(pointerType(pv.type), ctx);
}

CValuePtr codegenAllocNewValue(TypePtr t, CodegenContext* ctx)
{
    llvm::Type *llt = llvmType(t);
    llvm::Value *llv = ctx->initBuilder->CreateAlloca(llt);
    return new CValue(t, llv);
}



//
// codegenMultiArgsAsRef, codegenArgExprAsRef
//

MultiCValuePtr codegenMultiArgsAsRef(ExprListPtr exprs,
                                     EnvPtr env,
                                     CodegenContext* ctx)
{
    MultiCValuePtr out = new MultiCValue();
    for (unsigned i = 0; i < exprs->size(); ++i) {
        ExprPtr x = exprs->exprs[i];
        if (x->exprKind == UNPACK) {
            Unpack *y = (Unpack *)x.ptr();
            MultiCValuePtr mcv = codegenArgExprAsRef(y->expr, env, ctx);
            out->add(mcv);
        }
        else if (x->exprKind == PAREN) {
            MultiCValuePtr mcv = codegenArgExprAsRef(x, env, ctx);
            out->add(mcv);
        }
        else {
            MultiCValuePtr mcv = codegenArgExprAsRef(x, env, ctx);
            out->add(mcv);
        }
    }
    return out;
}

MultiCValuePtr codegenArgExprAsRef(ExprPtr x,
                                   EnvPtr env,
                                   CodegenContext* ctx)
{
    if (x->exprKind == DISPATCH_EXPR) {
        DispatchExpr *y = (DispatchExpr *)x.ptr();
        return codegenExprAsRef(y->expr, env, ctx);
    }
    return codegenExprAsRef(x, env, ctx);
}



//
// codegenForwardOneAsRef, codegenForwardMultiAsRef, codegenForwardExprAsRef
//

CValuePtr codegenForwardOneAsRef(ExprPtr expr,
                                 EnvPtr env,
                                 CodegenContext* ctx)
{
    MultiCValuePtr mcv = codegenForwardExprAsRef(expr, env, ctx);
    LocationContext loc(expr->location);
    ensureArity(mcv, 1);
    return mcv->values[0];
}

MultiCValuePtr codegenForwardMultiAsRef(ExprListPtr exprs,
                                        EnvPtr env,
                                        CodegenContext* ctx)
{
    MultiCValuePtr out = new MultiCValue();
    for (unsigned i = 0; i < exprs->size(); ++i) {
        ExprPtr x = exprs->exprs[i];
        if (x->exprKind == UNPACK) {
            Unpack *y = (Unpack *)x.ptr();
            MultiCValuePtr mcv = codegenForwardExprAsRef(y->expr, env, ctx);
            out->add(mcv);
        }
        else {
            MultiCValuePtr mcv = codegenForwardExprAsRef(x, env, ctx);
            out->add(mcv);
        }
    }
    return out;
}

MultiCValuePtr codegenForwardExprAsRef(ExprPtr expr,
                                       EnvPtr env,
                                       CodegenContext* ctx)
{
    MultiPValuePtr mpv = safeAnalyzeExpr(expr, env);
    MultiCValuePtr mcv = codegenExprAsRef(expr, env, ctx);
    assert(mpv->size() == mcv->size());
    MultiCValuePtr out = new MultiCValue();
    for (unsigned i = 0; i < mcv->size(); ++i) {
        CValuePtr cv = mcv->values[i];
        if (mpv->values[i].isTemp) {
            cv = new CValue(cv->type, cv->llValue);
            cv->forwardedRValue = true;
        }
        out->add(cv);
    }
    return out;
}



//
// codegenOneAsRef, codegenMultiAsRef, codegenExprAsRef
//

CValuePtr codegenOneAsRef(ExprPtr expr,
                          EnvPtr env,
                          CodegenContext* ctx)
{
    MultiCValuePtr mcv = codegenExprAsRef(expr, env, ctx);
    LocationContext loc(expr->location);
    ensureArity(mcv, 1);
    return mcv->values[0];
}

MultiCValuePtr codegenMultiAsRef(ExprListPtr exprs,
                                 EnvPtr env,
                                 CodegenContext* ctx)
{
    MultiCValuePtr out = new MultiCValue();
    for (unsigned i = 0; i < exprs->size(); ++i) {
        ExprPtr x = exprs->exprs[i];
        if (x->exprKind == UNPACK) {
            Unpack *y = (Unpack *)x.ptr();
            MultiCValuePtr mcv = codegenExprAsRef(y->expr, env, ctx);
            out->add(mcv);
        }
        else if (x->exprKind == PAREN) {
            MultiCValuePtr mcv = codegenExprAsRef(x, env, ctx);
            out->add(mcv);
        }
        else {
            CValuePtr cv = codegenOneAsRef(x, env, ctx);
            out->add(cv);
        }
    }
    return out;
}

static MultiCValuePtr codegenExprAsRef2(ExprPtr expr,
                                        EnvPtr env,
                                        CodegenContext* ctx)
{
    MultiPValuePtr mpv = safeAnalyzeExpr(expr, env);
    MultiCValuePtr mcv = new MultiCValue();
    for (unsigned i = 0; i < mpv->size(); ++i) {
        PVData const &pv = mpv->values[i];
        mcv->add(codegenAllocValueForPValue(pv, ctx));
    }
    codegenExpr(expr, env, ctx, mcv);
    MultiCValuePtr out = new MultiCValue();
    for (unsigned i = 0; i < mpv->size(); ++i) {
        if (mpv->values[i].isTemp) {
            out->add(mcv->values[i]);
            cgPushStackValue(mcv->values[i], ctx);
        }
        else {
            out->add(derefValue(mcv->values[i], ctx));
        }
    }
    return out;
}

static MultiCValuePtr codegenStaticObjectAsRef(ObjectPtr x,
                                               ExprPtr expr,
                                               EnvPtr env,
                                               CodegenContext* ctx)
{
    switch (x->objKind) {
    case CVALUE : {
        CValuePtr y = (CValue *)x.ptr();
        if (y->forwardedRValue)
            y = new CValue(y->type, y->llValue);
        return new MultiCValue(y);
    }
    case MULTI_CVALUE : {
        MultiCValue *y = (MultiCValue *)x.ptr();
        MultiCValuePtr out = new MultiCValue();
        for (unsigned i = 0; i < y->size(); ++i) {
            CValue *cv = y->values[i].ptr();
            if (cv->forwardedRValue)
                out->add(new CValue(cv->type, cv->llValue));
            else
                out->add(cv);
        }
        return out;
    }
    default :
        return codegenExprAsRef2(expr, env, ctx);
    }
}

MultiCValuePtr codegenExprAsRef(ExprPtr expr,
                                EnvPtr env,
                                CodegenContext* ctx)
{
    LocationContext loc(expr->location);

    switch (expr->exprKind) {
    case NAME_REF : {
        NameRef *x = (NameRef *)expr.ptr();
        ObjectPtr y = safeLookupEnv(env, x->name);
        if (y->objKind == EXPRESSION) {
            ExprPtr z = (Expr *)y.ptr();
            return codegenExprAsRef(z, env, ctx);
        }
        else if (y->objKind == EXPR_LIST) {
            ExprListPtr z = (ExprList *)y.ptr();
            return codegenMultiAsRef(z, env, ctx);
        }
        return codegenStaticObjectAsRef(y, expr, env, ctx);
    }
    case OBJECT_EXPR : {
        ObjectExpr *x = (ObjectExpr *)expr.ptr();
        return codegenStaticObjectAsRef(x->obj, expr, env, ctx);
    }
    default :
        break;
    }
    return codegenExprAsRef2(expr, env, ctx);
}



//
// codegenOneInto, codegenMultiInto, codegenExprInto
//

void codegenOneInto(ExprPtr expr,
                    EnvPtr env,
                    CodegenContext* ctx,
                    CValuePtr out)
{
    PVData pv = safeAnalyzeOne(expr, env);
    int marker = cgMarkStack(ctx);
    if (pv.isTemp) {
        codegenOne(expr, env, ctx, out);
    }
    else {
        CValuePtr cvPtr = codegenAllocValue(pointerType(pv.type), ctx);
        codegenOne(expr, env, ctx, cvPtr);
        codegenValueCopy(out, derefValue(cvPtr, ctx), ctx);
    }
    cgDestroyAndPopStack(marker, ctx, false);
}

void codegenMultiInto(ExprListPtr exprs,
                      EnvPtr env,
                      CodegenContext* ctx,
                      MultiCValuePtr out,
                      unsigned wantCount)
{
    int marker = cgMarkStack(ctx);
    int marker2 = marker;
    unsigned j = 0;
    ExprPtr unpackExpr = implicitUnpackExpr(wantCount, exprs);
    if (unpackExpr != NULL) {
        MultiPValuePtr mpv = safeAnalyzeExpr(unpackExpr, env);
        assert(j + mpv->size() <= out->size());
        MultiCValuePtr out2 = new MultiCValue();
        for (unsigned k = 0; k < mpv->size(); ++k)
            out2->add(out->values[j + k]);
        codegenExprInto(unpackExpr, env, ctx, out2);
        j += mpv->size();
    } else for (unsigned i = 0; i < exprs->size(); ++i) {
        unsigned prevJ = j;
        ExprPtr x = exprs->exprs[i];
        if (x->exprKind == UNPACK) {
            Unpack *y = (Unpack *)x.ptr();
            MultiPValuePtr mpv = safeAnalyzeExpr(y->expr, env);
            assert(j + mpv->size() <= out->size());
            MultiCValuePtr out2 = new MultiCValue();
            for (unsigned k = 0; k < mpv->size(); ++k)
                out2->add(out->values[j + k]);
            codegenExprInto(y->expr, env, ctx, out2);
            j += mpv->size();
        }
        else if (x->exprKind == PAREN) {
            MultiPValuePtr mpv = safeAnalyzeExpr(x, env);
            assert(j + mpv->size() <= out->size());
            MultiCValuePtr out2 = new MultiCValue();
            for (unsigned k = 0; k < mpv->size(); ++k)
                out2->add(out->values[j + k]);
            codegenExprInto(x, env, ctx, out2);
            j += mpv->size();
        }
        else {
            MultiPValuePtr mpv = safeAnalyzeExpr(x, env);
            if (mpv->size() != 1)
                arityError(x, 1, mpv->size());
            assert(j < out->size());
            codegenOneInto(x, env, ctx, out->values[j]);
            ++j;
        }
        cgDestroyAndPopStack(marker2, ctx, false);
        for (unsigned k = prevJ; k < j; ++k)
            cgPushStackValue(out->values[k], ctx);
        marker2 = cgMarkStack(ctx);
    }
    assert(j == out->size());
    cgPopStack(marker, ctx);
}

void codegenExprInto(ExprPtr expr,
                     EnvPtr env,
                     CodegenContext* ctx,
                     MultiCValuePtr out)
{
    MultiPValuePtr mpv = safeAnalyzeExpr(expr, env);
    assert(out->size() == mpv->size());
    int marker = cgMarkStack(ctx);
    MultiCValuePtr mcv = new MultiCValue();
    for (unsigned i = 0; i < mpv->size(); ++i) {
        PVData const &pv = mpv->values[i];
        if (pv.isTemp) {
            mcv->add(out->values[i]);
        }
        else {
            CValuePtr cvPtr = codegenAllocValue(pointerType(pv.type), ctx);
            mcv->add(cvPtr);
        }
    }
    codegenExpr(expr, env, ctx, mcv);
    int marker2 = cgMarkStack(ctx);
    for (unsigned i = 0; i < mpv->size(); ++i) {
        if (!mpv->values[i].isTemp) {
            CValuePtr cv = derefValue(mcv->values[i], ctx);
            codegenValueCopy(out->values[i], cv, ctx);
        }
        cgPushStackValue(out->values[i], ctx);
    }
    cgPopStack(marker2, ctx);
    cgDestroyAndPopStack(marker, ctx, false);
}



//
// codegenMulti, codegenOne, codegenExpr
//

void codegenMulti(ExprListPtr exprs,
                  EnvPtr env,
                  CodegenContext* ctx,
                  MultiCValuePtr out,
                  unsigned wantCount)
{
    int marker = cgMarkStack(ctx);
    int marker2 = marker;
    unsigned j = 0;
    ExprPtr unpackExpr = implicitUnpackExpr(wantCount, exprs);
    if (unpackExpr != NULL) {
        MultiPValuePtr mpv = safeAnalyzeExpr(unpackExpr, env);
        assert(j + mpv->size() <= out->size());
        MultiCValuePtr out2 = new MultiCValue();
        for (unsigned k = 0; k < mpv->size(); ++k)
            out2->add(out->values[j + k]);
        codegenExpr(unpackExpr, env, ctx, out2);
        j += mpv->size();
    } else for (unsigned i = 0; i < exprs->size(); ++i) {
        unsigned prevJ = j;
        ExprPtr x = exprs->exprs[i];
        if (x->exprKind == UNPACK) {
            Unpack *y = (Unpack *)x.ptr();
            MultiPValuePtr mpv = safeAnalyzeExpr(y->expr, env);
            assert(j + mpv->size() <= out->size());
            MultiCValuePtr out2 = new MultiCValue();
            for (unsigned k = 0; k < mpv->size(); ++k)
                out2->add(out->values[j + k]);
            codegenExpr(y->expr, env, ctx, out2);
            j += mpv->size();
        }
        else if (x->exprKind == PAREN) {
            Unpack *y = (Unpack *)x.ptr();
            MultiPValuePtr mpv = safeAnalyzeExpr(x, env);
            assert(j + mpv->size() <= out->size());
            MultiCValuePtr out2 = new MultiCValue();
            for (unsigned k = 0; k < mpv->size(); ++k)
                out2->add(out->values[j + k]);
            codegenExpr(x, env, ctx, out2);
            j += mpv->size();
        }
        else {
            MultiPValuePtr mpv = safeAnalyzeExpr(x, env);
            if (mpv->size() != 1)
                arityError(x, 1, mpv->size());
            assert(j < out->size());
            codegenOne(x, env, ctx, out->values[j]);
            ++j;
        }
        cgDestroyAndPopStack(marker2, ctx, false);
        for (unsigned k = prevJ; k < j; ++k)
            cgPushStackValue(out->values[k], ctx);
        marker2 = cgMarkStack(ctx);
    }
    assert(j == out->size());
    cgPopStack(marker, ctx);
}

void codegenOne(ExprPtr expr,
                EnvPtr env,
                CodegenContext* ctx,
                CValuePtr out)
{
    codegenExpr(expr, env, ctx, new MultiCValue(out));
}

void codegenExpr(ExprPtr expr,
                 EnvPtr env,
                 CodegenContext* ctx,
                 MultiCValuePtr out)
{
    LocationContext loc(expr->location);

    switch (expr->exprKind) {

    case BOOL_LITERAL : {
        BoolLiteral *x = (BoolLiteral *)expr.ptr();
        ValueHolderPtr y = boolToValueHolder(x->value);
        codegenValueHolder(y, ctx, out);
        break;
    }

    case INT_LITERAL : {
        IntLiteral *x = (IntLiteral *)expr.ptr();
        ValueHolderPtr y = parseIntLiteral(safeLookupModule(env), x);
        codegenValueHolder(y, ctx, out);
        break;
    }

    case FLOAT_LITERAL : {
        FloatLiteral *x = (FloatLiteral *)expr.ptr();
        ValueHolderPtr y = parseFloatLiteral(safeLookupModule(env), x);
        codegenValueHolder(y, ctx, out);
        break;
    }

    case CHAR_LITERAL : {
        CharLiteral *x = (CharLiteral *)expr.ptr();
        if (!x->desugared)
            x->desugared = desugarCharLiteral(x->value);
        codegenExpr(x->desugared, env, ctx, out);
        break;
    }

    case STRING_LITERAL :
        break;

    case FILE_EXPR :
    case ARG_EXPR :
        break;

    case LINE_EXPR : {
        Location location = safeLookupCallByNameLocation(env);
        int line, column, tabColumn;
        getLineCol(location, line, column, tabColumn);

        ValueHolderPtr vh = sizeTToValueHolder(line+1);
        codegenStaticObject(vh.ptr(), ctx, out);
        break;
    }
    case COLUMN_EXPR : {
        Location location = safeLookupCallByNameLocation(env);
        int line, column, tabColumn;
        getLineCol(location, line, column, tabColumn);

        ValueHolderPtr vh = sizeTToValueHolder(column);
        codegenStaticObject(vh.ptr(), ctx, out);
        break;
    }


    case NAME_REF : {
        NameRef *x = (NameRef *)expr.ptr();
        ObjectPtr y = safeLookupEnv(env, x->name);
        if (y->objKind == EXPRESSION) {
            ExprPtr z = (Expr *)y.ptr();
            codegenExpr(z, env, ctx, out);
        }
        else if (y->objKind == EXPR_LIST) {
            ExprListPtr z = (ExprList *)y.ptr();
            codegenMulti(z, env, ctx, out, 0);
        }
        else {
            codegenStaticObject(y, ctx, out);
        }
        break;
    }

    case TUPLE : {
        Tuple *x = (Tuple *)expr.ptr();
        ExprPtr tupleLiteral = operator_expr_tupleLiteral();
        codegenCallExpr(tupleLiteral, x->args, env, ctx, out);
        break;
    }

    case PAREN : {
        Paren *x = (Paren *)expr.ptr();
        codegenMulti(x->args, env, ctx, out, 0);
        break;
    }

    case INDEXING : {
        Indexing *x = (Indexing *)expr.ptr();
        codegenIndexingExpr(x->expr, x->args, env, ctx, out);
        break;
    }

    case CALL : {
        Call *x = (Call *)expr.ptr();
        codegenCallExpr(x->expr, x->allArgs(), env, ctx, out);
        break;
    }

    case FIELD_REF : {
        FieldRef *x = (FieldRef *)expr.ptr();
        PVData pv = safeAnalyzeOne(x->expr, env);
        if (pv.type->typeKind == STATIC_TYPE) {
            StaticType *st = (StaticType *)pv.type.ptr();
            if (st->obj->objKind == MODULE_HOLDER) {
                ModuleHolder *mh = (ModuleHolder *)st->obj.ptr();
                ObjectPtr obj = safeLookupModuleHolder(mh, x->name);
                codegenStaticObject(obj, ctx, out);
                break;
            }
        }
        if (!x->desugared)
            x->desugared = desugarFieldRef(x);
        codegenExpr(x->desugared, env, ctx, out);
        break;
    }

    case STATIC_INDEXING : {
        StaticIndexing *x = (StaticIndexing *)expr.ptr();
        if (!x->desugared)
            x->desugared = desugarStaticIndexing(x);
        codegenExpr(x->desugared, env, ctx, out);
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
        codegenExpr(x->desugared, env, ctx, out);
        break; 
    }

    case EVAL_EXPR : {
        EvalExpr *eval = (EvalExpr *)expr.ptr();
        // XXX compilation context
        ExprListPtr evaled = desugarEvalExpr(eval, env);
        codegenMulti(evaled, env, ctx, out, 0);
        break;
    }

    case AND : {
        And *x = (And *)expr.ptr();
        CValuePtr cv1 = codegenOneAsRef(x->expr1, env, ctx);
        llvm::Value *flag1 = codegenToBoolFlag(cv1, ctx);
        llvm::BasicBlock *trueBlock = newBasicBlock("andTrue1", ctx);
        llvm::BasicBlock *falseBlock = newBasicBlock("andFalse1", ctx);
        llvm::BasicBlock *mergeBlock = newBasicBlock("andMerge", ctx);
        ctx->builder->CreateCondBr(flag1, trueBlock, falseBlock);

        CValuePtr out0 = out->values[0];
        assert(out0->type == boolType);

        ctx->builder->SetInsertPoint(trueBlock);
        int marker = cgMarkStack(ctx);
        CValuePtr cv2 = codegenOneAsRef(x->expr2, env, ctx);
        llvm::Value *flag2 = codegenToBoolFlag(cv2, ctx);
        ctx->builder->CreateStore(flag2, out0->llValue);
        cgDestroyAndPopStack(marker, ctx, false);
        ctx->builder->CreateBr(mergeBlock);

        ctx->builder->SetInsertPoint(falseBlock);
        llvm::Value *zero = llvm::ConstantInt::get(llvmType(boolType), 0);
        ctx->builder->CreateStore(zero, out0->llValue);
        ctx->builder->CreateBr(mergeBlock);

        ctx->builder->SetInsertPoint(mergeBlock);
        break;
    }

    case OR : {
        Or *x = (Or *)expr.ptr();
        CValuePtr cv1 = codegenOneAsRef(x->expr1, env, ctx);
        llvm::Value *flag1 = codegenToBoolFlag(cv1, ctx);
        llvm::BasicBlock *falseBlock = newBasicBlock("orFalse1", ctx);
        llvm::BasicBlock *trueBlock = newBasicBlock("orTrue1", ctx);
        llvm::BasicBlock *mergeBlock = newBasicBlock("orMerge", ctx);
        ctx->builder->CreateCondBr(flag1, trueBlock, falseBlock);

        CValuePtr out0 = out->values[0];
        assert(out0->type == boolType);

        ctx->builder->SetInsertPoint(falseBlock);
        int marker = cgMarkStack(ctx);
        CValuePtr cv2 = codegenOneAsRef(x->expr2, env, ctx);
        llvm::Value *flag2 = codegenToBoolFlag(cv2, ctx);
        ctx->builder->CreateStore(flag2, out0->llValue);
        cgDestroyAndPopStack(marker, ctx, false);
        ctx->builder->CreateBr(mergeBlock);

        ctx->builder->SetInsertPoint(trueBlock);
        llvm::Value *one = llvm::ConstantInt::get(llvmType(boolType), 1);
        ctx->builder->CreateStore(one, out0->llValue);
        ctx->builder->CreateBr(mergeBlock);

        ctx->builder->SetInsertPoint(mergeBlock);
        break;
    }

    case LAMBDA : {
        Lambda *x = (Lambda *)expr.ptr();
        if (!x->initialized)
            initializeLambda(x, env);
        codegenExpr(x->converted, env, ctx, out);
        break;
    }

    case UNPACK : {
        error("incorrect usage of unpack operator");
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
        codegenExpr(x->expr, x->getEnv(), ctx, out);
        break;
    }

    case OBJECT_EXPR : {
        ObjectExpr *x = (ObjectExpr *)expr.ptr();
        codegenStaticObject(x->obj, ctx, out);
        break;
    }

    default :
        assert(false);

    }
}



//
// codegenStaticObject
//

void codegenStaticObject(ObjectPtr x,
                         CodegenContext* ctx,
                         MultiCValuePtr out)
{
    switch (x->objKind) {

    case ENUM_MEMBER : {
        EnumMember *y = (EnumMember *)x.ptr();
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0->type == y->type);
        assert(out0->type->typeKind == ENUM_TYPE);
        initializeEnumType((EnumType*)out0->type.ptr());
        llvm::Value *llv = llvm::ConstantInt::getSigned(
            llvmType(y->type), y->index);
        ctx->builder->CreateStore(llv, out0->llValue);
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
            assert(out->size() == 1);
            CValuePtr out0 = out->values[0];
            assert(out0->type == pointerType(z->type));
            ctx->builder->CreateStore(z->llGlobal, out0->llValue);
        }
        break;
    }

    case EXTERNAL_VARIABLE : {
        ExternalVariable *y = (ExternalVariable *)x.ptr();
        if (!y->llGlobal)
            codegenExternalVariable(y);
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0->type == pointerType(y->type2));
        ctx->builder->CreateStore(y->llGlobal, out0->llValue);
        break;
    }

    case EXTERNAL_PROCEDURE : {
        ExternalProcedure *y = (ExternalProcedure *)x.ptr();
        if (!y->llvmFunc)
            codegenExternalProcedure(y, false);
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0->type == y->ptrType);
        llvm::Value *opaqueValue = ctx->builder->CreateBitCast(
            y->llvmFunc, llvmType(out0->type));
        ctx->builder->CreateStore(opaqueValue, out0->llValue);
        break;
    }

    case GLOBAL_ALIAS : {
        GlobalAlias *y = (GlobalAlias *)x.ptr();
        if (y->hasParams()) {
            assert(out->size() == 1);
            assert(out->values[0]->type == staticType(x));
        }
        else {
            codegenExpr(y->expr, y->env, ctx, out);
        }
        break;
    }

    case VALUE_HOLDER : {
        ValueHolder *y = (ValueHolder *)x.ptr();
        codegenValueHolder(y, ctx, out);
        break;
    }

    case MULTI_STATIC : {
        MultiStatic *y = (MultiStatic *)x.ptr();
        assert(y->size() == out->size());
        for (unsigned i = 0; i < y->size(); ++i) {
            MultiCValuePtr out2 = new MultiCValue(out->values[i]);
            codegenStaticObject(y->values[i], ctx, out2);
        }
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

    case CVALUE : {
        CValue *y = (CValue *)x.ptr();
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        codegenValueForward(out0, y, ctx);
        break;
    }

    case MULTI_CVALUE : {
        MultiCValue *y = (MultiCValue *)x.ptr();
        assert(out->size() == y->size());
        for (unsigned i = 0; i < y->size(); ++i) {
            CValuePtr vi = y->values[i];
            CValuePtr outi = out->values[i];
            codegenValueForward(outi, vi, ctx);
        }
        break;
    }

    case PVALUE : {
        // allow values of static type
        PValue *yv = (PValue *)x.ptr();
        PVData const &y = yv->data;
        if (y.type->typeKind != STATIC_TYPE)
            invalidStaticObjectError(x);
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
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
            CValuePtr outi = out->values[i];
            if (pv.isTemp)
                assert(outi->type == pv.type);
            else
                assert(outi->type == pointerType(pv.type));
        }
        break;
    }

    case PATTERN : {
        error("pattern variable cannot be used as value");
        break;
    }

    default :
        invalidStaticObjectError(x);
        break;

    }
}



//
// codegenGVarInstance
//

static void printModuleQualification(llvm::raw_ostream &os, ModulePtr mod)
{
    if (mod != NULL)
        os << mod->moduleName << '.';
}

void codegenGVarInstance(GVarInstancePtr x)
{
    assert(!x->llGlobal);
    MultiPValuePtr mpv = safeAnalyzeGVarInstance(x);
    assert(mpv->size() == 1);
    PVData const &y = mpv->values[0];
    llvm::Constant *initializer =
        llvm::Constant::getNullValue(llvmType(y.type));

    llvm::SmallString<128> nameBuf;
    llvm::raw_svector_ostream nameStr(nameBuf);
    printModuleQualification(nameStr, safeLookupModule(x->env));
    nameStr << x->gvar->name->str;
    if (!x->params.empty()) {
        nameStr << '[';
        printNameList(nameStr, x->params);
        nameStr << ']';
    }

    llvm::SmallString<128> symbolBuf;
    llvm::raw_svector_ostream symbolStr(symbolBuf);
    symbolStr << nameStr.str() << " " << y.type << " clay";

    x->staticGlobal = new ValueHolder(y.type);
    x->llGlobal = new llvm::GlobalVariable(
        *llvmModule, llvmType(y.type), false,
        llvm::GlobalVariable::InternalLinkage,
        initializer, symbolStr.str());
    if (llvmDIBuilder != NULL) {
        int line, column;
        llvm::DIFile file = getDebugLineCol(x->gvar->location, line, column);
        x->debugInfo = (llvm::MDNode*)llvmDIBuilder->createGlobalVariable(
            nameStr.str(),
            file,
            line,
            llvmTypeDebugInfo(y.type),
            true, // isLocalToUnit
            x->llGlobal
        );
    }

    // generate initializer
    ExprPtr lhs;
    if (x->gvar->hasParams()) {
        ExprListPtr indexingArgs = new ExprList();
        for (unsigned i = 0; i < x->params.size(); ++i)
            indexingArgs->add(new ObjectExpr(x->params[i]));
        lhs = new Indexing(new NameRef(x->gvar->name), indexingArgs);
    }
    else {
        lhs = new NameRef(x->gvar->name);
    }
    lhs->location = x->gvar->name->location;
    StatementPtr init = new InitAssignment(lhs, x->expr);
    init->location = x->gvar->location;
    bool result = codegenStatement(init, x->env, constructorsCtx);
    assert(!result);

    // generate destructor procedure body
    InvokeEntry* entry =
        codegenCallable(operator_destroy(),
                        vector<TypePtr>(1, y.type),
                        vector<ValueTempness>(1, TEMPNESS_LVALUE));

    initializedGlobals.push_back(new CValue(y.type, x->llGlobal));
}



//
// codegenExternalVariable
//

void codegenExternalVariable(ExternalVariablePtr x)
{
    assert(!x->llGlobal);
    if (!x->attributesVerified)
        verifyAttributes(x);
    PVData pv = analyzeExternalVariable(x);
    llvm::GlobalVariable::LinkageTypes linkage;
    if (x->attrDLLImport)
        linkage = llvm::GlobalVariable::DLLImportLinkage;
    else if (x->attrDLLExport)
        linkage = llvm::GlobalVariable::DLLExportLinkage;
    else
        linkage = llvm::GlobalVariable::ExternalLinkage;
    x->llGlobal =
        new llvm::GlobalVariable(
            *llvmModule, llvmType(pv.type), false,
            linkage, NULL, x->name->str.str());
    if (llvmDIBuilder != NULL) {
        int line, column;
        llvm::DIFile file = getDebugLineCol(x->location, line, column);
        x->debugInfo = (llvm::MDNode*)llvmDIBuilder->createGlobalVariable(
            x->name->str,
            file,
            line,
            llvmTypeDebugInfo(x->type2),
            false, // isLocalToUnit
            x->llGlobal
        );
    }

}


//
// codegenExternalProcedure
//

void codegenExternalProcedure(ExternalProcedurePtr x, bool codegenBody)
{
    CompileContextPusher pusher(x.ptr());

    ExternalTargetPtr target = getExternalTarget();

    if (x->llvmFunc != NULL && (!codegenBody || x->bodyCodegenned))
        return;
    if (!x->analyzed)
        analyzeExternalProcedure(x);
    assert(x->analyzed);
    vector<llvm::Type *> llArgTypes;
    vector< pair<unsigned, llvm::Attributes> > llAttributes;
    llvm::Type *llRetType =
        target->pushReturnType(x->callingConv, x->returnType2, llArgTypes, llAttributes);
    for (unsigned i = 0; i < x->args.size(); ++i)
        target->pushArgumentType(x->callingConv, x->args[i]->type2, llArgTypes, llAttributes);
    llvm::FunctionType *llFuncType =
        llvm::FunctionType::get(llRetType, llArgTypes, x->hasVarArgs);

    llvm::GlobalVariable::LinkageTypes linkage;
    if (x->attrDLLImport)
        linkage = llvm::GlobalVariable::DLLImportLinkage;
    else if (x->attrDLLExport)
        linkage = llvm::GlobalVariable::DLLExportLinkage;
    else
        linkage = llvm::GlobalVariable::ExternalLinkage;

    string llvmFuncName;
    if (!x->attrAsmLabel.empty()) {
        llvm::StringRef llvmIntrinsicsPrefix = "llvm.";
        llvm::StringRef prefix(&x->attrAsmLabel[0], llvmIntrinsicsPrefix.size());
        if (prefix != llvmIntrinsicsPrefix) {
            // '\01' is the llvm marker to specify asm label
            llvmFuncName.push_back('\01');
            llvmFuncName.append(x->attrAsmLabel.begin(), x->attrAsmLabel.end());
        }
        else {
            llvmFuncName.append(x->attrAsmLabel.begin(), x->attrAsmLabel.end());
        }
    }
    else {
        llvmFuncName.append(x->name->str.begin(), x->name->str.end());
    }
    
    llvm::DIFile file(NULL);
    int line = 0, column = 0;
    if (x->llvmFunc == NULL) {
        llvm::Function *func = llvmModule->getFunction(llvmFuncName);
        if (!func) {
            func = llvm::Function::Create(llFuncType,
                                          linkage,
                                          llvmFuncName,
                                          llvmModule);
        }
        x->llvmFunc = func;
        llvm::CallingConv::ID callingConv = target->callingConvention(x->callingConv);
        x->llvmFunc->setCallingConv(callingConv);
        for (vector< pair<unsigned, llvm::Attributes> >::iterator attr = llAttributes.begin();
             attr != llAttributes.end();
             ++attr)
            func->addAttribute(attr->first, attr->second);

        if (llvmDIBuilder != NULL) {
            file = getDebugLineCol(x->location, line, column);

            vector<llvm::Value*> debugParamTypes;
            if (x->returnType2 == NULL)
                debugParamTypes.push_back(llvmVoidTypeDebugInfo());
            else
                debugParamTypes.push_back(llvmTypeDebugInfo(x->returnType2));
            for (unsigned i = 0; i < x->args.size(); ++i)
                debugParamTypes.push_back(llvmTypeDebugInfo(x->args[i]->type2));

            llvm::DIArray debugParamArray = llvmDIBuilder->getOrCreateArray(
                llvm::makeArrayRef(debugParamTypes));

            llvm::DIType typeDebugInfo = llvmDIBuilder->createSubroutineType(
                file, // file
                debugParamArray);

            x->debugInfo = (llvm::MDNode*)llvmDIBuilder->createFunction(
                lookupModuleDebugInfo(x->env), // scope
                x->name->str, // name
                llvmFuncName, // linkage name
                file, // file
                line, // line
                typeDebugInfo, // type
                false, // isLocalToUnit
                !!x->body, // isDefinition XXX
                0, // flags
                false, // isOptimized
                x->llvmFunc // function
            );
        }
    }

    if (!codegenBody)
        return;

    x->bodyCodegenned = true;
    if (!x->body)
        return;

    CodegenContext ctx(x->llvmFunc);

    if (llvmDIBuilder != NULL) {
        ctx.pushDebugScope(llvmDIBuilder->createLexicalBlock(
            x->getDebugInfo(),
            file,
            line,
            column
            ));
    }

    llvm::BasicBlock *initBlock = newBasicBlock("init", &ctx);
    llvm::BasicBlock *codeBlock = newBasicBlock("code", &ctx);
    llvm::BasicBlock *returnBlock = newBasicBlock("return", &ctx);
    llvm::BasicBlock *exceptionBlock = newBasicBlock("exception", &ctx);

    ctx.initBuilder = new llvm::IRBuilder<>(initBlock);
    ctx.builder = new llvm::IRBuilder<>(codeBlock);

    if (llvmDIBuilder != NULL) {
        llvm::DebugLoc debugLoc = llvm::DebugLoc::get(line, column, x->getDebugInfo());
        ctx.initBuilder->SetCurrentDebugLocation(debugLoc);
        ctx.builder->SetCurrentDebugLocation(debugLoc);
    }

    ctx.exceptionValue = ctx.initBuilder->CreateAlloca(exceptionReturnType(), NULL, "exception");

    EnvPtr env = new Env(x->env);
    vector<CReturn> returns;

    llvm::Function::arg_iterator ai = x->llvmFunc->arg_begin();

    target->allocReturnValue(x->callingConv, x->returnType2, ai, returns, &ctx);

    for (unsigned i = 0; i < x->args.size(); ++i) {
        ExternalArgPtr arg = x->args[i];
        CValuePtr cvalue = target->allocArgumentValue(
            x->callingConv, arg->type2, arg->name->str, ai, &ctx);
        addLocal(env, arg->name, cvalue.ptr());
        if (llvmDIBuilder != NULL) {
            int line, column;
            Location argLocation = arg->location;
            llvm::DIFile file = getDebugLineCol(argLocation, line, column);
            llvm::DIVariable debugVar = llvmDIBuilder->createLocalVariable(
                llvm::dwarf::DW_TAG_arg_variable, // tag
                x->getDebugInfo(), // scope
                arg->name->str, // name
                file, // file
                line, // line
                llvmTypeDebugInfo(arg->type2), // type
                true, // alwaysPreserve
                0, // flags
                i+1 // argNo
                );
            llvm::Value *argValue = cvalue->llValue;
            llvmDIBuilder->insertDeclare(
                argValue,
                debugVar,
                initBlock)
                ->setDebugLoc(ctx.initBuilder->getCurrentDebugLocation());
        }
    }

    ctx.returnLists.push_back(returns);
    JumpTarget returnTarget(returnBlock, cgMarkStack(&ctx));
    ctx.returnTargets.push_back(returnTarget);
    JumpTarget exceptionTarget(exceptionBlock, cgMarkStack(&ctx));
    ctx.exceptionTargets.push_back(exceptionTarget);

    bool terminated = codegenStatement(x->body, env, &ctx);
    if (!terminated) {
        if (x->returnType2.ptr()) {
            error(x, "not all paths have a return statement");
        }
        cgDestroyStack(returnTarget.stackMarker, &ctx, false);
        ctx.builder->CreateBr(returnBlock);
    }
    cgPopStack(returnTarget.stackMarker, &ctx);

    returnBlock->moveAfter(ctx.builder->GetInsertBlock());
    exceptionBlock->moveAfter(returnBlock);

    ctx.builder->SetInsertPoint(returnBlock);
    target->returnStatement(x->callingConv, x->returnType2, returns, &ctx);

    ctx.builder->SetInsertPoint(exceptionBlock);
    if (exceptionsEnabled()) {
        ObjectPtr callable = operator_unhandledExceptionInExternal();
        codegenCallValue(staticCValue(callable, &ctx),
                         new MultiCValue(),
                         &ctx,
                         new MultiCValue());
    }
    ctx.builder->CreateUnreachable();

    ctx.initBuilder->CreateBr(codeBlock);
}



//
// codegenValueHolder
//

void codegenValueHolder(ValueHolderPtr v,
                        CodegenContext* ctx,
                        MultiCValuePtr out)
{
    EValuePtr ev = new EValue(v->type, v->buf);
    codegenCompileTimeValue(ev, ctx, out);
}



//
// codegenCompileTimeValue
//

void codegenCompileTimeValue(EValuePtr ev,
                             CodegenContext* ctx,
                             MultiCValuePtr out)
{
    assert(out->size() == 1);
    CValuePtr out0 = out->values[0];
    assert(out0->type == ev->type);

    switch (ev->type->typeKind) {

    case BOOL_TYPE :
    case INTEGER_TYPE :
    case FLOAT_TYPE : {
        llvm::Value *llv = codegenSimpleConstant(ev);
        ctx->builder->CreateStore(llv, out0->llValue);
        break;
    }

    case COMPLEX_TYPE : {
        ComplexType *tt = (ComplexType *)ev->type.ptr();
        const llvm::StructLayout *layout = complexTypeLayout(tt);
        char *srcPtr = ev->addr + layout->getElementOffset(1);
        EValuePtr evSrc = new EValue(floatType(tt->bits), srcPtr);
        llvm::Value *destPtr = ctx->builder->CreateConstGEP2_32(out0->llValue, 0, 0);
        CValuePtr cgDest = new CValue(floatType(tt->bits), destPtr);
        codegenCompileTimeValue(evSrc, ctx, new MultiCValue(cgDest));
        char *srcPtr2 = ev->addr + layout->getElementOffset(0);
        EValuePtr evSrc2 = new EValue(imagType(tt->bits), srcPtr2);
        llvm::Value *destPtr2 = ctx->builder->CreateConstGEP2_32(out0->llValue, 0, 1);
        CValuePtr cgDest2 = new CValue(imagType(tt->bits), destPtr2);
        codegenCompileTimeValue(evSrc2, ctx, new MultiCValue(cgDest2));
        break;
    }

    case POINTER_TYPE :
    case CODE_POINTER_TYPE :
    case CCODE_POINTER_TYPE :
        error("pointer constants are not supported");
        break;

    case STATIC_TYPE : {
        break;
    }

    case TUPLE_TYPE : {
        TupleType *tt = (TupleType *)ev->type.ptr();
        const llvm::StructLayout *layout = tupleTypeLayout(tt);
        for (unsigned i = 0; i < tt->elementTypes.size(); ++i) {
            char *srcPtr = ev->addr + layout->getElementOffset(i);
            EValuePtr evSrc = new EValue(tt->elementTypes[i], srcPtr);
            llvm::Value *destPtr =
                ctx->builder->CreateConstGEP2_32(out0->llValue, 0, i);
            CValuePtr cgDest = new CValue(tt->elementTypes[i], destPtr);
            codegenCompileTimeValue(evSrc, ctx, new MultiCValue(cgDest));
        }
        break;
    }
    default : {
        string buf;
        llvm::raw_string_ostream sout(buf);
        sout << "constants of type " << ev->type
             << " are not yet supported";
        error(sout.str());
        break;
    }

    }
}



//
// codegenSimpleConstant
//

template <typename T>
llvm::Value *
_sintConstant(EValuePtr ev)
{
    return llvm::ConstantInt::getSigned(llvmType(ev->type),
                                        *((T *)ev->addr));
}

template <typename T>
llvm::Value *
_uintConstant(EValuePtr ev)
{
    return llvm::ConstantInt::get(llvmType(ev->type),
                                  *((T *)ev->addr));
}


llvm::Value *codegenSimpleConstant(EValuePtr ev)
{
    llvm::Value *val = NULL;
    uint64_t bits[] = {0, 0};

    switch (ev->type->typeKind) {
    case BOOL_TYPE : {
        int bv = (*(bool *)ev->addr) ? 1 : 0;
        return llvm::ConstantInt::get(llvmType(boolType), bv);
    }
    case INTEGER_TYPE : {
        IntegerType *t = (IntegerType *)ev->type.ptr();
        if (t->isSigned) {
            switch (t->bits) {
            case 8 :
                val = _sintConstant<char>(ev);
                break;
            case 16 :
                val = _sintConstant<short>(ev);
                break;
            case 32 :
                val = _sintConstant<int>(ev);
                break;
            case 64 :
                val = _sintConstant<long long>(ev);
                break;
            case 128 :
                val = _sintConstant<clay_int128>(ev);
                break;
            default :
                assert(false);
            }
        }
        else {
            switch (t->bits) {
            case 8 :
                val = _uintConstant<unsigned char>(ev);
                break;
            case 16 :
                val = _uintConstant<unsigned short>(ev);
                break;
            case 32 :
                val = _uintConstant<unsigned int>(ev);
                break;
            case 64 :
                val = _uintConstant<unsigned long long>(ev);
                break;
            case 128 :
                val = _uintConstant<clay_uint128>(ev);
                break;
            default :
                assert(false);
            }
        }
        break;
    }
    case FLOAT_TYPE : {
        FloatType *t = (FloatType *)ev->type.ptr();
        switch (t->bits) {
        case 32 :
            val = llvm::ConstantFP::get(llvmType(t), *((float *)ev->addr));
            break;
        case 64 :
            val = llvm::ConstantFP::get(llvmType(t), *((double *)ev->addr));
            break;
        case 80 :
            //use APfloat to get an 80bit value
            bits[0] = *(uint64_t*)ev->addr;
            bits[1] = *(uint16_t*)((uint64_t*)ev->addr + 1);
            val = llvm::ConstantFP::get( llvm::getGlobalContext(), llvm::APFloat(llvm::APInt(80, 2, bits)));
            break;
        default :
            assert(false);
        }
        break;
    }

    default :
        assert(false);
    }
    return val;
}






//
// codegenIndexingExpr
//

void codegenIndexingExpr(ExprPtr indexable,
                         ExprListPtr args,
                         EnvPtr env,
                         CodegenContext* ctx,
                         MultiCValuePtr out)
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
            codegenAliasIndexing(x, args, env, ctx, out);
            return;
        }
        if (obj->objKind == GLOBAL_VARIABLE) {
            GlobalVariable *x = (GlobalVariable *)obj.ptr();
            GVarInstancePtr y = analyzeGVarIndexing(x, args, env);
            if (!y->llGlobal)
                codegenGVarInstance(y);
            assert(out->size() == 1);
            CValuePtr out0 = out->values[0];
            assert(out0->type == pointerType(y->type));
            ctx->builder->CreateStore(y->llGlobal, out0->llValue);
            return;
        }
    }
    ExprListPtr args2 = new ExprList(indexable);
    args2->add(args);
    codegenCallExpr(operator_expr_index(), args2, env, ctx, out);
}



//
// codegenAliasIndexing
//

void codegenAliasIndexing(GlobalAliasPtr x,
                          ExprListPtr args,
                          EnvPtr env,
                          CodegenContext* ctx,
                          MultiCValuePtr out)
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
    AnalysisCachingDisabler disabler;
    codegenExpr(x->expr, bodyEnv, ctx, out);
}



//
// codegenCallExpr
//

void codegenCallExpr(ExprPtr callable,
                     ExprListPtr args,
                     EnvPtr env,
                     CodegenContext* ctx,
                     MultiCValuePtr out)
{
    PVData pv = safeAnalyzeOne(callable, env);

    switch (pv.type->typeKind) {
    case CODE_POINTER_TYPE :
        CValuePtr cv = codegenOneAsRef(callable, env, ctx);
        MultiCValuePtr mcv = codegenMultiAsRef(args, env, ctx);
        codegenCallPointer(cv, mcv, ctx, out);
        return;
    }

    if ((pv.type->typeKind == CCODE_POINTER_TYPE)
        && (callable->exprKind == NAME_REF))
    {
        CCodePointerType *t = (CCodePointerType *)pv.type.ptr();
        NameRef *x = (NameRef *)callable.ptr();
        ObjectPtr y = safeLookupEnv(env, x->name);
        if (y->objKind == EXTERNAL_PROCEDURE) {
            ExternalProcedure *z = (ExternalProcedure *)y.ptr();
            if (!z->llvmFunc)
                codegenExternalProcedure(z, false);
            ExprListPtr args2 = new ExprList(callable);
            args2->add(args);
            codegenCallExpr(operator_expr_call(), args2, env, ctx, out);
            return;
        }
    }

    if (pv.type->typeKind != STATIC_TYPE) {
        ExprListPtr args2 = new ExprList(callable);
        args2->add(args);
        codegenCallExpr(operator_expr_call(), args2, env, ctx, out);
        return;
    }

    StaticType *st = (StaticType *)pv.type.ptr();
    ObjectPtr obj = st->obj;

    switch (obj->objKind) {

    case TYPE :
    case RECORD :
    case VARIANT :
    case PROCEDURE :
    case GLOBAL_ALIAS :
    case PRIM_OP : {
        if ((obj->objKind == PRIM_OP) && !isOverloadablePrimOp(obj)) {
            PrimOpPtr x = (PrimOp *)obj.ptr();
            MultiCValuePtr mcv = codegenMultiAsRef(args, env, ctx);
            codegenPrimOp(x, mcv, ctx, out);
            break;
        }
        vector<unsigned> dispatchIndices;
        MultiPValuePtr mpv = safeAnalyzeMultiArgs(args, env, dispatchIndices);
        if (dispatchIndices.size() > 0) {
            MultiCValuePtr mcv = codegenMultiArgsAsRef(args, env, ctx);
            codegenDispatch(obj, mcv, mpv, dispatchIndices, ctx, out);
            break;
        }
        if (codegenShortcut(obj, mpv, args, env, ctx, out))
            break;
        vector<TypePtr> argsKey;
        vector<ValueTempness> argsTempness;
        computeArgsKey(mpv, argsKey, argsTempness);
        CompileContextPusher pusher(obj, argsKey);
        InvokeEntry* entry = safeAnalyzeCallable(obj, argsKey, argsTempness);
        if (entry->callByName) {
            codegenCallByName(entry, callable, args, env, ctx, out);
        }
        else {
            assert(entry->analyzed);
            MultiCValuePtr mcv = codegenMultiAsRef(args, env, ctx);
            codegenCallCode(entry, mcv, ctx, out);
        }
        break;
    }

    default :
        error("invalid call expression");
        break;

    }
}



//
// codegenDispatch
//

llvm::Value *codegenDispatchTag(CValuePtr cv, CodegenContext* ctx)
{
    CValuePtr ctag = codegenAllocValue(cIntType, ctx);
    codegenCallValue(staticCValue(operator_dispatchTag(), ctx),
                     new MultiCValue(cv),
                     ctx,
                     new MultiCValue(ctag));
    return ctx->builder->CreateLoad(ctag->llValue);
}

CValuePtr codegenDispatchIndex(CValuePtr cv, PVData const &pvOut, int tag, CodegenContext* ctx)
{
    MultiCValuePtr args = new MultiCValue();
    args->add(cv);
    ValueHolderPtr vh = intToValueHolder(tag);
    args->add(staticCValue(vh.ptr(), ctx));

    CValuePtr cvOut = codegenAllocValueForPValue(pvOut, ctx);
    MultiCValuePtr out = new MultiCValue(cvOut);

    codegenCallValue(staticCValue(operator_dispatchIndex(), ctx),
                     args,
                     ctx,
                     out);
    return derefValueForPValue(cvOut, pvOut, ctx);
}

void codegenDispatch(ObjectPtr obj,
                     MultiCValuePtr args,
                     MultiPValuePtr pvArgs,
                     const vector<unsigned> &dispatchIndices,
                     CodegenContext* ctx,
                     MultiCValuePtr out)
{
    if (dispatchIndices.empty()) {
        codegenCallValue(staticCValue(obj, ctx), args, pvArgs, ctx, out);
        return;
    }

    vector<TypePtr> argsKey;
    vector<ValueTempness> argsTempness;
    computeArgsKey(pvArgs, argsKey, argsTempness);
    CompileContextPusher pusher(obj, argsKey);

    unsigned index = dispatchIndices[0];
    vector<unsigned> dispatchIndices2(dispatchIndices.begin() + 1,
                                      dispatchIndices.end());

    MultiCValuePtr prefix = new MultiCValue();
    MultiPValuePtr pvPrefix = new MultiPValue();
    for (unsigned i = 0; i < index; ++i) {
        prefix->add(args->values[i]);
        pvPrefix->add(pvArgs->values[i]);
    }
    MultiCValuePtr suffix = new MultiCValue();
    MultiPValuePtr pvSuffix = new MultiPValue();
    for (unsigned i = index+1; i < args->size(); ++i) {
        suffix->add(args->values[i]);
        pvSuffix->add(pvArgs->values[i]);
    }
    CValuePtr cvDispatch = args->values[index];
    PVData const &pvDispatch = pvArgs->values[index];

    int iMemberCount = dispatchTagCount(pvDispatch.type);
    if (iMemberCount <= 0)
        argumentError(index, "DispatchMemberCount for type must be positive");
    size_t memberCount = (size_t)iMemberCount;

    llvm::Value *llTag = codegenDispatchTag(cvDispatch, ctx);

    vector<llvm::BasicBlock *> callBlocks;
    vector<llvm::BasicBlock *> elseBlocks;

    for (size_t i = 0; i < memberCount; ++i) {
        callBlocks.push_back(newBasicBlock("dispatchCase", ctx));
        elseBlocks.push_back(newBasicBlock("dispatchNext", ctx));
    }
    llvm::BasicBlock *finalBlock = newBasicBlock("finalBlock", ctx);

    for (size_t i = 0; i < memberCount; ++i) {
        llvm::Value *tagCase = llvm::ConstantInt::get(llvmIntType(32), i);
        llvm::Value *cond = ctx->builder->CreateICmpEQ(llTag, tagCase);
        ctx->builder->CreateCondBr(cond, callBlocks[i], elseBlocks[i]);

        ctx->builder->SetInsertPoint(callBlocks[i]);

        MultiPValuePtr pvArgs2 = new MultiPValue();
        pvArgs2->add(pvPrefix);
        PVData pvDispatch2 = analyzeDispatchIndex(pvDispatch, i);
        pvArgs2->add(pvDispatch2);
        pvArgs2->add(pvSuffix);
        MultiCValuePtr args2 = new MultiCValue();
        args2->add(prefix);
        args2->add(codegenDispatchIndex(cvDispatch, pvDispatch2, i, ctx));
        args2->add(suffix);

        codegenDispatch(obj, args2, pvArgs2, dispatchIndices2, ctx, out);

        ctx->builder->CreateBr(finalBlock);

        ctx->builder->SetInsertPoint(elseBlocks[i]);
    }

    codegenCallValue(staticCValue(operator_invalidDispatch(), ctx),
                     new MultiCValue(cvDispatch),
                     ctx,
                     new MultiCValue());
    ctx->builder->CreateBr(finalBlock);

    ctx->builder->SetInsertPoint(finalBlock);
}



//
// codegenCallValue
//

void codegenCallValue(CValuePtr callable,
                      MultiCValuePtr args,
                      CodegenContext* ctx,
                      MultiCValuePtr out)
{
    MultiPValuePtr pvArgs = new MultiPValue();
    for (unsigned i = 0; i < args->size(); ++i)
        pvArgs->add(PVData(args->values[i]->type, false));
    codegenCallValue(callable, args, pvArgs, ctx, out);
}


void codegenCallValue(CValuePtr callable,
                      MultiCValuePtr args,
                      MultiPValuePtr pvArgs,
                      CodegenContext* ctx,
                      MultiCValuePtr out)
{
    switch (callable->type->typeKind) {
    case CODE_POINTER_TYPE :
        codegenCallPointer(callable, args, ctx, out);
        return;
    }

    if (callable->type->typeKind != STATIC_TYPE) {
        MultiCValuePtr args2 = new MultiCValue(callable);
        args2->add(args);
        MultiPValuePtr pvArgs2 =
            new MultiPValue(PVData(callable->type, false));
        pvArgs2->add(pvArgs);
        codegenCallValue(staticCValue(operator_call(), ctx),
                         args2,
                         pvArgs2,
                         ctx,
                         out);
        return;
    }

    StaticType *st = (StaticType *)callable->type.ptr();
    ObjectPtr obj = st->obj;

    switch (obj->objKind) {

    case TYPE :
    case RECORD :
    case VARIANT :
    case PROCEDURE :
    case GLOBAL_ALIAS :
    case PRIM_OP : {
        if ((obj->objKind == PRIM_OP) && !isOverloadablePrimOp(obj)) {
            PrimOpPtr x = (PrimOp *)obj.ptr();
            codegenPrimOp(x, args, ctx, out);
            break;
        }
        vector<TypePtr> argsKey;
        vector<ValueTempness> argsTempness;
        computeArgsKey(pvArgs, argsKey, argsTempness);
        CompileContextPusher pusher(obj, argsKey);
        InvokeEntry* entry = safeAnalyzeCallable(obj, argsKey, argsTempness);
        if (entry->callByName) {
            ExprListPtr objectExprs = new ExprList();
            for (vector<CValuePtr>::const_iterator i = args->values.begin();
                 i != args->values.end();
                 ++i)
            {
                objectExprs->add(new ObjectExpr(i->ptr()));
            }
            ExprPtr callableObject = new ObjectExpr(callable.ptr());
            callableObject->location = topLocation();
            codegenCallByName(entry, callableObject, objectExprs, new Env(), ctx, out);
        } else {
            assert(entry->analyzed);
            codegenCallCode(entry, args, ctx, out);
        }
        break;
    }

    default :
        error("invalid call operation");
        break;

    }
}



//
// codegenShortcut
//

bool codegenShortcut(ObjectPtr callable,
                     MultiPValuePtr pvArgs,
                     ExprListPtr args,
                     EnvPtr env,
                     CodegenContext* ctx,
                     MultiCValuePtr out)
{
    switch (callable->objKind) {
    case TYPE : {
        TypePtr t = (Type *)callable.ptr();
        if (!isPrimitiveAggregateType(t))
            return false;
        if (pvArgs->size() != 1)
            return false;
        PVData const &pv = pvArgs->values[0];
        if ((pv.type == t) && (!isPrimitiveAggregateTooLarge(t))) {
            MultiCValuePtr cvArgs = codegenMultiAsRef(args, env, ctx);
            assert(cvArgs->size() == 1);
            CValuePtr cv = cvArgs->values[0];
            assert(cv->type == t);
            assert(out->size() == 1);
            CValuePtr out0 = out->values[0];
            assert(out0->type == t);
            llvm::Value *v = ctx->builder->CreateLoad(cv->llValue);
            ctx->builder->CreateStore(v, out0->llValue);
            return true;
        }
        return false;
    }
    case PROCEDURE : {
        if (callable == operator_destroy()) {
            if (pvArgs->size() != 1)
                return false;
            if (!isPrimitiveAggregateType(pvArgs->values[0].type))
                return false;
            return true;
        }
        return false;
    }
    default :
        return false;
    }
}



//
// codegenCallPointer
//

void codegenCallPointer(CValuePtr x,
                        MultiCValuePtr args,
                        CodegenContext* ctx,
                        MultiCValuePtr out)
{
    assert(x->type->typeKind == CODE_POINTER_TYPE);
    CodePointerType *t = (CodePointerType *)x->type.ptr();
    ensureArity(args, t->argTypes.size());
    llvm::Value *llCallable = ctx->builder->CreateLoad(x->llValue);
    vector<llvm::Value *> llArgs;
    for (unsigned i = 0; i < args->size(); ++i) {
        CValuePtr cv = args->values[i];
        if (cv->type != t->argTypes[i])
            argumentTypeError(i, t->argTypes[i], cv->type);
        llArgs.push_back(cv->llValue);
    }
    assert(out->size() == t->returnTypes.size());
    for (unsigned i = 0; i < out->size(); ++i) {
        CValuePtr cv = out->values[i];
        if (t->returnIsRef[i])
            assert(cv->type == pointerType(t->returnTypes[i]));
        else
            assert(cv->type == t->returnTypes[i]);
        llArgs.push_back(cv->llValue);
    }
    codegenLowlevelCall(llCallable, llArgs.begin(), llArgs.end(), ctx);
}



//
// codegenCallCCode, codegenCallCCodePointer
//

void codegenCallCCode(CCodePointerTypePtr t,
                      llvm::Value *llCallable,
                      MultiCValuePtr args,
                      CodegenContext* ctx,
                      MultiCValuePtr out)
{
    ExternalTargetPtr target = getExternalTarget();

    if (!t->hasVarArgs)
        ensureArity(args, t->argTypes.size());
    else if (args->size() < t->argTypes.size())
        arityError2(t->argTypes.size(), args->size());
    vector<llvm::Value *> llArgs;
    vector< pair<unsigned, llvm::Attributes> > llAttributes;
    target->loadStructRetArgument(t->callingConv, t->returnType, llArgs, llAttributes, ctx, out);
    for (unsigned i = 0; i < t->argTypes.size(); ++i) {
        CValuePtr cv = args->values[i];
        if (cv->type != t->argTypes[i])
            argumentTypeError(i, t->argTypes[i], cv->type);
        target->loadArgument(t->callingConv, cv, llArgs, llAttributes, ctx);
    }
    if (t->hasVarArgs) {
        for (unsigned i = t->argTypes.size(); i < args->size(); ++i) {
            CValuePtr cv = args->values[i];
            target->loadVarArgument(t->callingConv, cv, llArgs, llAttributes, ctx);
        }
    }
    llvm::Value *llCastCallable = t->callingConv == CC_LLVM
        ? llCallable
        : ctx->builder->CreateBitCast(llCallable, t->getCallType());
    llvm::CallInst *callInst =
        ctx->builder->CreateCall(llCastCallable, llvm::makeArrayRef(llArgs));
    llvm::CallingConv::ID callingConv = target->callingConvention(t->callingConv);
    callInst->setCallingConv(callingConv);
    for (vector< pair<unsigned, llvm::Attributes> >::iterator attr = llAttributes.begin();
         attr != llAttributes.end();
         ++attr)
        callInst->addAttribute(attr->first, attr->second);

    llvm::Value *llRet = callInst;
    target->storeReturnValue(t->callingConv, llRet, t->returnType, ctx, out);
}

void codegenCallCCodePointer(CValuePtr x,
                             MultiCValuePtr args,
                             CodegenContext* ctx,
                             MultiCValuePtr out)
{
    llvm::Value *llCallable = ctx->builder->CreateLoad(x->llValue);
    assert(x->type->typeKind == CCODE_POINTER_TYPE);
    CCodePointerType *t = (CCodePointerType *)x->type.ptr();
    codegenCallCCode(t, llCallable, args, ctx, out);
}



//
// codegenCallCode
//

void codegenCallCode(InvokeEntry* entry,
                     MultiCValuePtr args,
                     CodegenContext* ctx,
                     MultiCValuePtr out)
{
    if (inlineEnabled() && entry->isInline==FORCE_INLINE && !entry->code->isLLVMBody()) {
        codegenCallInline(entry, args, ctx, out);
        return;
    }
    if (!entry->llvmFunc)
        codegenCodeBody(entry);
    assert(entry->llvmFunc);
    ensureArity(args, entry->argsKey.size());
    vector<llvm::Value *> llArgs;
    for (unsigned i = 0; i < args->size(); ++i) {
        CValuePtr cv = args->values[i];
        if (cv->type != entry->argsKey[i]){
            argumentTypeError(i, entry->argsKey[i], cv->type);
        }
        llArgs.push_back(cv->llValue);
    }
    assert(out->size() == entry->returnTypes.size());
    for (unsigned i = 0; i < out->size(); ++i) {
        CValuePtr cv = out->values[i];
        if (entry->returnIsRef[i])
            assert(cv->type == pointerType(entry->returnTypes[i]));
        else
            assert(cv->type == entry->returnTypes[i]);
        llArgs.push_back(cv->llValue);
    }
    codegenLowlevelCall(entry->llvmFunc, llArgs.begin(), llArgs.end(), ctx);
}



//
// codegenLowlevelCall - generate exception checked call
//

void codegenLowlevelCall(llvm::Value *llCallable,
                         vector<llvm::Value *>::iterator argBegin,
                         vector<llvm::Value *>::iterator argEnd,
                         CodegenContext* ctx)
{
    llvm::Value *result =
        ctx->builder->CreateCall(llCallable,
            llvm::makeArrayRef(&*argBegin, &*argEnd));
    if (!exceptionsEnabled())
        return;
    if (!ctx->checkExceptions)
        return;
    llvm::Value *noException = noExceptionReturnValue();
    llvm::Value *cond = ctx->builder->CreateICmpEQ(result, noException);
    llvm::BasicBlock *landing = newBasicBlock("landing", ctx);
    llvm::BasicBlock *normal = newBasicBlock("normal", ctx);
    ctx->builder->CreateCondBr(cond, normal, landing);

    ctx->builder->SetInsertPoint(landing);
    assert(ctx->exceptionValue != NULL);
    ctx->builder->CreateStore(result, ctx->exceptionValue);
    JumpTarget *jt = &ctx->exceptionTargets.back();
    cgDestroyStack(jt->stackMarker, ctx, true);
    // jt might be invalidated at this point
    jt = &ctx->exceptionTargets.back();
    ctx->builder->CreateBr(jt->block);
    ++jt->useCount;

    ctx->builder->SetInsertPoint(normal);
}



//
// codegenCallable
//

InvokeEntry* codegenCallable(ObjectPtr x,
                               const vector<TypePtr> &argsKey,
                               const vector<ValueTempness> &argsTempness)
{
    InvokeEntry* entry =
        safeAnalyzeCallable(x, argsKey, argsTempness);
    if (!entry->callByName) {
        if (!entry->llvmFunc)
            codegenCodeBody(entry);
    }
    return entry;
}



//
// interpolateLLVMCode
//

static bool isFirstIdentChar(char c) {
    if (c == '_') return true;
    if ((c >= 'a') && (c <= 'z')) return true;
    if ((c >= 'A') && (c <= 'Z')) return true;
    return false;
}

static bool isIdentChar(char c) {
    if (isFirstIdentChar(c)) return true;
    if ((c >= '0') && (c <= '9')) return true;
    if (c == '?') return true;
    return false;
}

static const char * parseIdentifier(const char * first, const char * last)
{
    const char * i = first;
    if ((i == last) || !isFirstIdentChar(*i))
        return first;
    ++i;
    while ((i != last) && isIdentChar(*i))
        ++i;
    return i;
}

static const char * parseBracketedExpr(const char * first,
                                       const char * last)
{
    const char * i = first;
    if ((i == last) || (*i != '{'))
        return first;
    ++i;
    int count = 1;
    while (i != last) {
        if (*i == '{')
            ++count;
        else if (*i == '}')
            --count;
        ++i;
        if (count == 0)
            break;
    }
    if (count != 0)
        return first;
    return i;
}

static void interpolateExpr(SourcePtr source, unsigned offset, unsigned length,
                            EnvPtr env, llvm::raw_ostream &outstream)
{
    ExprPtr expr = parseExpr(source, offset, length);
    ObjectPtr x = evaluateOneStatic(expr, env);
    LocationContext loc(expr->location);
    if (x->objKind == TYPE) {
        Type *type = (Type *)x.ptr();
        llvmType(type)->print(outstream);
    }
    else if (x->objKind == VALUE_HOLDER) {
        ValueHolder *vh = (ValueHolder *)x.ptr();
        if (vh->type->typeKind == BOOL_TYPE) {
            bool result = (*(char *)vh->buf) != 0;
            outstream << (result ? 1 : 0);
        }
        else if ((vh->type->typeKind == INTEGER_TYPE)
                 ||(vh->type->typeKind == FLOAT_TYPE))
        {
            printValue(outstream, new EValue(vh->type, vh->buf));
        }
        else {
            error("only booleans, integers, and float values are supported");
        }
    }
    else if (x->objKind == IDENTIFIER) {
        Identifier *y = (Identifier *)x.ptr();
        outstream << y->str;
    }
    else {
        printName(outstream, x);
    }
}

static bool interpolateLLVMCode(LLVMCodePtr llvmBody, string &out, EnvPtr env)
{
    SourcePtr source = llvmBody->location.source;
    int startingOffset = llvmBody->location.offset;

    llvm::StringRef body = llvmBody->body;
    llvm::raw_string_ostream outstream(out);
    const char *i = body.begin();
    while (i != body.end()) {
        if (*i != '$') {
            outstream << *i;
            ++i;
            continue;
        }

        const char *first, *last;
        first = i + 1;

        last = parseIdentifier(first, body.end());
        if (last != first) {
            unsigned offset = (first - body.begin()) + startingOffset;
            unsigned length = last - first;
            interpolateExpr(source, offset, length, env, outstream);
            i = last;
            continue;
        }

        last = parseBracketedExpr(first, body.end());
        if (last != first) {
            first += 1;
            last -= 1;
            unsigned offset = (first - body.begin()) + startingOffset;
            unsigned length = last - first;
            interpolateExpr(source, offset, length, env, outstream);
            i = last + 1;
            continue;
        }

        outstream << *i;
        ++i;
    }
    outstream.flush();
    return true;
}



//
// codegenLLVMBody
//

void codegenLLVMBody(InvokeEntry* entry, llvm::StringRef callableName)
{
    string llFunc;
    llvm::raw_string_ostream out(llFunc);
    int argCount = 0;
    static int id = 1;
    llvm::SmallString<128> functionNameBuf;
    llvm::raw_svector_ostream functionName(functionNameBuf);

    functionName << callableName << id;
    id++;

    out << string("define internal i8* @\"")
        << functionName.str() << string("\"(");

    vector<llvm::Type *> llArgTypes;
    for (unsigned i = 0; i < entry->argsKey.size(); ++i) {
        llvm::Type *argType = llvmPointerType(entry->argsKey[i]);
        if (argCount > 0) out << string(", ");
        argType->print(out);
        out << string(" %\"") << entry->fixedArgNames[i]->str << string("\"");
        argCount ++;
    }
    for (unsigned i = 0; i < entry->returnTypes.size(); ++i) {
        llvm::Type *argType;
        TypePtr rt = entry->returnTypes[i];
        if (entry->returnIsRef[i])
            argType = llvmPointerType(pointerType(rt));
        else
            argType = llvmPointerType(rt);
        if (argCount > 0) out << string(", ");
        argType->print(out);
        assert(i < entry->code->returnSpecs.size());
        const ReturnSpecPtr spec = entry->code->returnSpecs[i];
        assert(spec->name.ptr());
        out << string(" %") << spec->name->str;
        argCount ++;
    }

    out << ") ";
    if (entry->isInline==FORCE_INLINE)
        out << "alwaysinline ";

    string body;
    if (!interpolateLLVMCode(entry->code->llvmBody, body, entry->env))
        error("failed to apply template");

    out << body;

    llvm::SMDiagnostic err;
    llvm::MemoryBuffer *buf =
        llvm::MemoryBuffer::getMemBuffer(llvm::StringRef(out.str()));

    if(!llvm::ParseAssembly(buf, llvmModule, err,
                llvm::getGlobalContext())) {
        llvm::errs() << out.str();
        err.print("\n", llvm::errs());
        llvm::errs() << "\n";
        error("llvm assembly parse error");
    }

    entry->llvmFunc = llvmModule->getFunction(functionName.str());
    assert(entry->llvmFunc);
}



//
// getCodeName
//

static string getCodeName(InvokeEntry* entry)
{
    SafePrintNameEnabler enabler;
    ObjectPtr x = entry->callable;
    llvm::SmallString<128> buf;
    llvm::raw_svector_ostream sout(buf);

    printModuleQualification(sout, staticModule(x));

    switch (x->objKind) {
    case TYPE : {
        sout << x;
        break;
    }
    case RECORD : {
        Record *y = (Record *)x.ptr();
        sout << y->name->str;
        break;
    }
    case VARIANT : {
        Variant *y = (Variant *)x.ptr();
        sout << y->name->str;
        break;
    }
    case PROCEDURE : {
        Procedure *y = (Procedure *)x.ptr();
        sout << y->name->str;
        break;
    }
    case GLOBAL_ALIAS : {
        GlobalAlias *y = (GlobalAlias *)x.ptr();
        sout << y->name->str;
        break;
    }
    case PRIM_OP : {
        assert(isOverloadablePrimOp(x));
        sout << primOpName((PrimOp *)x.ptr());
        break;
    }
    default :
        assert(false);
    }
    sout << '(';
    for (unsigned i = 0; i < entry->argsKey.size(); ++i) {
        if (i != 0)
            sout << ", ";
        sout << entry->argsKey[i];
    }
    sout << ')';
    if (!entry->returnTypes.empty()) {
        sout << ' ';
        for (unsigned i = 0; i < entry->returnTypes.size(); ++i) {
            if (i != 0)
                sout << ", ";
            sout << entry->returnTypes[i];
        }
    }

    sout << " clay";

    return sout.str();
}



//
// codegenCodeBody
//

void codegenCodeBody(InvokeEntry* entry)
{
    assert(entry->analyzed);
    assert(!entry->llvmFunc);

    string callableName = getCodeName(entry);

    if (entry->code->isLLVMBody()) {
        codegenLLVMBody(entry, callableName);
        return;
    }

    vector<llvm::Type *> llArgTypes;
    for (unsigned i = 0; i < entry->argsKey.size(); ++i)
        llArgTypes.push_back(llvmPointerType(entry->argsKey[i]));
    for (unsigned i = 0; i < entry->returnTypes.size(); ++i) {
        TypePtr rt = entry->returnTypes[i];
        if (entry->returnIsRef[i])
            llArgTypes.push_back(llvmPointerType(pointerType(rt)));
        else
            llArgTypes.push_back(llvmPointerType(rt));
    }

    llvm::FunctionType *llFuncType =
        llvm::FunctionType::get(exceptionReturnType(), llArgTypes, false);

    string llvmFuncName = callableName;

    llvm::Function *llFunc =
        llvm::Function::Create(llFuncType,
                               llvm::Function::InternalLinkage,
                               llvmFuncName,
                               llvmModule);

    switch(entry->isInline){
    case INLINE : 
        if(inlineEnabled())
        llFunc->addFnAttr(llvm::Attribute::InlineHint);
        break;
    case NEVER_INLINE :
        llFunc->addFnAttr(llvm::Attribute::NoInline);
        break;
    default:
        break;
    }

    for (unsigned i = 1; i <= llArgTypes.size(); ++i) {
        llFunc->addAttribute(i, llvm::Attribute::NoAlias);
    }

    entry->llvmFunc = llFunc;

    CodegenContext ctx(entry->llvmFunc);

    int line, column;
    llvm::DIFile file;
    if (llvmDIBuilder != NULL) {
        file = getDebugLineCol(entry->origCode->location, line, column);

        vector<llvm::Value*> debugParamTypes;
        debugParamTypes.push_back(llvmVoidTypeDebugInfo());
        for (unsigned i = 0; i < entry->argsKey.size(); ++i) {
            llvm::DIType argType = llvmTypeDebugInfo(entry->argsKey[i]);
            llvm::DIType argRefType
                = llvmDIBuilder->createReferenceType(argType);
            debugParamTypes.push_back(argRefType);
        }
        for (unsigned i = 0; i < entry->returnTypes.size(); ++i) {
            llvm::DIType returnType = llvmTypeDebugInfo(entry->returnTypes[i]);
            llvm::DIType returnRefType = entry->returnIsRef[i]
                ? llvmDIBuilder->createReferenceType(
                    llvmDIBuilder->createReferenceType(returnType))
                : llvmDIBuilder->createReferenceType(returnType);

            debugParamTypes.push_back(returnRefType);
        }

        llvm::DIArray debugParamArray = llvmDIBuilder->getOrCreateArray(
            llvm::makeArrayRef(debugParamTypes));

        llvm::DIType typeDebugInfo = llvmDIBuilder->createSubroutineType(
            file,
            debugParamArray);

        entry->debugInfo = (llvm::MDNode*)llvmDIBuilder->createFunction(
            lookupModuleDebugInfo(entry->env),
            callableName,
            llvmFuncName,
            file,
            line,
            typeDebugInfo,
            true, // isLocalToUnit
            true, // isDefinition
            0,
            false, // isOptimized
            entry->llvmFunc
        );

        ctx.pushDebugScope(llvmDIBuilder->createLexicalBlock(
            entry->getDebugInfo(),
            file,
            line,
            column
            ));
    }

    llvm::BasicBlock *initBlock = newBasicBlock("init", &ctx);
    llvm::BasicBlock *codeBlock = newBasicBlock("code", &ctx);
    llvm::BasicBlock *returnBlock = newBasicBlock("return", &ctx);
    llvm::BasicBlock *exceptionBlock = newBasicBlock("exception", &ctx);

    ctx.initBuilder = new llvm::IRBuilder<>(initBlock);
    ctx.builder = new llvm::IRBuilder<>(codeBlock);

    if (llvmDIBuilder != NULL) {
        llvm::DebugLoc debugLoc = llvm::DebugLoc::get(line, column, entry->getDebugInfo());
        ctx.initBuilder->SetCurrentDebugLocation(debugLoc);
        ctx.builder->SetCurrentDebugLocation(debugLoc);
    }

    ctx.exceptionValue = ctx.initBuilder->CreateAlloca(exceptionReturnType(), NULL, "exception");

    EnvPtr env = new Env(entry->env);

    llvm::Function::arg_iterator ai = llFunc->arg_begin();
    unsigned argNo = 1;
    for (unsigned i = 0; i < entry->fixedArgTypes.size(); ++i, ++ai, ++argNo) {
        llvm::Argument *llArgValue = &(*ai);
        llArgValue->setName(entry->fixedArgNames[i]->str.str());
        CValuePtr cvalue = new CValue(entry->fixedArgTypes[i], llArgValue);
        cvalue->forwardedRValue = entry->forwardedRValueFlags[i];
        addLocal(env, entry->fixedArgNames[i], cvalue.ptr());
        if (llvmDIBuilder != NULL) {
            int line, column;
            Location argLocation = entry->origCode->formalArgs[i]->location;
            llvm::DIFile file = getDebugLineCol(argLocation, line, column);
            llvm::DebugLoc debugLoc = llvm::DebugLoc::get(line, column, entry->getDebugInfo());
            llvm::DIVariable debugVar = llvmDIBuilder->createLocalVariable(
                llvm::dwarf::DW_TAG_arg_variable, // tag
                entry->getDebugInfo(), // scope
                entry->fixedArgNames[i]->str, // name
                file, // file
                line, // line
                llvmDIBuilder->createReferenceType(
                    llvmTypeDebugInfo(entry->fixedArgTypes[i])), // type
                true, // alwaysPreserve
                0, // flags
                argNo
                );
            llvmDIBuilder->insertDeclare(
                llArgValue,
                debugVar,
                initBlock)
                ->setDebugLoc(debugLoc);
            llvm::Value *debugAlloca =
                ctx.initBuilder->CreateAlloca(llArgValue->getType());
            ctx.initBuilder->CreateStore(llArgValue, debugAlloca);
        }
    }

    if (entry->varArgName.ptr()) {
        unsigned nFixed = entry->fixedArgTypes.size();
        MultiCValuePtr varArgs = new MultiCValue();

        int line, column;
        Location argLocation = entry->origCode->formalVarArg->location;
        llvm::DIFile file = getDebugLineCol(argLocation, line, column);

        for (unsigned i = 0; i < entry->varArgTypes.size(); ++i, ++ai, ++argNo) {
            llvm::Argument *llArgValue = &(*ai);
            llvm::SmallString<128> buf;
            llvm::raw_svector_ostream sout(buf);
            sout << entry->varArgName << ".." << i;
            llArgValue->setName(sout.str());
            CValuePtr cvalue = new CValue(entry->varArgTypes[i], llArgValue);
            cvalue->forwardedRValue = entry->forwardedRValueFlags[i+nFixed];
            varArgs->add(cvalue);

            if (llvmDIBuilder != NULL) {
                llvm::DebugLoc debugLoc = llvm::DebugLoc::get(line, column, entry->getDebugInfo());
                llvm::DIVariable debugVar = llvmDIBuilder->createLocalVariable(
                    llvm::dwarf::DW_TAG_arg_variable, // tag
                    entry->getDebugInfo(), // scope
                    sout.str(), // name
                    file, // file
                    line, // line
                    llvmDIBuilder->createReferenceType(
                        llvmTypeDebugInfo(entry->varArgTypes[i])), // type
                    true, // alwaysPreserve
                    0, // flags
                    argNo
                    );
                llvmDIBuilder->insertDeclare(
                    llArgValue,
                    debugVar,
                    initBlock)
                    ->setDebugLoc(debugLoc);
                llvm::Value *debugAlloca =
                    ctx.initBuilder->CreateAlloca(llArgValue->getType());
                ctx.initBuilder->CreateStore(llArgValue, debugAlloca);
            }
        }
        addLocal(env, entry->varArgName, varArgs.ptr());
    }

    // XXX debug info for returns

    vector<CReturn> returns;
    for (unsigned i = 0; i < entry->returnTypes.size(); ++i, ++ai) {
        llvm::Argument *llArgValue = &(*ai);
        TypePtr rt = entry->returnTypes[i];
        CValuePtr cv;
        if (entry->returnIsRef[i])
            cv = new CValue(pointerType(rt), llArgValue);
        else
            cv = new CValue(rt, llArgValue);
        returns.push_back(CReturn(entry->returnIsRef[i], rt, cv));
    }

    bool hasNamedReturn = false;
    if (entry->code->hasReturnSpecs()) {
        const vector<ReturnSpecPtr> &returnSpecs = entry->code->returnSpecs;
        unsigned i = 0;
        for (; i < returnSpecs.size(); ++i, ++argNo) {
            ReturnSpecPtr rspec = returnSpecs[i];
            llvm::SmallString<128> buf;
            llvm::raw_svector_ostream sout(buf);
            if (rspec->name != NULL) {
                hasNamedReturn = true;
                addLocal(env, rspec->name, returns[i].value.ptr());
                sout << rspec->name->str;
            } else
                sout << "return.." << i;
            returns[i].value->llValue->setName(sout.str());

            if (rspec->name != NULL && llvmDIBuilder != NULL) {
                int line, column;
                Location argLocation = rspec->location;
                llvm::DIFile file = getDebugLineCol(argLocation, line, column);
                llvm::DebugLoc debugLoc = llvm::DebugLoc::get(line, column, entry->getDebugInfo());
                llvm::DIVariable debugVar = llvmDIBuilder->createLocalVariable(
                    llvm::dwarf::DW_TAG_return_variable, // tag
                    entry->getDebugInfo(), // scope
                    rspec->name->str, // name
                    file, // file
                    line, // line
                    llvmDIBuilder->createReferenceType(
                        llvmTypeDebugInfo(returns[i].type)), // type
                    true, // alwaysPreserve
                    0, // flags
                    argNo
                    );
                llvmDIBuilder->insertDeclare(
                    returns[i].value->llValue,
                    debugVar,
                    initBlock)
                    ->setDebugLoc(debugLoc);
                //llvm::Value *debugAlloca =
                //    ctx.initBuilder->CreateAlloca(llArgValue->getType());
                //ctx.initBuilder->CreateStore(llArgValue, debugAlloca);
            }
        }
        ReturnSpecPtr varReturnSpec = entry->code->varReturnSpec;
        if (!varReturnSpec)
            assert(i == entry->returnTypes.size());
        if (varReturnSpec.ptr() && varReturnSpec->name.ptr()) {
            hasNamedReturn = true;
            MultiCValuePtr mcv = new MultiCValue();
            for (; i < entry->returnTypes.size(); ++i)
                mcv->add(returns[i].value);
            addLocal(env, varReturnSpec->name, mcv.ptr());
        }
    }

    ctx.returnLists.push_back(returns);
    JumpTarget returnTarget(returnBlock, cgMarkStack(&ctx));
    ctx.returnTargets.push_back(returnTarget);
    JumpTarget exceptionTarget(exceptionBlock, cgMarkStack(&ctx));
    ctx.exceptionTargets.push_back(exceptionTarget);

    assert(entry->code->body.ptr());
    bool terminated = codegenStatement(entry->code->body, env, &ctx);
    if (!terminated) {
        if ((returns.size() > 0) && !hasNamedReturn) {
            error(entry->code, "not all paths have a return statement");
        }
        cgDestroyStack(returnTarget.stackMarker, &ctx, false);
        ctx.builder->CreateBr(returnBlock);
    }
    cgPopStack(returnTarget.stackMarker, &ctx);

    ctx.initBuilder->CreateBr(codeBlock);

    returnBlock->moveAfter(ctx.builder->GetInsertBlock());
    exceptionBlock->moveAfter(returnBlock);

    ctx.builder->SetInsertPoint(returnBlock);
    llvm::Value *llRet = noExceptionReturnValue();
    ctx.builder->CreateRet(llRet);

    ctx.builder->SetInsertPoint(exceptionBlock);
    assert(ctx.exceptionValue != NULL);
    llvm::Value *llExcept = ctx.builder->CreateLoad(ctx.exceptionValue);
    ctx.builder->CreateRet(llExcept);
}



//
// codegenCWrapper
//

void codegenCWrapper(InvokeEntry* entry, CallingConv cc)
{
    assert(!entry->llvmCWrappers[cc]);
    ExternalTargetPtr target = getExternalTarget();

    string callableName = getCodeName(entry);

    vector<llvm::Type *> llArgTypes;
    vector< pair<unsigned, llvm::Attributes> > llAttributes;
    TypePtr returnType;
    if (entry->returnTypes.empty()) {
        returnType = NULL;
    }
    else {
        assert(entry->returnTypes.size() == 1);
        assert(!entry->returnIsRef[0]);
        returnType = entry->returnTypes[0];
    }
    llvm::Type *llReturnType =
        target->pushReturnType(cc, returnType, llArgTypes, llAttributes);

    for (unsigned i = 0; i < entry->argsKey.size(); ++i)
        target->pushArgumentType(cc, entry->argsKey[i], llArgTypes, llAttributes);

    llvm::FunctionType *llFuncType =
        llvm::FunctionType::get(llReturnType, llArgTypes, false);

    std::string ccName;
    switch (cc) {
    case CC_DEFAULT:
        ccName = "cdecl ";
        break;
    case CC_STDCALL:
        ccName = "stdcall ";
        break;
    case CC_FASTCALL:
        ccName = "fastcall ";
        break;
    case CC_THISCALL:
        ccName = "thiscall ";
        break;
    default:
        assert(false);
    }

    llvm::Function *llCWrapper =
        llvm::Function::Create(llFuncType,
                               llvm::Function::InternalLinkage,
                               ccName + callableName,
                               llvmModule);
    for (vector< pair<unsigned, llvm::Attributes> >::const_iterator attr = llAttributes.begin();
         attr != llAttributes.end();
         ++attr)
        llCWrapper->addAttribute(attr->first, attr->second);

    llCWrapper->setCallingConv(target->callingConvention(cc));

    entry->llvmCWrappers[cc] = llCWrapper;
    CodegenContext ctx(llCWrapper);

    llvm::BasicBlock *llInitBlock =
        llvm::BasicBlock::Create(llvm::getGlobalContext(),
                                 "init",
                                 llCWrapper);
    llvm::BasicBlock *llBlock =
        llvm::BasicBlock::Create(llvm::getGlobalContext(),
                                 "code",
                                 llCWrapper);
    ctx.initBuilder = new llvm::IRBuilder<>(llInitBlock);
    ctx.builder     = new llvm::IRBuilder<>(llBlock);

    ctx.exceptionValue = ctx.initBuilder->CreateAlloca(exceptionReturnType(), NULL, "exception");

    vector<llvm::Value *> innerArgs;
    vector<CReturn> returns;
    llvm::Function::arg_iterator ai = llCWrapper->arg_begin();
    target->allocReturnValue(cc, returnType, ai, returns, &ctx);
    for (unsigned i = 0; i < entry->argsKey.size(); ++i) {
        CValuePtr cv = target->allocArgumentValue(cc, entry->argsKey[i], "x", ai, &ctx);
        innerArgs.push_back(cv->llValue);
    }

    for (vector<CReturn>::const_iterator ret = returns.begin();
         ret != returns.end();
         ++ret)
        innerArgs.push_back(ret->value->llValue);

    // XXX check exception
    ctx.builder->CreateCall(entry->llvmFunc, llvm::makeArrayRef(innerArgs));

    target->returnStatement(cc, returnType, returns, &ctx);

    ctx.initBuilder->CreateBr(llBlock);
}



//
// codegenCallInline
//

void codegenCallInline(InvokeEntry* entry,
                       MultiCValuePtr args,
                       CodegenContext* ctx,
                       MultiCValuePtr out)
{
    assert(entry->isInline==FORCE_INLINE);
    assert(ctx->inlineDepth >= 0);
    if (entry->code->isLLVMBody())
        error(entry->code, "llvm procedures cannot be inlined");

    ++ctx->inlineDepth;

    ensureArity(args, entry->argsKey.size());

    EnvPtr env = new Env(entry->env);
    for (unsigned i = 0; i < entry->fixedArgNames.size(); ++i) {
        CValuePtr cv = args->values[i];
        CValuePtr carg = new CValue(cv->type, cv->llValue);
        carg->forwardedRValue = entry->forwardedRValueFlags[i];
        assert(carg->type == entry->argsKey[i]);
        addLocal(env, entry->fixedArgNames[i], carg.ptr());
    }

    if (entry->varArgName.ptr()) {
        unsigned nFixed = entry->fixedArgTypes.size();
        MultiCValuePtr varArgs = new MultiCValue();
        for (unsigned i = 0; i < entry->varArgTypes.size(); ++i) {
            CValuePtr cv = args->values[nFixed + i];
            CValuePtr carg = new CValue(cv->type, cv->llValue);
            carg->forwardedRValue = entry->forwardedRValueFlags[nFixed + i];
            varArgs->add(carg);
        }
        addLocal(env, entry->varArgName, varArgs.ptr());
    }

    vector<CReturn> returns;
    for (unsigned i = 0; i < entry->returnTypes.size(); ++i) {
        TypePtr rt = entry->returnTypes[i];
        bool isRef = entry->returnIsRef[i];
        CValuePtr cv = out->values[i];
        CValuePtr cret = new CValue(cv->type, cv->llValue);
        if (isRef)
            assert(cret->type == pointerType(rt));
        else
            assert(cret->type == rt);
        returns.push_back(CReturn(isRef, rt, cret));
    }

    bool hasNamedReturn = false;
    if (entry->code->hasReturnSpecs()) {
        const vector<ReturnSpecPtr> &returnSpecs = entry->code->returnSpecs;
        unsigned i = 0;
        for (; i < returnSpecs.size(); ++i) {
            ReturnSpecPtr rspec = returnSpecs[i];
            if (rspec->name.ptr()) {
                hasNamedReturn = true;
                addLocal(env, rspec->name, returns[i].value.ptr());
            }
        }
        ReturnSpecPtr varReturnSpec = entry->code->varReturnSpec;
        if (!varReturnSpec)
            assert(i == entry->returnTypes.size());
        if (varReturnSpec.ptr() && (varReturnSpec->name.ptr())) {
            hasNamedReturn = true;
            MultiCValuePtr mcv = new MultiCValue();
            for (; i < entry->returnTypes.size(); ++i)
                mcv->add(returns[i].value);
            addLocal(env, varReturnSpec->name, mcv.ptr());
        }
    }

    ctx->returnLists.push_back(returns);
    llvm::BasicBlock *returnBlock = newBasicBlock("return", ctx);
    JumpTarget returnTarget(returnBlock, cgMarkStack(ctx));
    ctx->returnTargets.push_back(returnTarget);

    assert(entry->code->body.ptr());
    bool terminated = codegenStatement(entry->code->body, env, ctx);
    if (!terminated) {
        if ((returns.size() > 0) && !hasNamedReturn) {
            error(entry->code, "not all paths have a return statement");
        }
        cgDestroyStack(returnTarget.stackMarker, ctx, false);
        ctx->builder->CreateBr(returnBlock);
    }
    cgPopStack(returnTarget.stackMarker, ctx);

    ctx->returnLists.pop_back();
    ctx->returnTargets.pop_back();

    returnBlock->moveAfter(ctx->builder->GetInsertBlock());

    ctx->builder->SetInsertPoint(returnBlock);

    --ctx->inlineDepth;
    assert(ctx->inlineDepth >= 0);
}



//
// codegenCallByName
//

void codegenCallByName(InvokeEntry* entry,
                       ExprPtr callable,
                       ExprListPtr args,
                       EnvPtr env,
                       CodegenContext* ctx,
                       MultiCValuePtr out)
{
    assert(entry->callByName);
    assert(ctx->inlineDepth >= 0);
    if (entry->varArgName.ptr())
        assert(args->size() >= entry->fixedArgNames.size());
    else
        assert(args->size() == entry->fixedArgNames.size());

    ++ctx->inlineDepth;

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

    vector<CReturn> returns;
    for (unsigned i = 0; i < mpv->size(); ++i) {
        PVData const &pv = mpv->values[i];
        CValuePtr cv = out->values[i];
        if (pv.isTemp)
            assert(cv->type == pv.type);
        else
            assert(cv->type == pointerType(pv.type));
        returns.push_back(CReturn(!pv.isTemp, pv.type, cv));
    }

    bool hasNamedReturn = false;
    if (entry->code->hasReturnSpecs()) {
        const vector<ReturnSpecPtr> &returnSpecs = entry->code->returnSpecs;
        unsigned i = 0;
        for (; i < returnSpecs.size(); ++i) {
            ReturnSpecPtr rspec = returnSpecs[i];
            if (rspec->name.ptr()) {
                hasNamedReturn = true;
                addLocal(bodyEnv, rspec->name, returns[i].value.ptr());
            }
        }
        ReturnSpecPtr varReturnSpec = entry->code->varReturnSpec;
        if (!varReturnSpec)
            assert(i == mpv->size());
        if (varReturnSpec.ptr() && varReturnSpec->name.ptr()) {
            hasNamedReturn = true;
            MultiCValuePtr mcv = new MultiCValue();
            for (; i < mpv->size(); ++i)
                mcv->add(returns[i].value);
            addLocal(bodyEnv, varReturnSpec->name, mcv.ptr());
        }
    }

    llvm::BasicBlock *returnBlock = newBasicBlock("return", ctx);

    ctx->returnLists.push_back(returns);
    JumpTarget returnTarget(returnBlock, cgMarkStack(ctx));
    ctx->returnTargets.push_back(returnTarget);

    assert(entry->code->body.ptr());
    bool terminated = codegenStatement(entry->code->body, bodyEnv, ctx);
    if (!terminated) {
        if ((returns.size() > 0) && !hasNamedReturn) {
            error(entry->code, "not all paths have a return statement");
        }
        cgDestroyStack(returnTarget.stackMarker, ctx, false);
        ctx->builder->CreateBr(returnBlock);
    }
    cgPopStack(returnTarget.stackMarker, ctx);

    ctx->returnLists.pop_back();
    ctx->returnTargets.pop_back();

    returnBlock->moveAfter(ctx->builder->GetInsertBlock());

    ctx->builder->SetInsertPoint(returnBlock);

    --ctx->inlineDepth;
    assert(ctx->inlineDepth >= 0);
}



//
// codegenStatement
//


bool codegenStatement(StatementPtr stmt,
                      EnvPtr env,
                      CodegenContext* ctx);

static void codegenBlockStatement(BlockPtr block,
                                  unsigned i,
                                  StatementPtr stmt,
                                  EnvPtr &env,
                                  CodegenContext* ctx,
                                  bool &terminated)
{
    if (stmt->stmtKind == LABEL) {
        Label *label = (Label *)stmt.ptr();
        llvm::StringMap<JumpTarget>::iterator li =
            ctx->labels.find(label->name->str);
        assert(li != ctx->labels.end());
        JumpTarget &jt = li->second;
        if (!terminated) {
            ctx->builder->CreateBr(jt.block);
            ++jt.useCount;
        }
        ctx->builder->SetInsertPoint(jt.block);
    }
    else if (terminated) {
        error(stmt, "unreachable code");
    }
    else if (stmt->stmtKind == BINDING) {
        env = codegenBinding((Binding *)stmt.ptr(), env, ctx);
        codegenCollectLabels(block->statements, i+1, ctx);
    }
    else if (stmt->stmtKind == EVAL_STATEMENT) {
        EvalStatement *eval = (EvalStatement*)stmt.ptr();
        vector<StatementPtr> const &evaled = desugarEvalStatement(eval, env);
        for (unsigned i = 0; i < evaled.size(); ++i)
            codegenBlockStatement(block, i, evaled[i], env, ctx, terminated);
    } else if (stmt->stmtKind == WITH) {
        error("unexpected with statement");
    } else {
        terminated = codegenStatement(stmt, env, ctx);
    }
}

static EnvPtr codegenStatementExpressionStatements(vector<StatementPtr> const &stmts,
    EnvPtr env,
    CodegenContext* ctx)
{
    EnvPtr env2 = env;

    for (vector<StatementPtr>::const_iterator i = stmts.begin(), end = stmts.end();
         i != end;
         ++i)
    {
        switch ((*i)->stmtKind) {
        case BINDING: {
            env2 = codegenBinding((Binding*)i->ptr(), env2, ctx);
            break;
        }

        case ASSIGNMENT:
        case VARIADIC_ASSIGNMENT:
        case INIT_ASSIGNMENT:
        case EXPR_STATEMENT: {
            bool terminated = codegenStatement(*i, env2, ctx);
            assert(!terminated);
            break;
        }

        default:
            assert(false);
            return NULL;
        }
    }
    return env2;
}

static int codegenBeginScope(StatementPtr scopeStmt, CodegenContext* ctx)
{
    if (llvmDIBuilder != NULL) {
        llvm::DILexicalBlock outerScope = ctx->getDebugScope();
        int line, column;
        llvm::DIFile file = getDebugLineCol(scopeStmt->location, line, column);
        ctx->pushDebugScope(llvmDIBuilder->createLexicalBlock(
            outerScope,
            file,
            line,
            column
            ));
    }
    return cgMarkStack(ctx);
}

static void codegenEndScope(int marker, bool terminated, CodegenContext* ctx)
{
    if (!terminated)
        cgDestroyStack(marker, ctx, false);
    cgPopStack(marker, ctx);
    if (llvmDIBuilder != NULL)
        ctx->popDebugScope();
}

bool codegenStatement(StatementPtr stmt,
                      EnvPtr env,
                      CodegenContext* ctx)
{
    DebugLocationContext loc(stmt->location, ctx);
    
    switch (stmt->stmtKind) {

    case BLOCK : {
        Block *block = (Block *)stmt.ptr();
        int blockMarker = codegenBeginScope(stmt, ctx);
        codegenCollectLabels(block->statements, 0, ctx);
        bool terminated = false;
        for (unsigned i = 0; i < block->statements.size(); ++i) {
            codegenBlockStatement(block, i, block->statements[i], env, ctx, terminated);
        }
        codegenEndScope(blockMarker, terminated, ctx);
        return terminated;
    }

    case LABEL :
        error("invalid label. labels can only appear within blocks");

    case BINDING :
        error("invalid binding. bindings can only appear within blocks");

    case ASSIGNMENT : {
        Assignment *x = (Assignment *)stmt.ptr();
        MultiPValuePtr mpvLeft = safeAnalyzeMulti(x->left, env, 0);
        MultiPValuePtr mpvRight = safeAnalyzeMulti(x->right, env, mpvLeft->size());
        if (mpvLeft->size() != mpvRight->size())
            arityMismatchError(mpvLeft->size(), mpvRight->size());

        int tempMarker = markTemps(ctx);
        int marker = cgMarkStack(ctx);

        MultiPValuePtr mpvRight2;
        MultiCValuePtr mcvRight;
        if (mpvRight->size() == 1) {
            mcvRight = codegenMultiAsRef(x->right, env, ctx);
            mpvRight2 = mpvRight;
        }
        else {
            mpvRight2 = new MultiPValue();
            mcvRight = new MultiCValue();
            for (unsigned i = 0; i < mpvRight->size(); ++i) {
                PVData const &pv = mpvRight->values[i];
                PVData pv2(pv.type, true);
                mpvRight2->add(pv2);
                CValuePtr cv = codegenAllocValue(pv.type, ctx);
                mcvRight->add(cv);
            }
            codegenMultiInto(x->right, env, ctx, mcvRight, mpvLeft->size());
            for (unsigned i = 0; i < mcvRight->size(); ++i)
                cgPushStackValue(mcvRight->values[i], ctx);
        }
        unsigned j = 0;
        for (unsigned i = 0; i < x->left->size(); ++i) {
            ExprPtr leftExpr = x->left->exprs[i];
            if (leftExpr->exprKind == UNPACK) {
                ExprListPtr leftExprList = new ExprList();
                leftExprList->add(leftExpr);
                MultiPValuePtr mpvLeftI = safeAnalyzeMulti(leftExprList, env, 0);
                MultiPValuePtr mpvRightI = new MultiPValue();
                MultiCValuePtr mcvRightI = new MultiCValue();
                for (unsigned k = 0; k < mpvLeftI->size(); ++k) {
                    mpvRightI->add(mpvRight2->values[j + k]);
                    mcvRightI->add(mcvRight->values[j + k]);
                }
                codegenMultiExprAssign(
                    leftExprList, mcvRightI, mpvRightI, env, ctx
                );
                j += mpvLeftI->size();
            }
            else {
                codegenExprAssign(
                    leftExpr, mcvRight->values[j], mpvRight2->values[j],
                    env, ctx
                );
                j += 1;
            }
        }
        assert(j == mpvRight2->size());

        cgDestroyAndPopStack(marker, ctx, false);
        clearTemps(tempMarker, ctx);
        return false;
    }

    case INIT_ASSIGNMENT : {
        InitAssignment *x = (InitAssignment *)stmt.ptr();
        MultiPValuePtr mpvLeft = safeAnalyzeMulti(x->left, env, 0);
        MultiPValuePtr mpvRight = safeAnalyzeMulti(x->right, env, mpvLeft->size());
        if (mpvLeft->size() != mpvRight->size())
            arityMismatchError(mpvLeft->size(), mpvRight->size());
        for (unsigned i = 0; i < mpvLeft->size(); ++i) {
            if (mpvLeft->values[i].isTemp)
                argumentError(i, "cannot assign to a temporary");
            if (mpvLeft->values[i].type != mpvRight->values[i].type) {
                argumentTypeError(i,
                                  mpvLeft->values[i].type,
                                  mpvRight->values[i].type);
            }
        }
        int tempMarker = markTemps(ctx);
        int marker = cgMarkStack(ctx);
        MultiCValuePtr mcvLeft = codegenMultiAsRef(x->left, env, ctx);
        codegenMultiInto(x->right, env, ctx, mcvLeft, mpvLeft->size());
        cgDestroyAndPopStack(marker, ctx, false);
        clearTemps(tempMarker, ctx);
        return false;
    }

    case VARIADIC_ASSIGNMENT : {
        VariadicAssignment *x = (VariadicAssignment *)stmt.ptr();
        PVData pvLeft = safeAnalyzeOne(x->exprs->exprs[1], env);
        if (x->exprs->exprs[1]->exprKind == INDEXING) {
            Indexing *y = (Indexing *)x->exprs->exprs[1].ptr();
            PVData pvIndexable = safeAnalyzeOne(y->expr, env);
            if (pvIndexable.type->typeKind != STATIC_TYPE) {
                CallPtr call = new Call(
                    operator_expr_indexUpdateAssign(), new ExprList()
                );
                call->parenArgs->add(x->exprs->exprs[0]);
                call->parenArgs->add(y->expr);
                call->parenArgs->add(y->args);
                call->parenArgs->exprs.insert(call->parenArgs->exprs.end(), x->exprs->exprs.begin()+2, x->exprs->exprs.end());
                return codegenStatement(new ExprStatement(call.ptr()), env, ctx);
            }
        }
        else if (x->exprs->exprs[1]->exprKind == STATIC_INDEXING) {
            StaticIndexing *y = (StaticIndexing *)x->exprs->exprs[1].ptr();
            CallPtr call = new Call(
                operator_expr_staticIndexUpdateAssign(), new ExprList()
            );
            call->parenArgs->add(x->exprs->exprs[0]);
            call->parenArgs->add(y->expr);
            ValueHolderPtr vh = sizeTToValueHolder(y->index);
            call->parenArgs->add(new StaticExpr(new ObjectExpr(vh.ptr())));
            call->parenArgs->exprs.insert(call->parenArgs->exprs.end(), x->exprs->exprs.begin()+2, x->exprs->exprs.end());
            return codegenStatement(new ExprStatement(call.ptr()), env, ctx);
        }
        else if (x->exprs->exprs[1]->exprKind == FIELD_REF) {
            FieldRef *y = (FieldRef *)x->exprs->exprs[1].ptr();
            PVData pvBase = safeAnalyzeOne(y->expr, env);
            if (pvBase.type->typeKind != STATIC_TYPE) {
                CallPtr call = new Call(
                    operator_expr_fieldRefUpdateAssign(), new ExprList()
                );
                call->parenArgs->add(x->exprs->exprs[0]);
                call->parenArgs->add(y->expr);
                call->parenArgs->add(new ObjectExpr(y->name.ptr()));
                call->parenArgs->exprs.insert(call->parenArgs->exprs.end(), x->exprs->exprs.begin()+2, x->exprs->exprs.end());
                return codegenStatement(new ExprStatement(call.ptr()), env, ctx);
            }
        }
        CallPtr call;
        if (x->op == PREFIX_OP)
            call = new Call(operator_expr_prefixUpdateAssign(), new ExprList());
        else
            call = new Call(operator_expr_updateAssign(), new ExprList());        
        call->parenArgs->add(x->exprs);
        return codegenStatement(new ExprStatement(call.ptr()), env, ctx);
    }

    case GOTO : {
        Goto *x = (Goto *)stmt.ptr();
        llvm::StringMap<JumpTarget>::iterator li =
            ctx->labels.find(x->labelName->str);
        if (li == ctx->labels.end()) {
            string buf;
            llvm::raw_string_ostream sout(buf);
            sout << "goto label not found: " << x->labelName->str;
            error(sout.str());
        }
        JumpTarget &jt = li->second;
        cgDestroyStack(jt.stackMarker, ctx, false);
        ctx->builder->CreateBr(jt.block);
        ++ jt.useCount;
        return true;
    }

    case RETURN : {
        Return *x = (Return *)stmt.ptr();
        MultiPValuePtr mpv = safeAnalyzeMulti(x->values, env, 0);
        MultiCValuePtr mcv = new MultiCValue();
        const vector<CReturn> &returns = ctx->returnLists.back();
        ensureArity(mpv, returns.size());
        for (unsigned i = 0; i < mpv->size(); ++i) {
            PVData const &pv = mpv->values[i];
            bool byRef = returnKindToByRef(x->returnKind, pv);
            const CReturn &y = returns[i];
            if (y.type != pv.type)
                argumentTypeError(i, y.type, pv.type);
            if (byRef != y.byRef)
                argumentError(i, "mismatching by-ref and by-value returns");
            if (byRef && pv.isTemp)
                argumentError(i, "cannot return a temporary by reference");
            mcv->add(y.value);
        }
        switch (x->returnKind) {
        case RETURN_VALUE :
            codegenMultiInto(x->values, env, ctx, mcv, 0);
            break;
        case RETURN_REF : {
            MultiCValuePtr mcvRef = codegenMultiAsRef(x->values, env, ctx);
            assert(mcv->size() == mcvRef->size());
            for (unsigned i = 0; i < mcv->size(); ++i) {
                CValuePtr cvPtr = mcv->values[i];
                CValuePtr cvRef = mcvRef->values[i];
                ctx->builder->CreateStore(cvRef->llValue, cvPtr->llValue);
            }
            break;
        }
        case RETURN_FORWARD :
            codegenMulti(x->values, env, ctx, mcv, 0);
            break;
        default :
            assert(false);
        }
        JumpTarget *jt = &ctx->returnTargets.back();
        cgDestroyStack(jt->stackMarker, ctx, false);
        // jt might be invalidated at this point
        jt = &ctx->returnTargets.back();
        ctx->builder->CreateBr(jt->block);
        ++ jt->useCount;
        return true;
    }

    case IF : {
        If *x = (If *)stmt.ptr();

        int scopeMarker = codegenBeginScope(stmt, ctx);
        EnvPtr env2 = codegenStatementExpressionStatements(x->conditionStatements, env, ctx);

        int tempMarker = markTemps(ctx);
        int marker = cgMarkStack(ctx);
        CValuePtr cv = codegenOneAsRef(x->condition, env2, ctx);
        llvm::Value *cond = codegenToBoolFlag(cv, ctx);
        cgDestroyAndPopStack(marker, ctx, false);
        clearTemps(tempMarker, ctx);

        llvm::BasicBlock *trueBlock = newBasicBlock("ifTrue", ctx);
        llvm::BasicBlock *falseBlock = newBasicBlock("ifFalse", ctx);
        llvm::BasicBlock *mergeBlock = NULL;

        ctx->builder->CreateCondBr(cond, trueBlock, falseBlock);

        bool terminated1 = false;
        bool terminated2 = false;

        ctx->builder->SetInsertPoint(trueBlock);
        terminated1 = codegenStatement(x->thenPart, env2, ctx);
        if (!terminated1) {
            if (!mergeBlock)
                mergeBlock = newBasicBlock("ifMerge", ctx);
            ctx->builder->CreateBr(mergeBlock);
        }

        ctx->builder->SetInsertPoint(falseBlock);
        if (x->elsePart.ptr())
            terminated2 = codegenStatement(x->elsePart, env2, ctx);
        if (!terminated2) {
            if (!mergeBlock)
                mergeBlock = newBasicBlock("ifMerge", ctx);
            ctx->builder->CreateBr(mergeBlock);
        }

        if (!terminated1 || !terminated2)
            ctx->builder->SetInsertPoint(mergeBlock);

        codegenEndScope(scopeMarker, terminated1 && terminated2, ctx);

        return terminated1 && terminated2;
    }

    case SWITCH : {
        Switch *x = (Switch *)stmt.ptr();
        if (!x->desugared)
            x->desugared = desugarSwitchStatement(x);
        return codegenStatement(x->desugared, env, ctx);
    }

    case EVAL_STATEMENT : {
        EvalStatement *eval = (EvalStatement*)stmt.ptr();
        vector<StatementPtr> const &evaled = desugarEvalStatement(eval, env);
        bool terminated = false;
        for (unsigned i = 0; i < evaled.size(); ++i) {
            if (terminated)
                error(evaled[i], "unreachable code");
            terminated = codegenStatement(evaled[i], env, ctx);
        }
        return terminated;
    }

    case EXPR_STATEMENT : {
        ExprStatement *x = (ExprStatement *)stmt.ptr();
        int tempMarker = markTemps(ctx);
        int marker = cgMarkStack(ctx);
        codegenExprAsRef(x->expr, env, ctx);
        cgDestroyAndPopStack(marker, ctx, false);
        clearTemps(tempMarker, ctx);
        return false;
    }

    case WHILE : {
        While *x = (While *)stmt.ptr();

        llvm::BasicBlock *whileBegin = newBasicBlock("whileBegin", ctx);
        llvm::BasicBlock *whileBody = newBasicBlock("whileBody", ctx);
        llvm::BasicBlock *whileContinue = newBasicBlock("whileContinue", ctx);
        llvm::BasicBlock *whileEnd = newBasicBlock("whileEnd", ctx);

        ctx->builder->CreateBr(whileBegin);
        ctx->builder->SetInsertPoint(whileBegin);

        int scopeMarker = codegenBeginScope(stmt, ctx);
        EnvPtr env2 = codegenStatementExpressionStatements(x->conditionStatements, env, ctx);

        int tempMarker = markTemps(ctx);
        int marker = cgMarkStack(ctx);
        CValuePtr cv = codegenOneAsRef(x->condition, env2, ctx);
        llvm::Value *cond = codegenToBoolFlag(cv, ctx);
        cgDestroyAndPopStack(marker, ctx, false);
        clearTemps(tempMarker, ctx);

        ctx->builder->CreateCondBr(cond, whileBody, whileEnd);

        ctx->breaks.push_back(JumpTarget(whileEnd, cgMarkStack(ctx)));
        ctx->continues.push_back(JumpTarget(whileContinue, cgMarkStack(ctx)));
        ctx->builder->SetInsertPoint(whileBody);
        bool terminated = codegenStatement(x->body, env2, ctx);
        if (!terminated) {
            ctx->builder->CreateBr(whileContinue);
        }
        ctx->builder->SetInsertPoint(whileContinue);
        cgDestroyStack(scopeMarker, ctx, false);
        ctx->builder->CreateBr(whileBegin);

        bool breakUsed = (ctx->breaks.back().useCount > 0);
        ctx->breaks.pop_back();
        ctx->continues.pop_back();
        ctx->builder->SetInsertPoint(whileEnd);

        if (!breakUsed && (x->condition->exprKind == BOOL_LITERAL)) {
            BoolLiteral *y = (BoolLiteral *)x->condition.ptr();
            if (y->value) {
                // it's a "while (true)" loop with no 'break'
                ctx->builder->CreateUnreachable();
                codegenEndScope(scopeMarker, true, ctx);
                return true;
            }
        }

        codegenEndScope(scopeMarker, false, ctx);
        return false;
    }

    case BREAK : {
        if (ctx->breaks.empty())
            error("invalid break statement");
        JumpTarget *jt = &ctx->breaks.back();
        cgDestroyStack(jt->stackMarker, ctx, false);
        // jt might be invalidated at this point
        jt = &ctx->breaks.back();
        ctx->builder->CreateBr(jt->block);
        ++ jt->useCount;
        return true;
    }

    case CONTINUE : {
        if (ctx->continues.empty())
            error("invalid continue statement");
        JumpTarget *jt = &ctx->continues.back();
        cgDestroyStack(jt->stackMarker, ctx, false);
        // jt might be invalidated at this point
        jt = &ctx->continues.back();
        ctx->builder->CreateBr(jt->block);
        ++ jt->useCount;
        return true;
    }

    case FOR : {
        For *x = (For *)stmt.ptr();
        if (!x->desugared)
            x->desugared = desugarForStatement(x);
        return codegenStatement(x->desugared, env, ctx);
    }

    case FOREIGN_STATEMENT : {
        ForeignStatement *x = (ForeignStatement *)stmt.ptr();
        return codegenStatement(x->statement, x->getEnv(), ctx);
    }

    case TRY : {
        Try *x = (Try *)stmt.ptr();
        if (!exceptionsEnabled())
            return codegenStatement(x->tryBlock, env, ctx);

        if (!x->desugaredCatchBlock)
            x->desugaredCatchBlock = desugarCatchBlocks(x->catchBlocks);

        llvm::BasicBlock *catchBegin = newBasicBlock("catchBegin", ctx);
        llvm::BasicBlock *catchEnd = NULL;

        JumpTarget exceptionTarget(catchBegin, cgMarkStack(ctx));
        ctx->exceptionTargets.push_back(exceptionTarget);
        bool tryTerminated = codegenStatement(x->tryBlock, env, ctx);
        if (!tryTerminated) {
            if (!catchEnd)
                catchEnd = newBasicBlock("catchEnd", ctx);
            ctx->builder->CreateBr(catchEnd);
        }
        ctx->exceptionTargets.pop_back();

        ctx->builder->SetInsertPoint(catchBegin);
        bool catchTerminated =
            codegenStatement(x->desugaredCatchBlock, env, ctx);
        if (!catchTerminated) {
            if (!catchEnd)
                catchEnd = newBasicBlock("catchEnd", ctx);
            ctx->builder->CreateBr(catchEnd);
        }

        if (catchEnd)
            ctx->builder->SetInsertPoint(catchEnd);

        return tryTerminated && catchTerminated;
    }

    case THROW : {
        Throw *x = (Throw *)stmt.ptr();
        if (!x->expr)
            error("throw statements need an argument");
        ExprPtr callable = operator_expr_throwValue();
        ExprListPtr args = new ExprList(x->expr);
        int tempMarker = markTemps(ctx);
        int marker = cgMarkStack(ctx);
        codegenCallExpr(callable, args, env, ctx, new MultiCValue());
        cgDestroyAndPopStack(marker, ctx, false);
        clearTemps(tempMarker, ctx);
        ctx->builder->CreateUnreachable();
        return true;
    }

    case STATIC_FOR : {
        StaticFor *x = (StaticFor *)stmt.ptr();
        MultiCValuePtr mcv = codegenForwardMultiAsRef(x->values, env, ctx);
        initializeStaticForClones(x, mcv->size());
        bool terminated = false;
        for (unsigned i = 0; i < mcv->size(); ++i) {
            if (terminated) {
                string buf;
                llvm::raw_string_ostream ostr(buf);
                ostr << "unreachable code in iteration " << (i+1);
                error(ostr.str());
            }
            EnvPtr env2 = new Env(env);
            addLocal(env2, x->variable, mcv->values[i].ptr());
            terminated = codegenStatement(x->clonedBodies[i], env2, ctx);
        }
        return terminated;
    }

    case FINALLY : {
        Finally *x = (Finally *)stmt.ptr();
        cgPushStackStatement(FINALLY_STATEMENT, env, x->body, ctx);
        return false;
    }

    case ONERROR : {
        OnError *x = (OnError *)stmt.ptr();
        cgPushStackStatement(ONERROR_STATEMENT, env, x->body, ctx);
        return false;
    }

    case UNREACHABLE : {
        ctx->builder->CreateUnreachable();
        return true;
    }

    default :
        assert(false);
        return false;

    }
}



//
// codegenCollectLabels
//

void codegenCollectLabels(const vector<StatementPtr> &statements,
                          unsigned startIndex,
                          CodegenContext* ctx)
{
    for (unsigned i = startIndex; i < statements.size(); ++i) {
        StatementPtr x = statements[i];
        switch (x->stmtKind) {
        case LABEL : {
            Label *y = (Label *)x.ptr();
            llvm::StringMap<JumpTarget>::iterator li =
                ctx->labels.find(y->name->str);
            if (li != ctx->labels.end())
                error(x, "label redefined");
            llvm::BasicBlock *bb = newBasicBlock(y->name->str, ctx);
            ctx->labels[y->name->str] = JumpTarget(bb, cgMarkStack(ctx));
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
// codegenBinding
//

EnvPtr codegenBinding(BindingPtr x, EnvPtr env, CodegenContext* ctx)
{
    
    DebugLocationContext loc(x->location, ctx);

    int line, column;
    llvm::DIFile file = getDebugLineCol(x->location, line, column);
    switch(x->bindingKind){
    case VAR : {
        MultiPValuePtr mpv = safeAnalyzeMulti(x->values, env, x->args.size());
        MultiCValuePtr mcv = new MultiCValue();
        for (unsigned i = 0; i < mpv->values.size(); ++i) {
            CValuePtr cv = codegenAllocNewValue(mpv->values[i].type, ctx);
            mcv->add(cv);
            if (llvmDIBuilder != NULL) {
                llvm::DILexicalBlock debugBlock = ctx->getDebugScope();
                llvm::DIVariable debugVar = llvmDIBuilder->createLocalVariable(
                    llvm::dwarf::DW_TAG_auto_variable, // tag
                    debugBlock, // scope
                    x->args[i]->name->str, // name
                    file, // file
                    line, // line
                    llvmTypeDebugInfo(cv->type), // type
                    true, // alwaysPreserve
                    0, // flags
                    0 // argNo
                    );
                llvmDIBuilder->insertDeclare(
                    cv->llValue,
                    debugVar,
                    ctx->builder->GetInsertBlock())
                    ->setDebugLoc(ctx->builder->getCurrentDebugLocation());
            }
        }
        int tempMarker = markTemps(ctx);
        int marker = cgMarkStack(ctx);
        codegenMultiInto(x->values, env, ctx, mcv, x->args.size());
        cgDestroyAndPopStack(marker, ctx, false);
        clearTemps(tempMarker, ctx);
        EnvPtr env2 = new Env(env);
        for (unsigned i = 0; i < x->patternVars.size(); ++i)
            addLocal(env2, x->patternVars[i].name, x->patternTypes[i]);
        for (unsigned i = 0; i < x->args.size(); ++i) {
            CValuePtr cv = mcv->values[i];
            cgPushStackValue(cv, ctx);
            addLocal(env2, x->args[i]->name, cv.ptr());
            llvm::SmallString<128> buf;
            llvm::raw_svector_ostream ostr(buf);
            ostr << x->args[i]->name->str << ":" << cv->type;
            cv->llValue->setName(ostr.str());
        }
        if (x->varg.ptr()) {
            unsigned nFixed = x->args.size();
            MultiCValuePtr varArgs = new MultiCValue();
            for (unsigned i = nFixed; i < mcv->values.size(); ++i) {
                CValuePtr cv = mcv->values[i];
                cgPushStackValue(cv, ctx);
                varArgs->add(cv);
            }
            addLocal(env2, x->varg->name, varArgs.ptr());  
        }
        return env2;
    }

    case REF : {
        MultiPValuePtr mpv = safeAnalyzeMulti(x->values, env, x->args.size());
        MultiCValuePtr mcv = new MultiCValue();
        for (unsigned i = 0; i < mpv->values.size(); ++i) {
            PVData const &pv = mpv->values[i];
            if (pv.isTemp)
                argumentError(i, "ref can only bind to an lvalue");
            TypePtr ptrType = pointerType(pv.type);
            CValuePtr cvRef = codegenAllocNewValue(ptrType, ctx);
            mcv->add(cvRef);
            if (llvmDIBuilder != NULL) {
                llvm::DILexicalBlock debugBlock = ctx->getDebugScope();
                llvm::DIVariable debugVar = llvmDIBuilder->createLocalVariable(
                    llvm::dwarf::DW_TAG_auto_variable, // tag
                    debugBlock, // scope
                    x->args[i]->name->str, // name
                    file, // file
                    line, // line
                    llvmDIBuilder->createReferenceType(
                        llvmTypeDebugInfo(pv.type)), // type
                    true, // alwaysPreserve
                    0, // flags
                    0 // argNo
                    );
                llvmDIBuilder->insertDeclare(
                    cvRef->llValue,
                    debugVar,
                    ctx->builder->GetInsertBlock())
                    ->setDebugLoc(ctx->builder->getCurrentDebugLocation());
            }
        }
        int tempMarker = markTemps(ctx);
        int marker = cgMarkStack(ctx);
        codegenMulti(x->values, env, ctx, mcv, x->args.size());
        cgDestroyAndPopStack(marker, ctx, false);
        clearTemps(tempMarker, ctx);
        EnvPtr env2 = new Env(env);
        for (unsigned i = 0; i < x->patternVars.size(); ++i)
            addLocal(env2, x->patternVars[i].name, x->patternTypes[i]);
        for (unsigned i = 0; i < x->args.size(); ++i) {
            CValuePtr cv = derefValue(mcv->values[i], ctx);
            addLocal(env2, x->args[i]->name, cv.ptr());
            llvm::SmallString<128> buf;
            llvm::raw_svector_ostream ostr(buf);
            ostr << x->args[i]->name->str << ":" << cv->type;
            cv->llValue->setName(ostr.str());
            
        }
        if (x->varg.ptr()) {
            unsigned nFixed = x->args.size();
            MultiCValuePtr varArgs = new MultiCValue();
            for (unsigned i = nFixed; i < mcv->values.size(); ++i) {
                CValuePtr cv = derefValue(mcv->values[i], ctx);
                varArgs->add(cv);
            }
            addLocal(env2, x->varg->name, varArgs.ptr());  
        }
        return env2;
    }

    case FORWARD : {
        
        MultiPValuePtr mpv = safeAnalyzeMulti(x->values, env, x->args.size());
        MultiCValuePtr mcv = new MultiCValue();
        for (unsigned i = 0; i < mpv->values.size(); ++i) {
            PVData const &pv = mpv->values[i];
            CValuePtr cv;
            if (pv.isTemp) {
                cv = codegenAllocNewValue(pv.type, ctx);
            }
            else {
                TypePtr ptrType = pointerType(pv.type);
                cv = codegenAllocNewValue(ptrType, ctx);
            }
            mcv->add(cv);
            if (llvmDIBuilder != NULL) {
                llvm::DILexicalBlock debugBlock = ctx->getDebugScope();
                llvm::DIType debugType = llvmTypeDebugInfo(cv->type);
                llvm::DIVariable debugVar = llvmDIBuilder->createLocalVariable(
                    llvm::dwarf::DW_TAG_auto_variable, // tag
                    debugBlock, // scope
                    x->args[i]->name->str, // name
                    file, // file
                    line, // line
                    pv.isTemp
                        ? llvmTypeDebugInfo(pv.type)
                        : llvmDIBuilder->createReferenceType(
                            llvmTypeDebugInfo(pv.type)), // type
                    true, // alwaysPreserve
                    0, // flags
                    0 // argNo
                    );
                llvmDIBuilder->insertDeclare(
                    cv->llValue,
                    debugVar,
                    ctx->builder->GetInsertBlock())
                    ->setDebugLoc(ctx->builder->getCurrentDebugLocation());
            }
        }
        int tempMarker = markTemps(ctx);
        int marker = cgMarkStack(ctx);
        codegenMulti(x->values, env, ctx, mcv, x->args.size());
        cgDestroyAndPopStack(marker, ctx, false);
        clearTemps(tempMarker, ctx);
        EnvPtr env2 = new Env(env);
        for (unsigned i = 0; i < x->patternVars.size(); ++i)
            addLocal(env2, x->patternVars[i].name, x->patternTypes[i]);
        for (unsigned i = 0; i < x->args.size(); ++i) {
            CValuePtr rcv, cv;
            rcv = mcv->values[i];
            if (mpv->values[i].isTemp) {
                cv = rcv;
                cgPushStackValue(cv, ctx);
            }
            else {
                cv = derefValue(rcv, ctx);
            }
            addLocal(env2, x->args[i]->name, cv.ptr());
            llvm::SmallString<128> buf;
            llvm::raw_svector_ostream ostr(buf);
            ostr << x->args[i]->name->str << ":" << cv->type;
            cv->llValue->setName(ostr.str());
        }
        if (x->varg.ptr()) {
            unsigned nFixed = x->args.size();
            MultiCValuePtr varArgs = new MultiCValue();
            for (unsigned i = nFixed; i < mcv->values.size(); ++i) {
                CValuePtr rcv, cv;
                rcv = mcv->values[i];
                if (mpv->values[i].isTemp) {
                    cv = rcv;
                    cgPushStackValue(cv, ctx);
                }
                else {
                    cv = derefValue(rcv, ctx);
                }
                varArgs->add(cv);
            }
            addLocal(env2, x->varg->name, varArgs.ptr());  
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
// codegenExprAssign, codegenMultiExprAssign
//

void codegenExprAssign(ExprPtr left,
                       CValuePtr cvRight,
                       PVData const &pvRight,
                       EnvPtr env,
                       CodegenContext* ctx)
{
    if (left->exprKind == INDEXING) {
        Indexing *x = (Indexing *)left.ptr();
        ExprPtr indexable = x->expr;
        ExprListPtr args = x->args;
        PVData pvIndexable = safeAnalyzeOne(indexable, env);
        if (pvIndexable.type->typeKind != STATIC_TYPE) {
            MultiPValuePtr pvArgs = new MultiPValue(pvIndexable);
            pvArgs->add(safeAnalyzeMulti(args, env, 0));
            pvArgs->add(pvRight);
            CValuePtr cvIndexable = codegenOneAsRef(indexable, env, ctx);
            MultiCValuePtr cvArgs = new MultiCValue(cvIndexable);
            cvArgs->add(codegenMultiAsRef(args, env, ctx));
            cvArgs->add(cvRight);
            codegenCallValue(
                staticCValue(operator_indexAssign(), ctx),
                cvArgs, pvArgs, ctx,
                new MultiCValue()
            );
            return;
        }
    }
    else if (left->exprKind == STATIC_INDEXING) {
        StaticIndexing *x = (StaticIndexing *)left.ptr();
        ValueHolderPtr vh = sizeTToValueHolder(x->index);
        CValuePtr cvIndex = staticCValue(vh.ptr(), ctx);
        MultiPValuePtr pvArgs = new MultiPValue(safeAnalyzeOne(x->expr, env));
        pvArgs->add(PVData(cvIndex->type, true));
        pvArgs->add(pvRight);
        MultiCValuePtr cvArgs = new MultiCValue(codegenOneAsRef(x->expr, env, ctx));
        cvArgs->add(cvIndex);
        cvArgs->add(cvRight);
        codegenCallValue(
            staticCValue(operator_staticIndexAssign(), ctx),
            cvArgs, pvArgs, ctx,
            new MultiCValue()
        );
        return;
    }
    else if (left->exprKind == FIELD_REF) {
        FieldRef *x = (FieldRef *)left.ptr();
        ExprPtr base = x->expr;
        PVData pvBase = safeAnalyzeOne(base, env);
        if (pvBase.type->typeKind != STATIC_TYPE) {
            CValuePtr cvName = staticCValue(x->name.ptr(), ctx);
            MultiPValuePtr pvArgs = new MultiPValue(pvBase);
            pvArgs->add(PVData(cvName->type, true));
            pvArgs->add(pvRight);
            MultiCValuePtr cvArgs = new MultiCValue(codegenOneAsRef(base, env, ctx));
            cvArgs->add(cvName);
            cvArgs->add(cvRight);
            codegenCallValue(
                staticCValue(operator_fieldRefAssign(), ctx),
                cvArgs, pvArgs, ctx,
                new MultiCValue()
            );
            return;
        }
    }
    PVData pvLeft = safeAnalyzeOne(left, env);
    CValuePtr cvLeft = codegenOneAsRef(left, env, ctx);
    codegenValueAssign(pvLeft, cvLeft, pvRight, cvRight, ctx);
}

void codegenMultiExprAssign(ExprListPtr left,
                            MultiCValuePtr mcvRight,
                            MultiPValuePtr mpvRight,
                            EnvPtr env,
                            CodegenContext* ctx)
{
    MultiPValuePtr mpvLeft = safeAnalyzeMulti(left, env, 0);
    MultiCValuePtr mcvLeft = codegenMultiAsRef(left, env, ctx);
    assert(mpvLeft->size() == mcvLeft->size() && mcvLeft->size() == mcvRight->size());
    for (unsigned i = 0; i < mcvLeft->size(); ++i) {
        PVData const &pvLeft = mpvLeft->values[i];
        CValuePtr cvLeft = mcvLeft->values[i];
        PVData const &pvRight = mpvRight->values[i];
        CValuePtr cvRight = mcvRight->values[i];
        codegenValueAssign(pvLeft, cvLeft, pvRight, cvRight, ctx);
    }
}



//
// valueToStatic, valueToStaticSizeTOrInt
// valueToType, valueToNumericType, valueToIntegerType,
// valueToPointerLikeType, valueToTupleType, valueToRecordType,
// valueToVariantType, valueToEnumType, valueToIdentifier
//

static ObjectPtr valueToStatic(CValuePtr cv)
{
    if (cv->type->typeKind != STATIC_TYPE)
        return NULL;
    StaticType *st = (StaticType *)cv->type.ptr();
    return st->obj;
}

static ObjectPtr valueToStatic(MultiCValuePtr args, unsigned index)
{
    ObjectPtr obj = valueToStatic(args->values[index]);
    if (!obj)
        argumentError(index, "expecting a static value");
    return obj;
}

static size_t valueToStaticSizeTOrInt(MultiCValuePtr args, unsigned index)
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

static TypePtr valueToType(MultiCValuePtr args, unsigned index)
{
    ObjectPtr obj = valueToStatic(args->values[index]);
    if (!obj || (obj->objKind != TYPE))
        argumentError(index, "expecting a type");
    return (Type *)obj.ptr();
}

static TypePtr valueToNumericType(MultiCValuePtr args, unsigned index)
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

static ComplexTypePtr valueToComplexType(MultiCValuePtr args, unsigned index)
{
    TypePtr t = valueToType(args, index);
    if (t->typeKind != COMPLEX_TYPE)
        argumentTypeError(index, "complex type", t);
    return (ComplexType *)t.ptr();
}

static IntegerTypePtr valueToIntegerType(MultiCValuePtr args, unsigned index)
{
    TypePtr t = valueToType(args, index);
    if (t->typeKind != INTEGER_TYPE)
        argumentTypeError(index, "integer type", t);
    return (IntegerType *)t.ptr();
}

static TypePtr valueToPointerLikeType(MultiCValuePtr args, unsigned index)
{
    TypePtr t = valueToType(args, index);
    if (!isPointerOrCodePointerType(t))
        argumentTypeError(index, "pointer or code-pointer type", t);
    return t;
}

static TupleTypePtr valueToTupleType(MultiCValuePtr args, unsigned index)
{
    TypePtr t = valueToType(args, index);
    if (t->typeKind != TUPLE_TYPE)
        argumentTypeError(index, "tuple type", t);
    return (TupleType *)t.ptr();
}

static UnionTypePtr valueToUnionType(MultiCValuePtr args, unsigned index)
{
    TypePtr t = valueToType(args, index);
    if (t->typeKind != UNION_TYPE)
        argumentTypeError(index, "union type", t);
    return (UnionType *)t.ptr();
}

static RecordTypePtr valueToRecordType(MultiCValuePtr args, unsigned index)
{
    TypePtr t = valueToType(args, index);
    if (t->typeKind != RECORD_TYPE)
        argumentTypeError(index, "record type", t);
    return (RecordType *)t.ptr();
}

static VariantTypePtr valueToVariantType(MultiCValuePtr args, unsigned index)
{
    TypePtr t = valueToType(args, index);
    if (t->typeKind != VARIANT_TYPE)
        argumentTypeError(index, "variant type", t);
    return (VariantType *)t.ptr();
}

static EnumTypePtr valueToEnumType(MultiCValuePtr args, unsigned index)
{
    TypePtr t = valueToType(args, index);
    if (t->typeKind != ENUM_TYPE)
        argumentTypeError(index, "enum type", t);
    initializeEnumType((EnumType*)t.ptr());
    return (EnumType *)t.ptr();
}

static IdentifierPtr valueToIdentifier(MultiCValuePtr args, unsigned index)
{
    ObjectPtr obj = valueToStatic(args->values[index]);
    if (!obj || (obj->objKind != IDENTIFIER))
        argumentError(index, "expecting identifier value");
    return (Identifier *)obj.ptr();
}



//
// numericValue, integerValue, pointerValue,
// pointerLikeValue, cCodePointerValue,
// arrayValue, tupleValue, recordValue, variantValue, enumValue
//

static llvm::Value *numericValue(MultiCValuePtr args,
                                 unsigned index,
                                 TypePtr &type,
                                 CodegenContext* ctx)
{
    CValuePtr cv = args->values[index];
    if (type.ptr()) {
        if (cv->type != type)
            argumentTypeError(index, type, cv->type);
    }
    else {
        switch (cv->type->typeKind) {
        case INTEGER_TYPE :
        case FLOAT_TYPE :
            break;
        default :
            argumentTypeError(index, "numeric type", cv->type);
        }
        type = cv->type;
    }
    return ctx->builder->CreateLoad(cv->llValue);
}

static llvm::Value *floatValue(MultiCValuePtr args,
                               unsigned index,
                               FloatTypePtr &type,
                               CodegenContext* ctx)
{
    CValuePtr cv = args->values[index];
    if (type.ptr()) {
        if (cv->type.ptr() != type.ptr())
            argumentTypeError(index, type.ptr(), cv->type);
    }
    else {
        switch (cv->type->typeKind) {
        case FLOAT_TYPE :
            break;
        default :
            argumentTypeError(index, "float type", cv->type);
        }
        type = (FloatType*)cv->type.ptr();
    }
    return ctx->builder->CreateLoad(cv->llValue);
}

static llvm::Value *integerOrPointerLikeValue(MultiCValuePtr args,
                                              unsigned index,
                                              TypePtr &type,
                                              CodegenContext* ctx)
{
    CValuePtr cv = args->values[index];
    if (type.ptr()) {
        if (cv->type != type)
            argumentTypeError(index, type, cv->type);
    }
    else {
        switch (cv->type->typeKind) {
        case INTEGER_TYPE :
        case POINTER_TYPE :
        case CODE_POINTER_TYPE :
        case CCODE_POINTER_TYPE :
            break;
        default :
            argumentTypeError(index, "integer, pointer, or code pointer type", cv->type);
        }
        type = cv->type;
    }
    return ctx->builder->CreateLoad(cv->llValue);
}

static llvm::Value *complexValue(MultiCValuePtr args,
                               unsigned index,
                               ComplexTypePtr &type)
{
    CValuePtr cv = args->values[index];
    if (type.ptr()) {
        if (cv->type != (Type *)type.ptr())
            argumentTypeError(index, type.ptr(), cv->type);
    }
    else {
        if (cv->type->typeKind != COMPLEX_TYPE)
            argumentTypeError(index, "complex type", cv->type);
        type = (ComplexType *)cv->type.ptr();
    }
    return cv->llValue;
}

static void checkIntegerValue(MultiCValuePtr args,
                              unsigned index,
                              IntegerTypePtr &type,
                              CodegenContext* ctx)
{
    CValuePtr cv = args->values[index];
    if (type.ptr()) {
        if (cv->type != type.ptr())
            argumentTypeError(index, type.ptr(), cv->type);
    }
    else {
        if (cv->type->typeKind != INTEGER_TYPE)
            argumentTypeError(index, "integer type", cv->type);
        type = (IntegerType *)cv->type.ptr();
    }
}

static llvm::Value *integerValue(MultiCValuePtr args,
                                 unsigned index,
                                 IntegerTypePtr &type,
                                 CodegenContext* ctx)
{
    checkIntegerValue(args, index, type, ctx);
    CValuePtr cv = args->values[index];
    return ctx->builder->CreateLoad(cv->llValue);
}

static llvm::Value *pointerValue(MultiCValuePtr args,
                                 unsigned index,
                                 PointerTypePtr &type,
                                 CodegenContext* ctx)
{
    CValuePtr cv = args->values[index];
    if (type.ptr()) {
        if (cv->type != (Type *)type.ptr())
            argumentTypeError(index, type.ptr(), cv->type);
    }
    else {
        if (cv->type->typeKind != POINTER_TYPE)
            argumentTypeError(index, "pointer type", cv->type);
        type = (PointerType *)cv->type.ptr();
        llvmType(type->pointeeType); // force the pointee type to be refined
    }
    return ctx->builder->CreateLoad(cv->llValue);
}

static llvm::Value *pointerLikeValue(MultiCValuePtr args,
                                     unsigned index,
                                     TypePtr &type,
                                     CodegenContext* ctx)
{
    CValuePtr cv = args->values[index];
    if (type.ptr()) {
        if (cv->type != type)
            argumentTypeError(index, type, cv->type);
    }
    else {
        if (!isPointerOrCodePointerType(cv->type))
            argumentTypeError(index,
                              "pointer or code-pointer type",
                              cv->type);
        type = cv->type;
    }
    return ctx->builder->CreateLoad(cv->llValue);
}

static llvm::Value* cCodePointerValue(MultiCValuePtr args,
                                      unsigned index,
                                      CCodePointerTypePtr &type)
{
    CValuePtr cv = args->values[index];
    if (type.ptr()) {
        if (cv->type != (Type *)type.ptr())
            argumentTypeError(index, type.ptr(), cv->type);
    }
    else {
        if (cv->type->typeKind != CCODE_POINTER_TYPE)
            argumentTypeError(index, "c code pointer type", cv->type);
        type = (CCodePointerType *)cv->type.ptr();
    }
    return cv->llValue;
}

static llvm::Value *arrayValue(MultiCValuePtr args,
                               unsigned index,
                               ArrayTypePtr &type)
{
    CValuePtr cv = args->values[index];
    if (type.ptr()) {
        if (cv->type != (Type *)type.ptr())
            argumentTypeError(index, type.ptr(), cv->type);
    }
    else {
        if (cv->type->typeKind != ARRAY_TYPE)
            argumentTypeError(index, "array type", cv->type);
        type = (ArrayType *)cv->type.ptr();
    }
    return cv->llValue;
}

static llvm::Value *tupleValue(MultiCValuePtr args,
                               unsigned index,
                               TupleTypePtr &type)
{
    CValuePtr cv = args->values[index];
    if (type.ptr()) {
        if (cv->type != (Type *)type.ptr())
            argumentTypeError(index, type.ptr(), cv->type);
    }
    else {
        if (cv->type->typeKind != TUPLE_TYPE)
            argumentTypeError(index, "tuple type", cv->type);
        type = (TupleType *)cv->type.ptr();
    }
    return cv->llValue;
}

static llvm::Value *recordValue(MultiCValuePtr args,
                                unsigned index,
                                RecordTypePtr &type)
{
    CValuePtr cv = args->values[index];
    if (type.ptr()) {
        if (cv->type != (Type *)type.ptr())
            argumentTypeError(index, type.ptr(), cv->type);
    }
    else {
        if (cv->type->typeKind != RECORD_TYPE)
            argumentTypeError(index, "record type", cv->type);
        type = (RecordType *)cv->type.ptr();
    }
    return cv->llValue;
}

static llvm::Value *variantValue(MultiCValuePtr args,
                                 unsigned index,
                                 VariantTypePtr &type)
{
    CValuePtr cv = args->values[index];
    if (type.ptr()) {
        if (cv->type != (Type *)type.ptr())
            argumentTypeError(index, type.ptr(), cv->type);
    }
    else {
        if (cv->type->typeKind != VARIANT_TYPE)
            argumentTypeError(index, "variant type", cv->type);
        type = (VariantType *)cv->type.ptr();
    }
    return cv->llValue;
}

static llvm::Value *enumValue(MultiCValuePtr args,
                              unsigned index,
                              EnumTypePtr &type,
                              CodegenContext* ctx)
{
    CValuePtr cv = args->values[index];
    if (type.ptr()) {
        if (cv->type != (Type *)type.ptr())
            argumentTypeError(index, type.ptr(), cv->type);
    }
    else {
        if (cv->type->typeKind != ENUM_TYPE)
            argumentTypeError(index, "enum type", cv->type);
        type = (EnumType *)cv->type.ptr();
    }
    return ctx->builder->CreateLoad(cv->llValue);
}

llvm::AtomicOrdering atomicOrderValue(MultiCValuePtr args, unsigned index)
{
    CValuePtr cv = args->values[index];
    ObjectPtr obj = unwrapStaticType(cv->type);
    if (obj != NULL && obj->objKind == PRIM_OP) {
        PrimOp *prim = (PrimOp*)obj.ptr();
        switch (prim->primOpCode) {
        case PRIM_OrderUnordered:
            return llvm::Unordered;
        case PRIM_OrderMonotonic:
            return llvm::Monotonic;
        case PRIM_OrderAcquire:
            return llvm::Acquire;
        case PRIM_OrderRelease:
            return llvm::Release;
        case PRIM_OrderAcqRel:
            return llvm::AcquireRelease;
        case PRIM_OrderSeqCst:
            return llvm::SequentiallyConsistent;
        }
    }
    argumentTypeError(index, "atomic ordering", cv->type);
    return llvm::Unordered;
}

llvm::AtomicRMWInst::BinOp atomicRMWOpValue(MultiCValuePtr args, unsigned index)
{
    CValuePtr cv = args->values[index];
    ObjectPtr obj = unwrapStaticType(cv->type);
    if (obj != NULL && obj->objKind == PRIM_OP) {
        PrimOp *prim = (PrimOp*)obj.ptr();
        switch (prim->primOpCode) {
        case PRIM_RMWXchg:
            return llvm::AtomicRMWInst::Xchg;
        case PRIM_RMWAdd:
            return llvm::AtomicRMWInst::Add;
        case PRIM_RMWSubtract:
            return llvm::AtomicRMWInst::Sub;
        case PRIM_RMWAnd:
            return llvm::AtomicRMWInst::And;
        case PRIM_RMWNAnd:
            return llvm::AtomicRMWInst::Nand;
        case PRIM_RMWOr:
            return llvm::AtomicRMWInst::Or;
        case PRIM_RMWXor:
            return llvm::AtomicRMWInst::Xor;
        case PRIM_RMWMin:
            return llvm::AtomicRMWInst::Min;
        case PRIM_RMWMax:
            return llvm::AtomicRMWInst::Max;
        case PRIM_RMWUMin:
            return llvm::AtomicRMWInst::UMin;
        case PRIM_RMWUMax:
            return llvm::AtomicRMWInst::UMax;
        }
    }
    argumentTypeError(index, "atomic rmw operation", cv->type);
    return llvm::AtomicRMWInst::Xchg;
}



//
// codegenPrimOp
//

static llvm::Constant *codegenStringTableConstant(llvm::StringRef s)
{
    llvm::StringMap<llvm::Constant*>::const_iterator oldConstant = stringTableConstants.find(s);
    if (oldConstant != stringTableConstants.end())
        return oldConstant->second;

    llvm::Constant *sizeInitializer =
        llvm::ConstantInt::get(llvmType(cSizeTType), s.size(), false);
    llvm::Constant *stringInitializer =
        llvm::ConstantDataArray::getString(llvm::getGlobalContext(), s, true);
    llvm::Constant *structEntries[] = {sizeInitializer, stringInitializer};
    llvm::Constant *initializer =
        llvm::ConstantStruct::getAnon(llvm::getGlobalContext(),
                structEntries,
                false);

    llvm::SmallString<128> buf;
    llvm::raw_svector_ostream symbolName(buf);
    symbolName << "\"" << s << "\" clay";

    llvm::GlobalVariable *gvar = new llvm::GlobalVariable(
        *llvmModule, initializer->getType(), true,
        llvm::GlobalVariable::PrivateLinkage,
        initializer, symbolName.str());
    llvm::Constant *idxs[] = {
        llvm::ConstantInt::get(llvmType(int32Type), 0),
        llvm::ConstantInt::get(llvmType(int32Type), 0),
    };

    llvm::Constant *theConstant = llvm::ConstantExpr::getGetElementPtr(gvar, idxs);
    stringTableConstants[s] = theConstant;
    return theConstant;
}

void codegenPrimOp(PrimOpPtr x,
                   MultiCValuePtr args,
                   CodegenContext* ctx,
                   MultiCValuePtr out)
{
    switch (x->primOpCode) {

    case PRIM_TypeP : {
        ensureArity(args, 1);
        ObjectPtr obj = valueToStatic(args->values[0]);
        bool isType = (obj.ptr() && (obj->objKind == TYPE));
        ValueHolderPtr vh = boolToValueHolder(isType);
        codegenStaticObject(vh.ptr(), ctx, out);
        break;
    }

    case PRIM_TypeSize : {
        ensureArity(args, 1);
        TypePtr t = valueToType(args, 0);
        ValueHolderPtr vh = sizeTToValueHolder(typeSize(t));
        codegenStaticObject(vh.ptr(), ctx, out);
        break;
    }

    case PRIM_TypeAlignment : {
        ensureArity(args, 1);
        TypePtr t = valueToType(args, 0);
        ValueHolderPtr vh = sizeTToValueHolder(typeAlignment(t));
        codegenStaticObject(vh.ptr(), ctx, out);
        break;
    }

    case PRIM_SymbolP : {
        ensureArity(args, 1);
        ObjectPtr obj = valueToStatic(args->values[0]);
        bool isSymbol = false; 
        if (obj.ptr() != NULL) {
            switch (obj->objKind) {
            case TYPE :
            case RECORD :
            case VARIANT :
            case PROCEDURE :
            case GLOBAL_ALIAS:
                isSymbol = true;
                break;
            default :
                break;
            }
        }
        ValueHolderPtr vh = boolToValueHolder(isSymbol);
        codegenStaticObject(vh.ptr(), ctx, out);
        break;
    }

    case PRIM_StaticCallDefinedP : {
        if (args->size() < 1)
            arityError2(1, args->size());
        ObjectPtr callable = valueToStatic(args->values[0]);
        if (!callable) {
            argumentError(0, "static callable expected");
        }
        switch (callable->objKind) {
        case TYPE :
        case RECORD :
        case VARIANT :
        case PROCEDURE :
        case GLOBAL_ALIAS:
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
        codegenStaticObject(vh.ptr(), ctx, out);
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
        codegenStaticObject(vh.ptr(), ctx, out);
        break;
    }

    case PRIM_StaticMonoInputTypes :
        break;

    case PRIM_bitcopy : {
        ensureArity(args, 2);
        CValuePtr cv0 = args->values[0];
        CValuePtr cv1 = args->values[1];
        if (!isPrimitiveType(cv0->type))
            argumentTypeError(0, "primitive type", cv0->type);
        if (cv0->type != cv1->type)
            argumentTypeError(1, cv0->type, cv1->type);
        llvm::Value *v = ctx->builder->CreateLoad(cv1->llValue);
        ctx->builder->CreateStore(v, cv0->llValue);
        assert(out->size() == 0);
        break;
    }

    case PRIM_boolNot : {
        ensureArity(args, 1);
        CValuePtr cv = args->values[0];
        if (cv->type != boolType)
            argumentTypeError(0, boolType, cv->type);
        assert(out->size() == 1);
        llvm::Value *v = ctx->builder->CreateLoad(cv->llValue);
        llvm::Value *zero = llvm::ConstantInt::get(llvmType(boolType), 0);
        llvm::Value *flag = ctx->builder->CreateICmpEQ(v, zero);
        CValuePtr out0 = out->values[0];
        assert(out0->type == boolType);
        ctx->builder->CreateStore(flag, out0->llValue);
        break;
    }

    case PRIM_integerEqualsP :
    case PRIM_integerLesserP : {
        ensureArity(args, 2);
        TypePtr t;
        llvm::Value *v0 = integerOrPointerLikeValue(args, 0, t, ctx);
        llvm::Value *v1 = integerOrPointerLikeValue(args, 1, t, ctx);

        llvm::CmpInst::Predicate pred;

        switch (x->primOpCode) {
        case PRIM_integerEqualsP:
            pred = llvm::CmpInst::ICMP_EQ;
            break;
        case PRIM_integerLesserP: {
            bool isSigned;
            switch (t->typeKind) {
            case INTEGER_TYPE: {
                IntegerType *it = (IntegerType*)t.ptr();
                isSigned = it->isSigned;
                break;
            }
            default:
                isSigned = false;
                break;
            }
            pred = isSigned
                ? llvm::CmpInst::ICMP_SLT
                : llvm::CmpInst::ICMP_ULT;
            break;
        }
        default:
            assert(false);
        }

        llvm::Value *flag = ctx->builder->CreateICmp(pred, v0, v1);
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0->type == boolType);
        ctx->builder->CreateStore(flag, out0->llValue);
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
        llvm::Value *v0 = floatValue(args, 0, t, ctx);
        llvm::Value *v1 = floatValue(args, 1, t, ctx);

        llvm::CmpInst::Predicate pred;

        switch (x->primOpCode) {
        case PRIM_floatOrderedEqualsP :
            pred = llvm::CmpInst::FCMP_OEQ;
            break;
        case PRIM_floatOrderedLesserP :
            pred = llvm::CmpInst::FCMP_OLT;
            break;
        case PRIM_floatOrderedLesserEqualsP :
            pred = llvm::CmpInst::FCMP_OLE;
            break;
        case PRIM_floatOrderedGreaterP :
            pred = llvm::CmpInst::FCMP_OGT;
            break;
        case PRIM_floatOrderedGreaterEqualsP :
            pred = llvm::CmpInst::FCMP_OGE;
            break;
        case PRIM_floatOrderedNotEqualsP :
            pred = llvm::CmpInst::FCMP_ONE;
            break;
        case PRIM_floatOrderedP :
            pred = llvm::CmpInst::FCMP_ORD;
            break;
        case PRIM_floatUnorderedEqualsP :
            pred = llvm::CmpInst::FCMP_UEQ;
            break;
        case PRIM_floatUnorderedLesserP :
            pred = llvm::CmpInst::FCMP_ULT;
            break;
        case PRIM_floatUnorderedLesserEqualsP :
            pred = llvm::CmpInst::FCMP_ULE;
            break;
        case PRIM_floatUnorderedGreaterP :
            pred = llvm::CmpInst::FCMP_UGT;
            break;
        case PRIM_floatUnorderedGreaterEqualsP :
            pred = llvm::CmpInst::FCMP_UGE;
            break;
        case PRIM_floatUnorderedNotEqualsP :
            pred = llvm::CmpInst::FCMP_UNE;
            break;
        case PRIM_floatUnorderedP :
            pred = llvm::CmpInst::FCMP_UNO;
            break;
        default:
            assert(false);
        }

        llvm::Value *flag = ctx->builder->CreateFCmp(pred, v0, v1);
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0->type == boolType);
        ctx->builder->CreateStore(flag, out0->llValue);
        break;
    }

    case PRIM_numericAdd : {
        ensureArity(args, 2);
        TypePtr t;
        llvm::Value *v0 = numericValue(args, 0, t, ctx);
        llvm::Value *v1 = numericValue(args, 1, t, ctx);
        llvm::Value *result;
        switch (t->typeKind) {
        case INTEGER_TYPE :
            result = ctx->builder->CreateAdd(v0, v1);
            break;
        case FLOAT_TYPE :
            result = ctx->builder->CreateFAdd(v0, v1);
            break;
        default :
            assert(false);
        }
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0->type == t);
        ctx->builder->CreateStore(result, out0->llValue);
        break;
    }

    case PRIM_numericSubtract : {
        ensureArity(args, 2);
        TypePtr t;
        llvm::Value *v0 = numericValue(args, 0, t, ctx);
        llvm::Value *v1 = numericValue(args, 1, t, ctx);
        llvm::Value *result;
        switch (t->typeKind) {
        case INTEGER_TYPE :
            result = ctx->builder->CreateSub(v0, v1);
            break;
        case FLOAT_TYPE :
            result = ctx->builder->CreateFSub(v0, v1);
            break;
        default :
            assert(false);
        }
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0->type == t);
        ctx->builder->CreateStore(result, out0->llValue);
        break;
    }

    case PRIM_numericMultiply : {
        ensureArity(args, 2);
        TypePtr t;
        llvm::Value *v0 = numericValue(args, 0, t, ctx);
        llvm::Value *v1 = numericValue(args, 1, t, ctx);
        llvm::Value *result;
        switch (t->typeKind) {
        case INTEGER_TYPE :
            result = ctx->builder->CreateMul(v0, v1);
            break;
        case FLOAT_TYPE :
            result = ctx->builder->CreateFMul(v0, v1);
            break;
        default :
            assert(false);
        }
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0->type == t);
        ctx->builder->CreateStore(result, out0->llValue);
        break;
    }

    case PRIM_floatDivide : {
        ensureArity(args, 2);
        FloatTypePtr t;
        llvm::Value *v0 = floatValue(args, 0, t, ctx);
        llvm::Value *v1 = floatValue(args, 1, t, ctx);
        llvm::Value *result;
        result = ctx->builder->CreateFDiv(v0, v1);
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0->type == t.ptr());
        ctx->builder->CreateStore(result, out0->llValue);
        break;
    }

    case PRIM_numericNegate : {
        ensureArity(args, 1);
        TypePtr t;
        llvm::Value *v = numericValue(args, 0, t, ctx);
        llvm::Value *result;
        switch (t->typeKind) {
        case INTEGER_TYPE :
            result = ctx->builder->CreateNeg(v);
            break;
        case FLOAT_TYPE :
            result = ctx->builder->CreateFNeg(v);
            break;
        default :
            assert(false);
        }
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0->type == t);
        ctx->builder->CreateStore(result, out0->llValue);
        break;
    }

    case PRIM_integerQuotient : {
        ensureArity(args, 2);
        IntegerTypePtr t;
        llvm::Value *v0 = integerValue(args, 0, t, ctx);
        llvm::Value *v1 = integerValue(args, 1, t, ctx);
        llvm::Value *result;
        IntegerType *it = (IntegerType *)t.ptr();
        if (it->isSigned)
            result = ctx->builder->CreateSDiv(v0, v1);
        else
            result = ctx->builder->CreateUDiv(v0, v1);
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0->type == t.ptr());
        ctx->builder->CreateStore(result, out0->llValue);
        break;
    }

    case PRIM_integerRemainder : {
        ensureArity(args, 2);
        IntegerTypePtr t;
        llvm::Value *v0 = integerValue(args, 0, t, ctx);
        llvm::Value *v1 = integerValue(args, 1, t, ctx);
        llvm::Value *result;
        IntegerType *it = (IntegerType *)t.ptr();
        if (it->isSigned)
            result = ctx->builder->CreateSRem(v0, v1);
        else
            result = ctx->builder->CreateURem(v0, v1);
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0->type == t.ptr());
        ctx->builder->CreateStore(result, out0->llValue);
        break;
    }

    case PRIM_integerShiftLeft : {
        ensureArity(args, 2);
        IntegerTypePtr t;
        llvm::Value *v0 = integerValue(args, 0, t, ctx);
        llvm::Value *v1 = integerValue(args, 1, t, ctx);
        llvm::Value *result = ctx->builder->CreateShl(v0, v1);
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0->type == t.ptr());
        ctx->builder->CreateStore(result, out0->llValue);
        break;
    }

    case PRIM_integerShiftRight : {
        ensureArity(args, 2);
        IntegerTypePtr t;
        llvm::Value *v0 = integerValue(args, 0, t, ctx);
        llvm::Value *v1 = integerValue(args, 1, t, ctx);
        llvm::Value *result;
        if (t->isSigned)
            result = ctx->builder->CreateAShr(v0, v1);
        else
            result = ctx->builder->CreateLShr(v0, v1);
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0->type == t.ptr());
        ctx->builder->CreateStore(result, out0->llValue);
        break;
    }

    case PRIM_integerBitwiseAnd : {
        ensureArity(args, 2);
        IntegerTypePtr t;
        llvm::Value *v0 = integerValue(args, 0, t, ctx);
        llvm::Value *v1 = integerValue(args, 1, t, ctx);
        llvm::Value *result = ctx->builder->CreateAnd(v0, v1);
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0->type == t.ptr());
        ctx->builder->CreateStore(result, out0->llValue);
        break;
    }

    case PRIM_integerBitwiseOr : {
        ensureArity(args, 2);
        IntegerTypePtr t;
        llvm::Value *v0 = integerValue(args, 0, t, ctx);
        llvm::Value *v1 = integerValue(args, 1, t, ctx);
        llvm::Value *result = ctx->builder->CreateOr(v0, v1);
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0->type == t.ptr());
        ctx->builder->CreateStore(result, out0->llValue);
        break;
    }

    case PRIM_integerBitwiseXor : {
        ensureArity(args, 2);
        IntegerTypePtr t;
        llvm::Value *v0 = integerValue(args, 0, t, ctx);
        llvm::Value *v1 = integerValue(args, 1, t, ctx);
        llvm::Value *result = ctx->builder->CreateXor(v0, v1);
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0->type == t.ptr());
        ctx->builder->CreateStore(result, out0->llValue);
        break;
    }

    case PRIM_integerBitwiseNot : {
        ensureArity(args, 1);
        IntegerTypePtr t;
        llvm::Value *v = integerValue(args, 0, t, ctx);
        llvm::Value *result = ctx->builder->CreateNot(v);
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0->type == t.ptr());
        ctx->builder->CreateStore(result, out0->llValue);
        break;
    }

    case PRIM_numericConvert : {
        ensureArity(args, 2);
        TypePtr dest = valueToNumericType(args, 0);
        TypePtr src;
        llvm::Value *v = numericValue(args, 1, src, ctx);
        llvm::Value *result;
        if (src == dest) {
            result = v;
        }
        else {
            switch (dest->typeKind) {
            case INTEGER_TYPE : {
                IntegerType *dest2 = (IntegerType *)dest.ptr();
                if (src->typeKind == INTEGER_TYPE) {
                    IntegerType *src2 = (IntegerType *)src.ptr();
                    if (dest2->bits < src2->bits)
                        result = ctx->builder->CreateTrunc(v, llvmType(dest));
                    else if (src2->isSigned)
                        result = ctx->builder->CreateSExt(v, llvmType(dest));
                    else
                        result = ctx->builder->CreateZExt(v, llvmType(dest));
                }
                else if (src->typeKind == FLOAT_TYPE) {
                    if (dest2->isSigned)
                        result = ctx->builder->CreateFPToSI(v, llvmType(dest));
                    else
                        result = ctx->builder->CreateFPToUI(v, llvmType(dest));
                }
                else {
                    assert(false);
                    result = NULL;
                }
                break;
            }
            case FLOAT_TYPE : {
                FloatType *dest2 = (FloatType *)dest.ptr();
                if (src->typeKind == INTEGER_TYPE) {
                    IntegerType *src2 = (IntegerType *)src.ptr();
                    if (src2->isSigned)
                        result = ctx->builder->CreateSIToFP(v, llvmType(dest));
                    else
                        result = ctx->builder->CreateUIToFP(v, llvmType(dest));
                }
                else if (src->typeKind == FLOAT_TYPE) {
                    FloatType *src2 = (FloatType *)src.ptr();
                    if (dest2->bits < src2->bits)
                        result = ctx->builder->CreateFPTrunc(v, llvmType(dest));
                    else
                        result = ctx->builder->CreateFPExt(v, llvmType(dest));
                }
                else {
                    assert(false);
                    result = NULL;
                }
                break;
            }

            default :
                assert(false);
                result = NULL;
                break;
            }
        }
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0->type == dest);
        ctx->builder->CreateStore(result, out0->llValue);
        break;
    }

    case PRIM_integerAddChecked : {
        ensureArity(args, 2);
        IntegerTypePtr t;
        checkIntegerValue(args, 0, t, ctx);
        checkIntegerValue(args, 1, t, ctx);
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0->type.ptr() == t.ptr());
        codegenCallValue(staticCValue(operator_doIntegerAddChecked(), ctx),
                         args,
                         ctx,
                         out);
        break;
    }

    case PRIM_integerSubtractChecked : {
        ensureArity(args, 2);
        IntegerTypePtr t;
        checkIntegerValue(args, 0, t, ctx);
        checkIntegerValue(args, 1, t, ctx);
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0->type.ptr() == t.ptr());
        codegenCallValue(staticCValue(operator_doIntegerSubtractChecked(), ctx),
                         args,
                         ctx,
                         out);
        break;
    }

    case PRIM_integerMultiplyChecked : {
        ensureArity(args, 2);
        IntegerTypePtr t;
        checkIntegerValue(args, 0, t, ctx);
        checkIntegerValue(args, 1, t, ctx);
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0->type.ptr() == t.ptr());
        codegenCallValue(staticCValue(operator_doIntegerMultiplyChecked(), ctx),
                         args,
                         ctx,
                         out);
        break;
    }

    case PRIM_integerQuotientChecked : {
        ensureArity(args, 2);
        IntegerTypePtr t;
        checkIntegerValue(args, 0, t, ctx);
        checkIntegerValue(args, 1, t, ctx);
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0->type.ptr() == t.ptr());
        codegenCallValue(staticCValue(operator_doIntegerQuotientChecked(), ctx),
                         args,
                         ctx,
                         out);
        break;
    }

    case PRIM_integerRemainderChecked : {
        ensureArity(args, 2);
        IntegerTypePtr t;
        checkIntegerValue(args, 0, t, ctx);
        checkIntegerValue(args, 1, t, ctx);
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0->type.ptr() == t.ptr());
        codegenCallValue(staticCValue(operator_doIntegerRemainderChecked(), ctx),
                         args,
                         ctx,
                         out);
        break;
    }

    case PRIM_integerShiftLeftChecked : {
        ensureArity(args, 2);
        IntegerTypePtr t;
        checkIntegerValue(args, 0, t, ctx);
        checkIntegerValue(args, 1, t, ctx);
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0->type.ptr() == t.ptr());
        codegenCallValue(staticCValue(operator_doIntegerShiftLeftChecked(), ctx),
                         args,
                         ctx,
                         out);
        break;
    }

    case PRIM_integerNegateChecked : {
        ensureArity(args, 1);
        IntegerTypePtr t;
        checkIntegerValue(args, 0, t, ctx);
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0->type.ptr() == t.ptr());
        codegenCallValue(staticCValue(operator_doIntegerNegateChecked(), ctx),
                         args,
                         ctx,
                         out);
        break;
    }

    case PRIM_integerConvertChecked : {
        ensureArity(args, 2);
        IntegerTypePtr dest = valueToIntegerType(args, 0);
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0->type == dest.ptr());
        codegenCallValue(staticCValue(operator_doIntegerConvertChecked(), ctx),
                         args,
                         ctx,
                         out);
        break;
    }

    case PRIM_Pointer :
        error("no Pointer primitive overload found");

    case PRIM_addressOf : {
        ensureArity(args, 1);
        CValuePtr cv = args->values[0];
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0->type == pointerType(cv->type));
        ctx->builder->CreateStore(cv->llValue, out0->llValue);
        break;
    }

    case PRIM_pointerDereference : {
        ensureArity(args, 1);
        PointerTypePtr t;
        llvm::Value *v = pointerValue(args, 0, t, ctx);
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0->type == t.ptr());
        ctx->builder->CreateStore(v, out0->llValue);
        break;
    }

    case PRIM_pointerOffset : {
        ensureArity(args, 2);
        PointerTypePtr t;
        llvm::Value *v0 = pointerValue(args, 0, t, ctx);
        IntegerTypePtr offsetT;
        llvm::Value *v1 = integerValue(args, 1, offsetT, ctx);
        if (!offsetT->isSigned && (size_t)offsetT->bits < typeSize(cSizeTType)*8)
            v1 = ctx->builder->CreateZExt(v1, llvmType(cSizeTType));
        vector<llvm::Value *> indices;
        indices.push_back(v1);
        llvm::Value *result = ctx->builder->CreateGEP(v0,
                                                    llvm::makeArrayRef(indices));
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0->type == t.ptr());
        ctx->builder->CreateStore(result, out0->llValue);
        break;
    }

    case PRIM_pointerToInt : {
        ensureArity(args, 2);
        IntegerTypePtr dest = valueToIntegerType(args, 0);
        llvm::Type *llDest = llvmType(dest.ptr());
        TypePtr pt;
        llvm::Value *v = pointerLikeValue(args, 1, pt, ctx);
        llvm::Value *result = ctx->builder->CreatePtrToInt(v, llDest);
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0->type == dest.ptr());
        ctx->builder->CreateStore(result, out0->llValue);
        break;
    }

    case PRIM_intToPointer : {
        ensureArity(args, 2);
        TypePtr dest = valueToPointerLikeType(args, 0);
        IntegerTypePtr t;
        llvm::Value *v = integerValue(args, 1, t, ctx);
        if (t->isSigned && (typeSize(t.ptr()) < typeSize(cPtrDiffTType)))
            v = ctx->builder->CreateSExt(v, llvmType(cPtrDiffTType));
        llvm::Value *result = ctx->builder->CreateIntToPtr(v, llvmType(dest));
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0->type == dest);
        ctx->builder->CreateStore(result, out0->llValue);
        break;
    }

    case PRIM_nullPointer : {
        ensureArity(args, 1);
        TypePtr dest = valueToPointerLikeType(args, 0);
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0->type == dest);
        llvm::PointerType *llType = llvm::dyn_cast<llvm::PointerType>(llvmType(dest));
        assert(llType != NULL);
        llvm::Value *result = llvm::ConstantPointerNull::get(llType);
        ctx->builder->CreateStore(result, out0->llValue);
        break;
    }

    case PRIM_ByRef :
        error("no ByRef primitive overload found");

    case PRIM_RecordWithProperties :
        error("no RecordWithProperties primitive overload found");

    case PRIM_CodePointer :
        error("no CodePointer primitive overload found");

    case PRIM_makeCodePointer : {
        if (args->size() < 1)
            arityError2(1, args->size());
        ObjectPtr callable = valueToStatic(args, 0);
        switch (callable->objKind) {
        case TYPE :
        case RECORD :
        case VARIANT :
        case PROCEDURE :
        case GLOBAL_ALIAS :
            break;
        case PRIM_OP :
            if (!isOverloadablePrimOp(callable))
                argumentError(0, "invalid callable");
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

        InvokeEntry* entry =
            safeAnalyzeCallable(callable, argsKey, argsTempness);
        if (entry->callByName)
            argumentError(0, "cannot create pointer to call-by-name code");
        assert(entry->analyzed);
        if (!entry->llvmFunc)
            codegenCodeBody(entry);
        assert(entry->llvmFunc);
        TypePtr cpType = codePointerType(argsKey,
                                         entry->returnIsRef,
                                         entry->returnTypes);
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0->type == cpType);
        ctx->builder->CreateStore(entry->llvmFunc, out0->llValue);
        break;
    }

    case PRIM_ExternalCodePointer :
        error("no ExternalCodePointer primitive overload found");

    case PRIM_makeExternalCodePointer : {
        if (args->size() < 3)
            arityError2(3, args->size());
        ObjectPtr callable = valueToStatic(args, 0);
        switch (callable->objKind) {
        case TYPE :
        case RECORD :
        case VARIANT :
        case PROCEDURE :
        case GLOBAL_ALIAS :
            break;
        case PRIM_OP :
            if (!isOverloadablePrimOp(callable))
                argumentError(0, "invalid callable");
            break;
        default :
            argumentError(0, "invalid callable");
        }

        ObjectPtr ccObj = valueToStatic(args, 1);
        ObjectPtr isVarArgObj = valueToStatic(args, 2);
        CallingConv cc;
        bool isVarArg;
        if (!staticToCallingConv(ccObj, cc))
            argumentError(1, "expecting a calling convention attribute");
        if (!staticToBool(isVarArgObj, isVarArg))
            argumentError(2, "expecting a static boolean");

        if (isVarArg)
            argumentError(2, "implementing variadic external functions is not yet supported");

        vector<TypePtr> argsKey;
        vector<ValueTempness> argsTempness;
        for (unsigned i = 3; i < args->size(); ++i) {
            TypePtr t = valueToType(args, i);
            argsKey.push_back(t);
            argsTempness.push_back(TEMPNESS_LVALUE);
        }

        CompileContextPusher pusher(callable, argsKey);

        InvokeEntry* entry =
            safeAnalyzeCallable(callable, argsKey, argsTempness);
        if (entry->callByName)
            argumentError(0, "cannot create pointer to call-by-name code");
        assert(entry->analyzed);
        if (!entry->llvmFunc)
            codegenCodeBody(entry);
        assert(entry->llvmFunc);
        if (!entry->llvmCWrappers[cc])
            codegenCWrapper(entry, cc);
        assert(entry->llvmCWrappers[cc]);
        TypePtr returnType;
        if (entry->returnTypes.size() == 0) {
            returnType = NULL;
        }
        else if (entry->returnTypes.size() == 1) {
            if (entry->returnIsRef[0])
                argumentError(0, "cannot create C compatible pointer "
                              " to return-by-reference code");
            returnType = entry->returnTypes[0];
        }
        else {
            argumentError(0, "cannot create C compatible pointer "
                          "to multi-return code");
        }
        TypePtr ccpType = cCodePointerType(cc,
                                           argsKey,
                                           isVarArg,
                                           returnType);
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0->type == ccpType);

        llvm::Value *opaqueValue = ctx->builder->CreateBitCast(
            entry->llvmCWrappers[cc], llvmType(out0->type));
        ctx->builder->CreateStore(opaqueValue, out0->llValue);
        break;
    }

    case PRIM_callExternalCodePointer : {
        if (args->size() < 1)
            arityError2(1, args->size());
        CCodePointerTypePtr cpt;
        cCodePointerValue(args, 0, cpt);
        MultiCValuePtr args2 = new MultiCValue();
        for (unsigned i = 1; i < args->size(); ++i)
            args2->add(args->values[i]);
        codegenCallCCodePointer(args->values[0], args2, ctx, out);
        break;
    }

    case PRIM_bitcast : {
        ensureArity(args, 2);
        TypePtr dest = valueToType(args, 0);
        CValuePtr src = args->values[1];
        if (typeSize(dest) > typeSize(src->type))
            error("destination type for bitcast is larger than source type");
        if (typeAlignment(dest) > typeAlignment(src->type))
            error("destination type for bitcast has stricter alignment than source type");
        llvm::Value *arg1 = args->values[1]->llValue;
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0->type == pointerType(dest));
        llvm::Value *result = ctx->builder->CreateBitCast(arg1, llvmPointerType(dest));
        ctx->builder->CreateStore(result, out0->llValue);
        break;
    }

    case PRIM_Array :
        error("no Array primitive overload found");

    case PRIM_arrayRef : {
        ensureArity(args, 2);
        ArrayTypePtr at;
        llvm::Value *av = arrayValue(args, 0, at);
        IntegerTypePtr indexType;
        llvm::Value *iv = integerValue(args, 1, indexType, ctx);
        if (!indexType->isSigned && (size_t)indexType->bits < typeSize(cSizeTType)*8)
            iv = ctx->builder->CreateZExt(iv, llvmType(cSizeTType));
        vector<llvm::Value *> indices;
        indices.push_back(llvm::ConstantInt::get(llvmIntType(32), 0));
        indices.push_back(iv);
        llvm::Value *ptr =
            ctx->builder->CreateGEP(av, llvm::makeArrayRef(indices));
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0->type == pointerType(at->elementType));
        ctx->builder->CreateStore(ptr, out0->llValue);
        break;
    }

    case PRIM_arrayElements : {
        ensureArity(args, 1);
        ArrayTypePtr at;
        llvm::Value *varray = arrayValue(args, 0, at);
        assert(out->size() == (unsigned)at->size);
        for (unsigned i = 0; i < (unsigned)at->size; ++i) {
            CValuePtr outi = out->values[i];
            assert(outi->type == pointerType(at->elementType));
            llvm::Value *ptr = ctx->builder->CreateConstGEP2_32(varray, 0, i);
            ctx->builder->CreateStore(ptr, outi->llValue);
        }
        break;
    }

    case PRIM_Vec :
        error("no Vec primitive overload found");

    case PRIM_Tuple :
        error("no Tuple primitive overload found");

    case PRIM_TupleElementCount : {
        ensureArity(args, 1);
        TupleTypePtr t = valueToTupleType(args, 0);
        ValueHolderPtr vh = sizeTToValueHolder(t->elementTypes.size());
        codegenStaticObject(vh.ptr(), ctx, out);
        break;
    }

    case PRIM_tupleRef : {
        ensureArity(args, 2);
        TupleTypePtr tt;
        llvm::Value *vtuple = tupleValue(args, 0, tt);
        size_t i = valueToStaticSizeTOrInt(args, 1);
        if (i >= tt->elementTypes.size())
            argumentIndexRangeError(1, "tuple element index",
                                    i, tt->elementTypes.size());
        llvm::Value *ptr = ctx->builder->CreateConstGEP2_32(vtuple, 0, i);
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0->type == pointerType(tt->elementTypes[i]));
        ctx->builder->CreateStore(ptr, out0->llValue);
        break;
    }

    case PRIM_tupleElements : {
        ensureArity(args, 1);
        TupleTypePtr tt;
        llvm::Value *vtuple = tupleValue(args, 0, tt);
        assert(out->size() == tt->elementTypes.size());
        for (unsigned i = 0; i < tt->elementTypes.size(); ++i) {
            CValuePtr outi = out->values[i];
            assert(outi->type == pointerType(tt->elementTypes[i]));
            llvm::Value *ptr = ctx->builder->CreateConstGEP2_32(vtuple, 0, i);
            ctx->builder->CreateStore(ptr, outi->llValue);
        }
        break;
    }

    case PRIM_Union :
        error("no Union primitive overload found");

    case PRIM_UnionMemberCount : {
        ensureArity(args, 1);
        UnionTypePtr t = valueToUnionType(args, 0);
        ValueHolderPtr vh = sizeTToValueHolder(t->memberTypes.size());
        codegenStaticObject(vh.ptr(), ctx, out);
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
        ValueHolderPtr vh = boolToValueHolder(isRecordType);
        codegenStaticObject(vh.ptr(), ctx, out);
        break;
    }

    case PRIM_RecordFieldCount : {
        ensureArity(args, 1);
        RecordTypePtr rt = valueToRecordType(args, 0);
        const vector<TypePtr> &fieldTypes = recordFieldTypes(rt);
        ValueHolderPtr vh = sizeTToValueHolder(fieldTypes.size());
        codegenStaticObject(vh.ptr(), ctx, out);
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
        codegenStaticObject(fieldNames[i].ptr(), ctx, out);
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
        codegenStaticObject(vh.ptr(), ctx, out);
        break;
    }

    case PRIM_recordFieldRef : {
        ensureArity(args, 2);
        RecordTypePtr rt;
        llvm::Value *vrec = recordValue(args, 0, rt);
        size_t i = valueToStaticSizeTOrInt(args, 1);
        const vector<TypePtr> &fieldTypes = recordFieldTypes(rt);
        if (i >= fieldTypes.size())
            argumentIndexRangeError(1, "record field index",
                                    i, fieldTypes.size());
        llvm::Value *ptr = ctx->builder->CreateConstGEP2_32(vrec, 0, i);
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0->type == pointerType(fieldTypes[i]));
        ctx->builder->CreateStore(ptr, out0->llValue);
        break;
    }

    case PRIM_recordFieldRefByName : {
        ensureArity(args, 2);
        RecordTypePtr rt;
        llvm::Value *vrec = recordValue(args, 0, rt);
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
        size_t index = fi->second;
        const vector<TypePtr> &fieldTypes = recordFieldTypes(rt);
        llvm::Value *ptr = ctx->builder->CreateConstGEP2_32(vrec, 0, index);
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0->type == pointerType(fieldTypes[index]));
        ctx->builder->CreateStore(ptr, out0->llValue);
        break;
    }

    case PRIM_recordFields : {
        ensureArity(args, 1);
        RecordTypePtr rt;
        llvm::Value *vrec = recordValue(args, 0, rt);
        const vector<TypePtr> &fieldTypes = recordFieldTypes(rt);
        assert(out->size() == fieldTypes.size());
        for (unsigned i = 0; i < fieldTypes.size(); ++i) {
            CValuePtr outi = out->values[i];
            assert(outi->type == pointerType(fieldTypes[i]));
            llvm::Value *ptr = ctx->builder->CreateConstGEP2_32(vrec, 0, i);
            ctx->builder->CreateStore(ptr, outi->llValue);
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
        ValueHolderPtr vh = boolToValueHolder(isVariantType);
        codegenStaticObject(vh.ptr(), ctx, out);
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
        ValueHolderPtr vh = sizeTToValueHolder(index);
        codegenStaticObject(vh.ptr(), ctx, out);
        break;
    }

    case PRIM_VariantMemberCount : {
        ensureArity(args, 1);
        VariantTypePtr vt = valueToVariantType(args, 0);
        const vector<TypePtr> &memberTypes = variantMemberTypes(vt);
        size_t size = memberTypes.size();
        ValueHolderPtr vh = sizeTToValueHolder(size);
        codegenStaticObject(vh.ptr(), ctx, out);
        break;
    }

    case PRIM_VariantMembers :
        break;

    case PRIM_variantRepr : {
        ensureArity(args, 1);
        VariantTypePtr vt;
        llvm::Value *vvar = variantValue(args, 0, vt);
        TypePtr reprType = variantReprType(vt);
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0->type == pointerType(reprType));
        ctx->builder->CreateStore(vvar, out0->llValue);
        break;
    }

    case PRIM_Static :
        error("no Static primitive overload found");

    case PRIM_StaticName :
    case PRIM_MainModule :
    case PRIM_StaticModule :
    case PRIM_ModuleName :
    case PRIM_ModuleMemberNames :
        break;

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
                CValuePtr outi = out->values[i];
                assert(outi->type == cIntType);
                llvm::Constant *value = llvm::ConstantInt::get(llvmIntType(32), i);
                ctx->builder->CreateStore(value, outi->llValue);
            }
        }
        else if (vh->type == cSizeTType) {
            size_t count = vh->as<size_t>();
            assert(out->size() == count);
            for (size_t i = 0; i < count; ++i) {
                ValueHolderPtr vhi = sizeTToValueHolder(i);
                CValuePtr outi = out->values[i];
                assert(outi->type == cSizeTType);
                llvm::Type *llSizeTType = llvmIntType(typeSize(cSizeTType)*8);
                llvm::Constant *value = llvm::ConstantInt::get(llSizeTType, i);
                ctx->builder->CreateStore(value, outi->llValue);
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
        codegenStaticObject(obj, ctx, out);
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
        ValueHolderPtr vh = boolToValueHolder(isEnumType);
        codegenStaticObject(vh.ptr(), ctx, out);
        break;
    }

    case PRIM_EnumMemberCount : {
        ensureArity(args, 1);
        EnumTypePtr et = valueToEnumType(args, 0);
        size_t n = et->enumeration->members.size();
        ValueHolderPtr vh = sizeTToValueHolder(n);
        codegenStaticObject(vh.ptr(), ctx, out);
        break;
    }

    case PRIM_EnumMemberName :
        break;

    case PRIM_enumToInt : {
        ensureArity(args, 1);
        EnumTypePtr et;
        llvm::Value *v = enumValue(args, 0, et, ctx);
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0->type == cIntType);
        ctx->builder->CreateStore(v, out0->llValue);
        break;
    }

    case PRIM_intToEnum : {
        ensureArity(args, 2);
        EnumTypePtr et = valueToEnumType(args, 0);
        IntegerTypePtr it = (IntegerType *)cIntType.ptr();
        llvm::Value *v = integerValue(args, 1, it, ctx);
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0->type == et.ptr());
        ctx->builder->CreateStore(v, out0->llValue);
        break;
    }

    case PRIM_StringLiteralP : {
        ensureArity(args, 1);
        bool result = false;
        CValuePtr cv0 = args->values[0];
        if (cv0->type->typeKind == STATIC_TYPE) {
            StaticType *st = (StaticType *)cv0->type.ptr();
            result = (st->obj->objKind == IDENTIFIER);
        }
        ValueHolderPtr vh = boolToValueHolder(result);
        codegenStaticObject(vh.ptr(), ctx, out);
        break;
    }

    case PRIM_stringLiteralByteIndex : {
        ensureArity(args, 2);
        IdentifierPtr ident = valueToIdentifier(args, 0);
        size_t n = valueToStaticSizeTOrInt(args, 1);
        if (n >= ident->str.size())
            argumentError(1, "string literal index out of bounds");

        assert(out->size() == 1);
        CValuePtr outi = out->values[0];
        assert(outi->type == cIntType);
        llvm::Constant *value = llvm::ConstantInt::get(llvmIntType(32), ident->str[n]);
        ctx->builder->CreateStore(value, outi->llValue);
        break;
    }

    case PRIM_stringLiteralBytes : {
        ensureArity(args, 1);
        IdentifierPtr ident = valueToIdentifier(args, 0);
        assert(out->size() == ident->str.size());
        for (size_t i = 0, sz = ident->str.size(); i < sz; ++i) {
            CValuePtr outi = out->values[i];
            assert(outi->type == cIntType);
            llvm::Constant *value = llvm::ConstantInt::get(llvmIntType(32), ident->str[i]);
            ctx->builder->CreateStore(value, outi->llValue);
        }
        break;
    }

    case PRIM_stringLiteralByteSize : {
        ensureArity(args, 1);
        IdentifierPtr ident = valueToIdentifier(args, 0);
        ValueHolderPtr vh = sizeTToValueHolder(ident->str.size());
        codegenStaticObject(vh.ptr(), ctx, out);
        break;
    }

    case PRIM_stringLiteralByteSlice :
    case PRIM_stringLiteralConcat :
    case PRIM_stringLiteralFromBytes :
        break;

    case PRIM_stringTableConstant : {
        ensureArity(args, 1);
        IdentifierPtr ident = valueToIdentifier(args, 0);
        llvm::Value *value = codegenStringTableConstant(ident->str);

        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0->type == pointerType(cSizeTType));
        ctx->builder->CreateStore(value, out0->llValue);
        break;
    }

    case PRIM_FlagP : {
        ensureArity(args, 1);
        IdentifierPtr ident = valueToIdentifier(args, 0);
        llvm::StringMap<string>::const_iterator flag = globalFlags.find(ident->str);
        ValueHolderPtr vh = boolToValueHolder(flag != globalFlags.end());
        codegenStaticObject(vh.ptr(), ctx, out);
        break;
    }

    case PRIM_Flag :
        break;

    case PRIM_atomicFence : {
        ensureArity(args, 1);
        llvm::AtomicOrdering order = atomicOrderValue(args, 0);

        ctx->builder->CreateFence(order);
        assert(out->size() == 0);
        break;
    }

    case PRIM_atomicRMW : {
        ensureArity(args, 4);
        llvm::AtomicOrdering order = atomicOrderValue(args, 0);
        llvm::AtomicRMWInst::BinOp op = atomicRMWOpValue(args, 1);

        PointerTypePtr ptrT;
        llvm::Value *ptr = pointerValue(args, 2, ptrT, ctx);
        TypePtr argT = args->values[3]->type;
        llvm::Value *arg = ctx->builder->CreateLoad(args->values[3]->llValue);

        if (ptrT->pointeeType != argT)
            argumentTypeError(3, ptrT->pointeeType, argT);

        llvm::Value *result = ctx->builder->CreateAtomicRMW(op, ptr, arg, order);

        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0->type == argT);
        ctx->builder->CreateStore(result, out0->llValue);
        break;
    }

    case PRIM_atomicLoad : {
        ensureArity(args, 2);
        llvm::AtomicOrdering order = atomicOrderValue(args, 0);

        PointerTypePtr ptrT;
        llvm::Value *ptr = pointerValue(args, 1, ptrT, ctx);

        llvm::Value *result = ctx->builder->Insert(
            new llvm::LoadInst(ptr, "", false, typeAlignment(ptrT->pointeeType), order));

        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0->type == ptrT->pointeeType);
        ctx->builder->CreateStore(result, out0->llValue);
        break;
    }

    case PRIM_atomicStore : {
        ensureArity(args, 3);
        llvm::AtomicOrdering order = atomicOrderValue(args, 0);

        PointerTypePtr ptrT;
        llvm::Value *ptr = pointerValue(args, 1, ptrT, ctx);
        TypePtr argT = args->values[2]->type;
        llvm::Value *arg = ctx->builder->CreateLoad(args->values[2]->llValue);

        if (ptrT->pointeeType != argT)
            argumentTypeError(2, ptrT->pointeeType, argT);

        ctx->builder->Insert(new llvm::StoreInst(
            arg, ptr, false, typeAlignment(argT), order));

        assert(out->size() == 0);
        break;
    }

    case PRIM_atomicCompareExchange : {
        ensureArity(args, 4);
        llvm::AtomicOrdering order = atomicOrderValue(args, 0);

        PointerTypePtr ptrT;
        llvm::Value *ptr = pointerValue(args, 1, ptrT, ctx);
        TypePtr oldvT = args->values[2]->type;
        llvm::Value *oldv = ctx->builder->CreateLoad(args->values[2]->llValue);
        TypePtr newvT = args->values[3]->type;
        llvm::Value *newv = ctx->builder->CreateLoad(args->values[3]->llValue);

        if (ptrT->pointeeType != oldvT)
            argumentTypeError(2, ptrT->pointeeType, oldvT);
        if (ptrT->pointeeType != newvT)
            argumentTypeError(3, ptrT->pointeeType, newvT);

        llvm::Value *result = ctx->builder->CreateAtomicCmpXchg(ptr, oldv, newv, order);

        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0->type == oldvT);
        ctx->builder->CreateStore(result, out0->llValue);
        break;
    }

    case PRIM_activeException : {
        ensureArity(args, 0);
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0->type == pointerType(uint8Type));

        assert(ctx->exceptionValue != NULL);

        llvm::Value *expv = ctx->builder->CreateLoad(ctx->exceptionValue);
        ctx->builder->CreateStore(expv, out0->llValue);
        break;
    }

    case PRIM_memcpy :
    case PRIM_memmove : {
        ensureArity(args, 3);
        PointerTypePtr topt;
        PointerTypePtr frompt;
        llvm::Value *toptr = pointerValue(args, 0, topt, ctx);
        llvm::Value *fromptr = pointerValue(args, 1, frompt, ctx);
        IntegerTypePtr it;
        llvm::Value *count = integerValue(args, 2, it, ctx);

        size_t pointerSize = typeSize(topt.ptr());
        llvm::Type *sizeType = llvmIntType(8*pointerSize);

        if (typeSize(it.ptr()) > pointerSize)
            argumentError(2, "integer type for memcpy must be pointer-sized or smaller");
        if (typeSize(it.ptr()) < pointerSize)
            count = ctx->builder->CreateZExt(count, sizeType);

        size_t alignment = std::min(
            typeAlignment(topt->pointeeType), typeAlignment(frompt->pointeeType));

        if (x->primOpCode == PRIM_memcpy)
            ctx->builder->CreateMemCpy(toptr, fromptr, count, alignment);
        else
            ctx->builder->CreateMemMove(toptr, fromptr, count, alignment);

        break;
    }

    case PRIM_countValues : {
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0->type == cIntType);
        llvm::Constant *value = llvm::ConstantInt::get(llvmIntType(32), args->size());
        ctx->builder->CreateStore(value, out0->llValue);
        break;
    }

    case PRIM_nthValue : {
        if (args->size() < 1)
            arityError2(1, args->size());
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];

        size_t i = valueToStaticSizeTOrInt(args, 0);
        if (i+1 >= args->size())
            argumentError(0, "nthValue argument out of bounds");

        codegenValueForward(out0, args->values[i+1], ctx);
        break;
    }

    case PRIM_withoutNthValue : {
        if (args->size() < 1)
            arityError2(1, args->size());
        assert(out->size() == args->size() - 2);

        size_t i = valueToStaticSizeTOrInt(args, 0);
        if (i+1 >= args->size())
            argumentError(0, "withoutNthValue argument out of bounds");

        for (unsigned argi = 1, outi = 0; argi < args->size(); ++argi) {
            if (argi == i+1)
                continue;
            assert(outi < out->size());
            codegenValueForward(out->values[outi], args->values[argi], ctx);
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
        for (unsigned argi = 1, outi = 0; argi < i+1; ++argi, ++outi) {
            assert(outi < out->size());
            codegenValueForward(out->values[outi], args->values[argi], ctx);
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
        for (unsigned argi = i+1, outi = 0; argi < args->size(); ++argi, ++outi) {
            assert(outi < out->size());
            codegenValueForward(out->values[outi], args->values[argi], ctx);
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
        codegenStaticObject(vh.ptr(), ctx, out);
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
        codegenStaticObject(vh.ptr(), ctx, out);
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
        codegenStaticObject(vh.ptr(), ctx, out);
        break;
    }

    case PRIM_LambdaMonoInputTypes :
        break;

    case PRIM_GetOverload :
        break;

    default :
        assert(false);
        break;

    }
}



//
// codegenTopLevelLLVM
//

static string stripEnclosingBraces(llvm::StringRef s) {
    const char * i = s.begin();
    const char * j = s.end();
    assert(i != j);
    assert(*i == '{');
    assert(*(j-1) == '}');
    return string(i+1, j-1);
}

static void codegenTopLevelLLVMRecursive(ModulePtr m)
{
    // XXX debug info for inline LLVM

    if (m->topLevelLLVMGenerated) return;
    m->topLevelLLVMGenerated = true;
    vector<ImportPtr>::iterator ii, iend;
    for (ii = m->imports.begin(), iend = m->imports.end(); ii != iend; ++ii)
        codegenTopLevelLLVMRecursive((*ii)->module);

    if (!m->topLevelLLVM) return;

    LocationContext loc(m->topLevelLLVM->location);

    string code;
    if (!interpolateLLVMCode(m->topLevelLLVM, code, m->env))
        error("failed to generate top level llvm");

    code = stripEnclosingBraces(code);

    llvm::SMDiagnostic err;
    llvm::MemoryBuffer *buf =
        llvm::MemoryBuffer::getMemBuffer(llvm::StringRef(code));

    if (!llvm::ParseAssembly(buf, llvmModule, err,
                             llvm::getGlobalContext())) {
        err.print("\n", llvm::errs());
        llvm::errs() << "\n";
        error("llvm assembly parse error");
    }
}

void codegenTopLevelLLVM(ModulePtr m) {
    codegenTopLevelLLVMRecursive(preludeModule());
    codegenTopLevelLLVMRecursive(m);
}



//
// codegenEntryPoints, codegenMain
//

static CodegenContext* setUpSimpleContext(CodegenContext *ctx, const char *name)
{
    llvm::FunctionType *llFuncType =
        llvm::FunctionType::get(llvmVoidType(),
                                vector<llvm::Type *>(),
                                false);
    llvm::Function *initGlobals =
        llvm::Function::Create(llFuncType,
                               llvm::Function::InternalLinkage,
                               name,
                               llvmModule);

    ctx->llvmFunc = initGlobals;

    llvm::BasicBlock *initBlock = newBasicBlock("init", ctx);
    llvm::BasicBlock *codeBlock = newBasicBlock("code", ctx);
    llvm::BasicBlock *returnBlock = newBasicBlock("return", ctx);
    llvm::BasicBlock *exceptionBlock = newBasicBlock("exception", ctx);

    ctx->initBuilder = new llvm::IRBuilder<>(initBlock);
    ctx->builder = new llvm::IRBuilder<>(codeBlock);

    ctx->exceptionValue = ctx->initBuilder->CreateAlloca(exceptionReturnType(), NULL, "exception");

    ctx->returnLists.push_back(vector<CReturn>());
    JumpTarget returnTarget(returnBlock, cgMarkStack(ctx));
    ctx->returnTargets.push_back(returnTarget);
    JumpTarget exceptionTarget(exceptionBlock, cgMarkStack(ctx));
    ctx->exceptionTargets.push_back(exceptionTarget);

    return ctx;
}

static void finalizeSimpleContext(CodegenContext* ctx,
                                  ObjectPtr errorProc)
{
    ctx->builder->CreateBr(ctx->returnTargets.back().block);

    ctx->builder->SetInsertPoint(ctx->returnTargets.back().block);
    ctx->builder->CreateRetVoid();

    ctx->builder->SetInsertPoint(ctx->exceptionTargets.back().block);
    if (exceptionsEnabled()) {
        codegenCallValue(staticCValue(errorProc, ctx),
                         new MultiCValue(),
                         ctx,
                         new MultiCValue());
    }
    ctx->builder->CreateUnreachable();

    llvm::Function::iterator bbi = ctx->llvmFunc->begin();
    ++bbi;
    ctx->initBuilder->CreateBr(&(*bbi));
}

static void initializeCtorsDtors()
{
    setUpSimpleContext(constructorsCtx, "clayglobals_init");
    setUpSimpleContext(destructorsCtx, "clayglobals_destroy");

    if (isMsvcTarget()) {
        constructorsCtx->llvmFunc->setSection(".text$yc");
        destructorsCtx->llvmFunc->setSection(".text$yd");
    }

    if (exceptionsEnabled()) {
        codegenCallable(operator_exceptionInInitializer(),
                        vector<TypePtr>(),
                        vector<ValueTempness>());
        codegenCallable(operator_exceptionInFinalizer(),
                        vector<TypePtr>(),
                        vector<ValueTempness>());
    }
}

static void codegenAtexitDestructors()
{
    llvm::Function *atexitFunc = llvmModule->getFunction("atexit");
    if (!atexitFunc) {
        vector<llvm::Type*> atexitArgTypes;
        atexitArgTypes.push_back(destructorsCtx->llvmFunc->getType());

        llvm::FunctionType *atexitType =
            llvm::FunctionType::get(llvmIntType(32), atexitArgTypes, false);

        atexitFunc = llvm::Function::Create(atexitType,
            llvm::Function::ExternalLinkage,
            "atexit",
            llvmModule);
    }

    constructorsCtx->builder->CreateCall(atexitFunc, destructorsCtx->llvmFunc);
}

static void finalizeCtorsDtors()
{
    if (isMsvcTarget())
        codegenAtexitDestructors();

    finalizeSimpleContext(constructorsCtx, operator_exceptionInInitializer());

    for (unsigned i = initializedGlobals.size(); i > 0; --i) {
        CValuePtr cv = initializedGlobals[i-1];
        codegenValueDestroy(cv, destructorsCtx);
    }
    finalizeSimpleContext(destructorsCtx, operator_exceptionInFinalizer());
}

static void generateLLVMCtorsAndDtors() {

    // make types for llvm.global_ctors, llvm.global_dtors
    vector<llvm::Type *> fieldTypes;
    fieldTypes.push_back(llvmIntType(32));
    llvm::Type *funcType = constructorsCtx->llvmFunc->getFunctionType();
    llvm::Type *funcPtrType = llvm::PointerType::getUnqual(funcType);
    fieldTypes.push_back(funcPtrType);
    llvm::StructType *structType =
        llvm::StructType::get(llvm::getGlobalContext(), fieldTypes);
    llvm::ArrayType *arrayType = llvm::ArrayType::get(structType, 1);

    // make constants for llvm.global_ctors
    vector<llvm::Constant*> structElems1;
    llvm::ConstantInt *prio1 =
        llvm::ConstantInt::get(llvm::getGlobalContext(),
                               llvm::APInt(32, llvm::StringRef("65535"), 10));
    structElems1.push_back(prio1);
    structElems1.push_back(constructorsCtx->llvmFunc);
    llvm::Constant *structVal1 = llvm::ConstantStruct::get(structType,
                                                           structElems1);
    vector<llvm::Constant*> arrayElems1;
    arrayElems1.push_back(structVal1);
    llvm::Constant *arrayVal1 = llvm::ConstantArray::get(arrayType,
                                                         arrayElems1);

    // define llvm.global_ctors
    new llvm::GlobalVariable(*llvmModule, arrayType, true,
                             llvm::GlobalVariable::AppendingLinkage,
                             arrayVal1, "llvm.global_ctors");

    // MSVC does not support a dtors mechanism. dtors are registered atexit
    // by the ctor
    if (!isMsvcTarget()) {
        // make constants for llvm.global_dtors
        vector<llvm::Constant*> structElems2;
        llvm::ConstantInt *prio2 =
            llvm::ConstantInt::get(llvm::getGlobalContext(),
                                   llvm::APInt(32, llvm::StringRef("65535"), 10));
        structElems2.push_back(prio2);
        structElems2.push_back(destructorsCtx->llvmFunc);
        llvm::Constant *structVal2 = llvm::ConstantStruct::get(structType,
                                                               structElems2);
        vector<llvm::Constant*> arrayElems2;
        arrayElems2.push_back(structVal2);
        llvm::Constant *arrayVal2 = llvm::ConstantArray::get(arrayType,
                                                             arrayElems2);

        // define llvm.global_dtors
        new llvm::GlobalVariable(*llvmModule, arrayType, true,
                                 llvm::GlobalVariable::AppendingLinkage,
                                 arrayVal2, "llvm.global_dtors");
    }
}

void codegenMain(ModulePtr module)
{
    IdentifierPtr main = Identifier::get("main");
    IdentifierPtr argc = Identifier::get("argc");
    IdentifierPtr argv = Identifier::get("argv");

    BlockPtr mainBody = new Block();

    ExprListPtr args;

    args = new ExprList(new NameRef(main));
    args->add(new NameRef(argc));
    args->add(new NameRef(argv));
    CallPtr mainCall = new Call(operator_expr_callMain(), args);
    
    ReturnPtr ret = new Return(RETURN_VALUE, new ExprList(mainCall.ptr()));
    mainBody->statements.push_back(ret.ptr());

    vector<ExternalArgPtr> mainArgs;
    ExprPtr argcType = new ObjectExpr(cIntType.ptr());
    mainArgs.push_back(new ExternalArg(argc, argcType));
    TypePtr charPtrPtr = pointerType(pointerType(int8Type));
    ExprPtr argvType = new ObjectExpr(charPtrPtr.ptr());
    mainArgs.push_back(new ExternalArg(argv, argvType));

    ExternalProcedurePtr entryProc =
        new ExternalProcedure(main,
                              PUBLIC,
                              mainArgs,
                              false,
                              new ObjectExpr(cIntType.ptr()),
                              mainBody.ptr(),
                              new ExprList());

    entryProc->env = module->env;

    codegenExternalProcedure(entryProc, true);
}

static void codegenModuleEntryPoints(ModulePtr module, bool importedExternals)
{
    module->externalsGenerated = true;
    for (unsigned i = 0; i < module->topLevelItems.size(); ++i) {
        TopLevelItemPtr x = module->topLevelItems[i];
        if (x->objKind == EXTERNAL_PROCEDURE) {
            ExternalProcedurePtr y = (ExternalProcedure *)x.ptr();
            if (y->body.ptr())
                codegenExternalProcedure(y, true);
        }
    }

    if (importedExternals) {
        vector<ImportPtr>::iterator ii, iend;
        for (ii = module->imports.begin(), iend = module->imports.end(); ii != iend; ++ii)
            if (!(*ii)->module->externalsGenerated)
                codegenModuleEntryPoints((*ii)->module, importedExternals);
    }
}

void codegenEntryPoints(ModulePtr module, bool importedExternals)
{
    CodegenContext theConstructorCtx;
    CodegenContext theDestructorCtx;

    constructorsCtx = &theConstructorCtx;
    destructorsCtx = &theDestructorCtx;

    codegenTopLevelLLVM(module);
    initializeCtorsDtors();
    generateLLVMCtorsAndDtors();
    codegenModuleEntryPoints(module, importedExternals);

    ObjectPtr mainProc = lookupPublic(module, Identifier::get("main"));
    
    if (mainProc != NULL)
        codegenMain(module);

    finalizeCtorsDtors();

    if (llvmDIBuilder != NULL)
        llvmDIBuilder->finalize();

    constructorsCtx = NULL;
    destructorsCtx = NULL;
}



//
// initLLVM
//

llvm::TargetMachine *initLLVM(llvm::StringRef targetTriple,
    llvm::StringRef name,
    llvm::StringRef flags,
    bool relocPic,
    bool debug,
    bool optimized)
{
    llvm::InitializeAllTargets();
    llvm::InitializeAllTargetMCs();
    llvm::InitializeAllAsmPrinters();
    llvm::InitializeAllAsmParsers();

    llvmModule = new llvm::Module(name, llvm::getGlobalContext());
    llvmModule->setTargetTriple(targetTriple);
    if (debug) {
        llvm::SmallString<260> absFileName(name);
        llvm::sys::fs::make_absolute(absFileName);
        llvmDIBuilder = new llvm::DIBuilder(*llvmModule);
        llvmDIBuilder->createCompileUnit(
            llvm::dwarf::DW_LANG_C_plus_plus, //DW_LANG_user_CLAY,
            llvm::sys::path::filename(absFileName),
            llvm::sys::path::parent_path(absFileName),
            "clay compiler " CLAY_COMPILER_VERSION,
            optimized,
            flags,
            0);
    } else
        llvmDIBuilder = NULL;

    string err;
    const llvm::Target *target = llvm::TargetRegistry::lookupTarget(targetTriple, err);
    if (target == NULL) {
        llvm::errs() << "error: unable to find target: " << err << '\n';
        return NULL;
    }
    llvm::Reloc::Model reloc = relocPic
        ? llvm::Reloc::PIC_
        : llvm::Reloc::Default;
    llvm::CodeModel::Model codeModel = llvm::CodeModel::Default;

    llvm::TargetMachine *targetMachine = target->createTargetMachine(
        targetTriple, "", "", llvm::TargetOptions(), reloc, codeModel);

    if (targetMachine != NULL) {
        llvmTargetData = targetMachine->getTargetData();
        llvmModule->setDataLayout(llvmTargetData->getStringRepresentation());
    }

    return targetMachine;
}

}
