#include "clay.hpp"
#include "libclaynames.hpp"
#include <llvm/Support/SourceMgr.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Assembly/Writer.h>
#include <llvm/Assembly/Parser.h>

llvm::Module *llvmModule;
llvm::ExecutionEngine *llvmEngine;
const llvm::TargetData *llvmTargetData;

static vector<CValuePtr> initializedGlobals;
static CodegenContextPtr constructorsCtx;
static CodegenContextPtr destructorsCtx;

void codegenValueInit(CValuePtr dest, CodegenContextPtr ctx);
void codegenValueDestroy(CValuePtr dest, CodegenContextPtr ctx);
void codegenValueCopy(CValuePtr dest, CValuePtr src, CodegenContextPtr ctx);
void codegenValueMove(CValuePtr dest, CValuePtr src, CodegenContextPtr ctx);
void codegenValueAssign(CValuePtr dest, CValuePtr src, CodegenContextPtr ctx);
void codegenValueMoveAssign(CValuePtr dest,
                            CValuePtr src,
                            CodegenContextPtr ctx);
llvm::Value *codegenToBoolFlag(CValuePtr a, CodegenContextPtr ctx);

int cgMarkStack(CodegenContextPtr ctx);
void cgDestroyStack(int marker, CodegenContextPtr ctx);
void cgPopStack(int marker, CodegenContextPtr ctx);
void cgDestroyAndPopStack(int marker, CodegenContextPtr ctx);
void cgPushStack(CValuePtr cv, CodegenContextPtr ctx);
CValuePtr codegenAllocValue(TypePtr t, CodegenContextPtr ctx);

MultiCValuePtr codegenMultiArgsAsRef(ExprListPtr exprs,
                                     EnvPtr env,
                                     CodegenContextPtr ctx);
MultiCValuePtr codegenArgExprAsRef(ExprPtr x,
                                   EnvPtr env,
                                   CodegenContextPtr ctx);

CValuePtr codegenForwardOneAsRef(ExprPtr expr,
                                 EnvPtr env,
                                 CodegenContextPtr ctx);
MultiCValuePtr codegenForwardMultiAsRef(ExprListPtr exprs,
                                        EnvPtr env,
                                        CodegenContextPtr ctx);
MultiCValuePtr codegenForwardExprAsRef(ExprPtr expr,
                                       EnvPtr env,
                                       CodegenContextPtr ctx);

CValuePtr codegenOneAsRef(ExprPtr expr,
                          EnvPtr env,
                          CodegenContextPtr ctx);
MultiCValuePtr codegenMultiAsRef(ExprListPtr exprs,
                                 EnvPtr env,
                                 CodegenContextPtr ctx);
MultiCValuePtr codegenExprAsRef(ExprPtr expr,
                                EnvPtr env,
                                CodegenContextPtr ctx);

void codegenOneInto(ExprPtr expr,
                    EnvPtr env,
                    CodegenContextPtr ctx,
                    CValuePtr out);
void codegenMultiInto(ExprListPtr exprs,
                      EnvPtr env,
                      CodegenContextPtr ctx,
                      MultiCValuePtr out);
void codegenExprInto(ExprPtr expr,
                     EnvPtr env,
                     CodegenContextPtr ctx,
                     MultiCValuePtr out);

void codegenMulti(ExprListPtr exprs,
                  EnvPtr env,
                  CodegenContextPtr ctx,
                  MultiCValuePtr out);
void codegenOne(ExprPtr expr,
                EnvPtr env,
                CodegenContextPtr ctx,
                CValuePtr out);
void codegenExpr(ExprPtr expr,
                 EnvPtr env,
                 CodegenContextPtr ctx,
                 MultiCValuePtr out);

void codegenStaticObject(ObjectPtr x,
                         CodegenContextPtr ctx,
                         MultiCValuePtr out);

void codegenGVarInstance(GVarInstancePtr x);
void codegenExternalVariable(ExternalVariablePtr x);
void codegenExternalProcedure(ExternalProcedurePtr x);

void codegenValueHolder(ValueHolderPtr x,
                        CodegenContextPtr ctx,
                        MultiCValuePtr out);
void codegenCompileTimeValue(EValuePtr ev,
                             CodegenContextPtr ctx,
                             MultiCValuePtr out);
llvm::Value *codegenSimpleConstant(EValuePtr ev);

void codegenIndexingExpr(ExprPtr indexable,
                         ExprListPtr args,
                         EnvPtr env,
                         CodegenContextPtr ctx,
                         MultiCValuePtr out);
void codegenAliasIndexing(GlobalAliasPtr x,
                          ExprListPtr args,
                          EnvPtr env,
                          CodegenContextPtr ctx,
                          MultiCValuePtr out);
void codegenCallExpr(ExprPtr callable,
                     ExprListPtr args,
                     EnvPtr env,
                     CodegenContextPtr ctx,
                     MultiCValuePtr out);
void codegenDispatch(ObjectPtr obj,
                     MultiCValuePtr args,
                     MultiPValuePtr pvArgs,
                     const vector<unsigned> &dispatchIndices,
                     CodegenContextPtr ctx,
                     MultiCValuePtr out);
void codegenCallValue(CValuePtr callable,
                      MultiCValuePtr args,
                      CodegenContextPtr ctx,
                      MultiCValuePtr out);
void codegenCallValue(CValuePtr callable,
                      MultiCValuePtr args,
                      MultiPValuePtr pvArgs,
                      CodegenContextPtr ctx,
                      MultiCValuePtr out);
bool codegenShortcut(ObjectPtr callable,
                     MultiPValuePtr pvArgs,
                     ExprListPtr args,
                     EnvPtr env,
                     CodegenContextPtr ctx,
                     MultiCValuePtr out);
void codegenCallPointer(CValuePtr x,
                        MultiCValuePtr args,
                        CodegenContextPtr ctx,
                        MultiCValuePtr out);
void codegenCallCCode(CCodePointerTypePtr type,
                      llvm::Value *llCallable,
                      MultiCValuePtr args,
                      CodegenContextPtr ctx,
                      MultiCValuePtr out);
void codegenCallCCodePointer(CValuePtr x,
                             MultiCValuePtr args,
                             CodegenContextPtr ctx,
                             MultiCValuePtr out);
void codegenCallCode(InvokeEntryPtr entry,
                     MultiCValuePtr args,
                     CodegenContextPtr ctx,
                     MultiCValuePtr out);
void codegenLowlevelCall(llvm::Value *llCallable,
                         vector<llvm::Value *>::iterator argBegin,
                         vector<llvm::Value *>::iterator argEnd,
                         CodegenContextPtr ctx);
void codegenCallInline(InvokeEntryPtr entry,
                       MultiCValuePtr args,
                       CodegenContextPtr ctx,
                       MultiCValuePtr out);
void codegenCallByName(InvokeEntryPtr entry,
                       ExprListPtr args,
                       EnvPtr env,
                       CodegenContextPtr ctx,
                       MultiCValuePtr out);

void codegenCodeBody(InvokeEntryPtr entry);

void codegenCWrapper(InvokeEntryPtr entry);

bool codegenStatement(StatementPtr stmt,
                      EnvPtr env,
                      CodegenContextPtr ctx);

void codegenCollectLabels(const vector<StatementPtr> &statements,
                          unsigned startIndex,
                          CodegenContextPtr ctx);
EnvPtr codegenBinding(BindingPtr x, EnvPtr env, CodegenContextPtr ctx);

void codegenPrimOp(PrimOpPtr x,
                   MultiCValuePtr args,
                   CodegenContextPtr ctx,
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


llvm::BasicBlock *newBasicBlock(const char *name, CodegenContextPtr ctx)
{
    return llvm::BasicBlock::Create(llvm::getGlobalContext(),
                                    name,
                                    ctx->llvmFunc);
}

static CValuePtr staticCValue(ObjectPtr obj, CodegenContextPtr ctx)
{
    TypePtr t = staticType(obj);
    return codegenAllocValue(t, ctx);
}

static CValuePtr derefValue(CValuePtr cvPtr, CodegenContextPtr ctx)
{
    assert(cvPtr->type->typeKind == POINTER_TYPE);
    PointerType *pt = (PointerType *)cvPtr->type.ptr();
    llvm::Value *ptrValue = ctx->builder->CreateLoad(cvPtr->llValue);
    return new CValue(pt->pointeeType, ptrValue);
}



//
// codegen value ops
//

void codegenValueInit(CValuePtr dest, CodegenContextPtr ctx)
{
    if (isPrimitiveAggregateType(dest->type))
        return;
    codegenCallValue(staticCValue(dest->type.ptr(), ctx),
                     new MultiCValue(),
                     ctx,
                     new MultiCValue(dest));
}

void codegenValueDestroy(CValuePtr dest, CodegenContextPtr ctx)
{
    if (isPrimitiveAggregateType(dest->type))
        return;
    bool savedCheckExceptions = ctx->checkExceptions;
    ctx->checkExceptions = false;
    codegenCallValue(staticCValue(prelude_destroy(), ctx),
                     new MultiCValue(dest),
                     ctx,
                     new MultiCValue());
    ctx->checkExceptions = savedCheckExceptions;
}

void codegenValueCopy(CValuePtr dest, CValuePtr src, CodegenContextPtr ctx)
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
    codegenCallValue(staticCValue(dest->type.ptr(), ctx),
                     new MultiCValue(src),
                     ctx,
                     new MultiCValue(dest));
}

void codegenValueMove(CValuePtr dest, CValuePtr src, CodegenContextPtr ctx)
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
    codegenCallValue(staticCValue(prelude_move(), ctx),
                     new MultiCValue(src),
                     ctx,
                     new MultiCValue(dest));
}

void codegenValueAssign(CValuePtr dest, CValuePtr src, CodegenContextPtr ctx)
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
    MultiCValuePtr args = new MultiCValue(dest);
    args->add(src);
    codegenCallValue(staticCValue(prelude_assign(), ctx),
                     args,
                     ctx,
                     new MultiCValue());
}

void codegenValueMoveAssign(CValuePtr dest,
                            CValuePtr src,
                            CodegenContextPtr ctx)
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
    MultiCValuePtr args = new MultiCValue(dest);
    args->add(src);
    MultiPValuePtr pvArgs = new MultiPValue(new PValue(dest->type, false));
    pvArgs->add(new PValue(src->type, true));
    codegenCallValue(staticCValue(prelude_assign(), ctx),
                     args,
                     pvArgs,
                     ctx,
                     new MultiCValue());
}

llvm::Value *codegenToBoolFlag(CValuePtr a, CodegenContextPtr ctx)
{
    if (a->type != boolType)
        typeError(boolType, a->type);
    llvm::Value *b1 = ctx->builder->CreateLoad(a->llValue);
    llvm::Value *zero = llvm::ConstantInt::get(llvmType(boolType), 0);
    llvm::Value *flag1 = ctx->builder->CreateICmpNE(b1, zero);
    return flag1;
}



//
// codegen temps
//

int cgMarkStack(CodegenContextPtr ctx)
{
    return ctx->valueStack.size();
}

void cgDestroyStack(int marker, CodegenContextPtr ctx)
{
    int i = (int)ctx->valueStack.size();
    assert(marker <= i);
    while (marker < i) {
        --i;
        codegenValueDestroy(ctx->valueStack[i], ctx);
    }
}

void cgPopStack(int marker, CodegenContextPtr ctx)
{
    assert(marker <= (int)ctx->valueStack.size());
    while (marker < (int)ctx->valueStack.size())
        ctx->valueStack.pop_back();
}

void cgDestroyAndPopStack(int marker, CodegenContextPtr ctx)
{
    assert(marker <= (int)ctx->valueStack.size());
    while (marker < (int)ctx->valueStack.size()) {
        codegenValueDestroy(ctx->valueStack.back(), ctx);
        ctx->valueStack.pop_back();
    }
}

void cgPushStack(CValuePtr cv, CodegenContextPtr ctx)
{
    ctx->valueStack.push_back(cv);
}

CValuePtr codegenAllocValue(TypePtr t, CodegenContextPtr ctx)
{
    const llvm::Type *llt = llvmType(t);
    llvm::Value *llv = ctx->initBuilder->CreateAlloca(llt);
    return new CValue(t, llv);
}



//
// codegenMultiArgsAsRef, codegenArgExprAsRef
//

MultiCValuePtr codegenMultiArgsAsRef(ExprListPtr exprs,
                                     EnvPtr env,
                                     CodegenContextPtr ctx)
{
    MultiCValuePtr out = new MultiCValue();
    for (unsigned i = 0; i < exprs->size(); ++i) {
        ExprPtr x = exprs->exprs[i];
        if (x->exprKind == UNPACK) {
            Unpack *y = (Unpack *)x.ptr();
            MultiCValuePtr mcv;
            if (y->expr->exprKind == TUPLE) {
                Tuple *y2 = (Tuple *)y->expr.ptr();
                mcv = codegenMultiArgsAsRef(y2->args, env, ctx);
            }
            else {
                mcv = codegenArgExprAsRef(y->expr, env, ctx);
            }
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
                                   CodegenContextPtr ctx)
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
                                 CodegenContextPtr ctx)
{
    MultiCValuePtr mcv = codegenForwardExprAsRef(expr, env, ctx);
    LocationContext loc(expr->location);
    ensureArity(mcv, 1);
    return mcv->values[0];
}

MultiCValuePtr codegenForwardMultiAsRef(ExprListPtr exprs,
                                        EnvPtr env,
                                        CodegenContextPtr ctx)
{
    MultiCValuePtr out = new MultiCValue();
    for (unsigned i = 0; i < exprs->size(); ++i) {
        ExprPtr x = exprs->exprs[i];
        if (x->exprKind == UNPACK) {
            Unpack *y = (Unpack *)x.ptr();
            MultiCValuePtr mcv;
            if (y->expr->exprKind == TUPLE) {
                Tuple *y2 = (Tuple *)y->expr.ptr();
                mcv = codegenForwardMultiAsRef(y2->args, env, ctx);
            }
            else {
                mcv = codegenForwardExprAsRef(y->expr, env, ctx);
            }
            out->add(mcv);
        }
        else {
            CValuePtr cv = codegenForwardOneAsRef(x, env, ctx);
            out->add(cv);
        }
    }
    return out;
}

MultiCValuePtr codegenForwardExprAsRef(ExprPtr expr,
                                       EnvPtr env,
                                       CodegenContextPtr ctx)
{
    MultiPValuePtr mpv = safeAnalyzeExpr(expr, env);
    MultiCValuePtr mcv = codegenExprAsRef(expr, env, ctx);
    assert(mpv->size() == mcv->size());
    MultiCValuePtr out = new MultiCValue();
    for (unsigned i = 0; i < mcv->size(); ++i) {
        CValuePtr cv = mcv->values[i];
        if (mpv->values[i]->isTemp) {
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
                          CodegenContextPtr ctx)
{
    MultiCValuePtr mcv = codegenExprAsRef(expr, env, ctx);
    LocationContext loc(expr->location);
    ensureArity(mcv, 1);
    return mcv->values[0];
}

MultiCValuePtr codegenMultiAsRef(ExprListPtr exprs,
                                 EnvPtr env,
                                 CodegenContextPtr ctx)
{
    MultiCValuePtr out = new MultiCValue();
    for (unsigned i = 0; i < exprs->size(); ++i) {
        ExprPtr x = exprs->exprs[i];
        if (x->exprKind == UNPACK) {
            Unpack *y = (Unpack *)x.ptr();
            MultiCValuePtr mcv;
            if (y->expr->exprKind == TUPLE) {
                Tuple *y2 = (Tuple *)y->expr.ptr();
                mcv = codegenMultiAsRef(y2->args, env, ctx);
            }
            else {
                mcv = codegenExprAsRef(y->expr, env, ctx);
            }
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
                                        CodegenContextPtr ctx)
{
    MultiPValuePtr mpv = safeAnalyzeExpr(expr, env);
    MultiCValuePtr mcv = new MultiCValue();
    for (unsigned i = 0; i < mpv->size(); ++i) {
        PValuePtr pv = mpv->values[i];
        if (pv->isTemp) {
            CValuePtr cv = codegenAllocValue(pv->type, ctx);
            mcv->add(cv);
        }
        else {
            CValuePtr cvPtr = codegenAllocValue(pointerType(pv->type), ctx);
            mcv->add(cvPtr);
        }
    }
    codegenExpr(expr, env, ctx, mcv);
    MultiCValuePtr out = new MultiCValue();
    for (unsigned i = 0; i < mpv->size(); ++i) {
        if (mpv->values[i]->isTemp) {
            out->add(mcv->values[i]);
            cgPushStack(mcv->values[i], ctx);
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
                                               CodegenContextPtr ctx)
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
                                CodegenContextPtr ctx)
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
    case CALL : {
        Call *x = (Call *)expr.ptr();
        PValuePtr pv = safeAnalyzeOne(x->expr, env);
        if (pv->type->typeKind == STATIC_TYPE) {
            StaticType *st = (StaticType *)pv->type.ptr();
            if (st->obj == prelude_move()) {
                MultiCValuePtr mcv = codegenMultiAsRef(x->args, env, ctx);
                ensureArity(mcv, 1);
                return mcv;
            }
        }
        break;
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
                    CodegenContextPtr ctx,
                    CValuePtr out)
{
    PValuePtr pv = safeAnalyzeOne(expr, env);
    int marker = cgMarkStack(ctx);
    if (pv->isTemp) {
        codegenOne(expr, env, ctx, out);
    }
    else {
        CValuePtr cvPtr = codegenAllocValue(pointerType(pv->type), ctx);
        codegenOne(expr, env, ctx, cvPtr);
        codegenValueCopy(out, derefValue(cvPtr, ctx), ctx);
    }
    cgDestroyAndPopStack(marker, ctx);
}

void codegenMultiInto(ExprListPtr exprs,
                      EnvPtr env,
                      CodegenContextPtr ctx,
                      MultiCValuePtr out)
{
    int marker = cgMarkStack(ctx);
    int marker2 = marker;
    unsigned j = 0;
    for (unsigned i = 0; i < exprs->size(); ++i) {
        unsigned prevJ = j;
        ExprPtr x = exprs->exprs[i];
        if (x->exprKind == UNPACK) {
            Unpack *y = (Unpack *)x.ptr();
            if (y->expr->exprKind == TUPLE) {
                Tuple *y2 = (Tuple *)y->expr.ptr();
                MultiPValuePtr mpv = safeAnalyzeMulti(y2->args, env);
                assert(j + mpv->size() <= out->size());
                MultiCValuePtr out2 = new MultiCValue();
                for (unsigned k = 0; k < mpv->size(); ++k)
                    out2->add(out->values[j + k]);
                codegenMultiInto(y2->args, env, ctx, out2);
                j += mpv->size();
            }
            else {
                MultiPValuePtr mpv = safeAnalyzeExpr(y->expr, env);
                assert(j + mpv->size() <= out->size());
                MultiCValuePtr out2 = new MultiCValue();
                for (unsigned k = 0; k < mpv->size(); ++k)
                    out2->add(out->values[j + k]);
                codegenExprInto(y->expr, env, ctx, out2);
                j += mpv->size();
            }
        }
        else {
            MultiPValuePtr mpv = safeAnalyzeExpr(x, env);
            if (mpv->size() != 1)
                arityError(x, 1, mpv->size());
            assert(j < out->size());
            codegenOneInto(x, env, ctx, out->values[j]);
            ++j;
        }
        cgDestroyAndPopStack(marker2, ctx);
        for (unsigned k = prevJ; k < j; ++k)
            cgPushStack(out->values[k], ctx);
        marker2 = cgMarkStack(ctx);
    }
    assert(j == out->size());
    cgPopStack(marker, ctx);
}

void codegenExprInto(ExprPtr expr,
                     EnvPtr env,
                     CodegenContextPtr ctx,
                     MultiCValuePtr out)
{
    MultiPValuePtr mpv = safeAnalyzeExpr(expr, env);
    assert(out->size() == mpv->size());
    int marker = cgMarkStack(ctx);
    MultiCValuePtr mcv = new MultiCValue();
    for (unsigned i = 0; i < mpv->size(); ++i) {
        PValuePtr pv = mpv->values[i];
        if (pv->isTemp) {
            mcv->add(out->values[i]);
        }
        else {
            CValuePtr cvPtr = codegenAllocValue(pointerType(pv->type), ctx);
            mcv->add(cvPtr);
        }
    }
    codegenExpr(expr, env, ctx, mcv);
    int marker2 = cgMarkStack(ctx);
    for (unsigned i = 0; i < mpv->size(); ++i) {
        if (!mpv->values[i]->isTemp) {
            CValuePtr cv = derefValue(mcv->values[i], ctx);
            codegenValueCopy(out->values[i], cv, ctx);
        }
        cgPushStack(out->values[i], ctx);
    }
    cgPopStack(marker2, ctx);
    cgDestroyAndPopStack(marker, ctx);
}



//
// codegenMulti, codegenOne, codegenExpr
//

void codegenMulti(ExprListPtr exprs,
                  EnvPtr env,
                  CodegenContextPtr ctx,
                  MultiCValuePtr out)
{
    int marker = cgMarkStack(ctx);
    int marker2 = marker;
    unsigned j = 0;
    for (unsigned i = 0; i < exprs->size(); ++i) {
        unsigned prevJ = j;
        ExprPtr x = exprs->exprs[i];
        if (x->exprKind == UNPACK) {
            Unpack *y = (Unpack *)x.ptr();
            if (y->expr->exprKind == TUPLE) {
                Tuple *y2 = (Tuple *)y->expr.ptr();
                MultiPValuePtr mpv = safeAnalyzeMulti(y2->args, env);
                assert(j + mpv->size() <= out->size());
                MultiCValuePtr out2 = new MultiCValue();
                for (unsigned k = 0; k < mpv->size(); ++k)
                    out2->add(out->values[j + k]);
                codegenMulti(y2->args, env, ctx, out2);
                j += mpv->size();
            }
            else {
                MultiPValuePtr mpv = safeAnalyzeExpr(y->expr, env);
                assert(j + mpv->size() <= out->size());
                MultiCValuePtr out2 = new MultiCValue();
                for (unsigned k = 0; k < mpv->size(); ++k)
                    out2->add(out->values[j + k]);
                codegenExpr(y->expr, env, ctx, out2);
                j += mpv->size();
            }
        }
        else {
            MultiPValuePtr mpv = safeAnalyzeExpr(x, env);
            if (mpv->size() != 1)
                arityError(x, 1, mpv->size());
            assert(j < out->size());
            codegenOne(x, env, ctx, out->values[j]);
            ++j;
        }
        cgDestroyAndPopStack(marker2, ctx);
        for (unsigned k = prevJ; k < j; ++k)
            cgPushStack(out->values[k], ctx);
        marker2 = cgMarkStack(ctx);
    }
    assert(j == out->size());
    cgPopStack(marker, ctx);
}

void codegenOne(ExprPtr expr,
                EnvPtr env,
                CodegenContextPtr ctx,
                CValuePtr out)
{
    codegenExpr(expr, env, ctx, new MultiCValue(out));
}

void codegenExpr(ExprPtr expr,
                 EnvPtr env,
                 CodegenContextPtr ctx,
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
        ValueHolderPtr y = parseIntLiteral(x);
        codegenValueHolder(y, ctx, out);
        break;
    }

    case FLOAT_LITERAL : {
        FloatLiteral *x = (FloatLiteral *)expr.ptr();
        ValueHolderPtr y = parseFloatLiteral(x);
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

    case STRING_LITERAL : {
        StringLiteral *x = (StringLiteral *)expr.ptr();
        llvm::Constant *initializer =
            llvm::ConstantArray::get(llvm::getGlobalContext(),
                                     x->value,
                                     true);
        TypePtr type = arrayType(int8Type, x->value.size() + 1);
        llvm::GlobalVariable *gvar = new llvm::GlobalVariable(
            *llvmModule, llvmType(type), true,
            llvm::GlobalVariable::PrivateLinkage,
            initializer, "clayliteral_str");
        llvm::Value *str =
            ctx->builder->CreateConstGEP2_32(gvar, 0, 0);
        llvm::Value *strEnd =
            ctx->builder->CreateConstGEP2_32(gvar, 0, x->value.size());
        TypePtr ptrInt8Type = pointerType(int8Type);
        CValuePtr cvFirst = codegenAllocValue(ptrInt8Type, ctx);
        CValuePtr cvLast = codegenAllocValue(ptrInt8Type, ctx);
        ctx->builder->CreateStore(str, cvFirst->llValue);
        ctx->builder->CreateStore(strEnd, cvLast->llValue);
        MultiCValuePtr args = new MultiCValue();
        args->add(cvFirst);
        args->add(cvLast);
        codegenCallValue(staticCValue(prelude_StringConstant(), ctx),
                         args,
                         ctx,
                         out);
        break;
    }

    case IDENTIFIER_LITERAL :
        break;

    case NAME_REF : {
        NameRef *x = (NameRef *)expr.ptr();
        ObjectPtr y = safeLookupEnv(env, x->name);
        if (y->objKind == EXPRESSION) {
            ExprPtr z = (Expr *)y.ptr();
            codegenExpr(z, env, ctx, out);
        }
        else if (y->objKind == EXPR_LIST) {
            ExprListPtr z = (ExprList *)y.ptr();
            codegenMulti(z, env, ctx, out);
        }
        else {
            codegenStaticObject(y, ctx, out);
        }
        break;
    }

    case TUPLE : {
        Tuple *x = (Tuple *)expr.ptr();
        if ((x->args->size() == 1) &&
            (x->args->exprs[0]->exprKind != UNPACK))
        {
            codegenExpr(x->args->exprs[0], env, ctx, out);
        }
        else {
            ExprPtr tupleLiteral = prelude_expr_tupleLiteral();
            codegenCallExpr(tupleLiteral, x->args, env, ctx, out);
        }
        break;
    }

    case ARRAY : {
        Array *x = (Array *)expr.ptr();
        codegenCallExpr(prelude_expr_arrayLiteral(), x->args, env, ctx, out);
        break;
    }

    case INDEXING : {
        Indexing *x = (Indexing *)expr.ptr();
        codegenIndexingExpr(x->expr, x->args, env, ctx, out);
        break;
    }

    case CALL : {
        Call *x = (Call *)expr.ptr();
        codegenCallExpr(x->expr, x->args, env, ctx, out);
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

    case UNARY_OP : {
        UnaryOp *x = (UnaryOp *)expr.ptr();
        if (x->op == ADDRESS_OF) {
            PValuePtr pv = safeAnalyzeOne(x->expr, env);
            if (pv->isTemp)
                error("can't take address of a temporary");
        }
        if (!x->desugared)
            x->desugared = desugarUnaryOp(x);
        codegenExpr(x->desugared, env, ctx, out);
        break;
    }

    case BINARY_OP : {
        BinaryOp *x = (BinaryOp *)expr.ptr();
        if (!x->desugared)
            x->desugared = desugarBinaryOp(x);
        codegenExpr(x->desugared, env, ctx, out);
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
        llvm::Value *v2 = ctx->builder->CreateZExt(flag2, llvmType(boolType));
        ctx->builder->CreateStore(v2, out0->llValue);
        cgDestroyAndPopStack(marker, ctx);
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
        llvm::Value *v2 = ctx->builder->CreateZExt(flag2, llvmType(boolType));
        ctx->builder->CreateStore(v2, out0->llValue);
        cgDestroyAndPopStack(marker, ctx);
        ctx->builder->CreateBr(mergeBlock);

        ctx->builder->SetInsertPoint(trueBlock);
        llvm::Value *one = llvm::ConstantInt::get(llvmType(boolType), 1);
        ctx->builder->CreateStore(one, out0->llValue);
        ctx->builder->CreateBr(mergeBlock);

        ctx->builder->SetInsertPoint(mergeBlock);
        break;
    }

    case IF_EXPR : {
        IfExpr *x = (IfExpr *)expr.ptr();
        if (!x->desugared)
            x->desugared = desugarIfExpr(x);
        codegenExpr(x->desugared, env, ctx, out);
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

    case NEW : {
        New *x = (New *)expr.ptr();
        if (!x->desugared)
            x->desugared = desugarNew(x);
        codegenExpr(x->desugared, env, ctx, out);
        break;
    }

    case STATIC_EXPR : {
        StaticExpr *x = (StaticExpr *)expr.ptr();
        if (!x->desugared)
            x->desugared = desugarStaticExpr(x);
        codegenExpr(x->desugared, env, ctx, out);
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
                         CodegenContextPtr ctx,
                         MultiCValuePtr out)
{
    switch (x->objKind) {

    case ENUM_MEMBER : {
        EnumMember *y = (EnumMember *)x.ptr();
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0->type == y->type);
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
            codegenExternalProcedure(y);
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0->type == y->ptrType);
        ctx->builder->CreateStore(y->llvmFunc, out0->llValue);
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
        if (y->forwardedRValue) {
            assert(out0->type == y->type);
            codegenValueMove(out0, y, ctx);
        }
        else {
            assert(out0->type == pointerType(y->type));
            ctx->builder->CreateStore(y->llValue, out0->llValue);
        }
        break;
    }

    case MULTI_CVALUE : {
        MultiCValue *y = (MultiCValue *)x.ptr();
        assert(out->size() == y->size());
        for (unsigned i = 0; i < y->size(); ++i) {
            CValuePtr vi = y->values[i];
            CValuePtr outi = out->values[i];
            if (vi->forwardedRValue) {
                assert(outi->type == vi->type);
                codegenValueMove(outi, vi, ctx);
            }
            else {
                assert(outi->type == pointerType(vi->type));
                ctx->builder->CreateStore(vi->llValue, outi->llValue);
            }
        }
        break;
    }

    case PVALUE : {
        // allow values of static type
        PValue *y = (PValue *)x.ptr();
        if (y->type->typeKind != STATIC_TYPE)
            invalidStaticObjectError(x);
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
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
            CValuePtr outi = out->values[i];
            if (pv->isTemp)
                assert(outi->type == pv->type);
            else
                assert(outi->type == pointerType(pv->type));
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

void codegenGVarInstance(GVarInstancePtr x)
{
    assert(!x->llGlobal);
    MultiPValuePtr mpv = safeAnalyzeGVarInstance(x);
    assert(mpv->size() == 1);
    PValuePtr y = mpv->values[0];
    llvm::Constant *initializer =
        llvm::Constant::getNullValue(llvmType(y->type));
    ostringstream ostr;
    ostr << "clay_" << x->gvar->name->str << "_" << y->type;
    x->llGlobal =
        new llvm::GlobalVariable(
            *llvmModule, llvmType(y->type), false,
            llvm::GlobalVariable::InternalLinkage,
            initializer, ostr.str());

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
    InvokeEntryPtr entry =
        codegenCallable(prelude_destroy(),
                        vector<TypePtr>(1, y->type),
                        vector<ValueTempness>(1, TEMPNESS_LVALUE));

    initializedGlobals.push_back(new CValue(y->type, x->llGlobal));
}



//
// codegenExternalVariable
//

void codegenExternalVariable(ExternalVariablePtr x)
{
    assert(!x->llGlobal);
    if (!x->attributesVerified)
        verifyAttributes(x);
    PValuePtr pv = analyzeExternalVariable(x);
    llvm::GlobalVariable::LinkageTypes linkage;
    if (x->attrDLLImport)
        linkage = llvm::GlobalVariable::DLLImportLinkage;
    else if (x->attrDLLExport)
        linkage = llvm::GlobalVariable::DLLExportLinkage;
    else
        linkage = llvm::GlobalVariable::ExternalLinkage;
    x->llGlobal =
        new llvm::GlobalVariable(
            *llvmModule, llvmType(pv->type), false,
            linkage, NULL, x->name->str);
}



//
// codegenExternalProcedure
//

void codegenExternalProcedure(ExternalProcedurePtr x)
{
    if (x->llvmFunc != NULL)
        return;
    if (!x->analyzed)
        analyzeExternalProcedure(x);
    assert(x->analyzed);
    assert(!x->llvmFunc);
    vector<const llvm::Type *> llArgTypes;
    for (unsigned i = 0; i < x->args.size(); ++i)
        llArgTypes.push_back(llvmType(x->args[i]->type2));
    const llvm::Type *llRetType =
        x->returnType2.ptr() ? llvmType(x->returnType2) : llvmVoidType();
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
        string llvmIntrinsicsPrefix = "llvm.";
        string prefix = x->attrAsmLabel.substr(0, llvmIntrinsicsPrefix.size());
        if (prefix != llvmIntrinsicsPrefix) {
            // '\01' is the llvm marker to specify asm label
            llvmFuncName = "\01" + x->attrAsmLabel;
        }
        else {
            llvmFuncName = x->attrAsmLabel;
        }
    }
    else {
        llvmFuncName = x->name->str;
    }

    llvm::Function *func = llvmModule->getFunction(llvmFuncName);
    if (!func) {
        func = llvm::Function::Create(llFuncType,
                                      linkage,
                                      llvmFuncName,
                                      llvmModule);
    }
    x->llvmFunc = func;
    switch (x->callingConv) {
    case CC_DEFAULT :
        break;
    case CC_STDCALL :
        x->llvmFunc->setCallingConv(llvm::CallingConv::X86_StdCall);
        break;
    case CC_FASTCALL :
        x->llvmFunc->setCallingConv(llvm::CallingConv::X86_FastCall);
        break;
    default :
        assert(false);
    }

    if (!x->body) return;

    CodegenContextPtr ctx = new CodegenContext(x->llvmFunc);

    llvm::BasicBlock *initBlock = newBasicBlock("init", ctx);
    llvm::BasicBlock *codeBlock = newBasicBlock("code", ctx);
    llvm::BasicBlock *returnBlock = newBasicBlock("return", ctx);
    llvm::BasicBlock *exceptionBlock = newBasicBlock("exception", ctx);

    ctx->initBuilder = new llvm::IRBuilder<>(initBlock);
    ctx->builder = new llvm::IRBuilder<>(codeBlock);

    EnvPtr env = new Env(x->env);

    llvm::Function::arg_iterator ai = x->llvmFunc->arg_begin();
    for (unsigned i = 0; i < x->args.size(); ++i, ++ai) {
        ExternalArgPtr arg = x->args[i];
        llvm::Argument *llArg = &(*ai);
        llArg->setName(arg->name->str);
        llvm::Value *llArgVar =
            ctx->initBuilder->CreateAlloca(llvmType(arg->type2));
        llArgVar->setName(arg->name->str);
        ctx->builder->CreateStore(llArg, llArgVar);
        CValuePtr cvalue = new CValue(arg->type2, llArgVar);
        addLocal(env, arg->name, cvalue.ptr());
    }

    vector<CReturn> returns;
    if (x->returnType2.ptr()) {
        llvm::Value *llRetVal =
            ctx->initBuilder->CreateAlloca(llvmType(x->returnType2));
        CValuePtr cret = new CValue(x->returnType2, llRetVal);
        returns.push_back(CReturn(false, x->returnType2, cret));
    }

    ctx->returnLists.push_back(returns);
    JumpTarget returnTarget(returnBlock, cgMarkStack(ctx));
    ctx->returnTargets.push_back(returnTarget);
    JumpTarget exceptionTarget(exceptionBlock, cgMarkStack(ctx));
    ctx->exceptionTargets.push_back(exceptionTarget);

    bool terminated = codegenStatement(x->body, env, ctx);
    if (!terminated) {
        if (x->returnType2.ptr()) {
            error(x, "not all paths have a return statement");
        }
        cgDestroyStack(returnTarget.stackMarker, ctx);
        ctx->builder->CreateBr(returnBlock);
    }
    cgPopStack(returnTarget.stackMarker, ctx);

    ctx->builder->SetInsertPoint(returnBlock);
    if (!x->returnType2) {
        ctx->builder->CreateRetVoid();
    }
    else {
        CValuePtr retVal = returns[0].value;
        llvm::Value *v = ctx->builder->CreateLoad(retVal->llValue);
        ctx->builder->CreateRet(v);
    }

    ctx->builder->SetInsertPoint(exceptionBlock);
    if (exceptionsEnabled()) {
        ObjectPtr callable = prelude_unhandledExceptionInExternal();
        codegenCallValue(staticCValue(callable, ctx),
                         new MultiCValue(),
                         ctx,
                         new MultiCValue());
    }
    ctx->builder->CreateUnreachable();

    ctx->initBuilder->CreateBr(codeBlock);
}



//
// codegenValueHolder
//

void codegenValueHolder(ValueHolderPtr v,
                        CodegenContextPtr ctx,
                        MultiCValuePtr out)
{
    EValuePtr ev = new EValue(v->type, v->buf);
    codegenCompileTimeValue(ev, ctx, out);
}



//
// codegenCompileTimeValue
//

void codegenCompileTimeValue(EValuePtr ev,
                             CodegenContextPtr ctx,
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
        ostringstream sout;
        sout << "constants of type " << ev->type
             << " are not yet supported";
        // TODO: support complex constants
        
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
                         CodegenContextPtr ctx,
                         MultiCValuePtr out)
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
    codegenCallExpr(prelude_expr_index(), args2, env, ctx, out);
}



//
// codegenAliasIndexing
//

void codegenAliasIndexing(GlobalAliasPtr x,
                          ExprListPtr args,
                          EnvPtr env,
                          CodegenContextPtr ctx,
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
    codegenExpr(x->expr, bodyEnv, ctx, out);
}



//
// codegenCallExpr
//

void codegenCallExpr(ExprPtr callable,
                     ExprListPtr args,
                     EnvPtr env,
                     CodegenContextPtr ctx,
                     MultiCValuePtr out)
{
    PValuePtr pv = safeAnalyzeOne(callable, env);

    switch (pv->type->typeKind) {
    case CODE_POINTER_TYPE :
        CValuePtr cv = codegenOneAsRef(callable, env, ctx);
        MultiCValuePtr mcv = codegenMultiAsRef(args, env, ctx);
        codegenCallPointer(cv, mcv, ctx, out);
        return;
    }

    if ((pv->type->typeKind == CCODE_POINTER_TYPE)
        && (callable->exprKind == NAME_REF))
    {
        CCodePointerType *t = (CCodePointerType *)pv->type.ptr();
        NameRef *x = (NameRef *)callable.ptr();
        ObjectPtr y = safeLookupEnv(env, x->name);
        if (y->objKind == EXTERNAL_PROCEDURE) {
            ExternalProcedure *z = (ExternalProcedure *)y.ptr();
            if (!z->llvmFunc)
                codegenExternalProcedure(z);
            MultiCValuePtr mcv = codegenMultiAsRef(args, env, ctx);
            codegenCallCCode(t, z->llvmFunc, mcv, ctx, out);
            return;
        }
    }

    if (pv->type->typeKind != STATIC_TYPE) {
        ExprListPtr args2 = new ExprList(callable);
        args2->add(args);
        codegenCallExpr(prelude_expr_call(), args2, env, ctx, out);
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
        InvokeEntryPtr entry = safeAnalyzeCallable(obj, argsKey, argsTempness);
        if (entry->callByName) {
            codegenCallByName(entry, args, env, ctx, out);
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

llvm::Value *codegenVariantTag(CValuePtr cv, CodegenContextPtr ctx)
{
    CValuePtr ctag = codegenAllocValue(cIntType, ctx);
    codegenCallValue(staticCValue(prelude_variantTag(), ctx),
                     new MultiCValue(cv),
                     ctx,
                     new MultiCValue(ctag));
    return ctx->builder->CreateLoad(ctag->llValue);
}

CValuePtr codegenVariantIndex(CValuePtr cv, int tag, CodegenContextPtr ctx)
{
    assert(cv->type->typeKind == VARIANT_TYPE);
    VariantType *vt = (VariantType *)cv->type.ptr();
    const vector<TypePtr> &memberTypes = variantMemberTypes(vt);
    assert((tag >= 0) && (unsigned(tag) < memberTypes.size()));

    MultiCValuePtr args = new MultiCValue();
    args->add(cv);
    ValueHolderPtr vh = intToValueHolder(tag);
    args->add(staticCValue(vh.ptr(), ctx));

    CValuePtr cvPtr = codegenAllocValue(pointerType(memberTypes[tag]), ctx);
    MultiCValuePtr out = new MultiCValue(cvPtr);

    codegenCallValue(staticCValue(prelude_unsafeVariantIndex(), ctx),
                     args,
                     ctx,
                     out);
    llvm::Value *llv = ctx->builder->CreateLoad(cvPtr->llValue);
    return new CValue(memberTypes[tag], llv);
}

void codegenDispatch(ObjectPtr obj,
                     MultiCValuePtr args,
                     MultiPValuePtr pvArgs,
                     const vector<unsigned> &dispatchIndices,
                     CodegenContextPtr ctx,
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

    llvm::Value *llTag = codegenVariantTag(cvDispatch, ctx);

    vector<llvm::BasicBlock *> callBlocks;
    vector<llvm::BasicBlock *> elseBlocks;

    for (unsigned i = 0; i < memberTypes.size(); ++i) {
        callBlocks.push_back(newBasicBlock("dispatchCase", ctx));
        elseBlocks.push_back(newBasicBlock("dispatchNext", ctx));
    }
    llvm::BasicBlock *finalBlock = newBasicBlock("finalBlock", ctx);

    for (unsigned i = 0; i < memberTypes.size(); ++i) {
        llvm::Value *tagCase = llvm::ConstantInt::get(llvmIntType(32), i);
        llvm::Value *cond = ctx->builder->CreateICmpEQ(llTag, tagCase);
        ctx->builder->CreateCondBr(cond, callBlocks[i], elseBlocks[i]);

        ctx->builder->SetInsertPoint(callBlocks[i]);

        MultiCValuePtr args2 = new MultiCValue();
        args2->add(prefix);
        args2->add(codegenVariantIndex(cvDispatch, i, ctx));
        args2->add(suffix);
        MultiPValuePtr pvArgs2 = new MultiPValue();
        pvArgs2->add(pvPrefix);
        pvArgs2->add(new PValue(memberTypes[i], pvDispatch->isTemp));
        pvArgs2->add(pvSuffix);
        codegenDispatch(obj, args2, pvArgs2, dispatchIndices2, ctx, out);
        
        ctx->builder->CreateBr(finalBlock);

        ctx->builder->SetInsertPoint(elseBlocks[i]);
    }

    codegenCallValue(staticCValue(prelude_invalidVariant(), ctx),
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
                      CodegenContextPtr ctx,
                      MultiCValuePtr out)
{
    MultiPValuePtr pvArgs = new MultiPValue();
    for (unsigned i = 0; i < args->size(); ++i)
        pvArgs->add(new PValue(args->values[i]->type, false));
    codegenCallValue(callable, args, pvArgs, ctx, out);
}


void codegenCallValue(CValuePtr callable,
                      MultiCValuePtr args,
                      MultiPValuePtr pvArgs,
                      CodegenContextPtr ctx,
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
            new MultiPValue(new PValue(callable->type, false));
        pvArgs2->add(pvArgs);
        codegenCallValue(staticCValue(prelude_call(), ctx),
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
        InvokeEntryPtr entry = safeAnalyzeCallable(obj, argsKey, argsTempness);
        if (entry->callByName)
            error("call to call-by-name code not allowed in this context");
        assert(entry->analyzed);
        codegenCallCode(entry, args, ctx, out);
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
                     CodegenContextPtr ctx,
                     MultiCValuePtr out)
{
    switch (callable->objKind) {
    case TYPE : {
        TypePtr t = (Type *)callable.ptr();
        if (!isPrimitiveAggregateType(t))
            return false;
        if (pvArgs->size() > 1)
            return false;
        if (pvArgs->size() == 0)
            return true;
        PValuePtr pv = pvArgs->values[0];
        if ((pv->type == t) && (!isPrimitiveAggregateTooLarge(t))) {
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
        if (callable == prelude_destroy()) {
            if (pvArgs->size() != 1)
                return false;
            if (!isPrimitiveAggregateType(pvArgs->values[0]->type))
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
                        CodegenContextPtr ctx,
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

static llvm::Value *promoteCVarArg(TypePtr t,
                                   llvm::Value *llv,
                                   CodegenContextPtr ctx)
{
    switch (t->typeKind) {
    case INTEGER_TYPE : {
        IntegerType *it = (IntegerType *)t.ptr();
        if (it->bits < 32) {
            if (it->isSigned)
                return ctx->builder->CreateSExt(llv, llvmType(int32Type));
            else
                return ctx->builder->CreateZExt(llv, llvmType(uint32Type));
        }
        return llv;
    }
    case FLOAT_TYPE : {
        FloatType *ft = (FloatType *)t.ptr();
        if (ft->bits == 32)
            return ctx->builder->CreateFPExt(llv, llvmType(float64Type));
        return llv;
    }
    default :
        return llv;
    }
}

void codegenCallCCode(CCodePointerTypePtr t,
                      llvm::Value *llCallable,
                      MultiCValuePtr args,
                      CodegenContextPtr ctx,
                      MultiCValuePtr out)
{
    if (!t->hasVarArgs)
        ensureArity(args, t->argTypes.size());
    else if (args->size() < t->argTypes.size())
        arityError2(t->argTypes.size(), args->size());
    vector<llvm::Value *> llArgs;
    for (unsigned i = 0; i < t->argTypes.size(); ++i) {
        CValuePtr cv = args->values[i];
        if (cv->type != t->argTypes[i])
            argumentTypeError(i, t->argTypes[i], cv->type);
        llvm::Value *llv = ctx->builder->CreateLoad(cv->llValue);
        llArgs.push_back(llv);
    }
    if (t->hasVarArgs) {
        for (unsigned i = t->argTypes.size(); i < args->size(); ++i) {
            CValuePtr cv = args->values[i];
            llvm::Value *llv = ctx->builder->CreateLoad(cv->llValue);
            llvm::Value *llv2 = promoteCVarArg(cv->type, llv, ctx);
            llArgs.push_back(llv2);
        }
    }
    llvm::CallInst *callInst =
        ctx->builder->CreateCall(llCallable,
                                 llArgs.begin(),
                                 llArgs.end());
    switch (t->callingConv) {
    case CC_DEFAULT :
        break;
    case CC_STDCALL :
        callInst->setCallingConv(llvm::CallingConv::X86_StdCall);
        break;
    case CC_FASTCALL :
        callInst->setCallingConv(llvm::CallingConv::X86_FastCall);
        break;
    default :
        assert(false);
    }

    llvm::Value *llRet = callInst;
    if (t->returnType.ptr()) {
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0->type == t->returnType);
        ctx->builder->CreateStore(llRet, out0->llValue);
    }
    else {
        assert(out->size() == 0);
    }
}

void codegenCallCCodePointer(CValuePtr x,
                             MultiCValuePtr args,
                             CodegenContextPtr ctx,
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

void codegenCallCode(InvokeEntryPtr entry,
                     MultiCValuePtr args,
                     CodegenContextPtr ctx,
                     MultiCValuePtr out)
{
    if (inlineEnabled() && entry->isInline) {
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
        if (cv->type != entry->argsKey[i])
            argumentTypeError(i, entry->argsKey[i], cv->type);
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
                         CodegenContextPtr ctx)
{
    llvm::Value *result =
        ctx->builder->CreateCall(llCallable, argBegin, argEnd);
    if (!exceptionsEnabled())
        return;
    if (!ctx->checkExceptions)
        return;
    llvm::Value *zero = llvm::ConstantInt::get(llvmIntType(32), 0);
    llvm::Value *cond = ctx->builder->CreateICmpEQ(result, zero);
    llvm::BasicBlock *landing = newBasicBlock("landing", ctx);
    llvm::BasicBlock *normal = newBasicBlock("normal", ctx);
    ctx->builder->CreateCondBr(cond, normal, landing);

    ctx->builder->SetInsertPoint(landing);
    JumpTarget &jt = ctx->exceptionTargets.back();
    cgDestroyStack(jt.stackMarker, ctx);
    ctx->builder->CreateBr(jt.block);
    ++jt.useCount;

    ctx->builder->SetInsertPoint(normal);
}



//
// codegenCallable
//

InvokeEntryPtr codegenCallable(ObjectPtr x,
                               const vector<TypePtr> &argsKey,
                               const vector<ValueTempness> &argsTempness)
{
    InvokeEntryPtr entry =
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

static string::const_iterator parseIdentifier(string::const_iterator first,
                                              string::const_iterator last)
{
    string::const_iterator i = first;
    if ((i == last) || !isFirstIdentChar(*i))
        return first;
    ++i;
    while ((i != last) && isIdentChar(*i))
        ++i;
    return i;
}

static string::const_iterator parseBracketedExpr(string::const_iterator first,
                                                 string::const_iterator last)
{
    string::const_iterator i = first;
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
                            EnvPtr env, llvm::raw_string_ostream &outstream)
{
    ExprPtr expr = parseExpr(source, offset, length);
    ObjectPtr x = evaluateOneStatic(expr, env);
    LocationContext loc(expr->location);
    if (x->objKind == TYPE) {
        Type *type = (Type *)x.ptr();
        WriteTypeSymbolic(outstream, llvmType(type), NULL);
    }
    else if (x->objKind == VALUE_HOLDER) {
        ValueHolder *vh = (ValueHolder *)x.ptr();
        if ((vh->type->typeKind == BOOL_TYPE)
            ||(vh->type->typeKind == INTEGER_TYPE)
            ||(vh->type->typeKind == FLOAT_TYPE))
        {
            ostringstream sout;
            printValue(sout, new EValue(vh->type, vh->buf));
            outstream << sout.str();
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
        ostringstream sout;
        printName(sout, x);
        outstream << sout.str();
    }
}

static bool interpolateLLVMCode(LLVMCodePtr llvmBody, string &out, EnvPtr env)
{
    SourcePtr source = llvmBody->location->source;
    int startingOffset = llvmBody->location->offset;

    const string &body = llvmBody->body;
    llvm::raw_string_ostream outstream(out);
    string::const_iterator i = body.begin();
    while (i != body.end()) {
        if (*i != '$') {
            outstream << *i;
            ++i;
            continue;
        }

        string::const_iterator first, last;
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
    out = outstream.str();
    return true;
}



//
// codegenLLVMBody
//

void codegenLLVMBody(InvokeEntryPtr entry, const string &callableName) 
{
    string llFunc;
    llvm::raw_string_ostream out(llFunc);
    int argCount = 0;
    static int id = 1;
    ostringstream functionName;

    functionName << "clay_" << callableName << id;
    id++;

    out << string("define internal i32 @\"") 
        << functionName.str() << string("\"(");

    vector<const llvm::Type *> llArgTypes;
    for (unsigned i = 0; i < entry->argsKey.size(); ++i) {
        const llvm::Type *argType = llvmPointerType(entry->argsKey[i]);
        if (argCount > 0) out << string(", ");
        WriteTypeSymbolic(out, argType, NULL);
        out << string(" %\"") << entry->fixedArgNames[i]->str << string("\"");
        argCount ++;
    }
    for (unsigned i = 0; i < entry->returnTypes.size(); ++i) {
        const llvm::Type *argType;
        TypePtr rt = entry->returnTypes[i];
        if (entry->returnIsRef[i])
            argType = llvmPointerType(pointerType(rt));
        else
            argType = llvmPointerType(rt);
        if (argCount > 0) out << string(", ");
        WriteTypeSymbolic(out, argType, NULL);
        assert(i < entry->code->returnSpecs.size());
        const ReturnSpecPtr spec = entry->code->returnSpecs[i];
        assert(spec->name.ptr());
        out << string(" %") << spec->name->str;
        argCount ++;
    }

    string body;
    if (!interpolateLLVMCode(entry->code->llvmBody, body, entry->env))
        error("failed to apply template");

    out << string(") ") << body;

    llvm::SMDiagnostic err;
    llvm::MemoryBuffer *buf = 
        llvm::MemoryBuffer::getMemBuffer(llvm::StringRef(out.str()));

    if(!llvm::ParseAssembly(buf, llvmModule, err, 
                llvm::getGlobalContext())) {
        err.Print("\n", out);
        std::cerr << out.str() << std::endl;
        error("llvm assembly parse error");
    }

    entry->llvmFunc = llvmModule->getFunction(functionName.str());
    assert(entry->llvmFunc);
}



//
// getCodeName
//

static string getCodeName(InvokeEntryPtr entry)
{
    SafePrintNameEnabler enabler;
    ObjectPtr x = entry->callable;
    ostringstream sout;
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
    return sout.str();
}



//
// codegenCodeBody
//

void codegenCodeBody(InvokeEntryPtr entry)
{
    assert(entry->analyzed);
    assert(!entry->llvmFunc);

    string callableName = getCodeName(entry);

    if (entry->code->isInlineLLVM()) {
        codegenLLVMBody(entry, callableName);
        return;
    }

    vector<const llvm::Type *> llArgTypes;
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
        llvm::FunctionType::get(llvmIntType(32), llArgTypes, false);

    llvm::Function *llFunc =
        llvm::Function::Create(llFuncType,
                               llvm::Function::InternalLinkage,
                               "clay_" + callableName,
                               llvmModule);

    entry->llvmFunc = llFunc;

    CodegenContextPtr ctx = new CodegenContext(llFunc);

    llvm::BasicBlock *initBlock = newBasicBlock("init", ctx);
    llvm::BasicBlock *codeBlock = newBasicBlock("code", ctx);
    llvm::BasicBlock *returnBlock = newBasicBlock("return", ctx);
    llvm::BasicBlock *exceptionBlock = newBasicBlock("exception", ctx);

    ctx->initBuilder = new llvm::IRBuilder<>(initBlock);
    ctx->builder = new llvm::IRBuilder<>(codeBlock);

    EnvPtr env = new Env(entry->env);

    llvm::Function::arg_iterator ai = llFunc->arg_begin();
    for (unsigned i = 0; i < entry->fixedArgTypes.size(); ++i, ++ai) {
        llvm::Argument *llArgValue = &(*ai);
        llArgValue->setName(entry->fixedArgNames[i]->str);
        CValuePtr cvalue = new CValue(entry->fixedArgTypes[i], llArgValue);
        cvalue->forwardedRValue = entry->forwardedRValueFlags[i];
        addLocal(env, entry->fixedArgNames[i], cvalue.ptr());
    }

    if (entry->varArgName.ptr()) {
        unsigned nFixed = entry->fixedArgTypes.size();
        MultiCValuePtr varArgs = new MultiCValue();
        for (unsigned i = 0; i < entry->varArgTypes.size(); ++i, ++ai) {
            llvm::Argument *llArgValue = &(*ai);
            ostringstream sout;
            sout << "varg" << i;
            llArgValue->setName(sout.str());
            CValuePtr cvalue = new CValue(entry->varArgTypes[i], llArgValue);
            cvalue->forwardedRValue = entry->forwardedRValueFlags[i+nFixed];
            varArgs->add(cvalue);
        }
        addLocal(env, entry->varArgName, varArgs.ptr());
    }

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
        for (; i < returnSpecs.size(); ++i) {
            ReturnSpecPtr rspec = returnSpecs[i];
            if (rspec->name.ptr()) {
                hasNamedReturn = true;
                addLocal(env, rspec->name, returns[i].value.ptr());
                returns[i].value->llValue->setName(rspec->name->str);
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

    ctx->returnLists.push_back(returns);
    JumpTarget returnTarget(returnBlock, cgMarkStack(ctx));
    ctx->returnTargets.push_back(returnTarget);
    JumpTarget exceptionTarget(exceptionBlock, cgMarkStack(ctx));
    ctx->exceptionTargets.push_back(exceptionTarget);

    assert(entry->code->body.ptr());
    bool terminated = codegenStatement(entry->code->body, env, ctx);
    if (!terminated) {
        if ((returns.size() > 0) && !hasNamedReturn) {
            error(entry->code, "not all paths have a return statement");
        }
        cgDestroyStack(returnTarget.stackMarker, ctx);
        ctx->builder->CreateBr(returnBlock);
    }
    cgPopStack(returnTarget.stackMarker, ctx);

    ctx->initBuilder->CreateBr(codeBlock);

    ctx->builder->SetInsertPoint(returnBlock);
    llvm::Value *llRet = llvm::ConstantInt::get(llvmIntType(32), 0);
    ctx->builder->CreateRet(llRet);

    ctx->builder->SetInsertPoint(exceptionBlock);
    llvm::Value *llExcept = llvm::ConstantInt::get(llvmIntType(32), 1);
    ctx->builder->CreateRet(llExcept);
}



//
// codegenCWrapper
//

void codegenCWrapper(InvokeEntryPtr entry)
{
    assert(!entry->llvmCWrapper);

    string callableName = getCodeName(entry);

    vector<const llvm::Type *> llArgTypes;
    for (unsigned i = 0; i < entry->argsKey.size(); ++i)
        llArgTypes.push_back(llvmType(entry->argsKey[i]));

    TypePtr returnType;
    const llvm::Type *llReturnType = NULL;
    if (entry->returnTypes.empty()) {
        returnType = NULL;
        llReturnType = llvmVoidType();
    }
    else {
        assert(entry->returnTypes.size() == 1);
        assert(!entry->returnIsRef[0]);
        returnType = entry->returnTypes[0];
        llReturnType = llvmType(returnType);
    }

    llvm::FunctionType *llFuncType =
        llvm::FunctionType::get(llReturnType, llArgTypes, false);

    llvm::Function *llCWrapper =
        llvm::Function::Create(llFuncType,
                               llvm::Function::InternalLinkage,
                               "clay_cwrapper_" + callableName,
                               llvmModule);

    entry->llvmCWrapper = llCWrapper;

    llvm::BasicBlock *llBlock =
        llvm::BasicBlock::Create(llvm::getGlobalContext(),
                                 "body",
                                 llCWrapper);
    llvm::IRBuilder<> llBuilder(llBlock);

    vector<llvm::Value *> innerArgs;
    llvm::Function::arg_iterator ai = llCWrapper->arg_begin();
    for (unsigned i = 0; i < llArgTypes.size(); ++i, ++ai) {
        llvm::Argument *llArg = &(*ai);
        llvm::Value *llv = llBuilder.CreateAlloca(llArgTypes[i]);
        llBuilder.CreateStore(llArg, llv);
        innerArgs.push_back(llv);
    }

    if (!returnType) {
        llBuilder.CreateCall(entry->llvmFunc,
                             innerArgs.begin(), innerArgs.end());
        llBuilder.CreateRetVoid();
    }
    else {
        llvm::Value *llRetVal = llBuilder.CreateAlloca(llvmType(returnType));
        innerArgs.push_back(llRetVal);
        llBuilder.CreateCall(entry->llvmFunc,
                             innerArgs.begin(), innerArgs.end());
        llvm::Value *llRet = llBuilder.CreateLoad(llRetVal);
        llBuilder.CreateRet(llRet);
    }
}



//
// codegenCallInline
//

void codegenCallInline(InvokeEntryPtr entry,
                       MultiCValuePtr args,
                       CodegenContextPtr ctx,
                       MultiCValuePtr out)
{
    assert(entry->isInline);
    if (entry->code->isInlineLLVM())
        error(entry->code, "llvm procedures cannot be inlined");

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
        cgDestroyStack(returnTarget.stackMarker, ctx);
        ctx->builder->CreateBr(returnBlock);
    }
    cgPopStack(returnTarget.stackMarker, ctx);

    ctx->returnLists.pop_back();
    ctx->returnTargets.pop_back();

    ctx->builder->SetInsertPoint(returnBlock);
}



//
// codegenCallByName
//

void codegenCallByName(InvokeEntryPtr entry,
                       ExprListPtr args,
                       EnvPtr env,
                       CodegenContextPtr ctx,
                       MultiCValuePtr out)
{
    assert(entry->callByName);
    if (entry->varArgName.ptr())
        assert(args->size() >= entry->fixedArgNames.size());
    else
        assert(args->size() == entry->fixedArgNames.size());

    EnvPtr bodyEnv = new Env(entry->env);

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

    MultiPValuePtr mpv = safeAnalyzeCallByName(entry, args, env);
    assert(mpv->size() == out->size());

    vector<CReturn> returns;
    for (unsigned i = 0; i < mpv->size(); ++i) {
        PValuePtr pv = mpv->values[i];
        CValuePtr cv = out->values[i];
        if (pv->isTemp)
            assert(cv->type == pv->type);
        else
            assert(cv->type == pointerType(pv->type));
        returns.push_back(CReturn(!pv->isTemp, pv->type, cv));
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
        cgDestroyStack(returnTarget.stackMarker, ctx);
        ctx->builder->CreateBr(returnBlock);
    }
    cgPopStack(returnTarget.stackMarker, ctx);

    ctx->returnLists.pop_back();
    ctx->returnTargets.pop_back();

    ctx->builder->SetInsertPoint(returnBlock);
}



//
// codegenStatement
//

bool codegenStatement(StatementPtr stmt,
                      EnvPtr env,
                      CodegenContextPtr ctx)
{
    LocationContext loc(stmt->location);

    switch (stmt->stmtKind) {

    case BLOCK : {
        Block *x = (Block *)stmt.ptr();
        int blockMarker = cgMarkStack(ctx);
        codegenCollectLabels(x->statements, 0, ctx);
        bool terminated = false;
        for (unsigned i = 0; i < x->statements.size(); ++i) {
            StatementPtr y = x->statements[i];
            if (y->stmtKind == LABEL) {
                Label *z = (Label *)y.ptr();
                map<string, JumpTarget>::iterator li =
                    ctx->labels.find(z->name->str);
                assert(li != ctx->labels.end());
                JumpTarget &jt = li->second;
                if (!terminated) {
                    ctx->builder->CreateBr(jt.block);
                    ++ jt.useCount;
                }
                ctx->builder->SetInsertPoint(jt.block);
            }
            else if (terminated) {
                error(y, "unreachable code");
            }
            else if (y->stmtKind == BINDING) {
                env = codegenBinding((Binding *)y.ptr(), env, ctx);
                codegenCollectLabels(x->statements, i+1, ctx);
            }
            else {
                terminated = codegenStatement(y, env, ctx);
            }
        }
        if (!terminated)
            cgDestroyStack(blockMarker, ctx);
        cgPopStack(blockMarker, ctx);
        return terminated;
    }

    case LABEL :
        error("invalid label. labels can only appear within blocks");

    case BINDING :
        error("invalid binding. bindings can only appear within blocks");

    case ASSIGNMENT : {
        Assignment *x = (Assignment *)stmt.ptr();
        MultiPValuePtr mpvLeft = safeAnalyzeMulti(x->left, env);
        MultiPValuePtr mpvRight = safeAnalyzeMulti(x->right, env);
        if (mpvLeft->size() != mpvRight->size())
            arityMismatchError(mpvLeft->size(), mpvRight->size());
        for (unsigned i = 0; i < mpvLeft->size(); ++i) {
            if (mpvLeft->values[i]->isTemp)
                argumentError(i, "cannot assign to a temporary");
        }
        int marker = cgMarkStack(ctx);
        if (mpvLeft->size() == 1) {
            MultiCValuePtr mcvRight = codegenMultiAsRef(x->right, env, ctx);
            MultiCValuePtr mcvLeft = codegenMultiAsRef(x->left, env, ctx);
            CValuePtr cvRight = mcvRight->values[0];
            CValuePtr cvLeft = mcvLeft->values[0];
            if (mpvRight->values[0]->isTemp)
                codegenValueMoveAssign(cvLeft, cvRight, ctx);
            else
                codegenValueAssign(cvLeft, cvRight, ctx);
        }
        else {
            MultiCValuePtr mcvRight = new MultiCValue();
            for (unsigned i = 0; i < mpvRight->size(); ++i) {
                PValuePtr pv = mpvRight->values[i];
                CValuePtr cv = codegenAllocValue(pv->type, ctx);
                mcvRight->add(cv);
            }
            codegenMultiInto(x->right, env, ctx, mcvRight);
            for (unsigned i = 0; i < mcvRight->size(); ++i)
                cgPushStack(mcvRight->values[i], ctx);
            MultiCValuePtr mcvLeft = codegenMultiAsRef(x->left, env, ctx);
            assert(mcvLeft->size() == mcvRight->size());
            for (unsigned i = 0; i < mcvLeft->size(); ++i) {
                CValuePtr cvLeft = mcvLeft->values[i];
                CValuePtr cvRight = mcvRight->values[i];
                codegenValueMoveAssign(cvLeft, cvRight, ctx);
            }
        }
        cgDestroyAndPopStack(marker, ctx);
        return false;
    }

    case INIT_ASSIGNMENT : {
        InitAssignment *x = (InitAssignment *)stmt.ptr();
        MultiPValuePtr mpvLeft = safeAnalyzeMulti(x->left, env);
        MultiPValuePtr mpvRight = safeAnalyzeMulti(x->right, env);
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
        int marker = cgMarkStack(ctx);
        MultiCValuePtr mcvLeft = codegenMultiAsRef(x->left, env, ctx);
        codegenMultiInto(x->right, env, ctx, mcvLeft);
        cgDestroyAndPopStack(marker, ctx);
        return false;
    }

    case UPDATE_ASSIGNMENT : {
        UpdateAssignment *x = (UpdateAssignment *)stmt.ptr();
        PValuePtr pvLeft = safeAnalyzeOne(x->left, env);
        if (pvLeft->isTemp)
            error(x->left, "cannot assign to a temporary");
        CallPtr call = new Call(updateOperatorExpr(x->op), new ExprList());
        call->args->add(x->left);
        call->args->add(x->right);
        return codegenStatement(new ExprStatement(call.ptr()), env, ctx);
    }

    case GOTO : {
        Goto *x = (Goto *)stmt.ptr();
        map<string, JumpTarget>::iterator li =
            ctx->labels.find(x->labelName->str);
        if (li == ctx->labels.end()) {
            ostringstream sout;
            sout << "goto label not found: " << x->labelName->str;
            error(sout.str());
        }
        JumpTarget &jt = li->second;
        cgDestroyStack(jt.stackMarker, ctx);
        ctx->builder->CreateBr(jt.block);
        ++ jt.useCount;
        return true;
    }

    case RETURN : {
        Return *x = (Return *)stmt.ptr();
        MultiPValuePtr mpv = safeAnalyzeMulti(x->values, env);
        MultiCValuePtr mcv = new MultiCValue();
        const vector<CReturn> &returns = ctx->returnLists.back();
        ensureArity(mpv, returns.size());
        for (unsigned i = 0; i < mpv->size(); ++i) {
            PValuePtr pv = mpv->values[i];
            bool byRef = returnKindToByRef(x->returnKind, pv);
            const CReturn &y = returns[i];
            if (y.type != pv->type)
                argumentTypeError(i, y.type, pv->type);
            if (byRef != y.byRef)
                argumentError(i, "mismatching by-ref and by-value returns");
            if (byRef && pv->isTemp)
                argumentError(i, "cannot return a temporary by reference");
            mcv->add(y.value);
        }
        switch (x->returnKind) {
        case RETURN_VALUE :
            codegenMultiInto(x->values, env, ctx, mcv);
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
            codegenMulti(x->values, env, ctx, mcv);
            break;
        default :
            assert(false);
        }
        JumpTarget &jt = ctx->returnTargets.back();
        cgDestroyStack(jt.stackMarker, ctx);
        ctx->builder->CreateBr(jt.block);
        ++ jt.useCount;
        return true;
    }

    case IF : {
        If *x = (If *)stmt.ptr();
        int marker = cgMarkStack(ctx);
        CValuePtr cv = codegenOneAsRef(x->condition, env, ctx);
        llvm::Value *cond = codegenToBoolFlag(cv, ctx);
        cgDestroyAndPopStack(marker, ctx);

        llvm::BasicBlock *trueBlock = newBasicBlock("ifTrue", ctx);
        llvm::BasicBlock *falseBlock = newBasicBlock("ifFalse", ctx);
        llvm::BasicBlock *mergeBlock = NULL;

        ctx->builder->CreateCondBr(cond, trueBlock, falseBlock);

        bool terminated1 = false;
        bool terminated2 = false;

        ctx->builder->SetInsertPoint(trueBlock);
        terminated1 = codegenStatement(x->thenPart, env, ctx);
        if (!terminated1) {
            if (!mergeBlock)
                mergeBlock = newBasicBlock("ifMerge", ctx);
            ctx->builder->CreateBr(mergeBlock);
        }

        ctx->builder->SetInsertPoint(falseBlock);
        if (x->elsePart.ptr())
            terminated2 = codegenStatement(x->elsePart, env, ctx);
        if (!terminated2) {
            if (!mergeBlock)
                mergeBlock = newBasicBlock("ifMerge", ctx);
            ctx->builder->CreateBr(mergeBlock);
        }

        if (!terminated1 || !terminated2)
            ctx->builder->SetInsertPoint(mergeBlock);

        return terminated1 && terminated2;
    }

    case SWITCH : {
        Switch *x = (Switch *)stmt.ptr();
        if (!x->desugared)
            x->desugared = desugarSwitchStatement(x);
        llvm::BasicBlock *switchEnd = newBasicBlock("switchEnd", ctx);
        ctx->breaks.push_back(JumpTarget(switchEnd, cgMarkStack(ctx)));
        codegenStatement(x->desugared, env, ctx);
        int useCount = ctx->breaks.back().useCount;
        ctx->breaks.pop_back();
        if (useCount == 0)
            switchEnd->eraseFromParent();
        else
            ctx->builder->SetInsertPoint(switchEnd);
        return useCount == 0;
    }

    case CASE_BODY : {
        CaseBody *x = (CaseBody *)stmt.ptr();
        bool terminated = false;
        for (unsigned i = 0; i < x->statements.size(); ++i) {
            if (terminated)
                error(x->statements[i], "unreachable code");
            terminated = codegenStatement(x->statements[i], env, ctx);
        }
        if (!terminated)
            error("unterminated case block");
        return terminated;
    }

    case EXPR_STATEMENT : {
        ExprStatement *x = (ExprStatement *)stmt.ptr();
        int marker = cgMarkStack(ctx);
        codegenExprAsRef(x->expr, env, ctx);
        cgDestroyAndPopStack(marker, ctx);
        return false;
    }

    case WHILE : {
        While *x = (While *)stmt.ptr();

        llvm::BasicBlock *whileBegin = newBasicBlock("whileBegin", ctx);
        llvm::BasicBlock *whileBody = newBasicBlock("whileBody", ctx);
        llvm::BasicBlock *whileEnd = newBasicBlock("whileEnd", ctx);

        ctx->builder->CreateBr(whileBegin);
        ctx->builder->SetInsertPoint(whileBegin);

        int marker = cgMarkStack(ctx);
        CValuePtr cv = codegenOneAsRef(x->condition, env, ctx);
        llvm::Value *cond = codegenToBoolFlag(cv, ctx);
        cgDestroyAndPopStack(marker, ctx);

        ctx->builder->CreateCondBr(cond, whileBody, whileEnd);

        ctx->breaks.push_back(JumpTarget(whileEnd, cgMarkStack(ctx)));
        ctx->continues.push_back(JumpTarget(whileBegin, cgMarkStack(ctx)));
        ctx->builder->SetInsertPoint(whileBody);
        bool terminated = codegenStatement(x->body, env, ctx);
        if (!terminated)
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
                return true;
            }
        }

        return false;
    }

    case BREAK : {
        if (ctx->breaks.empty())
            error("invalid break statement");
        JumpTarget &jt = ctx->breaks.back();
        cgDestroyStack(jt.stackMarker, ctx);
        ctx->builder->CreateBr(jt.block);
        ++ jt.useCount;
        return true;
    }

    case CONTINUE : {
        if (ctx->continues.empty())
            error("invalid continue statement");
        JumpTarget &jt = ctx->continues.back();
        cgDestroyStack(jt.stackMarker, ctx);
        ctx->builder->CreateBr(jt.block);
        ++ jt.useCount;
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
        ExprPtr callable = prelude_expr_throwValue();
        ExprListPtr args = new ExprList(x->expr);
        int marker = cgMarkStack(ctx);
        codegenCallExpr(callable, args, env, ctx, new MultiCValue());
        cgDestroyAndPopStack(marker, ctx);
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
                ostringstream ostr;
                ostr << "unreachable code in iteration " << (i+1);
                error(ostr.str());
            }
            EnvPtr env2 = new Env(env);
            addLocal(env2, x->variable, mcv->values[i].ptr());
            terminated = codegenStatement(x->clonedBodies[i], env2, ctx);
        }
        return terminated;
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
                          CodegenContextPtr ctx)
{
    for (unsigned i = startIndex; i < statements.size(); ++i) {
        StatementPtr x = statements[i];
        switch (x->stmtKind) {
        case LABEL : {
            Label *y = (Label *)x.ptr();
            map<string, JumpTarget>::iterator li =
                ctx->labels.find(y->name->str);
            if (li != ctx->labels.end())
                error(x, "label redefined");
            llvm::BasicBlock *bb = newBasicBlock(y->name->str.c_str(), ctx);
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

EnvPtr codegenBinding(BindingPtr x, EnvPtr env, CodegenContextPtr ctx)
{
    LocationContext loc(x->location);

    switch (x->bindingKind) {

    case VAR : {
        MultiPValuePtr mpv = safeAnalyzeMulti(x->values, env);
        if (mpv->size() != x->names.size())
            arityError(x->names.size(), mpv->size());
        MultiCValuePtr mcv = new MultiCValue();
        for (unsigned i = 0; i < x->names.size(); ++i) {
            CValuePtr cv = codegenAllocValue(mpv->values[i]->type, ctx);
            mcv->add(cv);
        }
        int marker = cgMarkStack(ctx);
        codegenMultiInto(x->values, env, ctx, mcv);
        cgDestroyAndPopStack(marker, ctx);
        EnvPtr env2 = new Env(env);
        for (unsigned i = 0; i < x->names.size(); ++i) {
            CValuePtr cv = mcv->values[i];
            cgPushStack(cv, ctx);
            addLocal(env2, x->names[i], cv.ptr());

            ostringstream ostr;
            ostr << x->names[i]->str << "_" << cv->type;
            cv->llValue->setName(ostr.str());
        }
        return env2;
    }

    case REF : {
        MultiPValuePtr mpv = safeAnalyzeMulti(x->values, env);
        if (mpv->size() != x->names.size())
            arityError(x->names.size(), mpv->size());
        MultiCValuePtr mcv = new MultiCValue();
        for (unsigned i = 0; i < x->names.size(); ++i) {
            PValuePtr pv = mpv->values[i];
            if (pv->isTemp) {
                CValuePtr cv = codegenAllocValue(pv->type, ctx);
                mcv->add(cv);
            }
            else {
                TypePtr ptrType = pointerType(pv->type);
                CValuePtr cvRef = codegenAllocValue(ptrType, ctx);
                mcv->add(cvRef);
            }
        }
        int marker = cgMarkStack(ctx);
        codegenMulti(x->values, env, ctx, mcv);
        cgDestroyAndPopStack(marker, ctx);
        EnvPtr env2 = new Env(env);
        for (unsigned i = 0; i < x->names.size(); ++i) {
            CValuePtr cv;
            if (mpv->values[i]->isTemp) {
                cv = mcv->values[i];
                cgPushStack(cv, ctx);
            }
            else {
                cv = derefValue(mcv->values[i], ctx);
            }
            addLocal(env2, x->names[i], cv.ptr());

            ostringstream ostr;
            ostr << x->names[i]->str << "_" << cv->type;
            cv->llValue->setName(ostr.str());
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
                                 CodegenContextPtr ctx)
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

static llvm::Value *integerValue(MultiCValuePtr args,
                                 unsigned index,
                                 IntegerTypePtr &type,
                                 CodegenContextPtr ctx)
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
    return ctx->builder->CreateLoad(cv->llValue);
}

static llvm::Value *pointerValue(MultiCValuePtr args,
                                 unsigned index,
                                 PointerTypePtr &type,
                                 CodegenContextPtr ctx)
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
    }
    return ctx->builder->CreateLoad(cv->llValue);
}

static llvm::Value *pointerLikeValue(MultiCValuePtr args,
                                     unsigned index,
                                     TypePtr &type,
                                     CodegenContextPtr ctx)
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
                              CodegenContextPtr ctx)
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



//
// codegenPrimOp
//

void codegenPrimOp(PrimOpPtr x,
                   MultiCValuePtr args,
                   CodegenContextPtr ctx,
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

    case PRIM_CallDefinedP : {
        if (args->size() < 1)
            arityError2(1, args->size());
        ObjectPtr callable = valueToStatic(args->values[0]);
        if (!callable) {
            CValuePtr cvCall = staticCValue(prelude_call(), ctx);
            MultiCValuePtr args2 = new MultiCValue(cvCall);
            args2->add(args);
            codegenPrimOp(x, args2, ctx, out);
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
        codegenStaticObject(vh.ptr(), ctx, out);
        break;
    }

    case PRIM_primitiveCopy : {
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
        llvm::Value *v2 = ctx->builder->CreateZExt(flag, llvmType(boolType));
        CValuePtr out0 = out->values[0];
        assert(out0->type == boolType);
        ctx->builder->CreateStore(v2, out0->llValue);
        break;
    }

    case PRIM_numericEqualsP : {
        ensureArity(args, 2);
        TypePtr t;
        llvm::Value *v0 = numericValue(args, 0, t, ctx);
        llvm::Value *v1 = numericValue(args, 1, t, ctx);
        llvm::Value *flag;
        switch (t->typeKind) {
        case INTEGER_TYPE :
            flag = ctx->builder->CreateICmpEQ(v0, v1);
            break;
        case FLOAT_TYPE :
            flag = ctx->builder->CreateFCmpUEQ(v0, v1);
            break;
        default :
            assert(false);
        }
        llvm::Value *result =
            ctx->builder->CreateZExt(flag, llvmType(boolType));
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0->type == boolType);
        ctx->builder->CreateStore(result, out0->llValue);
        break;
    }

    case PRIM_numericLesserP : {
        ensureArity(args, 2);
        TypePtr t;
        llvm::Value *v0 = numericValue(args, 0, t, ctx);
        llvm::Value *v1 = numericValue(args, 1, t, ctx);
        llvm::Value *flag;
        switch (t->typeKind) {
        case INTEGER_TYPE : {
            IntegerType *it = (IntegerType *)t.ptr();
            if (it->isSigned)
                flag = ctx->builder->CreateICmpSLT(v0, v1);
            else
                flag = ctx->builder->CreateICmpULT(v0, v1);
            break;
        }
        case FLOAT_TYPE :
            flag = ctx->builder->CreateFCmpULT(v0, v1);
            break;
        default :
            assert(false);
        }
        llvm::Value *result =
            ctx->builder->CreateZExt(flag, llvmType(boolType));
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0->type == boolType);
        ctx->builder->CreateStore(result, out0->llValue);
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

    case PRIM_numericDivide : {
        ensureArity(args, 2);
        TypePtr t;
        llvm::Value *v0 = numericValue(args, 0, t, ctx);
        llvm::Value *v1 = numericValue(args, 1, t, ctx);
        llvm::Value *result;
        switch (t->typeKind) {
        case INTEGER_TYPE : {
            IntegerType *it = (IntegerType *)t.ptr();
            if (it->isSigned)
                result = ctx->builder->CreateSDiv(v0, v1);
            else
                result = ctx->builder->CreateUDiv(v0, v1);
            break;
        }
        case FLOAT_TYPE :
            result = ctx->builder->CreateFDiv(v0, v1);
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

    case PRIM_Pointer :
        error("Pointer type constructor cannot be called");

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

    case PRIM_pointerEqualsP : {
        ensureArity(args, 2);
        PointerTypePtr t;
        llvm::Value *v0 = pointerValue(args, 0, t, ctx);
        llvm::Value *v1 = pointerValue(args, 1, t, ctx);
        llvm::Value *flag = ctx->builder->CreateICmpEQ(v0, v1);
        llvm::Value *result = ctx->builder->CreateZExt(flag, llvmType(boolType));
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0->type == boolType);
        ctx->builder->CreateStore(result, out0->llValue);
        break;
    }

    case PRIM_pointerLesserP : {
        ensureArity(args, 2);
        PointerTypePtr t;
        llvm::Value *v0 = pointerValue(args, 0, t, ctx);
        llvm::Value *v1 = pointerValue(args, 1, t, ctx);
        llvm::Value *flag = ctx->builder->CreateICmpULT(v0, v1);
        llvm::Value *result = ctx->builder->CreateZExt(flag, llvmType(boolType));
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0->type == boolType);
        ctx->builder->CreateStore(result, out0->llValue);
        break;
    }

    case PRIM_pointerOffset : {
        ensureArity(args, 2);
        PointerTypePtr t;
        llvm::Value *v0 = pointerValue(args, 0, t, ctx);
        IntegerTypePtr offsetT;
        llvm::Value *v1 = integerValue(args, 1, offsetT, ctx);
        vector<llvm::Value *> indices;
        indices.push_back(v1);
        llvm::Value *result = ctx->builder->CreateGEP(v0,
                                                     indices.begin(),
                                                     indices.end());
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0->type == t.ptr());
        ctx->builder->CreateStore(result, out0->llValue);
        break;
    }

    case PRIM_pointerToInt : {
        ensureArity(args, 2);
        IntegerTypePtr dest = valueToIntegerType(args, 0);
        const llvm::Type *llDest = llvmType(dest.ptr());
        PointerTypePtr pt;
        llvm::Value *v = pointerValue(args, 1, pt, ctx);
        llvm::Value *result = ctx->builder->CreatePtrToInt(v, llDest);
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0->type == dest.ptr());
        ctx->builder->CreateStore(result, out0->llValue);
        break;
    }

    case PRIM_intToPointer : {
        ensureArity(args, 2);
        TypePtr pointeeType = valueToType(args, 0);
        TypePtr dest = pointerType(pointeeType);
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

    case PRIM_CodePointer :
        error("CodePointer type constructor cannot be called");

    case PRIM_makeCodePointer : {
        if (args->size() < 1)
            arityError2(1, args->size());
        ObjectPtr callable = valueToStatic(args, 0);
        switch (callable->objKind) {
        case TYPE :
        case RECORD :
        case VARIANT :
        case PROCEDURE :
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

        InvokeEntryPtr entry =
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

    case PRIM_CCodePointerP : {
        ensureArity(args, 1);
        bool isCCodePointerType = false;
        ObjectPtr obj = valueToStatic(args->values[0]);
        if (obj.ptr() && (obj->objKind == TYPE)) {
            Type *t = (Type *)obj.ptr();
            if (t->typeKind == CCODE_POINTER_TYPE)
                isCCodePointerType = true;
        }
        ValueHolderPtr vh = boolToValueHolder(isCCodePointerType);
        codegenStaticObject(vh.ptr(), ctx, out);
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

    case PRIM_makeCCodePointer : {
        if (args->size() < 1)
            arityError2(1, args->size());
        ObjectPtr callable = valueToStatic(args, 0);
        switch (callable->objKind) {
        case TYPE :
        case RECORD :
        case VARIANT :
        case PROCEDURE :
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

        InvokeEntryPtr entry =
            safeAnalyzeCallable(callable, argsKey, argsTempness);
        if (entry->callByName)
            argumentError(0, "cannot create pointer to call-by-name code");
        assert(entry->analyzed);
        if (!entry->llvmFunc)
            codegenCodeBody(entry);
        assert(entry->llvmFunc);
        if (!entry->llvmCWrapper)
            codegenCWrapper(entry);
        assert(entry->llvmCWrapper);
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
        TypePtr ccpType = cCodePointerType(CC_DEFAULT,
                                           argsKey,
                                           false,
                                           returnType);
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0->type == ccpType);
        ctx->builder->CreateStore(entry->llvmCWrapper, out0->llValue);
        break;
    }

    case PRIM_callCCodePointer : {
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

    case PRIM_pointerCast : {
        ensureArity(args, 2);
        TypePtr dest = valueToPointerLikeType(args, 0);
        TypePtr src;
        llvm::Value *v = pointerLikeValue(args, 1, src, ctx);
        llvm::Value *result = ctx->builder->CreateBitCast(v, llvmType(dest));
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0->type == dest);
        ctx->builder->CreateStore(result, out0->llValue);
        break;
    }

    case PRIM_Array :
        error("Array type constructor cannot be called");

    case PRIM_arrayRef : {
        ensureArity(args, 2);
        ArrayTypePtr at;
        llvm::Value *av = arrayValue(args, 0, at);
        IntegerTypePtr indexType;
        llvm::Value *iv = integerValue(args, 1, indexType, ctx);
        vector<llvm::Value *> indices;
        indices.push_back(llvm::ConstantInt::get(llvmIntType(32), 0));
        indices.push_back(iv);
        llvm::Value *ptr =
            ctx->builder->CreateGEP(av, indices.begin(), indices.end());
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0->type == pointerType(at->elementType));
        ctx->builder->CreateStore(ptr, out0->llValue);
        break;
    }

    case PRIM_Vec :
        error("Vec type constructor cannot be called");

    case PRIM_Tuple :
        error("Tuple type constructor cannot be called");

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
        error("Union type constructor cannot be called");

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
                const map<string, size_t> &fieldIndexMap =
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
        error("Static type constructor cannot be called");

    case PRIM_ModuleName : {
        ensureArity(args, 1);
        ObjectPtr obj = valueToStatic(args, 0);
        ModulePtr m = staticModule(obj);
        if (!m)
            argumentError(0, "value has no associated module");
        ExprPtr z = new StringLiteral(m->moduleName);
        codegenExpr(z, new Env(), ctx, out);
        break;
    }

    case PRIM_StaticName : {
        ensureArity(args, 1);
        ObjectPtr obj = valueToStatic(args, 0);
        ostringstream sout;
        printStaticName(sout, obj);
        ExprPtr z = new StringLiteral(sout.str());
        codegenExpr(z, new Env(), ctx, out);
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
                CValuePtr outi = out->values[i];
                assert(outi->type == staticType(vhi.ptr()));
            }
        }
        else if (vh->type == cSizeTType) {
            size_t count = *((size_t *)vh->buf);
            assert(out->size() == count);
            for (size_t i = 0; i < count; ++i) {
                ValueHolderPtr vhi = sizeTToValueHolder(i);
                CValuePtr outi = out->values[i];
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

    case PRIM_EnumMemberName : {
        ensureArity(args, 2);
        EnumTypePtr et = valueToEnumType(args, 0);
        size_t i = valueToStaticSizeTOrInt(args, 1);
        EnumerationPtr e = et->enumeration;
        if (i >= e->members.size())
            argumentIndexRangeError(1, "enum member index",
                                    i, e->members.size());
        ExprPtr str = new StringLiteral(e->members[i]->name->str);
        codegenExpr(str, new Env(), ctx, out);
        break;
    }

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

    case PRIM_IdentifierP : {
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

    case PRIM_IdentifierSize : {
        ensureArity(args, 1);
        IdentifierPtr ident = valueToIdentifier(args, 0);
        ValueHolderPtr vh = sizeTToValueHolder(ident->str.size());
        codegenStaticObject(vh.ptr(), ctx, out);
        break;
    }

    case PRIM_IdentifierConcat : {
        string result;
        for (unsigned i = 0; i < args->size(); ++i) {
            IdentifierPtr ident = valueToIdentifier(args, i);
            result.append(ident->str);
        }
        codegenStaticObject(new Identifier(result), ctx, out);
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
        codegenStaticObject(new Identifier(result), ctx, out);
        break;
    }

    default :
        assert(false);
        break;

    }
}



//
// codegenTopLevelLLVM
//

string stripEnclosingBraces(const string &s) {
    string::const_iterator i = s.begin();
    string::const_iterator j = s.end();
    assert(i != j);
    assert(*i == '{');
    assert(*(j-1) == '}');
    return string(i+1, j-1);
}

void codegenTopLevelLLVM(ModulePtr m)
{
    if (m->topLevelLLVMGenerated) return;
    m->topLevelLLVMGenerated = true;
    vector<ImportPtr>::iterator ii, iend;
    for (ii = m->imports.begin(), iend = m->imports.end(); ii != iend; ++ii)
        codegenTopLevelLLVM((*ii)->module);

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
        string errBuf;
        llvm::raw_string_ostream errOut(errBuf);
        err.Print("\n", errOut);
        std::cerr << errOut.str() << std::endl;
        error("llvm assembly parse error");
    }
}



//
// codegenSharedLib, codegenExe
//

static CodegenContextPtr makeSimpleContext(const char *name)
{
    llvm::FunctionType *llFuncType =
        llvm::FunctionType::get(llvmVoidType(),
                                vector<const llvm::Type *>(),
                                false);
    llvm::Function *initGlobals =
        llvm::Function::Create(llFuncType,
                               llvm::Function::InternalLinkage,
                               name,
                               llvmModule);

    CodegenContextPtr ctx = new CodegenContext(initGlobals);

    llvm::BasicBlock *initBlock = newBasicBlock("init", ctx);
    llvm::BasicBlock *codeBlock = newBasicBlock("code", ctx);
    llvm::BasicBlock *returnBlock = newBasicBlock("return", ctx);
    llvm::BasicBlock *exceptionBlock = newBasicBlock("exception", ctx);

    ctx->initBuilder = new llvm::IRBuilder<>(initBlock);
    ctx->builder = new llvm::IRBuilder<>(codeBlock);

    ctx->returnLists.push_back(vector<CReturn>());
    JumpTarget returnTarget(returnBlock, cgMarkStack(ctx));
    ctx->returnTargets.push_back(returnTarget);
    JumpTarget exceptionTarget(exceptionBlock, cgMarkStack(ctx));
    ctx->exceptionTargets.push_back(exceptionTarget);

    return ctx;
}

static void finalizeSimpleContext(CodegenContextPtr ctx,
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
    constructorsCtx = makeSimpleContext("clayglobals_init");
    destructorsCtx = makeSimpleContext("clayglobals_destroy");

    if (exceptionsEnabled()) {
        codegenCallable(prelude_exceptionInInitializer(),
                        vector<TypePtr>(),
                        vector<ValueTempness>());
        codegenCallable(prelude_exceptionInFinalizer(),
                        vector<TypePtr>(),
                        vector<ValueTempness>());
    }
}

static void finalizeCtorsDtors()
{
    finalizeSimpleContext(constructorsCtx, prelude_exceptionInInitializer());

    for (unsigned i = initializedGlobals.size(); i > 0; --i) {
        CValuePtr cv = initializedGlobals[i-1];
        codegenValueDestroy(cv, destructorsCtx);
    }
    finalizeSimpleContext(destructorsCtx, prelude_exceptionInFinalizer());
}

static void generateLLVMCtorsAndDtors() {

    // make types for llvm.global_ctors, llvm.global_dtors
    vector<const llvm::Type *> fieldTypes;
    fieldTypes.push_back(llvmIntType(32));
    const llvm::Type *funcType = constructorsCtx->llvmFunc->getFunctionType();
    const llvm::Type *funcPtrType = llvm::PointerType::getUnqual(funcType);
    fieldTypes.push_back(funcPtrType);
    const llvm::StructType *structType =
        llvm::StructType::get(llvm::getGlobalContext(), fieldTypes);
    const llvm::ArrayType *arrayType = llvm::ArrayType::get(structType, 1);

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

    // define llvm.global_ctors
    new llvm::GlobalVariable(*llvmModule, arrayType, true,
                             llvm::GlobalVariable::AppendingLinkage,
                             arrayVal1, "llvm.global_ctors");

    // define llvm.global_dtors
    new llvm::GlobalVariable(*llvmModule, arrayType, true,
                             llvm::GlobalVariable::AppendingLinkage,
                             arrayVal2, "llvm.global_dtors");
}


void codegenSharedLib(ModulePtr module)
{
    codegenTopLevelLLVM(module);
    initializeCtorsDtors();
    generateLLVMCtorsAndDtors();

    for (unsigned i = 0; i < module->topLevelItems.size(); ++i) {
        TopLevelItemPtr x = module->topLevelItems[i];
        if (x->objKind == EXTERNAL_PROCEDURE) {
            ExternalProcedurePtr y = (ExternalProcedure *)x.ptr();
            if (y->body.ptr())
                codegenExternalProcedure(y);
        }
    }
    finalizeCtorsDtors();
}

void codegenExe(ModulePtr module)
{
    codegenTopLevelLLVM(module);
    initializeCtorsDtors();
    generateLLVMCtorsAndDtors();

    IdentifierPtr main = new Identifier("main");
    IdentifierPtr argc = new Identifier("argc");
    IdentifierPtr argv = new Identifier("argv");

    BlockPtr mainBody = new Block();

    ExprListPtr args;

    args = new ExprList();
    args->add(new NameRef(argc));
    args->add(new NameRef(argv));
    CallPtr initCmdLine = new Call(prelude_expr_setArgcArgv(), args);
    mainBody->statements.push_back(new ExprStatement(initCmdLine.ptr()));

    args = new ExprList(new NameRef(main));
    CallPtr mainCall = new Call(prelude_expr_callMain(), args);

    ReturnPtr ret = new Return(RETURN_VALUE, new ExprList(mainCall.ptr()));
    mainBody->statements.push_back(ret.ptr());

    vector<ExternalArgPtr> mainArgs;
    ExprPtr argcType = new ObjectExpr(cIntType.ptr());
    mainArgs.push_back(new ExternalArg(argc, argcType));
    TypePtr charPtrPtr = pointerType(pointerType(int8Type));
    ExprPtr argvType = new ObjectExpr(charPtrPtr.ptr());
    mainArgs.push_back(new ExternalArg(argv, argvType));

    ExternalProcedurePtr entryProc =
        new ExternalProcedure(new Identifier("main"),
                              PUBLIC,
                              mainArgs,
                              false,
                              new ObjectExpr(cIntType.ptr()),
                              mainBody.ptr(),
                              new ExprList());

    entryProc->env = module->env;

    codegenExternalProcedure(entryProc);
    finalizeCtorsDtors();
}



//
// initLLVM
//

bool initLLVM(std::string const &targetTriple)
{
    llvm::InitializeAllTargets();
    llvm::InitializeAllAsmPrinters();
    llvmModule = new llvm::Module("clay", llvm::getGlobalContext());
    llvmModule->setTargetTriple(targetTriple);
    llvm::EngineBuilder eb(llvmModule);
    llvmEngine = eb.create();
    if (llvmEngine) {
        llvmTargetData = llvmEngine->getTargetData();
        llvmModule->setDataLayout(llvmTargetData->getStringRepresentation());
        return true;
    } else {
        return false;
    }
}
