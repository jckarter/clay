#include "clay.hpp"
#include "evaluator.hpp"
#include "codegen.hpp"
#include "externals.hpp"
#include "loader.hpp"
#include "operators.hpp"
#include "lambdas.hpp"
#include "analyzer.hpp"
#include "invoketables.hpp"
#include "literals.hpp"
#include "desugar.hpp"
#include "constructors.hpp"
#include "parser.hpp"
#include "env.hpp"
#include "objects.hpp"
#include "error.hpp"
#include "codegen_op.hpp"

#include "codegen.hpp"


#pragma clang diagnostic ignored "-Wcovered-switch-default"


namespace clay {

llvm::Module *llvmModule = NULL;
llvm::DIBuilder *llvmDIBuilder = NULL;
llvm::ExecutionEngine *llvmEngine;
const llvm::DataLayout *llvmDataLayout;

static vector<CValuePtr> initializedGlobals;
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

size_t cgMarkStack(CodegenContext* ctx);
void cgDestroyStack(size_t marker, CodegenContext* ctx, bool exception);
void cgPopStack(size_t marker, CodegenContext* ctx);
void cgDestroyAndPopStack(size_t marker, CodegenContext* ctx, bool exception);
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
                      size_t count);
void codegenExprInto(ExprPtr expr,
                     EnvPtr env,
                     CodegenContext* ctx,
                     MultiCValuePtr out);

void codegenMulti(ExprListPtr exprs,
                  EnvPtr env,
                  CodegenContext* ctx,
                  MultiCValuePtr out,
                  size_t count);
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
                     llvm::ArrayRef<unsigned> dispatchIndices,
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
                         llvm::ArrayRef<llvm::Value *> args,
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

void codegenCollectLabels(llvm::ArrayRef<StatementPtr> statements,
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

CValuePtr staticCValue(ObjectPtr obj, CodegenContext* ctx)
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

void codegenValueForward(CValuePtr dest, CValuePtr src, CodegenContext* ctx)
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
    for (size_t i = ctx->discardedSlots.size(); i > 0; --i) {
        if (ctx->discardedSlots[i-1].llType == llType) {
            llv = ctx->discardedSlots[i-1].llValue;
            ctx->discardedSlots.erase(ctx->discardedSlots.begin() + long(i)-1);
            break;
        }
    }
    if (!llv)
        llv = ctx->initBuilder->CreateAlloca(llType);
    ctx->allocatedSlots.push_back(StackSlot(llType, llv));
    return llv;
}

static size_t markTemps(CodegenContext* ctx) {
    return ctx->allocatedSlots.size();
}

static void clearTemps(size_t marker, CodegenContext* ctx) {
    while (marker < ctx->allocatedSlots.size()) {
        ctx->discardedSlots.push_back(ctx->allocatedSlots.back());
        ctx->allocatedSlots.pop_back();
    }
}



//
// codegen value stack
//

size_t cgMarkStack(CodegenContext* ctx)
{
    return ctx->valueStack.size();
}

void cgDestroyStack(size_t marker, CodegenContext* ctx, bool exception)
{
    size_t i = ctx->valueStack.size();
    assert(marker <= i);
    while (marker < i) {
        --i;
        codegenStackEntryDestroy(ctx->valueStack[i], ctx, exception);
    }
}

void cgPopStack(size_t marker, CodegenContext* ctx)
{
    assert(marker <= ctx->valueStack.size());
    while (marker < ctx->valueStack.size())
        ctx->valueStack.pop_back();
}

void cgDestroyAndPopStack(size_t marker, CodegenContext* ctx, bool exception)
{
    assert(marker <= ctx->valueStack.size());
    while (marker < ctx->valueStack.size()) {
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
    for (size_t i = 0; i < exprs->size(); ++i) {
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
    for (size_t i = 0; i < exprs->size(); ++i) {
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
    for (size_t i = 0; i < exprs->size(); ++i) {
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
        for (size_t i = 0; i < y->size(); ++i) {
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
    size_t marker = cgMarkStack(ctx);
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
                      size_t wantCount)
{
    size_t marker = cgMarkStack(ctx);
    size_t marker2 = marker;
    size_t j = 0;
    ExprPtr unpackExpr = implicitUnpackExpr(wantCount, exprs);
    if (unpackExpr != NULL) {
        MultiPValuePtr mpv = safeAnalyzeExpr(unpackExpr, env);
        assert(j + mpv->size() <= out->size());
        MultiCValuePtr out2 = new MultiCValue();
        for (size_t k = 0; k < mpv->size(); ++k)
            out2->add(out->values[j + k]);
        codegenExprInto(unpackExpr, env, ctx, out2);
        j += mpv->size();
    } else for (size_t i = 0; i < exprs->size(); ++i) {
        size_t prevJ = j;
        ExprPtr x = exprs->exprs[i];
        if (x->exprKind == UNPACK) {
            Unpack *y = (Unpack *)x.ptr();
            MultiPValuePtr mpv = safeAnalyzeExpr(y->expr, env);
            assert(j + mpv->size() <= out->size());
            MultiCValuePtr out2 = new MultiCValue();
            for (size_t k = 0; k < mpv->size(); ++k)
                out2->add(out->values[j + k]);
            codegenExprInto(y->expr, env, ctx, out2);
            j += mpv->size();
        }
        else if (x->exprKind == PAREN) {
            MultiPValuePtr mpv = safeAnalyzeExpr(x, env);
            assert(j + mpv->size() <= out->size());
            MultiCValuePtr out2 = new MultiCValue();
            for (size_t k = 0; k < mpv->size(); ++k)
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
        for (size_t k = prevJ; k < j; ++k)
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
    size_t marker = cgMarkStack(ctx);
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
    size_t marker2 = cgMarkStack(ctx);
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
                  size_t wantCount)
{
    size_t marker = cgMarkStack(ctx);
    size_t marker2 = marker;
    unsigned j = 0;
    ExprPtr unpackExpr = implicitUnpackExpr(wantCount, exprs);
    if (unpackExpr != NULL) {
        MultiPValuePtr mpv = safeAnalyzeExpr(unpackExpr, env);
        assert(j + mpv->size() <= out->size());
        MultiCValuePtr out2 = new MultiCValue();
        for (unsigned k = 0; k < mpv->size(); ++k)
            out2->add(out->values[j + k]);
        codegenExpr(unpackExpr, env, ctx, out2);
        j += unsigned(mpv->size());
    } else for (size_t i = 0; i < exprs->size(); ++i) {
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
            j += unsigned(mpv->size());
        }
        else if (x->exprKind == PAREN) {
            MultiPValuePtr mpv = safeAnalyzeExpr(x, env);
            assert(j + mpv->size() <= out->size());
            MultiCValuePtr out2 = new MultiCValue();
            for (unsigned k = 0; k < mpv->size(); ++k)
                out2->add(out->values[j + k]);
            codegenExpr(x, env, ctx, out2);
            j += unsigned(mpv->size());
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
        unsigned line, column, tabColumn;
        getLineCol(location, line, column, tabColumn);

        ValueHolderPtr vh = sizeTToValueHolder(line+1);
        codegenStaticObject(vh.ptr(), ctx, out);
        break;
    }
    case COLUMN_EXPR : {
        Location location = safeLookupCallByNameLocation(env);
        unsigned line, column, tabColumn;
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
        if (!x->desugared)
            desugarFieldRef(x, safeLookupModule(env));
        if (x->isDottedModuleName) {
            codegenExpr(x->desugared, env, ctx, out);
            break;
        }
        
        PVData pv = safeAnalyzeOne(x->expr, env);
        if (pv.type->typeKind == STATIC_TYPE) {
            StaticType *st = (StaticType *)pv.type.ptr();
            if (st->obj->objKind == MODULE) {
                Module *m = (Module *)st->obj.ptr();
                ObjectPtr obj = safeLookupPublic(m, x->name);
                codegenStaticObject(obj, ctx, out);
                break;
            }
        }
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
        size_t marker = cgMarkStack(ctx);
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
        size_t marker = cgMarkStack(ctx);
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
        Unpack *unpack = (Unpack *)expr.ptr();
        if (unpack->expr->exprKind != FOREIGN_EXPR)
            error("incorrect usage of unpack operator");
        codegenExpr(unpack->expr, env, ctx, out);
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
        unsigned line, column;
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
        for (size_t i = 0; i < x->params.size(); ++i)
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
        unsigned line, column;
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

    vector<TypePtr> argTypes;
    for (size_t i = 0; i < x->args.size(); ++i)
        argTypes.push_back(x->args[i]->type2);

    ExternalFunction *extFunc = target->getExternalFunction(
        x->callingConv, x->returnType2, argTypes,
        argTypes.size(), x->hasVarArgs);

    llvm::FunctionType *llFuncType = extFunc->getLLVMFunctionType();

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
    unsigned line = 0, column = 0;
    if (x->llvmFunc == NULL) {
        llvm::Function *func = llvmModule->getFunction(llvmFuncName);
        if (!func) {
            func = llvm::Function::Create(llFuncType,
                                          linkage,
                                          llvmFuncName,
                                          llvmModule);
        }
        x->llvmFunc = func;
        llvm::CallingConv::ID callingConv = extFunc->llConv;
        x->llvmFunc->setCallingConv(callingConv);
        for (vector< pair<unsigned, llvm::Attributes> >::iterator
             attr = extFunc->attrs.begin();
             attr != extFunc->attrs.end();
             ++attr)
            func->addAttribute(attr->first, attr->second);

        if (llvmDIBuilder != NULL) {
            file = getDebugLineCol(x->location, line, column);

            vector<llvm::Value*> debugParamTypes;
            if (x->returnType2 == NULL)
                debugParamTypes.push_back(llvmVoidTypeDebugInfo());
            else
                debugParamTypes.push_back(llvmTypeDebugInfo(x->returnType2));
            for (size_t i = 0; i < x->args.size(); ++i)
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

    ctx.initBuilder.reset(new llvm::IRBuilder<>(initBlock));
    ctx.builder.reset(new llvm::IRBuilder<>(codeBlock));

    if (llvmDIBuilder != NULL) {
        llvm::DebugLoc debugLoc = llvm::DebugLoc::get(line, column, x->getDebugInfo());
        ctx.initBuilder->SetCurrentDebugLocation(debugLoc);
        ctx.builder->SetCurrentDebugLocation(debugLoc);
    }

    ctx.exceptionValue = ctx.initBuilder->CreateAlloca(exceptionReturnType(), NULL, "exception");

    EnvPtr env = new Env(x->env);
    vector<CReturn> returns;

    llvm::Function::arg_iterator ai = x->llvmFunc->arg_begin();

    extFunc->allocReturnValue(extFunc->retInfo, ai, returns, &ctx);

    for (size_t i = 0; i < x->args.size(); ++i) {
        ExternalArgPtr arg = x->args[i];
        CValuePtr cvalue = extFunc->allocArgumentValue(
            extFunc->argInfos[i], arg->name->str, ai, &ctx);
        addLocal(env, arg->name, cvalue.ptr());
        if (llvmDIBuilder != NULL) {
            unsigned line, column;
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
                unsigned(i)+1 // argNo
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
    extFunc->returnStatement(extFunc->retInfo, returns, &ctx);

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
        unsigned bv = (*(bool *)ev->addr) ? 1 : 0;
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
    for (size_t i = 0; i < x->params.size(); ++i) {
        addLocal(bodyEnv, x->params[i], params->values[i]);
    }
    if (x->varParam.ptr()) {
        MultiStaticPtr varParams = new MultiStatic();
        for (size_t i = x->params.size(); i < params->size(); ++i)
            varParams->add(params->values[i]);
        addLocal(bodyEnv, x->varParam, varParams.ptr());
    }
    AnalysisCachingDisabler disabler;
    codegenExpr(x->expr, bodyEnv, ctx, out);
}


//
// codegenIntrinsic
//
    
llvm::Constant *valueHolderToLLVMConstant(ValueHolder *vh, CodegenContext *ctx)
{
    switch (vh->type->typeKind) {
        case INTEGER_TYPE: {
            IntegerType *intType = (IntegerType*)vh->type.ptr();
            switch (intType->bits) {
                case 8:
                    return ctx->builder->getInt8(vh->as<uint8_t>());
                case 16:
                    return ctx->builder->getInt16(vh->as<uint16_t>());
                case 32:
                    return ctx->builder->getInt32(vh->as<uint32_t>());
                case 64:
                    return ctx->builder->getInt64(vh->as<uint64_t>());
                case 128:
                    error("static Int128 arguments to intrinsics not yet supported");
                    return NULL;
                default:
                    assert(false && "unexpected int type used as static intrinsic argument!");
                    return NULL;
            }
        }
        case BOOL_TYPE: {
            return ctx->builder->getInt1(vh->as<bool>());
        }
        default:
            // XXX should catch this at analysis time
            error("static argument to intrinsic must be a boolean or integer static");
            return NULL;
    }
}

static void codegenIntrinsic(IntrinsicSymbol *intrin, MultiCValue *args,
                             CodegenContext *ctx, MultiCValuePtr out)
{
    vector<TypePtr> argsKey;
    args->toArgsKey(&argsKey);
    
    map<vector<TypePtr>, IntrinsicInstance>::const_iterator instanceI
        = intrin->instances.find(argsKey);
    assert(instanceI != intrin->instances.end());
    
    IntrinsicInstance const &instance = instanceI->second;
    if (instance.function == NULL) {
        assert(!instance.error.empty());
        error(instance.error);
    }
    
    llvm::FunctionType *funTy = instance.function->getFunctionType();
    llvm::SmallVector<llvm::Value*, 8> funArgs;
    
    assert(args->size() == funTy->getNumParams());
    
    for (vector<CValuePtr>::const_iterator i = args->values.begin(), end = args->values.end();
         i != end;
         ++i)
    {
        Type *type = (*i)->type.ptr();
        llvm::Value *value;
        if (type->typeKind == STATIC_TYPE) {
            StaticType *staticType = (StaticType *)type;
            assert(staticType->obj->objKind == VALUE_HOLDER);
            ValueHolder *vh = (ValueHolder*)staticType->obj.ptr();
            value = valueHolderToLLVMConstant(vh, ctx);
        } else {
            value = ctx->builder->CreateLoad((*i)->llValue);
        }
        assert(funTy->getParamType(unsigned(funArgs.size())) == value->getType());
        funArgs.push_back(value);
    }
    
    llvm::Value *retValue = ctx->builder->CreateCall(instance.function, funArgs);
    if (retValue->getType()->isVoidTy()) {
        assert(out->size() == 0);
    } else {
        assert(out->size() == 1);
        ctx->builder->CreateStore(retValue, out->values[0]->llValue);
    }
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
    case CODE_POINTER_TYPE : {
        CValuePtr cv = codegenOneAsRef(callable, env, ctx);
        MultiCValuePtr mcv = codegenMultiAsRef(args, env, ctx);
        codegenCallPointer(cv, mcv, ctx, out);
        return;
    }
    default:
        break;
    }

    if ((pv.type->typeKind == CCODE_POINTER_TYPE)
        && (callable->exprKind == NAME_REF))
    {
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
    case RECORD_DECL :
    case VARIANT_DECL :
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
            
    case INTRINSIC : {
        IntrinsicSymbol *intrin = (IntrinsicSymbol *)obj.ptr();
        MultiCValuePtr mcv = codegenMultiAsRef(args, env, ctx);
        codegenIntrinsic(intrin, mcv.ptr(), ctx, out);
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

CValuePtr codegenDispatchIndex(CValuePtr cv, PVData const &pvOut, unsigned tag, CodegenContext* ctx)
{
    MultiCValuePtr args = new MultiCValue();
    args->add(cv);
    ValueHolderPtr vh = intToValueHolder((int)tag);
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
                     llvm::ArrayRef<unsigned> dispatchIndices,
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

    unsigned memberCount = dispatchTagCount(pvDispatch.type);
    llvm::Value *llTag = codegenDispatchTag(cvDispatch, ctx);

    vector<llvm::BasicBlock *> callBlocks;
    vector<llvm::BasicBlock *> elseBlocks;

    for (unsigned i = 0; i < memberCount; ++i) {
        callBlocks.push_back(newBasicBlock("dispatchCase", ctx));
        elseBlocks.push_back(newBasicBlock("dispatchNext", ctx));
    }
    llvm::BasicBlock *finalBlock = newBasicBlock("finalBlock", ctx);

    for (unsigned i = 0; i < memberCount; ++i) {
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
    default:
        break;
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
    case RECORD_DECL :
    case VARIANT_DECL :
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
            for (vector<CValuePtr>::const_iterator i = args->values.begin(), end = args->values.end();
                 i != end;
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
            
    case INTRINSIC : {
        IntrinsicSymbol *intrin = (IntrinsicSymbol *)obj.ptr();
        codegenIntrinsic(intrin, args.ptr(), ctx, out);
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
    codegenLowlevelCall(llCallable, llArgs, ctx);
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
    if (!entry->runtimeNop)
        codegenLowlevelCall(entry->llvmFunc, llArgs, ctx);
}



//
// codegenLowlevelCall - generate exception checked call
//

void codegenLowlevelCall(llvm::Value *llCallable,
                         llvm::ArrayRef<llvm::Value *> args,
                         CodegenContext* ctx)
{
    llvm::Value *result = ctx->builder->CreateCall(llCallable, args);
    if (!exceptionsEnabled())
        return;
    if (!ctx->checkExceptions)
        return;
    llvm::Value *noException = noExceptionReturnValue();
    llvm::Value *intNoException = llvm::ConstantInt::get(llvmType(cSizeTType), 0);
    llvm::Value *intResult = ctx->builder->CreatePtrToInt(
        result, llvmType(cSizeTType));
    llvm::Function *expect = llvm::Intrinsic::getDeclaration(
        llvmModule,
        llvm::Intrinsic::expect,
        llvmType(cSizeTType));
    llvm::Value *expectArgs[2] = {intResult, intNoException};
    llvm::Value *expectResult = ctx->builder->CreateCall(expect, expectArgs);
    llvm::Value *ptrResult = ctx->builder->CreateIntToPtr(expectResult, exceptionReturnType());

    llvm::Value *cond = ctx->builder->CreateICmpEQ(ptrResult, noException);
    llvm::BasicBlock *landing = newBasicBlock("landing", ctx);
    llvm::BasicBlock *normal = newBasicBlock("normal", ctx);
    ctx->builder->CreateCondBr(cond, normal, landing);

    ctx->builder->SetInsertPoint(landing);
    assert(ctx->exceptionValue != NULL);
    ctx->builder->CreateStore(ptrResult, ctx->exceptionValue);
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
                               llvm::ArrayRef<TypePtr> argsKey,
                               llvm::ArrayRef<ValueTempness> argsTempness)
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
    unsigned startingOffset = llvmBody->location.offset;

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
            unsigned offset = unsigned(first - body.begin()) + startingOffset;
            unsigned length = unsigned(last - first);
            interpolateExpr(source, offset, length, env, outstream);
            i = last;
            continue;
        }

        last = parseBracketedExpr(first, body.end());
        if (last != first) {
            first += 1;
            last -= 1;
            unsigned offset = unsigned(first - body.begin()) + startingOffset;
            unsigned length = unsigned(last - first);
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
    for (size_t i = 0; i < entry->argsKey.size(); ++i) {
        llvm::Type *argType = llvmPointerType(entry->argsKey[i]);
        if (argCount > 0) out << string(", ");
        argType->print(out);
        out << string(" %\"") << entry->fixedArgNames[i]->str << string("\"");
        argCount ++;
    }
    for (size_t i = 0; i < entry->returnTypes.size(); ++i) {
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

string getCodeName(InvokeEntry* entry)
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
    case RECORD_DECL : {
        RecordDecl *y = (RecordDecl *)x.ptr();
        sout << y->name->str;
        break;
    }
    case VARIANT_DECL : {
        VariantDecl *y = (VariantDecl *)x.ptr();
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
    for (size_t i = 0; i < entry->argsKey.size(); ++i) {
        if (i != 0)
            sout << ", ";
        sout << entry->argsKey[i];
    }
    sout << ')';
    if (!entry->returnTypes.empty()) {
        sout << ' ';
        for (size_t i = 0; i < entry->returnTypes.size(); ++i) {
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

static bool blockIsNop(llvm::BasicBlock *codeBlock, llvm::BasicBlock *returnBlock)
{
    if (codeBlock->size() == 1) {
        if (llvm::BranchInst *branch = llvm::dyn_cast<llvm::BranchInst>(&codeBlock->front())) {
            return branch->getNumSuccessors() == 1 && branch->getSuccessor(0) == returnBlock;
        }
    }
    return false;
}

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
    for (size_t i = 0; i < entry->argsKey.size(); ++i)
        llArgTypes.push_back(llvmPointerType(entry->argsKey[i]));
    for (size_t i = 0; i < entry->returnTypes.size(); ++i) {
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
        llFunc->addFnAttr(llvm::Attributes::InlineHint);
        break;
    case NEVER_INLINE :
        llFunc->addFnAttr(llvm::Attributes::NoInline);
        break;
    default:
        break;
    }

    for (unsigned i = 1; i <= llArgTypes.size(); ++i) {
        llvm::Attributes attrs = llvm::Attributes::get(
            llFunc->getContext(),
            llvm::Attributes::NoAlias);
        llFunc->addAttribute(i, attrs);
    }

    entry->llvmFunc = llFunc;

    CodegenContext ctx(entry->llvmFunc);

    unsigned line, column;
    llvm::DIFile file;
    if (llvmDIBuilder != NULL) {
        file = getDebugLineCol(entry->origCode->location, line, column);

        vector<llvm::Value*> debugParamTypes;
        debugParamTypes.push_back(llvmVoidTypeDebugInfo());
        for (size_t i = 0; i < entry->argsKey.size(); ++i) {
            llvm::DIType argType = llvmTypeDebugInfo(entry->argsKey[i]);
            llvm::DIType argRefType
                = llvmDIBuilder->createReferenceType(
                    llvm::dwarf::DW_TAG_reference_type,
                    argType);
            debugParamTypes.push_back(argRefType);
        }
        for (size_t i = 0; i < entry->returnTypes.size(); ++i) {
            llvm::DIType returnType = llvmTypeDebugInfo(entry->returnTypes[i]);
            llvm::DIType returnRefType = entry->returnIsRef[i]
                ? llvmDIBuilder->createReferenceType(
                    llvm::dwarf::DW_TAG_reference_type,
                    llvmDIBuilder->createReferenceType(
                        llvm::dwarf::DW_TAG_reference_type,
                        returnType))
                : llvmDIBuilder->createReferenceType(
                    llvm::dwarf::DW_TAG_reference_type,
                    returnType);

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

    ctx.initBuilder.reset(new llvm::IRBuilder<>(initBlock));
    ctx.builder.reset(new llvm::IRBuilder<>(codeBlock));

    if (llvmDIBuilder != NULL) {
        llvm::DebugLoc debugLoc = llvm::DebugLoc::get(line, column, entry->getDebugInfo());
        ctx.initBuilder->SetCurrentDebugLocation(debugLoc);
        ctx.builder->SetCurrentDebugLocation(debugLoc);
    }

    ctx.exceptionValue = ctx.initBuilder->CreateAlloca(exceptionReturnType(), NULL, "exception");

    EnvPtr env = new Env(entry->env);

    llvm::Function::arg_iterator ai = llFunc->arg_begin();
    unsigned argNo = 1;
    unsigned i = 0;
    for (; i < entry->varArgPosition; ++i, ++ai, ++argNo) {
        llvm::Argument *llArgValue = &(*ai);
        llArgValue->setName(entry->fixedArgNames[i]->str.str());
        CValuePtr cvalue = new CValue(entry->fixedArgTypes[i], llArgValue);
        cvalue->forwardedRValue = entry->forwardedRValueFlags[i];
        addLocal(env, entry->fixedArgNames[i], cvalue.ptr());
        if (llvmDIBuilder != NULL) {
            unsigned line, column;
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
                    llvm::dwarf::DW_TAG_reference_type,
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
        MultiCValuePtr varArgs = new MultiCValue();

        unsigned line, column;
        Location argLocation = entry->origCode->formalArgs[entry->varArgPosition]->location;
        llvm::DIFile file = getDebugLineCol(argLocation, line, column);
        unsigned j = 0;
        for (; j < entry->varArgTypes.size(); ++j, ++ai, ++argNo) {
            llvm::Argument *llArgValue = &(*ai);
            llvm::SmallString<128> buf;
            llvm::raw_svector_ostream sout(buf);
            sout << entry->varArgName << ".." << j;
            llArgValue->setName(sout.str());
            CValuePtr cvalue = new CValue(entry->varArgTypes[j], llArgValue);
            cvalue->forwardedRValue = entry->forwardedRValueFlags[i+j];
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
                        llvm::dwarf::DW_TAG_reference_type,
                        llvmTypeDebugInfo(entry->varArgTypes[j])), // type
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
        for (; i < entry->fixedArgNames.size(); ++i, ++ai, ++argNo) {
            llvm::Argument *llArgValue = &(*ai);
            llArgValue->setName(entry->fixedArgNames[i]->str.str());
            CValuePtr cvalue = new CValue(entry->fixedArgTypes[i], llArgValue);
            cvalue->forwardedRValue = entry->forwardedRValueFlags[i+j];
            addLocal(env, entry->fixedArgNames[i], cvalue.ptr());
            if (llvmDIBuilder != NULL) {
                unsigned line, column;
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
                        llvm::dwarf::DW_TAG_reference_type,
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
    }

    // XXX debug info for returns

    vector<CReturn> returns;
    for (size_t i = 0; i < entry->returnTypes.size(); ++i, ++ai) {
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
        llvm::ArrayRef<ReturnSpecPtr> returnSpecs = entry->code->returnSpecs;
        size_t i = 0;
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
                unsigned line, column;
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
                        llvm::dwarf::DW_TAG_reference_type,
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

    entry->runtimeNop = blockIsNop(codeBlock, returnBlock);

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
    
    unsigned i = 0, j = 0;
    for (; i < entry->varArgPosition; ++i) {
        CValuePtr cv = args->values[i];
        CValuePtr carg = new CValue(cv->type, cv->llValue);
        carg->forwardedRValue = entry->forwardedRValueFlags[i];
        assert(carg->type == entry->argsKey[i]);
        addLocal(env, entry->fixedArgNames[i], carg.ptr());
    }
    if (entry->varArgName.ptr()) {
        MultiCValuePtr varArgs = new MultiCValue();
        for (; j < entry->varArgTypes.size(); ++j) {
            CValuePtr cv = args->values[i+j];
            CValuePtr carg = new CValue(cv->type, cv->llValue);
            carg->forwardedRValue = entry->forwardedRValueFlags[i+j];
            varArgs->add(carg);
        }
        addLocal(env, entry->varArgName, varArgs.ptr());
        for (; i < entry->fixedArgNames.size(); ++i) {
            CValuePtr cv = args->values[i+j];
            CValuePtr carg = new CValue(cv->type, cv->llValue);
            carg->forwardedRValue = entry->forwardedRValueFlags[i+j];
            assert(carg->type == entry->argsKey[i+j]);
            addLocal(env, entry->fixedArgNames[i], carg.ptr());
        }
    }

    vector<CReturn> returns;
    for (size_t i = 0; i < entry->returnTypes.size(); ++i) {
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
        llvm::ArrayRef<ReturnSpecPtr> returnSpecs = entry->code->returnSpecs;
        size_t i = 0;
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

    ++ctx->callByNameDepth;
    if (ctx->callByNameDepth > 100) {
        error("alias functions stack overflow");
    }

    EnvPtr bodyEnv = new Env(entry->env);
    bodyEnv->callByNameExprHead = callable;

    unsigned k = 0, j = 0;
    for (; k < entry->varArgPosition; ++k) {
        ExprPtr expr = foreignExpr(env, args->exprs[k]);
        addLocal(bodyEnv, entry->fixedArgNames[k], expr.ptr());
    }
    if (entry->varArgName.ptr()) {
        ExprListPtr varArgs = new ExprList();
        for (; j < args->size() - entry->fixedArgNames.size(); ++j) {
            ExprPtr expr = foreignExpr(env, args->exprs[k+j]);
            varArgs->add(expr);
        }
        addLocal(bodyEnv, entry->varArgName, varArgs.ptr());
        for (; k < entry->fixedArgNames.size(); ++k) {
            ExprPtr expr = foreignExpr(env, args->exprs[k+j]);
            addLocal(bodyEnv, entry->fixedArgNames[k], expr.ptr());
        }
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
        llvm::ArrayRef<ReturnSpecPtr> returnSpecs = entry->code->returnSpecs;
        size_t i = 0;
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
    --ctx->callByNameDepth;
    assert(ctx->callByNameDepth >= 0);
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
        llvm::ArrayRef<StatementPtr> evaled = desugarEvalStatement(eval, env);
        for (unsigned i = 0; i < evaled.size(); ++i)
            codegenBlockStatement(block, i, evaled[i], env, ctx, terminated);
    } else {
        terminated = codegenStatement(stmt, env, ctx);
    }
}

static EnvPtr codegenStatementExpressionStatements(llvm::ArrayRef<StatementPtr> stmts,
    EnvPtr env,
    CodegenContext* ctx)
{
    EnvPtr env2 = env;

    for (StatementPtr const *i = stmts.begin(), *end = stmts.end();
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

static size_t codegenBeginScope(StatementPtr scopeStmt, CodegenContext* ctx)
{
    if (llvmDIBuilder != NULL) {
        llvm::DILexicalBlock outerScope = ctx->getDebugScope();
        unsigned line, column;
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

static void codegenEndScope(size_t marker, bool terminated, CodegenContext* ctx)
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
    if (llvmDIBuilder != NULL)
        DebugLocationContext loc(stmt->location, ctx);
    
    switch (stmt->stmtKind) {

    case BLOCK : {
        Block *block = (Block *)stmt.ptr();
        size_t blockMarker = codegenBeginScope(stmt, ctx);
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
            arityMismatchError(mpvLeft->size(), mpvRight->size(), /*hasVarArg=*/false);

        size_t tempMarker = markTemps(ctx);
        size_t marker = cgMarkStack(ctx);

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
                j += unsigned(mpvLeftI->size());
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
        size_t tempMarker = markTemps(ctx);
        size_t marker = cgMarkStack(ctx);
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
            call = new Call(operator_expr_prefixUpdateAssign(), x->exprs);
        else
            call = new Call(operator_expr_updateAssign(), x->exprs);        
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
        MultiPValuePtr mpv = safeAnalyzeMulti(x->values, env, 1);
        MultiCValuePtr mcv = new MultiCValue();
        llvm::ArrayRef<CReturn> returns = ctx->returnLists.back();
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
            codegenMultiInto(x->values, env, ctx, mcv, 1);
            break;
        case RETURN_REF : {
            MultiCValuePtr mcvRef = codegenMultiAsRef(x->values, env, ctx);
            assert(mcv->size() == mcvRef->size());
            for (size_t i = 0; i < mcv->size(); ++i) {
                CValuePtr cvPtr = mcv->values[i];
                CValuePtr cvRef = mcvRef->values[i];
                ctx->builder->CreateStore(cvRef->llValue, cvPtr->llValue);
            }
            break;
        }
        case RETURN_FORWARD :
            codegenMulti(x->values, env, ctx, mcv, 1);
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

        size_t scopeMarker = codegenBeginScope(stmt, ctx);
        EnvPtr env2 = codegenStatementExpressionStatements(x->conditionStatements, env, ctx);

        size_t tempMarker = markTemps(ctx);
        size_t marker = cgMarkStack(ctx);
        CValuePtr cv = codegenOneAsRef(x->condition, env2, ctx);
        BoolKind condBoolKind = typeBoolKind(cv->type);
        cgDestroyAndPopStack(marker, ctx, false);
        clearTemps(tempMarker, ctx);

        llvm::BasicBlock *trueBlock = NULL;
        llvm::BasicBlock *falseBlock = NULL;
        llvm::BasicBlock *mergeBlock = NULL;
        if (condBoolKind == BOOL_EXPR || condBoolKind == BOOL_STATIC_TRUE) {
            trueBlock = newBasicBlock("ifTrue", ctx);
        }
        if (condBoolKind == BOOL_EXPR || condBoolKind == BOOL_STATIC_FALSE) {
            falseBlock = newBasicBlock("ifFalse", ctx);
        }

        if (condBoolKind == BOOL_EXPR) {
            llvm::Value *cond = codegenToBoolFlag(cv, ctx);
            ctx->builder->CreateCondBr(cond, trueBlock, falseBlock);
        } else {
            ctx->builder->CreateBr(condBoolKind == BOOL_STATIC_TRUE ? trueBlock : falseBlock);
        }

        bool terminated1 = false;
        bool terminated2 = false;

        if (condBoolKind == BOOL_EXPR || condBoolKind == BOOL_STATIC_TRUE) {
            ctx->builder->SetInsertPoint(trueBlock);
            terminated1 = codegenStatement(x->thenPart, env2, ctx);
            if (!terminated1) {
                if (!mergeBlock)
                    mergeBlock = newBasicBlock("ifMerge", ctx);
                ctx->builder->CreateBr(mergeBlock);
            }
        } else {
            terminated1 = true;
        }

        if (condBoolKind == BOOL_EXPR || condBoolKind == BOOL_STATIC_FALSE) {
            ctx->builder->SetInsertPoint(falseBlock);
            if (x->elsePart.ptr())
                terminated2 = codegenStatement(x->elsePart, env2, ctx);
            if (!terminated2) {
                if (!mergeBlock)
                    mergeBlock = newBasicBlock("ifMerge", ctx);
                ctx->builder->CreateBr(mergeBlock);
            }
        } else {
            terminated2 = true;
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
        llvm::ArrayRef<StatementPtr> evaled = desugarEvalStatement(eval, env);
        bool terminated = false;
        for (size_t i = 0; i < evaled.size(); ++i) {
            if (terminated)
                error(evaled[i], "unreachable code");
            terminated = codegenStatement(evaled[i], env, ctx);
        }
        return terminated;
    }

    case EXPR_STATEMENT : {
        ExprStatement *x = (ExprStatement *)stmt.ptr();
        size_t tempMarker = markTemps(ctx);
        size_t marker = cgMarkStack(ctx);
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

        size_t scopeMarker = codegenBeginScope(stmt, ctx);
        EnvPtr env2 = codegenStatementExpressionStatements(x->conditionStatements, env, ctx);

        size_t tempMarker = markTemps(ctx);
        size_t marker = cgMarkStack(ctx);
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

        EnvPtr catchEnv = new Env(env, true);

        bool catchTerminated =
            codegenStatement(x->desugaredCatchBlock, catchEnv, ctx);
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

        if (!x->expr && !lookupExceptionAvailable(env.ptr()))
            error("rethrow statement can be used only from within catch block");

        desugarThrow(x);
        ExprPtr callable = operator_expr_throwValue();
        callable->location = stmt->location;
        ExprListPtr args = new ExprList();
        args->exprs.push_back(x->desugaredExpr);
        if (x->desugaredContext != NULL)
            args->exprs.push_back(x->desugaredContext);

        size_t tempMarker = markTemps(ctx);
        size_t marker = cgMarkStack(ctx);
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
        for (size_t i = 0; i < mcv->size(); ++i) {
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

    case STATIC_ASSERT_STATEMENT : {
        StaticAssertStatement *x = (StaticAssertStatement *)stmt.ptr();
        evaluateStaticAssert(x->location, x->cond, x->message, env);
        return false;
    }

    default :
        assert(false);
        return false;

    }
}



//
// codegenCollectLabels
//

void codegenCollectLabels(llvm::ArrayRef<StatementPtr> statements,
                          unsigned startIndex,
                          CodegenContext* ctx)
{
    for (size_t i = startIndex; i < statements.size(); ++i) {
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

llvm::SmallString<16> getBindingVariableName(BindingPtr x, unsigned i)
{
    if (i >= x->args.size()){
        assert(x->hasVarArg);

        llvm::SmallString<16> buf;
        llvm::raw_svector_ostream ostr(buf);
        ostr << x->args.back()->name->str << "_" << (i - x->args.size() + 1);
        return ostr.str();
    } else {
        return x->args[i]->name->str;
    }
}

EnvPtr codegenBinding(BindingPtr x, EnvPtr env, CodegenContext* ctx)
{
    unsigned line, column;
    llvm::DIFile file;
    if (llvmDIBuilder != NULL) {
        DebugLocationContext loc(x->location, ctx);
        getDebugLineCol(x->location, line, column);
    }
    switch(x->bindingKind){
    case VAR : {

        MultiPValuePtr mpv = safeAnalyzeMulti(x->values, env, 1);
        if(x->hasVarArg){
            if (mpv->values.size() < x->args.size()-1)
                arityMismatchError(x->args.size()-1, mpv->values.size(), /*hasVarArg=*/ true);
        } else
            if (mpv->values.size() != x->args.size())
                arityMismatchError(x->args.size(), mpv->values.size(), /*hasVarArg=*/ false);
        MultiCValuePtr mcv = new MultiCValue();
        for (unsigned i = 0; i < mpv->values.size(); ++i) {
            CValuePtr cv = codegenAllocNewValue(mpv->values[i].type, ctx);
            mcv->add(cv);
            if (llvmDIBuilder != NULL) {
                llvm::DILexicalBlock debugBlock = ctx->getDebugScope();
                llvm::DIVariable debugVar = llvmDIBuilder->createLocalVariable(
                    llvm::dwarf::DW_TAG_auto_variable, // tag
                    debugBlock, // scope
                    getBindingVariableName(x, i), // name
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
        size_t tempMarker = markTemps(ctx);
        size_t marker = cgMarkStack(ctx);
        codegenMultiInto(x->values, env, ctx, mcv, x->args.size());
        cgDestroyAndPopStack(marker, ctx, false);
        clearTemps(tempMarker, ctx);
        EnvPtr env2 = new Env(env);
        for (unsigned i = 0; i < x->patternVars.size(); ++i)
            addLocal(env2, x->patternVars[i].name, x->patternTypes[i]);
        size_t varArgSize = mcv->values.size()-x->args.size()+1;
        for (unsigned i = 0, j = 0; i < x->args.size(); ++i) {
            if (x->args[i]->varArg) {
                MultiCValuePtr varArgs = new MultiCValue();
                for (; j < varArgSize; ++j) {
                    CValuePtr cv = mcv->values[i+j];
                    cgPushStackValue(cv, ctx);
                    varArgs->add(cv);
                }
                --j;
                addLocal(env2, x->args[i]->name, varArgs.ptr());  
            } else {
                CValuePtr cv = mcv->values[i+j];
                cgPushStackValue(cv, ctx);
                addLocal(env2, x->args[i]->name, cv.ptr());
                llvm::SmallString<128> buf;
                llvm::raw_svector_ostream ostr(buf);
                ostr << x->args[i]->name->str << ":" << cv->type;
                cv->llValue->setName(ostr.str());
            }
        }
        return env2;
    }

    case REF : {

        MultiPValuePtr mpv = safeAnalyzeMulti(x->values, env, x->args.size());
        if(x->hasVarArg){
            if (mpv->values.size() < x->args.size())
                arityMismatchError(x->args.size(), mpv->values.size(), /*hasVarArg=*/true);
        } else
            if (mpv->values.size() != x->args.size())
                arityMismatchError(x->args.size(), mpv->values.size(), /*hasVarArg=*/false);
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
                    getBindingVariableName(x, i), // name
                    file, // file
                    line, // line
                    llvmDIBuilder->createReferenceType(
                        llvm::dwarf::DW_TAG_reference_type,
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
        size_t tempMarker = markTemps(ctx);
        size_t marker = cgMarkStack(ctx);
        codegenMulti(x->values, env, ctx, mcv, x->args.size());
        cgDestroyAndPopStack(marker, ctx, false);
        clearTemps(tempMarker, ctx);
        EnvPtr env2 = new Env(env);
        for (unsigned i = 0; i < x->patternVars.size(); ++i)
            addLocal(env2, x->patternVars[i].name, x->patternTypes[i]);
        size_t varArgSize = mcv->values.size()-x->args.size()+1;
        for (unsigned i = 0, j = 0; i < x->args.size(); ++i) {
            if (x->args[i]->varArg) {
                MultiCValuePtr varArgs = new MultiCValue();
                for (; j < varArgSize; ++j) {
                    CValuePtr cv = derefValue(mcv->values[i+j], ctx);
                    varArgs->add(cv);
                }
                --j;
                addLocal(env2, x->args[i]->name, varArgs.ptr());  
            }else{
                CValuePtr cv = derefValue(mcv->values[i+j], ctx);
                addLocal(env2, x->args[i]->name, cv.ptr());
                llvm::SmallString<128> buf;
                llvm::raw_svector_ostream ostr(buf);
                ostr << x->args[i]->name->str << ":" << cv->type;
                cv->llValue->setName(ostr.str());
            }    
        }
        return env2;
    }

    case FORWARD : {
        
        MultiPValuePtr mpv = safeAnalyzeMulti(x->values, env, x->args.size());
        if(x->hasVarArg){
            if (mpv->values.size() < x->args.size())
                arityMismatchError(x->args.size(), mpv->values.size(), /*hasVarArg=*/true);
        } else
            if (mpv->values.size() != x->args.size())
                arityMismatchError(x->args.size(), mpv->values.size(), /*hasVarArg=*/false);
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
                llvm::DIType debugType = llvmTypeDebugInfo(pv.type);
                llvm::DIVariable debugVar = llvmDIBuilder->createLocalVariable(
                    llvm::dwarf::DW_TAG_auto_variable, // tag
                    debugBlock, // scope
                    getBindingVariableName(x, i), // name
                    file, // file
                    line, // line
                    pv.isTemp
                        ? debugType
                        : llvmDIBuilder->createReferenceType(
                            llvm::dwarf::DW_TAG_reference_type,
                            debugType), // type
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
        size_t tempMarker = markTemps(ctx);
        size_t marker = cgMarkStack(ctx);
        codegenMulti(x->values, env, ctx, mcv, x->args.size());
        cgDestroyAndPopStack(marker, ctx, false);
        clearTemps(tempMarker, ctx);
        EnvPtr env2 = new Env(env);
        for (unsigned i = 0; i < x->patternVars.size(); ++i)
            addLocal(env2, x->patternVars[i].name, x->patternTypes[i]);
        size_t varArgSize = mcv->values.size()-x->args.size()+1;
        for (unsigned i = 0, j = 0; i < x->args.size(); ++i) {
            if (x->args[i]->varArg) {
                MultiCValuePtr varArgs = new MultiCValue();
                for (; j < varArgSize; ++j) {
                    CValuePtr rcv, cv;
                    rcv = mcv->values[i+j];
                    if (mpv->values[i+j].isTemp) {
                        cv = rcv;
                        cgPushStackValue(cv, ctx);
                    }
                    else {
                        cv = derefValue(rcv, ctx);
                    }
                    varArgs->add(cv);
                }
                --j;
                addLocal(env2, x->args[i]->name, varArgs.ptr());  
            }else{

                CValuePtr rcv, cv;
                rcv = mcv->values[i+j];
                if (mpv->values[i+j].isTemp) {
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

    ctx->initBuilder.reset(new llvm::IRBuilder<>(initBlock));
    ctx->builder.reset(new llvm::IRBuilder<>(codeBlock));

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

    for (size_t i = initializedGlobals.size(); i > 0; --i) {
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
        new ExternalProcedure(module.ptr(),
                              main,
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
    for (size_t i = 0; i < module->topLevelItems.size(); ++i) {
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

    ObjectPtr mainProc = lookupPrivate(module, Identifier::get("main"));
    
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
    llvm::StringRef targetCPU,
    llvm::StringRef targetFeatures,
    bool softFloat,
    llvm::StringRef name,
    llvm::StringRef flags,
    bool relocPic,
    bool debug,
    unsigned optLevel)
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
            optLevel > 0,
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

    llvm::CodeGenOpt::Level level;
    switch (optLevel) {
    case 0 : level = llvm::CodeGenOpt::None; break;
    case 1 : level = llvm::CodeGenOpt::Less; break;
    case 2 : level = llvm::CodeGenOpt::Default; break;
    default : level = llvm::CodeGenOpt::Aggressive; break;
    }

    llvm::TargetOptions opts;
    if (optLevel < 2 || debug)
        opts.NoFramePointerElim = 1;
    if (softFloat) {
        opts.UseSoftFloat = 1;
        opts.FloatABIType = llvm::FloatABI::Soft;
    }

    llvm::TargetMachine *targetMachine = target->createTargetMachine(
        targetTriple, targetCPU, targetFeatures,
        opts, reloc, codeModel, level);

    if (targetMachine != NULL) {
        llvmDataLayout = targetMachine->getDataLayout();
        if (llvmDataLayout == NULL) {
            return NULL;
        }
        llvmModule->setDataLayout(llvmDataLayout->getStringRepresentation());
    }

    return targetMachine;
}

void codegenBeforeRepl(ModulePtr module) {
    CodegenContext* theConstructorCtx = new CodegenContext();
    CodegenContext* theDestructorCtx = new CodegenContext();
    delete constructorsCtx;
    delete destructorsCtx;
    constructorsCtx = theConstructorCtx;
    destructorsCtx = theDestructorCtx;
    //codegenTopLevelLLVM(module);
    codegenTopLevelLLVMRecursive(module);
    initializeCtorsDtors();
    generateLLVMCtorsAndDtors();
    codegenModuleEntryPoints(module, false);
}

void codegenAfterRepl(llvm::Function*& ctor, llvm::Function*& dtor) {
    finalizeCtorsDtors();
    ctor = constructorsCtx->llvmFunc;
    dtor = destructorsCtx->llvmFunc;
}

}
