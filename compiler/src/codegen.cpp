#include "clay.hpp"
#include <llvm/Support/SourceMgr.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Assembly/Writer.h>
#include <llvm/Assembly/Parser.h>

void codegenValueInit(CValuePtr dest, CodegenContextPtr ctx);
void codegenValueDestroy(CValuePtr dest, CodegenContextPtr ctx);
void codegenValueCopy(CValuePtr dest, CValuePtr src, CodegenContextPtr ctx);
void codegenValueMove(CValuePtr dest, CValuePtr src, CodegenContextPtr ctx);
void codegenValueAssign(CValuePtr dest, CValuePtr src, CodegenContextPtr ctx);
llvm::Value *codegenToBoolFlag(CValuePtr a, CodegenContextPtr ctx);

int cgMarkStack();
void cgDestroyStack(int marker, CodegenContextPtr ctx);
void cgPopStack(int marker);
void cgDestroyAndPopStack(int marker, CodegenContextPtr ctx);
CValuePtr codegenAllocValue(TypePtr t);

CValuePtr codegenOneAsRef(ExprPtr expr,
                          EnvPtr env,
                          CodegenContextPtr ctx);
MultiCValuePtr codegenMultiAsRef(const vector<ExprPtr> &exprs,
                                 EnvPtr env,
                                 CodegenContextPtr ctx);
MultiCValuePtr codegenExprAsRef(ExprPtr expr,
                                EnvPtr env,
                                CodegenContextPtr ctx);

void codegenOneInto(ExprPtr expr,
                    EnvPtr env,
                    CodegenContextPtr ctx,
                    CValuePtr out);
void codegenMultiInto(const vector<ExprPtr> &exprs,
                      EnvPtr env,
                      CodegenContextPtr ctx,
                      MultiCValuePtr out);
void codegenExprInto(ExprPtr expr,
                     EnvPtr env,
                     CodegenContextPtr ctx,
                     MultiCValuePtr out);

void codegenMulti(const vector<ExprPtr> &exprs,
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

void codegenGlobalVariable(GlobalVariablePtr x);
void codegenExternalVariable(ExternalVariablePtr x);
void codegenExternalProcedure(ExternalProcedurePtr x);

void codegenValueHolder(ValueHolderPtr x,
                        CodegenContextPtr ctx,
                        MultiCValuePtr out);
llvm::Value *codegenSimpleConstant(ValueHolderPtr v);

void codegenIndexingExpr(ExprPtr indexable,
                         const vector<ExprPtr> &args,
                         EnvPtr env,
                         CodegenContextPtr ctx,
                         MultiCValuePtr out);
void codegenAliasIndexing(GlobalAliasPtr x,
                          const vector<ExprPtr> &args,
                          EnvPtr env,
                          CodegenContextPtr ctx,
                          MultiCValuePtr out);
void codegenFieldRefExpr(ExprPtr base,
                         IdentifierPtr name,
                         EnvPtr env,
                         CodegenContextPtr ctx,
                         MultiCValuePtr out);
void codegenCallExpr(ExprPtr callable,
                     const vector<ExprPtr> &args,
                     EnvPtr env,
                     CodegenContextPtr ctx,
                     MultiCValuePtr out);
void codegenCallValue(CValuePtr callable,
                      MultiCValuePtr args,
                      CodegenContextPtr ctx,
                      MultiCValuePtr out);
void codegenCallPointer(CValuePtr x,
                        MultiCValuePtr args,
                        CodegenContextPtr ctx,
                        MultiCValuePtr out);
void codegenCallCode(InvokeEntryPtr entry,
                     MultiCValuePtr args,
                     CodegenContextPtr ctx,
                     MultiCValuePtr out);
void codegenCallInlined(InvokeEntryPtr entry,
                        const vector<ExprPtr> &args,
                        EnvPtr env,
                        CodegenContextPtr ctx,
                        MultiCValuePtr out);

void codegenCodeBody(InvokeEntryPtr entry, const string &callableName);

void codegenCWrapper(InvokeEntryPtr entry, const string &callableName);

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
// utility procs
//

static CValuePtr staticCValue(ObjectPtr obj)
{
    TypePtr t = staticType(obj);
    return new CValue(t, NULL);
}

static CValuePtr kernelCValue(const string &name)
{
    return staticCValue(kernelName(name));
}

static CValuePtr derefValue(CValuePtr cvPtr)
{
    assert(cvPtr->type->typeKind == POINTER_TYPE);
    PointerType *pt = (PointerType *)cvPtr->type.ptr();
    llvm::Value *ptrValue = llvmBuilder->CreateLoad(cvPtr->llValue);
    return new CValue(pt->pointeeType, ptrValue);
}



//
// codegen value ops
//

void codegenValueInit(CValuePtr dest, CodegenContextPtr ctx)
{
    if (isPrimitiveType(dest->type))
        return;
    codegenCallValue(staticCValue(dest->type.ptr()),
                     new MultiCValue(),
                     ctx,
                     new MultiCValue(dest));
}

void codegenValueDestroy(CValuePtr dest, CodegenContextPtr ctx)
{
    if (isPrimitiveType(dest->type))
        return;
    codegenCallValue(kernelCValue("destroy"),
                     new MultiCValue(dest),
                     ctx,
                     new MultiCValue());
}

void codegenValueCopy(CValuePtr dest, CValuePtr src, CodegenContextPtr ctx)
{
    if (isPrimitiveType(dest->type) && (dest->type == src->type)) {
        if (dest->type->typeKind != STATIC_TYPE) {
            llvm::Value *v = llvmBuilder->CreateLoad(src->llValue);
            llvmBuilder->CreateStore(v, dest->llValue);
        }
        return;
    }
    codegenCallValue(staticCValue(dest->type.ptr()),
                     new MultiCValue(src),
                     ctx,
                     new MultiCValue(dest));
}

void codegenValueMove(CValuePtr dest, CValuePtr src, CodegenContextPtr ctx)
{
    if (isPrimitiveType(dest->type) && (dest->type == src->type)) {
        if (dest->type->typeKind != STATIC_TYPE) {
            llvm::Value *v = llvmBuilder->CreateLoad(src->llValue);
            llvmBuilder->CreateStore(v, dest->llValue);
        }
        return;
    }
    codegenCallValue(kernelCValue("move"),
                     new MultiCValue(src),
                     ctx,
                     new MultiCValue(dest));
}

void codegenValueAssign(CValuePtr dest, CValuePtr src, CodegenContextPtr ctx)
{
    if (isPrimitiveType(dest->type) && (dest->type == src->type)) {
        if (dest->type->typeKind != STATIC_TYPE) {
            llvm::Value *v = llvmBuilder->CreateLoad(src->llValue);
            llvmBuilder->CreateStore(v, dest->llValue);
        }
        return;
    }
    MultiCValuePtr args = new MultiCValue(dest);
    args->add(src);
    codegenCallValue(kernelCValue("assign"),
                     args,
                     ctx,
                     new MultiCValue());
}

llvm::Value *codegenToBoolFlag(CValuePtr a, CodegenContextPtr ctx)
{
    if (a->type != boolType)
        error("expecting bool type");
    llvm::Value *b1 = llvmBuilder->CreateLoad(a->llValue);
    llvm::Value *zero = llvm::ConstantInt::get(llvmType(boolType), 0);
    llvm::Value *flag1 = llvmBuilder->CreateICmpNE(b1, zero);
    return flag1;
}



//
// codegen temps
//

struct StackEntry {
    CValuePtr cv;
    llvm::BasicBlock *landingPad;
    llvm::BasicBlock *destructor;
    StackEntry(CValuePtr cv)
        : cv(cv), landingPad(NULL), destructor(NULL) {}
};

static vector<StackEntry> valueStack;

int cgMarkStack()
{
    return valueStack.size();
}

void cgDestroyStack(int marker, CodegenContextPtr ctx)
{
    int i = (int)valueStack.size();
    assert(marker <= i);
    while (marker < i) {
        --i;
        codegenValueDestroy(valueStack[i].cv, ctx);
    }
}

void cgPopStack(int marker)
{
    assert(marker <= (int)valueStack.size());
    while (marker < (int)valueStack.size())
        valueStack.pop_back();
}

void cgDestroyAndPopStack(int marker, CodegenContextPtr ctx)
{
    assert(marker <= (int)valueStack.size());
    while (marker < (int)valueStack.size()) {
        codegenValueDestroy(valueStack.back().cv, ctx);
        valueStack.pop_back();
    }
}

void cgPushStack(CValuePtr cv)
{
    valueStack.push_back(StackEntry(cv));
}

CValuePtr codegenAllocValue(TypePtr t)
{
    const llvm::Type *llt = llvmType(t);
    llvm::Value *llv = llvmInitBuilder->CreateAlloca(llt);
    return new CValue(t, llv);
}



//
// exception support
//

static bool exceptionsEnabled = true;

void setExceptionsEnabled(bool enabled)
{
    exceptionsEnabled = enabled;
}

static llvm::BasicBlock *createLandingPad(CodegenContextPtr ctx)
{
    llvm::BasicBlock *lpad = newBasicBlock("lpad");
    llvmBuilder->SetInsertPoint(lpad);
    return lpad;
}

static llvm::BasicBlock *createUnwindBlock(CodegenContextPtr ctx) 
{
    llvm::BasicBlock *unwindBlock = newBasicBlock("Unwind");
    llvm::IRBuilder<> llBuilder(unwindBlock);
    llvm::Value *one = llvm::ConstantInt::get(llvmIntType(32), 1);
    llBuilder.CreateRet(one);
    return unwindBlock;
}


static llvm::Value *createCall(llvm::Value *llCallable, 
                               vector<llvm::Value *>::iterator argBegin,
                               vector<llvm::Value *>::iterator argEnd,
                               CodegenContextPtr ctx) 
{  
    // We don't have a context
    assert(ctx.ptr());
    if (!exceptionsEnabled)
        return llvmBuilder->CreateCall(llCallable, argBegin, argEnd);

    llvm::BasicBlock *landingPad;

    int startMarker = ctx->returnTarget.stackMarker;
    int endMarker = cgMarkStack();

    // If we are inside a try block then adjust marker
    if (ctx->catchBlock)
        startMarker = ctx->tryBlockStackMarker;

    llvm::BasicBlock *savedInsertPoint = llvmBuilder->GetInsertBlock();
   
    for (int i = startMarker; i < endMarker; ++i) {
        if (valueStack[i].destructor)
            continue;
        
        valueStack[i].destructor = newBasicBlock("destructor");
        llvmBuilder->SetInsertPoint(valueStack[i].destructor);
        codegenValueDestroy(valueStack[i].cv, ctx);
        
        if (ctx->catchBlock && ctx->tryBlockStackMarker == i)
            llvmBuilder->CreateBr(ctx->catchBlock);
        else if (i == ctx->returnTarget.stackMarker) {
            if (!ctx->unwindBlock)
                ctx->unwindBlock = createUnwindBlock(ctx);
            llvmBuilder->CreateBr(ctx->unwindBlock);
        }
        else {
            assert(valueStack[i-1].destructor);
            llvmBuilder->CreateBr(valueStack[i-1].destructor);
        }
    }

    // No live vars, but we were inside a try block
    if (endMarker <= startMarker) {
        landingPad = createLandingPad(ctx);
        if (ctx->catchBlock)
            llvmBuilder->CreateBr(ctx->catchBlock);
        else {
            if (!ctx->unwindBlock)
                ctx->unwindBlock = createUnwindBlock(ctx);
            llvmBuilder->CreateBr(ctx->unwindBlock);
        }
    }
    else {
        if (!valueStack[endMarker-1].landingPad) {
            valueStack[endMarker-1].landingPad = createLandingPad(ctx);
            llvmBuilder->CreateBr(valueStack[endMarker-1].destructor);
        }
        landingPad = valueStack[endMarker-1].landingPad;
    }

    llvmBuilder->SetInsertPoint(savedInsertPoint);
    
    llvm::BasicBlock *normalBlock = newBasicBlock("normal");
    llvm::Value *result = llvmBuilder->CreateCall(llCallable, argBegin, argEnd);
    llvm::Value *zero = llvm::ConstantInt::get(llvmIntType(32), 0);
    llvm::Value *llExcept = llvmBuilder->CreateICmpEQ(result, zero);
    llvmBuilder->CreateCondBr(llExcept, normalBlock, landingPad);
    llvmBuilder->SetInsertPoint(normalBlock);
    return result;
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

MultiCValuePtr codegenMultiAsRef(const vector<ExprPtr> &exprs,
                                 EnvPtr env,
                                 CodegenContextPtr ctx)
{
    MultiCValuePtr out = new MultiCValue();
    for (unsigned i = 0; i < exprs.size(); ++i) {
        ExprPtr x = exprs[i];
        if (x->exprKind == UNPACK) {
            Unpack *y = (Unpack *)x.ptr();
            MultiCValuePtr mcv = codegenExprAsRef(y->expr, env, ctx);
            out->add(mcv);
        }
        else {
            MultiCValuePtr mcv = codegenExprAsRef(x, env, ctx);
            out->add(mcv);
        }
    }
    return out;
}

MultiCValuePtr codegenExprAsRef(ExprPtr expr,
                                EnvPtr env,
                                CodegenContextPtr ctx)
{
    MultiPValuePtr mpv = analyzeExpr(expr, env);
    assert(mpv.ptr());
    MultiCValuePtr mcv = new MultiCValue();
    for (unsigned i = 0; i < mpv->size(); ++i) {
        PValuePtr pv = mpv->values[i];
        if (pv->isTemp) {
            CValuePtr cv = codegenAllocValue(pv->type);
            mcv->add(cv);
        }
        else {
            CValuePtr cvPtr = codegenAllocValue(pointerType(pv->type));
            mcv->add(cvPtr);
        }
    }
    codegenExpr(expr, env, ctx, mcv);
    MultiCValuePtr out = new MultiCValue();
    for (unsigned i = 0; i < mpv->size(); ++i) {
        if (mpv->values[i]->isTemp) {
            out->add(mcv->values[i]);
            cgPushStack(mcv->values[i]);
        }
        else {
            out->add(derefValue(mcv->values[i]));
        }
    }
    return out;
}



//
// codegenOneInto, codegenMultiInto, codegenExprInto
//

void codegenOneInto(ExprPtr expr,
                    EnvPtr env,
                    CodegenContextPtr ctx,
                    CValuePtr out)
{
    PValuePtr pv = analyzeOne(expr, env);
    assert(pv.ptr());
    int marker = cgMarkStack();
    if (pv->isTemp) {
        codegenOne(expr, env, ctx, out);
    }
    else {
        CValuePtr cvPtr = codegenAllocValue(pointerType(pv->type));
        codegenOne(expr, env, ctx, cvPtr);
        codegenValueCopy(out, derefValue(cvPtr), ctx);
    }
    cgDestroyAndPopStack(marker, ctx);
}

void codegenMultiInto(const vector<ExprPtr> &exprs,
                      EnvPtr env,
                      CodegenContextPtr ctx,
                      MultiCValuePtr out)
{
    int marker = cgMarkStack();
    int marker2 = marker;
    unsigned j = 0;
    for (unsigned i = 0; i < exprs.size(); ++i) {
        unsigned prevJ = j;
        ExprPtr x = exprs[i];
        if (x->exprKind == UNPACK) {
            Unpack *y = (Unpack *)x.ptr();
            MultiPValuePtr mpv = analyzeExpr(y->expr, env);
            assert(mpv.ptr());
            assert(j + mpv->size() <= out->size());
            MultiCValuePtr out2 = new MultiCValue();
            for (unsigned k = 0; k < mpv->size(); ++k)
                out2->add(out->values[j + k]);
            codegenExprInto(y->expr, env, ctx, out2);
            j += mpv->size();
        }
        else {
            MultiPValuePtr mpv = analyzeExpr(x, env);
            assert(mpv.ptr());
            if (mpv->size() != 1)
                arityError(x, 1, mpv->size());
            assert(j < out->size());
            codegenOneInto(x, env, ctx, out->values[j]);
            ++j;
        }
        cgDestroyAndPopStack(marker2, ctx);
        for (unsigned k = prevJ; k < j; ++k)
            cgPushStack(out->values[k]);
        marker2 = cgMarkStack();
    }
    assert(j == out->size());
    cgPopStack(marker);
}

void codegenExprInto(ExprPtr expr,
                     EnvPtr env,
                     CodegenContextPtr ctx,
                     MultiCValuePtr out)
{
    MultiPValuePtr mpv = analyzeExpr(expr, env);
    assert(mpv.ptr());
    assert(out->size() == mpv->size());
    int marker = cgMarkStack();
    MultiCValuePtr mcv = new MultiCValue();
    for (unsigned i = 0; i < mpv->size(); ++i) {
        PValuePtr pv = mpv->values[i];
        if (pv->isTemp) {
            mcv->add(out->values[i]);
        }
        else {
            CValuePtr cvPtr = codegenAllocValue(pointerType(pv->type));
            mcv->add(cvPtr);
        }
    }
    codegenExpr(expr, env, ctx, mcv);
    int marker2 = cgMarkStack();
    for (unsigned i = 0; i < mpv->size(); ++i) {
        if (!mpv->values[i]->isTemp) {
            codegenValueCopy(out->values[i], derefValue(mcv->values[i]), ctx);
        }
        cgPushStack(out->values[i]);
    }
    cgPopStack(marker2);
    cgDestroyAndPopStack(marker, ctx);
}



//
// codegenMulti, codegenOne, codegenExpr
//

void codegenMulti(const vector<ExprPtr> &exprs,
                  EnvPtr env,
                  CodegenContextPtr ctx,
                  MultiCValuePtr out)
{
    int marker = cgMarkStack();
    int marker2 = marker;
    unsigned j = 0;
    for (unsigned i = 0; i < exprs.size(); ++i) {
        unsigned prevJ = j;
        ExprPtr x = exprs[i];
        if (x->exprKind == UNPACK) {
            Unpack *y = (Unpack *)x.ptr();
            MultiPValuePtr mpv = analyzeExpr(y->expr, env);
            assert(mpv.ptr());
            assert(j + mpv->size() <= out->size());
            MultiCValuePtr out2 = new MultiCValue();
            for (unsigned k = 0; k < mpv->size(); ++k)
                out2->add(out->values[j + k]);
            codegenExpr(y->expr, env, ctx, out2);
            j += mpv->size();
        }
        else {
            MultiPValuePtr mpv = analyzeExpr(x, env);
            assert(mpv.ptr());
            if (mpv->size() != 1)
                arityError(x, 1, mpv->size());
            assert(j < out->size());
            codegenOne(x, env, ctx, out->values[j]);
            ++j;
        }
        cgDestroyAndPopStack(marker2, ctx);
        for (unsigned k = prevJ; k < j; ++k)
            cgPushStack(out->values[k]);
        marker2 = cgMarkStack();
    }
    assert(j == out->size());
    cgPopStack(marker);
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
                                     false);
        TypePtr type = arrayType(int8Type, x->value.size());
        llvm::GlobalVariable *gvar = new llvm::GlobalVariable(
            *llvmModule, llvmType(type), true,
            llvm::GlobalVariable::PrivateLinkage,
            initializer, "clayliteral_str");
        CValuePtr cv = new CValue(type, gvar);
        codegenCallValue(kernelCValue("stringRef"),
                         new MultiCValue(cv),
                         ctx,
                         out);
        break;
    }

    case IDENTIFIER_LITERAL :
        break;

    case NAME_REF : {
        NameRef *x = (NameRef *)expr.ptr();
        ObjectPtr y = lookupEnv(env, x->name);
        if (y->objKind == EXPRESSION) {
            ExprPtr z = (Expr *)y.ptr();
            codegenExpr(z, env, ctx, out);
        }
        else if (y->objKind == MULTI_EXPR) {
            MultiExprPtr z = (MultiExpr *)y.ptr();
            codegenMulti(z->values, env, ctx, out);
        }
        else {
            codegenStaticObject(y, ctx, out);
        }
        break;
    }

    case TUPLE : {
        Tuple *x = (Tuple *)expr.ptr();
        if ((x->args.size() == 1) &&
            (x->args[0]->exprKind != UNPACK))
        {
            codegenExpr(x->args[0], env, ctx, out);
        }
        else {
            ExprPtr makeTuple = kernelNameRef("makeTuple");
            codegenCallExpr(makeTuple, x->args, env, ctx, out);
        }
        break;
    }

    case ARRAY : {
        Array *x = (Array *)expr.ptr();
        codegenCallExpr(kernelNameRef("makeArray"), x->args, env, ctx, out);
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
        codegenFieldRefExpr(x->expr, x->name, env, ctx, out);
        break;
    }

    case TUPLE_REF : {
        TupleRef *x = (TupleRef *)expr.ptr();
        CValuePtr cv = codegenOneAsRef(x->expr, env, ctx);
        ValueHolderPtr vh = sizeTToValueHolder(x->index);
        MultiCValuePtr args = new MultiCValue(cv);
        args->add(staticCValue(vh.ptr()));
        codegenCallValue(kernelCValue("tupleRef"), args, ctx, out);
        break;
    }

    case UNARY_OP : {
        UnaryOp *x = (UnaryOp *)expr.ptr();
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
        PValuePtr pv1 = analyzeOne(x->expr1, env);
        assert(pv1.ptr());
        PValuePtr pv2 = analyzeOne(x->expr2, env);
        assert(pv2.ptr());
        if (pv1->type != pv2->type)
            error("type mismatch in 'and' expression");
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        llvm::BasicBlock *trueBlock = newBasicBlock("andTrue1");
        llvm::BasicBlock *falseBlock = newBasicBlock("andFalse1");
        llvm::BasicBlock *mergeBlock = newBasicBlock("andMerge");
        if (pv1->isTemp || pv2->isTemp) {
            assert(out0->type == pv1->type);
            codegenOneInto(x->expr1, env, ctx, out0);
            llvm::Value *flag1 = codegenToBoolFlag(out0, ctx);
            llvmBuilder->CreateCondBr(flag1, trueBlock, falseBlock);

            llvmBuilder->SetInsertPoint(trueBlock);
            codegenValueDestroy(out0, ctx);
            codegenOneInto(x->expr2, env, ctx, out0);
            llvmBuilder->CreateBr(mergeBlock);

            llvmBuilder->SetInsertPoint(falseBlock);
            llvmBuilder->CreateBr(mergeBlock);

            llvmBuilder->SetInsertPoint(mergeBlock);
        }
        else {
            assert(out0->type == pointerType(pv1->type));
            codegenOne(x->expr1, env, ctx, out0);
            llvm::Value *flag1 = codegenToBoolFlag(derefValue(out0), ctx);
            llvmBuilder->CreateCondBr(flag1, trueBlock, falseBlock);

            llvmBuilder->SetInsertPoint(trueBlock);
            codegenOne(x->expr2, env, ctx, out0);
            llvmBuilder->CreateBr(mergeBlock);

            llvmBuilder->SetInsertPoint(falseBlock);
            llvmBuilder->CreateBr(mergeBlock);

            llvmBuilder->SetInsertPoint(mergeBlock);
        }
        break;
    }

    case OR : {
        Or *x = (Or *)expr.ptr();
        PValuePtr pv1 = analyzeOne(x->expr1, env);
        assert(pv1.ptr());
        PValuePtr pv2 = analyzeOne(x->expr2, env);
        assert(pv2.ptr());
        if (pv1->type != pv2->type)
            error("type mismatch in 'or' expression");
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        llvm::BasicBlock *trueBlock = newBasicBlock("orTrue1");
        llvm::BasicBlock *falseBlock = newBasicBlock("orFalse1");
        llvm::BasicBlock *mergeBlock = newBasicBlock("orMerge");
        if (pv1->isTemp || pv2->isTemp) {
            assert(out0->type == pv1->type);
            codegenOneInto(x->expr1, env, ctx, out0);
            llvm::Value *flag1 = codegenToBoolFlag(out0, ctx);
            llvmBuilder->CreateCondBr(flag1, trueBlock, falseBlock);

            llvmBuilder->SetInsertPoint(falseBlock);
            codegenValueDestroy(out0, ctx);
            codegenOneInto(x->expr2, env, ctx, out0);
            llvmBuilder->CreateBr(mergeBlock);

            llvmBuilder->SetInsertPoint(trueBlock);
            llvmBuilder->CreateBr(mergeBlock);

            llvmBuilder->SetInsertPoint(mergeBlock);
        }
        else {
            assert(out0->type == pointerType(pv1->type));
            codegenOne(x->expr1, env, ctx, out0);
            llvm::Value *flag1 = codegenToBoolFlag(derefValue(out0), ctx);
            llvmBuilder->CreateCondBr(flag1, trueBlock, falseBlock);

            llvmBuilder->SetInsertPoint(falseBlock);
            codegenOne(x->expr2, env, ctx, out0);
            llvmBuilder->CreateBr(mergeBlock);

            llvmBuilder->SetInsertPoint(trueBlock);
            llvmBuilder->CreateBr(mergeBlock);

            llvmBuilder->SetInsertPoint(mergeBlock);
        }
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
        llvmBuilder->CreateStore(llv, out0->llValue);
        break;
    }

    case GLOBAL_VARIABLE : {
        GlobalVariable *y = (GlobalVariable *)x.ptr();
        if (!y->llGlobal)
            codegenGlobalVariable(y);
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0->type == pointerType(y->type));
        llvmBuilder->CreateStore(y->llGlobal, out0->llValue);
        break;
    }

    case EXTERNAL_VARIABLE : {
        ExternalVariable *y = (ExternalVariable *)x.ptr();
        if (!y->llGlobal)
            codegenExternalVariable(y);
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0->type == pointerType(y->type2));
        llvmBuilder->CreateStore(y->llGlobal, out0->llValue);
        break;
    }

    case EXTERNAL_PROCEDURE : {
        ExternalProcedure *y = (ExternalProcedure *)x.ptr();
        if (!y->llvmFunc)
            codegenExternalProcedure(y);
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0->type == y->ptrType);
        llvmBuilder->CreateStore(y->llvmFunc, out0->llValue);
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

    case TYPE :
    case PRIM_OP :
    case PROCEDURE :
    case RECORD :
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
            llvmBuilder->CreateStore(y->llValue, out0->llValue);
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
                llvmBuilder->CreateStore(vi->llValue, outi->llValue);
            }
        }
        break;
    }

    case PATTERN : {
        error("pattern variable cannot be used as value");
        break;
    }

    default :
        error("invalid static object");
        break;

    }
}



//
// codegenGlobalVariable
//

void codegenGlobalVariable(GlobalVariablePtr x)
{
    assert(!x->llGlobal);
    PValuePtr y = analyzeGlobalVariable(x);
    llvm::Constant *initializer =
        llvm::Constant::getNullValue(llvmType(y->type));
    x->llGlobal =
        new llvm::GlobalVariable(
            *llvmModule, llvmType(y->type), false,
            llvm::GlobalVariable::InternalLinkage,
            initializer, "clay_" + x->name->str);
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
        // '\01' is the llvm marker to specify asm label
        llvmFuncName = "\01" + x->attrAsmLabel;
    }
    else {
        llvmFuncName = x->name->str;
    }

    x->llvmFunc = llvm::Function::Create(llFuncType,
                                         linkage,
                                         llvmFuncName,
                                         llvmModule);
    if (x->attrStdCall)
        x->llvmFunc->setCallingConv(llvm::CallingConv::X86_StdCall);
    else if (x->attrFastCall)
        x->llvmFunc->setCallingConv(llvm::CallingConv::X86_FastCall);

    if (!x->body) return;

    llvm::Function *savedLLVMFunction = llvmFunction;
    llvm::IRBuilder<> *savedLLVMInitBuilder = llvmInitBuilder;
    llvm::IRBuilder<> *savedLLVMBuilder = llvmBuilder;

    llvmFunction = x->llvmFunc;

    llvm::BasicBlock *initBlock = newBasicBlock("init");
    llvm::BasicBlock *codeBlock = newBasicBlock("code");
    llvm::BasicBlock *returnBlock = newBasicBlock("return");

    llvmInitBuilder = new llvm::IRBuilder<>(initBlock);
    llvmBuilder = new llvm::IRBuilder<>(codeBlock);

    EnvPtr env = new Env(x->env);

    llvm::Function::arg_iterator ai = llvmFunction->arg_begin();
    for (unsigned i = 0; i < x->args.size(); ++i, ++ai) {
        ExternalArgPtr arg = x->args[i];
        llvm::Argument *llArg = &(*ai);
        llArg->setName(arg->name->str);
        llvm::Value *llArgVar =
            llvmInitBuilder->CreateAlloca(llvmType(arg->type2));
        llArgVar->setName(arg->name->str);
        llvmBuilder->CreateStore(llArg, llArgVar);
        CValuePtr cvalue = new CValue(arg->type2, llArgVar);
        addLocal(env, arg->name, cvalue.ptr());
    }

    vector<CReturn> returns;
    if (x->returnType2.ptr()) {
        llvm::Value *llRetVal =
            llvmInitBuilder->CreateAlloca(llvmType(x->returnType2));
        CValuePtr cret = new CValue(x->returnType2, llRetVal);
        returns.push_back(CReturn(false, x->returnType2, cret));
    }

    JumpTarget returnTarget(returnBlock, cgMarkStack());
    CodegenContextPtr ctx = new CodegenContext(returns, returnTarget);

    bool terminated = codegenStatement(x->body, env, ctx);
    if (!terminated) {
        cgDestroyStack(returnTarget.stackMarker, ctx);
        llvmBuilder->CreateBr(returnBlock);
    }
    cgPopStack(returnTarget.stackMarker);

    llvmInitBuilder->CreateBr(codeBlock);

    llvmBuilder->SetInsertPoint(returnBlock);
    if (!x->returnType2) {
        llvmBuilder->CreateRetVoid();
    }
    else {
        CValuePtr retVal = returns[0].value;
        llvm::Value *v = llvmBuilder->CreateLoad(retVal->llValue);
        llvmBuilder->CreateRet(v);
    }

    delete llvmInitBuilder;
    delete llvmBuilder;

    llvmInitBuilder = savedLLVMInitBuilder;
    llvmBuilder = savedLLVMBuilder;
    llvmFunction = savedLLVMFunction;
}



//
// codegenValueHolder
//

void codegenValueHolder(ValueHolderPtr v,
                        CodegenContextPtr ctx,
                        MultiCValuePtr out)
{
    assert(out->size() == 1);
    CValuePtr out0 = out->values[0];
    assert(out0->type == v->type);

    switch (v->type->typeKind) {

    case BOOL_TYPE :
    case INTEGER_TYPE :
    case FLOAT_TYPE : {
        llvm::Value *llv = codegenSimpleConstant(v);
        llvmBuilder->CreateStore(llv, out0->llValue);
        break;
    }

    case POINTER_TYPE :
    case CODE_POINTER_TYPE :
    case CCODE_POINTER_TYPE :
        error("pointer constants are not supported");
        break;

    default :
        // TODO: support complex constants
        error("complex constants are not supported yet");
        break;

    }
}



//
// codegenSimpleConstant
//

template <typename T>
llvm::Value *
_sintConstant(ValueHolderPtr v)
{
    return llvm::ConstantInt::getSigned(llvmType(v->type), *((T *)v->buf));
}

template <typename T>
llvm::Value *
_uintConstant(ValueHolderPtr v)
{
    return llvm::ConstantInt::get(llvmType(v->type), *((T *)v->buf));
}

llvm::Value *codegenSimpleConstant(ValueHolderPtr v)
{
    llvm::Value *val = NULL;
    switch (v->type->typeKind) {
    case BOOL_TYPE : {
        int bv = (*(bool *)v->buf) ? 1 : 0;
        return llvm::ConstantInt::get(llvmType(boolType), bv);
    }
    case INTEGER_TYPE : {
        IntegerType *t = (IntegerType *)v->type.ptr();
        if (t->isSigned) {
            switch (t->bits) {
            case 8 :
                val = _sintConstant<char>(v);
                break;
            case 16 :
                val = _sintConstant<short>(v);
                break;
            case 32 :
                val = _sintConstant<int>(v);
                break;
            case 64 :
                val = _sintConstant<long long>(v);
                break;
            default :
                assert(false);
            }
        }
        else {
            switch (t->bits) {
            case 8 :
                val = _uintConstant<unsigned char>(v);
                break;
            case 16 :
                val = _uintConstant<unsigned short>(v);
                break;
            case 32 :
                val = _uintConstant<unsigned int>(v);
                break;
            case 64 :
                val = _uintConstant<unsigned long long>(v);
                break;
            default :
                assert(false);
            }
        }
        break;
    }
    case FLOAT_TYPE : {
        FloatType *t = (FloatType *)v->type.ptr();
        switch (t->bits) {
        case 32 :
            val = llvm::ConstantFP::get(llvmType(t), *((float *)v->buf));
            break;
        case 64 :
            val = llvm::ConstantFP::get(llvmType(t), *((double *)v->buf));
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
                         const vector<ExprPtr> &args,
                         EnvPtr env,
                         CodegenContextPtr ctx,
                         MultiCValuePtr out)
{
    MultiPValuePtr mpv = analyzeIndexingExpr(indexable, args, env);
    assert(mpv.ptr());
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
    PValuePtr pv = analyzeOne(indexable, env);
    assert(pv.ptr());
    if (pv->type->typeKind == STATIC_TYPE) {
        StaticType *st = (StaticType *)pv->type.ptr();
        ObjectPtr obj = st->obj;
        if (obj->objKind == GLOBAL_ALIAS) {
            GlobalAlias *x = (GlobalAlias *)obj.ptr();
            codegenAliasIndexing(x, args, env, ctx, out);
            return;
        }
    }
    vector<ExprPtr> args2;
    args2.push_back(indexable);
    args2.insert(args2.end(), args.begin(), args.end());
    codegenCallExpr(kernelNameRef("index"), args2, env, ctx, out);
}



//
// codegenAliasIndexing
//

void codegenAliasIndexing(GlobalAliasPtr x,
                          const vector<ExprPtr> &args,
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
// codegenFieldRefExpr
//

void codegenFieldRefExpr(ExprPtr base,
                         IdentifierPtr name,
                         EnvPtr env,
                         CodegenContextPtr ctx,
                         MultiCValuePtr out)
{
    PValuePtr pv = analyzeOne(base, env);
    assert(pv.ptr());
    if (pv->type->typeKind == STATIC_TYPE) {
        StaticType *st = (StaticType *)pv->type.ptr();
        ObjectPtr obj = st->obj;
        if (obj->objKind == MODULE_HOLDER) {
            ModuleHolderPtr y = (ModuleHolder *)obj.ptr();
            ObjectPtr z = lookupModuleMember(y, name);
            codegenStaticObject(z, ctx, out);
            return;
        }
        if (obj->objKind != VALUE_HOLDER)
            error("invalid field access");
    }
    vector<ExprPtr> args2;
    args2.push_back(base);
    args2.push_back(new ObjectExpr(name.ptr()));
    codegenCallExpr(kernelNameRef("fieldRef"), args2, env, ctx, out);
}



//
// codegenCallExpr
//

void codegenCallExpr(ExprPtr callable,
                     const vector<ExprPtr> &args,
                     EnvPtr env,
                     CodegenContextPtr ctx,
                     MultiCValuePtr out)
{
    PValuePtr pv = analyzeOne(callable, env);
    assert(pv.ptr());

    switch (pv->type->typeKind) {
    case CODE_POINTER_TYPE :
    case CCODE_POINTER_TYPE : {
        CValuePtr cv = codegenOneAsRef(callable, env, ctx);
        MultiCValuePtr mcv = codegenMultiAsRef(args, env, ctx);
        codegenCallPointer(cv, mcv, ctx, out);
        return;
    }
    }

    if (pv->type->typeKind != STATIC_TYPE) {
        vector<ExprPtr> args2;
        args2.push_back(callable);
        args2.insert(args2.end(), args.begin(), args.end());
        codegenCallExpr(kernelNameRef("call"), args2, env, ctx, out);
        return;
    }

    StaticType *st = (StaticType *)pv->type.ptr();
    ObjectPtr obj = st->obj;

    switch (obj->objKind) {

    case TYPE :
    case RECORD :
    case PROCEDURE : {
        MultiPValuePtr mpv = analyzeMulti(args, env);
        assert(mpv.ptr());
        vector<TypePtr> argsKey;
        vector<ValueTempness> argsTempness;
        computeArgsKey(mpv, argsKey, argsTempness);
        InvokeStackContext invokeStackContext(obj, argsKey);
        InvokeEntryPtr entry = analyzeCallable(obj, argsKey, argsTempness);
        if (entry->inlined) {
            codegenCallInlined(entry, args, env, ctx, out);
        }
        else {
            assert(entry->analyzed);
            MultiCValuePtr mcv = codegenMultiAsRef(args, env, ctx);
            codegenCallCode(entry, mcv, ctx, out);
        }
        break;
    }

    case PRIM_OP : {
        PrimOpPtr x = (PrimOp *)obj.ptr();
        MultiCValuePtr mcv = codegenMultiAsRef(args, env, ctx);
        codegenPrimOp(x, mcv, ctx, out);
        break;
    }

    default :
        error("invalid call expression");
        break;

    }
}



//
// codegenCallValue
//

void codegenCallValue(CValuePtr callable,
                      MultiCValuePtr args,
                      CodegenContextPtr ctx,
                      MultiCValuePtr out)
{
    switch (callable->type->typeKind) {
    case CODE_POINTER_TYPE :
    case CCODE_POINTER_TYPE :
        codegenCallPointer(callable, args, ctx, out);
        return;
    }

    if (callable->type->typeKind != STATIC_TYPE) {
        MultiCValuePtr args2 = new MultiCValue(callable);
        args2->add(args2);
        codegenCallValue(kernelCValue("call"), args2, ctx, out);
        return;
    }

    StaticType *st = (StaticType *)callable->type.ptr();
    ObjectPtr obj = st->obj;

    switch (obj->objKind) {

    case TYPE :
    case RECORD :
    case PROCEDURE : {
        MultiPValuePtr mpv = new MultiPValue();
        for (unsigned i = 0; i < args->size(); ++i)
            mpv->add(new PValue(args->values[i]->type, false));
        vector<TypePtr> argsKey;
        vector<ValueTempness> argsTempness;
        computeArgsKey(mpv, argsKey, argsTempness);
        InvokeStackContext invokeStackContext(obj, argsKey);
        InvokeEntryPtr entry = analyzeCallable(obj, argsKey, argsTempness);
        if (entry->inlined)
            error("call to inlined code is not allowed in this context");
        assert(entry->analyzed);
        codegenCallCode(entry, args, ctx, out);
        break;
    }

    case PRIM_OP : {
        PrimOpPtr x = (PrimOp *)obj.ptr();
        codegenPrimOp(x, args, ctx, out);
        break;
    }

    default :
        error("invalid call operation");
        break;

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
    switch (x->type->typeKind) {

    case CODE_POINTER_TYPE : {
        CodePointerType *t = (CodePointerType *)x->type.ptr();
        ensureArity(args, t->argTypes.size());
        llvm::Value *llCallable = llvmBuilder->CreateLoad(x->llValue);
        vector<llvm::Value *> llArgs;
        for (unsigned i = 0; i < args->size(); ++i) {
            CValuePtr cv = args->values[i];
            if (cv->type != t->argTypes[i])
                argumentError(i, "type mismatch");
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
        createCall(llCallable, llArgs.begin(), llArgs.end(), ctx);
        break;
    }

    case CCODE_POINTER_TYPE : {
        CCodePointerType *t = (CCodePointerType *)x->type.ptr();
        if (!t->hasVarArgs)
            ensureArity(args, t->argTypes.size());
        else if (args->size() < t->argTypes.size())
            arityError2(t->argTypes.size(), args->size());
        llvm::Value *llCallable = llvmBuilder->CreateLoad(x->llValue);
        vector<llvm::Value *> llArgs;
        for (unsigned i = 0; i < t->argTypes.size(); ++i) {
            CValuePtr cv = args->values[i];
            if (cv->type != t->argTypes[i])
                argumentError(i, "type mismatch");
            llvm::Value *llv = llvmBuilder->CreateLoad(cv->llValue);
            llArgs.push_back(llv);
        }
        if (t->hasVarArgs) {
            for (unsigned i = t->argTypes.size(); i < args->size(); ++i) {
                CValuePtr cv = args->values[i];
                llvm::Value *llv = llvmBuilder->CreateLoad(cv->llValue);
                llArgs.push_back(llv);
            }
        }
        llvm::CallInst *callInst =
            llvmBuilder->CreateCall(llCallable,
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
            llvmBuilder->CreateStore(llRet, out0->llValue);
        }
        else {
            assert(out->size() == 0);
        }
        break;
    }

    default :
        assert(false);

    }
}



//
// codegenCallCode
//

void codegenCallCode(InvokeEntryPtr entry,
                     MultiCValuePtr args,
                     CodegenContextPtr ctx,
                     MultiCValuePtr out)
{
    if (!entry->llvmFunc)
        codegenCodeBody(entry, getCodeName(entry->callable));
    assert(entry->llvmFunc);
    ensureArity(args, entry->argsKey.size());
    vector<llvm::Value *> llArgs;
    for (unsigned i = 0; i < args->size(); ++i) {
        CValuePtr cv = args->values[i];
        if (cv->type != entry->argsKey[i])
            argumentError(i, "type mismatch");
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
    createCall(entry->llvmFunc, llArgs.begin(), llArgs.end(), ctx);
}



//
// codegenCallable
//

InvokeEntryPtr codegenCallable(ObjectPtr x,
                               const vector<TypePtr> &argsKey,
                               const vector<ValueTempness> &argsTempness)
{
    InvokeEntryPtr entry =
        analyzeCallable(x, argsKey, argsTempness);
    if (!entry->inlined) {
        if (!entry->llvmFunc)
            codegenCodeBody(entry, getCodeName(x));
    }
    return entry;
}



//
// codegenLLVMBody
//

static bool terminator(char c) {
    return (c == ' ' || c == '\n' || c == '\r' || c == '*' || c == '\t');
}

static bool renderTemplate(LLVMBodyPtr llvmBody, string &out, EnvPtr env)
{
    SourcePtr source = llvmBody->location->source;
    int startingOffset = llvmBody->location->offset;
    startingOffset += strlen(LLVM_TOKEN_PREFIX);

    const string &body = llvmBody->body;
    llvm::raw_string_ostream outstream(out);
    for(string::const_iterator i = body.begin(); i != body.end(); ++i) {
        if (*i != '$') {
            outstream << *i;
            continue;
        }

        ++i;

        string::const_iterator typeExprBegin = i;
        while ((i != body.end()) && !terminator(*i))
            ++i;
        string::const_iterator typeExprEnd = i;

        int offset = (typeExprBegin - body.begin()) + startingOffset;
        int length = typeExprEnd - typeExprBegin;

        ExprPtr typeExpr = parseExpr(source, offset, length);
        TypePtr type = evaluateType(typeExpr, env);
        WriteTypeSymbolic(outstream, llvmType(type), llvmModule);

        outstream << *i;
    }
    out = outstream.str();
    return true;
}

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
        WriteTypeSymbolic(out, argType, llvmModule);
        out << string(" %") << entry->fixedArgNames[i]->str;
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
        WriteTypeSymbolic(out, argType, llvmModule);
        assert(i < entry->code->returnSpecs.size());
        const ReturnSpecPtr spec = entry->code->returnSpecs[i];
        assert(spec->name.ptr());
        out << string(" %") << spec->name->str;
        argCount ++;
    }

    string body;
    if (!renderTemplate(entry->code->llvmBody, body, entry->env))
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
// codegenCodeBody
//

void codegenCodeBody(InvokeEntryPtr entry, const string &callableName)
{
    assert(entry->analyzed);
    assert(!entry->llvmFunc);

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

    llvm::Function *savedLLVMFunction = llvmFunction;
    llvm::IRBuilder<> *savedLLVMInitBuilder = llvmInitBuilder;
    llvm::IRBuilder<> *savedLLVMBuilder = llvmBuilder;

    llvmFunction = llFunc;

    llvm::BasicBlock *initBlock = newBasicBlock("init");
    llvm::BasicBlock *codeBlock = newBasicBlock("code");
    llvm::BasicBlock *returnBlock = newBasicBlock("return");

    llvmInitBuilder = new llvm::IRBuilder<>(initBlock);
    llvmBuilder = new llvm::IRBuilder<>(codeBlock);

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

    const vector<ReturnSpecPtr> &returnSpecs = entry->code->returnSpecs;
    if (!returnSpecs.empty()) {
        assert(returnSpecs.size() == entry->returnTypes.size());
    }

    vector<CReturn> returns;
    for (unsigned i = 0; i < entry->returnTypes.size(); ++i, ++ai) {
        llvm::Argument *llArgValue = &(*ai);
        TypePtr rt = entry->returnTypes[i];
        ReturnSpecPtr rspec;
        if (!returnSpecs.empty())
            rspec = returnSpecs[i];
        CValuePtr cv;
        if (entry->returnIsRef[i])
            cv = new CValue(pointerType(rt), llArgValue);
        else
            cv = new CValue(rt, llArgValue);
        returns.push_back(CReturn(entry->returnIsRef[i], rt, cv));
        if (rspec.ptr() && rspec->name.ptr()) {
            addLocal(env, rspec->name, cv.ptr());
            llArgValue->setName(rspec->name->str);
        }
    }

    JumpTarget returnTarget(returnBlock, cgMarkStack());
    CodegenContextPtr ctx = new CodegenContext(returns, returnTarget);

    assert(entry->code->body.ptr());
    bool terminated = codegenStatement(entry->code->body, env, ctx);
    if (!terminated) {
        cgDestroyStack(returnTarget.stackMarker, ctx);
        llvmBuilder->CreateBr(returnBlock);
    }
    cgPopStack(returnTarget.stackMarker);

    llvmInitBuilder->CreateBr(codeBlock);

    llvm::Value *llRet = llvm::ConstantInt::get(llvmIntType(32), 0);

    llvmBuilder->SetInsertPoint(returnBlock);
    llvmBuilder->CreateRet(llRet);

    delete llvmInitBuilder;
    delete llvmBuilder;

    llvmInitBuilder = savedLLVMInitBuilder;
    llvmBuilder = savedLLVMBuilder;
    llvmFunction = savedLLVMFunction;
}



//
// codegenCWrapper
//

void codegenCWrapper(InvokeEntryPtr entry, const string &callableName)
{
    assert(!entry->llvmCWrapper);

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
// codegenCallInlined
//

void codegenCallInlined(InvokeEntryPtr entry,
                        const vector<ExprPtr> &args,
                        EnvPtr env,
                        CodegenContextPtr ctx,
                        MultiCValuePtr out)
{
    assert(entry->inlined);
    if (entry->varArgName.ptr())
        assert(args.size() >= entry->fixedArgNames.size());
    else
        assert(args.size() == entry->fixedArgNames.size());

    EnvPtr bodyEnv = new Env(entry->env);

    for (unsigned i = 0; i < entry->fixedArgNames.size(); ++i) {
        ExprPtr expr = foreignExpr(env, args[i]);
        addLocal(bodyEnv, entry->fixedArgNames[i], expr.ptr());
    }

    if (entry->varArgName.ptr()) {
        MultiExprPtr varArgs = new MultiExpr();
        for (unsigned i = entry->fixedArgNames.size(); i < args.size(); ++i) {
            ExprPtr expr = foreignExpr(env, args[i]);
            varArgs->add(expr);
        }
        addLocal(bodyEnv, entry->varArgName, varArgs.ptr());
    }

    MultiPValuePtr mpv = analyzeCallInlined(entry, args, env);
    assert(mpv->size() == out->size());

    const vector<ReturnSpecPtr> &returnSpecs = entry->code->returnSpecs;
    if (!returnSpecs.empty()) {
        assert(returnSpecs.size() == mpv->size());
    }

    vector<CReturn> returns;
    for (unsigned i = 0; i < mpv->size(); ++i) {
        PValuePtr pv = mpv->values[i];
        CValuePtr cv = out->values[i];
        if (pv->isTemp)
            assert(cv->type == pv->type);
        else
            assert(cv->type == pointerType(pv->type));
        returns.push_back(CReturn(!pv->isTemp, pv->type, cv));
        if (!returnSpecs.empty() && returnSpecs[i]->name.ptr()) {
            addLocal(bodyEnv, returnSpecs[i]->name, cv.ptr());
        }
    }

    llvm::BasicBlock *returnBlock = newBasicBlock("return");

    JumpTarget returnTarget(returnBlock, cgMarkStack());
    CodegenContextPtr bodyCtx = new CodegenContext(returns, returnTarget);

    assert(entry->code->body.ptr());
    bool terminated = codegenStatement(entry->code->body, bodyEnv, bodyCtx);
    if (!terminated) {
        cgDestroyStack(returnTarget.stackMarker, bodyCtx);
        llvmBuilder->CreateBr(returnBlock);
    }
    cgPopStack(returnTarget.stackMarker);

    llvmBuilder->SetInsertPoint(returnBlock);
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
        int blockMarker = cgMarkStack();
        codegenCollectLabels(x->statements, 0, ctx);
        bool terminated = false;
        for (unsigned i = 0; i < x->statements.size(); ++i) {
            StatementPtr y = x->statements[i];
            if (y->stmtKind == LABEL) {
                Label *z = (Label *)y.ptr();
                map<string, JumpTarget>::iterator li =
                    ctx->labels.find(z->name->str);
                assert(li != ctx->labels.end());
                const JumpTarget &jt = li->second;
                if (!terminated)
                    llvmBuilder->CreateBr(jt.block);
                llvmBuilder->SetInsertPoint(jt.block);
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
        cgPopStack(blockMarker);
        return terminated;
    }

    case LABEL :
    case BINDING :
        error("invalid statement");

    case ASSIGNMENT : {
        Assignment *x = (Assignment *)stmt.ptr();
        MultiPValuePtr mpvLeft = analyzeMulti(x->left, env);
        MultiPValuePtr mpvRight = analyzeMulti(x->right, env);
        assert(mpvLeft.ptr());
        assert(mpvRight.ptr());
        if (mpvLeft->size() != mpvRight->size())
            error("arity mismatch between left-side and right-side");
        for (unsigned i = 0; i < mpvLeft->size(); ++i) {
            if (mpvLeft->values[i]->isTemp)
                argumentError(i, "cannot assign to a temporary");
        }
        int marker = cgMarkStack();
        if (mpvLeft->size() == 1) {
            MultiCValuePtr mcvRight = codegenMultiAsRef(x->right, env, ctx);
            MultiCValuePtr mcvLeft = codegenMultiAsRef(x->left, env, ctx);
            assert(mcvLeft->size() == 1);
            assert(mcvRight->size() == 1);
            codegenValueAssign(mcvLeft->values[0], mcvRight->values[0], ctx);
        }
        else {
            MultiCValuePtr mcvRight = new MultiCValue();
            for (unsigned i = 0; i < mpvRight->size(); ++i) {
                PValuePtr pv = mpvRight->values[i];
                CValuePtr cv = codegenAllocValue(pv->type);
                mcvRight->add(cv);
            }
            codegenMultiInto(x->right, env, ctx, mcvRight);
            for (unsigned i = 0; i < mcvRight->size(); ++i)
                cgPushStack(mcvRight->values[i]);
            MultiCValuePtr mcvLeft = codegenMultiAsRef(x->left, env, ctx);
            assert(mcvLeft->size() == mcvRight->size());
            for (unsigned i = 0; i < mcvLeft->size(); ++i) {
                CValuePtr cvLeft = mcvLeft->values[i];
                CValuePtr cvRight = mcvRight->values[i];
                codegenValueAssign(cvLeft, cvRight, ctx);
            }
        }
        cgDestroyAndPopStack(marker, ctx);
        return false;
    }

    case INIT_ASSIGNMENT : {
        InitAssignment *x = (InitAssignment *)stmt.ptr();
        MultiPValuePtr mpvLeft = analyzeMulti(x->left, env);
        MultiPValuePtr mpvRight = analyzeMulti(x->right, env);
        assert(mpvLeft.ptr());
        assert(mpvRight.ptr());
        if (mpvLeft->size() != mpvRight->size())
            error("arity mismatch between left-side and right-side");
        for (unsigned i = 0; i < mpvLeft->size(); ++i) {
            if (mpvLeft->values[i]->isTemp)
                argumentError(i, "cannot assign to a temporary");
            if (mpvLeft->values[i]->type != mpvRight->values[i]->type)
                argumentError(i, "type mismatch");
        }
        int marker = cgMarkStack();
        MultiCValuePtr mcvLeft = codegenMultiAsRef(x->left, env, ctx);
        codegenMultiInto(x->right, env, ctx, mcvLeft);
        cgDestroyAndPopStack(marker, ctx);
        return false;
    }

    case UPDATE_ASSIGNMENT : {
        UpdateAssignment *x = (UpdateAssignment *)stmt.ptr();
        PValuePtr pvLeft = analyzeOne(x->left, env);
        if (pvLeft->isTemp)
            error(x->left, "cannot assign to a temporary");
        CallPtr call = new Call(kernelNameRef(updateOperatorName(x->op)));
        call->args.push_back(x->left);
        call->args.push_back(x->right);
        return codegenStatement(new ExprStatement(call.ptr()), env, ctx);
    }

    case GOTO : {
        Goto *x = (Goto *)stmt.ptr();
        map<string, JumpTarget>::iterator li =
            ctx->labels.find(x->labelName->str);
        if (li == ctx->labels.end())
            error("goto label not found");
        const JumpTarget &jt = li->second;
        cgDestroyStack(jt.stackMarker, ctx);
        llvmBuilder->CreateBr(jt.block);
        return true;
    }

    case RETURN : {
        Return *x = (Return *)stmt.ptr();
        MultiPValuePtr mpv = analyzeMulti(x->exprs, env);
        MultiCValuePtr mcv = new MultiCValue();
        ensureArity(mpv, ctx->returns.size());
        for (unsigned i = 0; i < mpv->size(); ++i) {
            PValuePtr pv = mpv->values[i];
            bool byRef = returnKindToByRef(x->returnKind, pv);
            CReturn &y = ctx->returns[i];
            if (y.type != pv->type)
                argumentError(i, "type mismatch");
            if (byRef != y.byRef)
                argumentError(i, "mismatching by-ref and by-value returns");
            if (byRef && pv->isTemp)
                argumentError(i, "cannot return a temporary by reference");
            mcv->add(y.value);
        }
        switch (x->returnKind) {
        case RETURN_VALUE :
            codegenMultiInto(x->exprs, env, ctx, mcv);
            break;
        case RETURN_REF : {
            MultiCValuePtr mcvRef = codegenMultiAsRef(x->exprs, env, ctx);
            assert(mcv->size() == mcvRef->size());
            for (unsigned i = 0; i < mcv->size(); ++i) {
                CValuePtr cvPtr = mcv->values[i];
                CValuePtr cvRef = mcvRef->values[i];
                llvmBuilder->CreateStore(cvRef->llValue, cvPtr->llValue);
            }
            break;
        }
        case RETURN_FORWARD :
            codegenMulti(x->exprs, env, ctx, mcv);
            break;
        default :
            assert(false);
        }
        const JumpTarget &jt = ctx->returnTarget;
        cgDestroyStack(jt.stackMarker, ctx);
        llvmBuilder->CreateBr(jt.block);
        return true;
    }

    case IF : {
        If *x = (If *)stmt.ptr();
        int marker = cgMarkStack();
        CValuePtr cv = codegenOneAsRef(x->condition, env, ctx);
        llvm::Value *cond = codegenToBoolFlag(cv, ctx);
        cgDestroyAndPopStack(marker, ctx);

        llvm::BasicBlock *trueBlock = newBasicBlock("ifTrue");
        llvm::BasicBlock *falseBlock = newBasicBlock("ifFalse");
        llvm::BasicBlock *mergeBlock = NULL;

        llvmBuilder->CreateCondBr(cond, trueBlock, falseBlock);

        bool terminated1 = false;
        bool terminated2 = false;

        llvmBuilder->SetInsertPoint(trueBlock);
        terminated1 = codegenStatement(x->thenPart, env, ctx);
        if (!terminated1) {
            if (!mergeBlock)
                mergeBlock = newBasicBlock("ifMerge");
            llvmBuilder->CreateBr(mergeBlock);
        }

        llvmBuilder->SetInsertPoint(falseBlock);
        if (x->elsePart.ptr())
            terminated2 = codegenStatement(x->elsePart, env, ctx);
        if (!terminated2) {
            if (!mergeBlock)
                mergeBlock = newBasicBlock("ifMerge");
            llvmBuilder->CreateBr(mergeBlock);
        }

        if (!terminated1 || !terminated2)
            llvmBuilder->SetInsertPoint(mergeBlock);

        return terminated1 && terminated2;
    }

    case EXPR_STATEMENT : {
        ExprStatement *x = (ExprStatement *)stmt.ptr();
        int marker = cgMarkStack();
        codegenExprAsRef(x->expr, env, ctx);
        cgDestroyAndPopStack(marker, ctx);
        return false;
    }

    case WHILE : {
        While *x = (While *)stmt.ptr();

        llvm::BasicBlock *whileBegin = newBasicBlock("whileBegin");
        llvm::BasicBlock *whileBody = newBasicBlock("whileBody");
        llvm::BasicBlock *whileEnd = newBasicBlock("whileEnd");

        llvmBuilder->CreateBr(whileBegin);
        llvmBuilder->SetInsertPoint(whileBegin);

        int marker = cgMarkStack();
        CValuePtr cv = codegenOneAsRef(x->condition, env, ctx);
        llvm::Value *cond = codegenToBoolFlag(cv, ctx);
        cgDestroyAndPopStack(marker, ctx);

        llvmBuilder->CreateCondBr(cond, whileBody, whileEnd);

        ctx->breaks.push_back(JumpTarget(whileEnd, cgMarkStack()));
        ctx->continues.push_back(JumpTarget(whileBegin, cgMarkStack()));
        llvmBuilder->SetInsertPoint(whileBody);
        bool terminated = codegenStatement(x->body, env, ctx);
        if (!terminated)
            llvmBuilder->CreateBr(whileBegin);
        ctx->breaks.pop_back();
        ctx->continues.pop_back();

        llvmBuilder->SetInsertPoint(whileEnd);
        return false;
    }

    case BREAK : {
        if (ctx->breaks.empty())
            error("invalid break statement");
        const JumpTarget &jt = ctx->breaks.back();
        cgDestroyStack(jt.stackMarker, ctx);
        llvmBuilder->CreateBr(jt.block);
        return true;
    }

    case CONTINUE : {
        if (ctx->continues.empty())
            error("invalid continue statement");
        const JumpTarget &jt = ctx->continues.back();
        cgDestroyStack(jt.stackMarker, ctx);
        llvmBuilder->CreateBr(jt.block);
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

        llvm::BasicBlock *catchBegin = newBasicBlock("catchBegin");
        llvm::BasicBlock *catchEnd = newBasicBlock("catchEnd");
        llvm::BasicBlock *finallyEnd = newBasicBlock("finallyEnd");
        
        llvm::BasicBlock *savedCatchBegin = ctx->catchBlock;
        ctx->catchBlock = catchBegin;
        ctx->tryBlockStackMarker = cgMarkStack();

        bool tryTerminated = codegenStatement(x->tryBlock, env, ctx);
        ctx->catchBlock = savedCatchBegin;
        llvmBuilder->CreateBr(catchEnd);

        bool catchTerminated = false, finallyTerminated = false;

        llvmBuilder->SetInsertPoint(catchBegin);
        if (x->catchBlock.ptr())
            catchTerminated = codegenStatement(x->catchBlock, env, ctx);
        llvmBuilder->CreateBr(catchEnd);

        llvmBuilder->SetInsertPoint(catchEnd);
        if (x->finallyBlock.ptr())
            finallyTerminated = codegenStatement(x->finallyBlock, env, ctx);
        llvmBuilder->CreateBr(finallyEnd);
        
        llvmBuilder->SetInsertPoint(finallyEnd);

        return (tryTerminated && catchTerminated) || finallyTerminated;
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
            llvm::BasicBlock *bb = newBasicBlock(y->name->str.c_str());
            ctx->labels[y->name->str] = JumpTarget(bb, cgMarkStack());
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
        MultiPValuePtr mpv = analyzeMulti(x->exprs, env);
        if (mpv->size() != x->names.size())
            arityError(x->names.size(), mpv->size());
        MultiCValuePtr mcv = new MultiCValue();
        for (unsigned i = 0; i < x->names.size(); ++i) {
            CValuePtr cv = codegenAllocValue(mpv->values[i]->type);
            mcv->add(cv);
        }
        int marker = cgMarkStack();
        codegenMultiInto(x->exprs, env, ctx, mcv);
        cgDestroyAndPopStack(marker, ctx);
        EnvPtr env2 = new Env(env);
        for (unsigned i = 0; i < x->names.size(); ++i) {
            cgPushStack(mcv->values[i]);
            addLocal(env2, x->names[i], mcv->values[i].ptr());
        }
        return env2;
    }

    case REF : {
        MultiPValuePtr mpv = analyzeMulti(x->exprs, env);
        assert(mpv.ptr());
        if (mpv->size() != x->names.size())
            arityError(x->names.size(), mpv->size());
        MultiCValuePtr mcv = new MultiCValue();
        for (unsigned i = 0; i < x->names.size(); ++i) {
            PValuePtr pv = mpv->values[i];
            if (pv->isTemp) {
                CValuePtr cv = codegenAllocValue(pv->type);
                mcv->add(cv);
            }
            else {
                CValuePtr cvRef = codegenAllocValue(pointerType(pv->type));
                mcv->add(cvRef);
            }
        }
        int marker = cgMarkStack();
        codegenMulti(x->exprs, env, ctx, mcv);
        cgDestroyAndPopStack(marker, ctx);
        EnvPtr env2 = new Env(env);
        for (unsigned i = 0; i < x->names.size(); ++i) {
            if (mpv->values[i]->isTemp) {
                CValuePtr cv = mcv->values[i];
                cgPushStack(cv);
                addLocal(env2, x->names[i], cv.ptr());
            }
            else {
                CValuePtr cvPtr = mcv->values[i];
                addLocal(env2, x->names[i], derefValue(cvPtr).ptr());
            }
        }
        return env2;
    }

    case ALIAS : {
        ensureArity(x->names, 1);
        ensureArity(x->exprs, 1);
        EnvPtr env2 = new Env(env);
        ExprPtr y = foreignExpr(env, x->exprs[0]);
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
// valueToStatic, valueToStaticSizeT
// valueToType, valueToNumericType, valueToIntegerType,
// valueToPointerLikeType, valueToTupleType, valueToRecordType,
// valueToEnumType, valueToIdentifier
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

static size_t valueToStaticSizeT(MultiCValuePtr args, unsigned index)
{
    ObjectPtr obj = valueToStatic(args->values[index]);
    if (!obj || (obj->objKind != VALUE_HOLDER))
        argumentError(index, "expecting a static SizeT value");
    ValueHolder *vh = (ValueHolder *)obj.ptr();
    if (vh->type != cSizeTType)
        argumentError(index, "expecting a static SizeT value");
    return *((size_t *)vh->buf);
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
        argumentError(index, "expecting a numeric type");
        return NULL;
    }
}

static IntegerTypePtr valueToIntegerType(MultiCValuePtr args, unsigned index)
{
    TypePtr t = valueToType(args, index);
    if (t->typeKind != INTEGER_TYPE)
        argumentError(index, "expecting an integer type");
    return (IntegerType *)t.ptr();
}

static TypePtr valueToPointerLikeType(MultiCValuePtr args, unsigned index)
{
    TypePtr t = valueToType(args, index);
    switch (t->typeKind) {
    case POINTER_TYPE :
    case CODE_POINTER_TYPE :
    case CCODE_POINTER_TYPE :
        break;
    default :
        argumentError(index, "expecting a pointer or code-pointer type");
    }
    return t;
}

static TupleTypePtr valueToTupleType(MultiCValuePtr args, unsigned index)
{
    TypePtr t = valueToType(args, index);
    if (t->typeKind != TUPLE_TYPE)
        argumentError(index, "expecting a tuple type");
    return (TupleType *)t.ptr();
}

static RecordTypePtr valueToRecordType(MultiCValuePtr args, unsigned index)
{
    TypePtr t = valueToType(args, index);
    if (t->typeKind != RECORD_TYPE)
        argumentError(index, "expecting a tuple type");
    return (RecordType *)t.ptr();
}

static EnumTypePtr valueToEnumType(MultiCValuePtr args, unsigned index)
{
    TypePtr t = valueToType(args, index);
    if (t->typeKind != ENUM_TYPE)
        argumentError(index, "expecting a tuple type");
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
// numericValue, integerValue, pointerValue, pointerLikeValue,
// arrayValue, tupleValue, recordValue, enumValue
//

static llvm::Value *numericValue(MultiCValuePtr args, unsigned index,
                                 TypePtr &type)
{
    CValuePtr cv = args->values[index];
    if (type.ptr()) {
        if (cv->type != type)
            argumentError(index, "argument type mismatch");
    }
    else {
        switch (cv->type->typeKind) {
        case INTEGER_TYPE :
        case FLOAT_TYPE :
            break;
        default :
            argumentError(index, "expecting value of numeric type");
        }
        type = cv->type;
    }
    return llvmBuilder->CreateLoad(cv->llValue);
}

static llvm::Value *integerValue(MultiCValuePtr args, unsigned index,
                                 IntegerTypePtr &type)
{
    CValuePtr cv = args->values[index];
    if (type.ptr()) {
        if (cv->type != type.ptr())
            argumentError(index, "argument type mismatch");
    }
    else {
        if (cv->type->typeKind != INTEGER_TYPE)
            argumentError(index, "expecting value of integer type");
        type = (IntegerType *)cv->type.ptr();
    }
    return llvmBuilder->CreateLoad(cv->llValue);
}

static llvm::Value *pointerValue(MultiCValuePtr args, unsigned index,
                                 PointerTypePtr &type)
{
    CValuePtr cv = args->values[index];
    if (type.ptr()) {
        if (cv->type != (Type *)type.ptr())
            argumentError(index, "argument type mismatch");
    }
    else {
        if (cv->type->typeKind != POINTER_TYPE)
            argumentError(index, "expecting value of pointer type");
        type = (PointerType *)cv->type.ptr();
    }
    return llvmBuilder->CreateLoad(cv->llValue);
}

static llvm::Value *pointerLikeValue(MultiCValuePtr args, unsigned index,
                                     TypePtr &type)
{
    CValuePtr cv = args->values[index];
    if (type.ptr()) {
        if (cv->type != type)
            argumentError(index, "argument type mismatch");
    }
    else {
        switch (cv->type->typeKind) {
        case POINTER_TYPE :
        case CODE_POINTER_TYPE :
        case CCODE_POINTER_TYPE :
            break;
        default :
            argumentError(index, "expecting a value of "
                          "pointer or code-pointer type");
        }
        type = cv->type;
    }
    return llvmBuilder->CreateLoad(cv->llValue);
}

static llvm::Value *arrayValue(MultiCValuePtr args, unsigned index,
                               ArrayTypePtr &type)
{
    CValuePtr cv = args->values[index];
    if (type.ptr()) {
        if (cv->type != (Type *)type.ptr())
            argumentError(index, "argument type mismatch");
    }
    else {
        if (cv->type->typeKind != ARRAY_TYPE)
            argumentError(index, "expecting a value of array type");
        type = (ArrayType *)cv->type.ptr();
    }
    return cv->llValue;
}

static llvm::Value *tupleValue(MultiCValuePtr args, unsigned index,
                               TupleTypePtr &type)
{
    CValuePtr cv = args->values[index];
    if (type.ptr()) {
        if (cv->type != (Type *)type.ptr())
            argumentError(index, "argument type mismatch");
    }
    else {
        if (cv->type->typeKind != TUPLE_TYPE)
            argumentError(index, "expecting a value of tuple type");
        type = (TupleType *)cv->type.ptr();
    }
    return cv->llValue;
}

static llvm::Value *recordValue(MultiCValuePtr args, unsigned index,
                                RecordTypePtr &type)
{
    CValuePtr cv = args->values[index];
    if (type.ptr()) {
        if (cv->type != (Type *)type.ptr())
            argumentError(index, "argument type mismatch");
    }
    else {
        if (cv->type->typeKind != RECORD_TYPE)
            argumentError(index, "expecting a value of record type");
        type = (RecordType *)cv->type.ptr();
    }
    return cv->llValue;
}

static llvm::Value *enumValue(MultiCValuePtr args, unsigned index,
                              EnumTypePtr &type)
{
    CValuePtr cv = args->values[index];
    if (type.ptr()) {
        if (cv->type != (Type *)type.ptr())
            argumentError(index, "argument type mismatch");
    }
    else {
        if (cv->type->typeKind != ENUM_TYPE)
            argumentError(index, "expecting a value of enum type");
        type = (EnumType *)cv->type.ptr();
    }
    return llvmBuilder->CreateLoad(cv->llValue);
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

    case PRIM_CallDefinedP : {
        if (args->size() < 1)
            arityError2(1, args->size());
        ObjectPtr callable = valueToStatic(args->values[0]);
        if (!callable) {
            MultiCValuePtr args2 = new MultiCValue(kernelCValue("call"));
            args2->add(args);
            codegenPrimOp(x, args2, ctx, out);
            break;
        }
        switch (callable->objKind) {
        case TYPE :
        case RECORD :
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
        InvokeStackContext invokeStackContext(callable, argsKey);
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
            argumentError(0, "expecting a value of primitive type");
        if (cv0->type != cv1->type)
            argumentError(1, "argument type mismatch");
        llvm::Value *v = llvmBuilder->CreateLoad(cv1->llValue);
        llvmBuilder->CreateStore(v, cv0->llValue);
        assert(out->size() == 0);
        break;
    }

    case PRIM_boolNot : {
        ensureArity(args, 1);
        CValuePtr cv = args->values[0];
        if (cv->type != boolType)
            argumentError(0, "expecting a value of bool type");
        assert(out->size() == 1);
        llvm::Value *v = llvmBuilder->CreateLoad(cv->llValue);
        llvm::Value *zero = llvm::ConstantInt::get(llvmType(boolType), 0);
        llvm::Value *flag = llvmBuilder->CreateICmpEQ(v, zero);
        llvm::Value *v2 = llvmBuilder->CreateZExt(flag, llvmType(boolType));
        CValuePtr out0 = out->values[0];
        assert(out0->type == boolType);
        llvmBuilder->CreateStore(v2, out0->llValue);
        break;
    }

    case PRIM_numericEqualsP : {
        ensureArity(args, 2);
        TypePtr t;
        llvm::Value *v0 = numericValue(args, 0, t);
        llvm::Value *v1 = numericValue(args, 1, t);
        llvm::Value *flag;
        switch (t->typeKind) {
        case INTEGER_TYPE :
            flag = llvmBuilder->CreateICmpEQ(v0, v1);
            break;
        case FLOAT_TYPE :
            flag = llvmBuilder->CreateFCmpUEQ(v0, v1);
            break;
        default :
            assert(false);
        }
        llvm::Value *result =
            llvmBuilder->CreateZExt(flag, llvmType(boolType));
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0->type == boolType);
        llvmBuilder->CreateStore(result, out0->llValue);
        break;
    }

    case PRIM_numericLesserP : {
        ensureArity(args, 2);
        TypePtr t;
        llvm::Value *v0 = numericValue(args, 0, t);
        llvm::Value *v1 = numericValue(args, 1, t);
        llvm::Value *flag;
        switch (t->typeKind) {
        case INTEGER_TYPE : {
            IntegerType *it = (IntegerType *)t.ptr();
            if (it->isSigned)
                flag = llvmBuilder->CreateICmpSLT(v0, v1);
            else
                flag = llvmBuilder->CreateICmpULT(v0, v1);
            break;
        }
        case FLOAT_TYPE :
            flag = llvmBuilder->CreateFCmpULT(v0, v1);
            break;
        default :
            assert(false);
        }
        llvm::Value *result =
            llvmBuilder->CreateZExt(flag, llvmType(boolType));
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0->type == boolType);
        llvmBuilder->CreateStore(result, out0->llValue);
        break;
    }

    case PRIM_numericAdd : {
        ensureArity(args, 2);
        TypePtr t;
        llvm::Value *v0 = numericValue(args, 0, t);
        llvm::Value *v1 = numericValue(args, 1, t);
        llvm::Value *result;
        switch (t->typeKind) {
        case INTEGER_TYPE :
            result = llvmBuilder->CreateAdd(v0, v1);
            break;
        case FLOAT_TYPE :
            result = llvmBuilder->CreateFAdd(v0, v1);
            break;
        default :
            assert(false);
        }
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0->type == t);
        llvmBuilder->CreateStore(result, out0->llValue);
        break;
    }

    case PRIM_numericSubtract : {
        ensureArity(args, 2);
        TypePtr t;
        llvm::Value *v0 = numericValue(args, 0, t);
        llvm::Value *v1 = numericValue(args, 1, t);
        llvm::Value *result;
        switch (t->typeKind) {
        case INTEGER_TYPE :
            result = llvmBuilder->CreateSub(v0, v1);
            break;
        case FLOAT_TYPE :
            result = llvmBuilder->CreateFSub(v0, v1);
            break;
        default :
            assert(false);
        }
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0->type == t);
        llvmBuilder->CreateStore(result, out0->llValue);
        break;
    }

    case PRIM_numericMultiply : {
        ensureArity(args, 2);
        TypePtr t;
        llvm::Value *v0 = numericValue(args, 0, t);
        llvm::Value *v1 = numericValue(args, 1, t);
        llvm::Value *result;
        switch (t->typeKind) {
        case INTEGER_TYPE :
            result = llvmBuilder->CreateMul(v0, v1);
            break;
        case FLOAT_TYPE :
            result = llvmBuilder->CreateFMul(v0, v1);
            break;
        default :
            assert(false);
        }
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0->type == t);
        llvmBuilder->CreateStore(result, out0->llValue);
        break;
    }

    case PRIM_numericDivide : {
        ensureArity(args, 2);
        TypePtr t;
        llvm::Value *v0 = numericValue(args, 0, t);
        llvm::Value *v1 = numericValue(args, 1, t);
        llvm::Value *result;
        switch (t->typeKind) {
        case INTEGER_TYPE : {
            IntegerType *it = (IntegerType *)t.ptr();
            if (it->isSigned)
                result = llvmBuilder->CreateSDiv(v0, v1);
            else
                result = llvmBuilder->CreateUDiv(v0, v1);
            break;
        }
        case FLOAT_TYPE :
            result = llvmBuilder->CreateFDiv(v0, v1);
            break;
        default :
            assert(false);
        }
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0->type == t);
        llvmBuilder->CreateStore(result, out0->llValue);
        break;
    }

    case PRIM_numericNegate : {
        ensureArity(args, 1);
        TypePtr t;
        llvm::Value *v = numericValue(args, 0, t);
        llvm::Value *result;
        switch (t->typeKind) {
        case INTEGER_TYPE :
            result = llvmBuilder->CreateNeg(v);
            break;
        case FLOAT_TYPE :
            result = llvmBuilder->CreateFNeg(v);
            break;
        default :
            assert(false);
        }
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0->type == t);
        llvmBuilder->CreateStore(result, out0->llValue);
        break;
    }

    case PRIM_integerRemainder : {
        ensureArity(args, 2);
        IntegerTypePtr t;
        llvm::Value *v0 = integerValue(args, 0, t);
        llvm::Value *v1 = integerValue(args, 1, t);
        llvm::Value *result;
        IntegerType *it = (IntegerType *)t.ptr();
        if (it->isSigned)
            result = llvmBuilder->CreateSRem(v0, v1);
        else
            result = llvmBuilder->CreateURem(v0, v1);
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0->type == t.ptr());
        llvmBuilder->CreateStore(result, out0->llValue);
        break;
    }

    case PRIM_integerShiftLeft : {
        ensureArity(args, 2);
        IntegerTypePtr t;
        llvm::Value *v0 = integerValue(args, 0, t);
        llvm::Value *v1 = integerValue(args, 1, t);
        llvm::Value *result = llvmBuilder->CreateShl(v0, v1);
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0->type == t.ptr());
        llvmBuilder->CreateStore(result, out0->llValue);
        break;
    }

    case PRIM_integerShiftRight : {
        ensureArity(args, 2);
        IntegerTypePtr t;
        llvm::Value *v0 = integerValue(args, 0, t);
        llvm::Value *v1 = integerValue(args, 1, t);
        llvm::Value *result;
        if (t->isSigned)
            result = llvmBuilder->CreateAShr(v0, v1);
        else
            result = llvmBuilder->CreateLShr(v0, v1);
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0->type == t.ptr());
        llvmBuilder->CreateStore(result, out0->llValue);
        break;
    }

    case PRIM_integerBitwiseAnd : {
        ensureArity(args, 2);
        IntegerTypePtr t;
        llvm::Value *v0 = integerValue(args, 0, t);
        llvm::Value *v1 = integerValue(args, 1, t);
        llvm::Value *result = llvmBuilder->CreateAnd(v0, v1);
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0->type == t.ptr());
        llvmBuilder->CreateStore(result, out0->llValue);
        break;
    }

    case PRIM_integerBitwiseOr : {
        ensureArity(args, 2);
        IntegerTypePtr t;
        llvm::Value *v0 = integerValue(args, 0, t);
        llvm::Value *v1 = integerValue(args, 1, t);
        llvm::Value *result = llvmBuilder->CreateOr(v0, v1);
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0->type == t.ptr());
        llvmBuilder->CreateStore(result, out0->llValue);
        break;
    }

    case PRIM_integerBitwiseXor : {
        ensureArity(args, 2);
        IntegerTypePtr t;
        llvm::Value *v0 = integerValue(args, 0, t);
        llvm::Value *v1 = integerValue(args, 1, t);
        llvm::Value *result = llvmBuilder->CreateXor(v0, v1);
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0->type == t.ptr());
        llvmBuilder->CreateStore(result, out0->llValue);
        break;
    }

    case PRIM_integerBitwiseNot : {
        ensureArity(args, 1);
        IntegerTypePtr t;
        llvm::Value *v = integerValue(args, 0, t);
        llvm::Value *result = llvmBuilder->CreateNot(v);
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0->type == t.ptr());
        llvmBuilder->CreateStore(result, out0->llValue);
        break;
    }

    case PRIM_numericConvert : {
        ensureArity(args, 2);
        TypePtr dest = valueToNumericType(args, 0);
        TypePtr src;
        llvm::Value *v = numericValue(args, 1, src);
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
                        result = llvmBuilder->CreateTrunc(v, llvmType(dest));
                    else if (src2->isSigned)
                        result = llvmBuilder->CreateSExt(v, llvmType(dest));
                    else
                        result = llvmBuilder->CreateZExt(v, llvmType(dest));
                }
                else if (src->typeKind == FLOAT_TYPE) {
                    if (dest2->isSigned)
                        result = llvmBuilder->CreateFPToSI(v, llvmType(dest));
                    else
                        result = llvmBuilder->CreateFPToUI(v, llvmType(dest));
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
                        result = llvmBuilder->CreateSIToFP(v, llvmType(dest));
                    else
                        result = llvmBuilder->CreateUIToFP(v, llvmType(dest));
                }
                else if (src->typeKind == FLOAT_TYPE) {
                    FloatType *src2 = (FloatType *)src.ptr();
                    if (dest2->bits < src2->bits)
                        result = llvmBuilder->CreateFPTrunc(v, llvmType(dest));
                    else
                        result = llvmBuilder->CreateFPExt(v, llvmType(dest));
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
        llvmBuilder->CreateStore(result, out0->llValue);
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
        llvmBuilder->CreateStore(cv->llValue, out0->llValue);
        break;
    }

    case PRIM_pointerDereference : {
        ensureArity(args, 1);
        PointerTypePtr t;
        llvm::Value *v = pointerValue(args, 0, t);
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0->type == t.ptr());
        llvmBuilder->CreateStore(v, out0->llValue);
        break;
    }

    case PRIM_pointerEqualsP : {
        ensureArity(args, 2);
        PointerTypePtr t;
        llvm::Value *v0 = pointerValue(args, 0, t);
        llvm::Value *v1 = pointerValue(args, 1, t);
        llvm::Value *flag = llvmBuilder->CreateICmpEQ(v0, v1);
        llvm::Value *result = llvmBuilder->CreateZExt(flag, llvmType(boolType));
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0->type == boolType);
        llvmBuilder->CreateStore(result, out0->llValue);
        break;
    }

    case PRIM_pointerLesserP : {
        ensureArity(args, 2);
        PointerTypePtr t;
        llvm::Value *v0 = pointerValue(args, 0, t);
        llvm::Value *v1 = pointerValue(args, 1, t);
        llvm::Value *flag = llvmBuilder->CreateICmpULT(v0, v1);
        llvm::Value *result = llvmBuilder->CreateZExt(flag, llvmType(boolType));
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0->type == boolType);
        llvmBuilder->CreateStore(result, out0->llValue);
        break;
    }

    case PRIM_pointerOffset : {
        ensureArity(args, 2);
        PointerTypePtr t;
        llvm::Value *v0 = pointerValue(args, 0, t);
        IntegerTypePtr offsetT;
        llvm::Value *v1 = integerValue(args, 1, offsetT);
        vector<llvm::Value *> indices;
        indices.push_back(v1);
        llvm::Value *result = llvmBuilder->CreateGEP(v0,
                                                     indices.begin(),
                                                     indices.end());
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0->type == t.ptr());
        llvmBuilder->CreateStore(result, out0->llValue);
        break;
    }

    case PRIM_pointerToInt : {
        ensureArity(args, 2);
        IntegerTypePtr dest = valueToIntegerType(args, 0);
        const llvm::Type *llDest = llvmType(dest.ptr());
        PointerTypePtr pt;
        llvm::Value *v = pointerValue(args, 1, pt);
        llvm::Value *result = llvmBuilder->CreatePtrToInt(v, llDest);
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0->type == dest.ptr());
        llvmBuilder->CreateStore(result, out0->llValue);
        break;
    }

    case PRIM_intToPointer : {
        ensureArity(args, 2);
        TypePtr pointeeType = valueToType(args, 0);
        TypePtr dest = pointerType(pointeeType);
        IntegerTypePtr t;
        llvm::Value *v = integerValue(args, 1, t);
        llvm::Value *result = llvmBuilder->CreateIntToPtr(v, llvmType(dest));
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0->type == dest);
        llvmBuilder->CreateStore(result, out0->llValue);
        break;
    }

    case PRIM_CodePointerP : {
        ensureArity(args, 1);
        bool isCodePointerType = false;
        ObjectPtr obj = valueToStatic(args->values[0]);
        if (obj.ptr() && (obj->objKind == TYPE)) {
            Type *t = (Type *)obj.ptr();
            if (t->typeKind == CODE_POINTER_TYPE)
                isCodePointerType = true;
        }
        ValueHolderPtr vh = boolToValueHolder(isCodePointerType);
        codegenStaticObject(vh.ptr(), ctx, out);
        break;
    }

    case PRIM_CodePointer :
        error("CodePointer type constructor cannot be called");

    case PRIM_RefCodePointer :
        error("RefCodePointer type constructor cannot be called");

    case PRIM_makeCodePointer : {
        if (args->size() < 1)
            arityError2(1, args->size());
        ObjectPtr callable = valueToStatic(args, 0);
        switch (callable->objKind) {
        case TYPE :
        case RECORD :
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

        InvokeStackContext invokeStackContext(callable, argsKey);

        InvokeEntryPtr entry =
            analyzeCallable(callable, argsKey, argsTempness);
        if (entry->inlined)
            argumentError(0, "cannot create pointer to inlined code");
        assert(entry->analyzed);
        if (!entry->llvmFunc)
            codegenCodeBody(entry, getCodeName(entry->callable));
        assert(entry->llvmFunc);
        TypePtr cpType = codePointerType(argsKey,
                                         entry->returnIsRef,
                                         entry->returnTypes);
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0->type == cpType);
        llvmBuilder->CreateStore(entry->llvmFunc, out0->llValue);
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

        InvokeStackContext invokeStackContext(callable, argsKey);

        InvokeEntryPtr entry =
            analyzeCallable(callable, argsKey, argsTempness);
        if (entry->inlined)
            argumentError(0, "cannot create pointer to inlined code");
        assert(entry->analyzed);
        if (!entry->llvmFunc)
            codegenCodeBody(entry, getCodeName(entry->callable));
        assert(entry->llvmFunc);
        if (!entry->llvmCWrapper)
            codegenCWrapper(entry, getCodeName(entry->callable));
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
        llvmBuilder->CreateStore(entry->llvmCWrapper, out0->llValue);
        break;
    }

    case PRIM_pointerCast : {
        ensureArity(args, 2);
        TypePtr dest = valueToPointerLikeType(args, 0);
        TypePtr src;
        llvm::Value *v = pointerLikeValue(args, 1, src);
        llvm::Value *result = llvmBuilder->CreateBitCast(v, llvmType(dest));
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0->type == dest);
        llvmBuilder->CreateStore(result, out0->llValue);
        break;
    }

    case PRIM_Array :
        error("Array type constructor cannot be called");

    case PRIM_arrayRef : {
        ensureArity(args, 2);
        ArrayTypePtr at;
        llvm::Value *av = arrayValue(args, 0, at);
        IntegerTypePtr indexType;
        llvm::Value *iv = integerValue(args, 1, indexType);
        vector<llvm::Value *> indices;
        indices.push_back(llvm::ConstantInt::get(llvmIntType(32), 0));
        indices.push_back(iv);
        llvm::Value *ptr =
            llvmBuilder->CreateGEP(av, indices.begin(), indices.end());
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0->type == pointerType(at->elementType));
        llvmBuilder->CreateStore(ptr, out0->llValue);
        break;
    }

    case PRIM_ValArray :
        error("ValArray type constructor cannot be called");

    case PRIM_TupleP : {
        ensureArity(args, 1);
        bool isTupleType = false;
        ObjectPtr obj = valueToStatic(args->values[0]);
        if (obj.ptr() && (obj->objKind == TYPE)) {
            Type *t = (Type *)obj.ptr();
            if (t->typeKind == TUPLE_TYPE)
                isTupleType = true;
        }
        ValueHolderPtr vh = boolToValueHolder(isTupleType);
        codegenStaticObject(vh.ptr(), ctx, out);
        break;
    }

    case PRIM_Tuple :
        error("Tuple type constructor cannot be called");

    case PRIM_TupleElementCount : {
        ensureArity(args, 1);
        TupleTypePtr t = valueToTupleType(args, 0);
        ValueHolderPtr vh = sizeTToValueHolder(t->elementTypes.size());
        codegenStaticObject(vh.ptr(), ctx, out);
        break;
    }

    case PRIM_TupleElementOffset : {
        ensureArity(args, 2);
        TupleTypePtr t = valueToTupleType(args, 0);
        size_t i = valueToStaticSizeT(args, 1);
        if (i >= t->elementTypes.size())
            argumentError(1, "tuple element index out of range");
        const llvm::StructLayout *layout = tupleTypeLayout(t.ptr());
        ValueHolderPtr vh = sizeTToValueHolder(layout->getElementOffset(i));
        codegenStaticObject(vh.ptr(), ctx, out);
        break;
    }

    case PRIM_tupleRef : {
        ensureArity(args, 2);
        TupleTypePtr tt;
        llvm::Value *vtuple = tupleValue(args, 0, tt);
        size_t i = valueToStaticSizeT(args, 1);
        if (i >= tt->elementTypes.size())
            argumentError(1, "tuple element index out of range");
        llvm::Value *ptr = llvmBuilder->CreateConstGEP2_32(vtuple, 0, i);
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0->type == pointerType(tt->elementTypes[i]));
        llvmBuilder->CreateStore(ptr, out0->llValue);
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

    case PRIM_RecordFieldOffset : {
        ensureArity(args, 2);
        RecordTypePtr rt = valueToRecordType(args, 0);
        size_t i = valueToStaticSizeT(args, 1);
        const vector<TypePtr> &fieldTypes = recordFieldTypes(rt);
        if (i >= fieldTypes.size())
            argumentError(1, "record field index out of range");
        const llvm::StructLayout *layout = recordTypeLayout(rt.ptr());
        ValueHolderPtr vh = sizeTToValueHolder(layout->getElementOffset(i));
        codegenStaticObject(vh.ptr(), ctx, out);
        break;
    }

    case PRIM_RecordFieldIndex : {
        ensureArity(args, 2);
        RecordTypePtr rt = valueToRecordType(args, 0);
        IdentifierPtr fname = valueToIdentifier(args, 1);
        const map<string, size_t> &fieldIndexMap = recordFieldIndexMap(rt);
        map<string, size_t>::const_iterator fi =
            fieldIndexMap.find(fname->str);
        if (fi == fieldIndexMap.end())
            argumentError(1, "field not found in record");
        ValueHolderPtr vh = sizeTToValueHolder(fi->second);
        codegenStaticObject(vh.ptr(), ctx, out);
        break;
    }

    case PRIM_recordFieldRef : {
        ensureArity(args, 2);
        RecordTypePtr rt;
        llvm::Value *vrec = recordValue(args, 0, rt);
        size_t i = valueToStaticSizeT(args, 1);
        const vector<TypePtr> &fieldTypes = recordFieldTypes(rt);
        if (i >= fieldTypes.size())
            argumentError(1, "record field index out of range");
        llvm::Value *ptr = llvmBuilder->CreateConstGEP2_32(vrec, 0, i);
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0->type == pointerType(fieldTypes[i]));
        llvmBuilder->CreateStore(ptr, out0->llValue);
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
        if (fi == fieldIndexMap.end())
            argumentError(1, "field not found in record");
        size_t index = fi->second;
        const vector<TypePtr> &fieldTypes = recordFieldTypes(rt);
        llvm::Value *ptr = llvmBuilder->CreateConstGEP2_32(vrec, 0, index);
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0->type == pointerType(fieldTypes[index]));
        llvmBuilder->CreateStore(ptr, out0->llValue);
        break;
    }

    case PRIM_Static :
        error("Static type constructor cannot be called");

    case PRIM_StaticName : {
        ensureArity(args, 1);
        ObjectPtr obj = valueToStatic(args, 0);
        ostringstream sout;
        printName(sout, obj);
        ExprPtr z = new StringLiteral(sout.str());
        codegenExpr(z, new Env(), ctx, out);
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

    case PRIM_enumToInt : {
        ensureArity(args, 1);
        EnumTypePtr et;
        llvm::Value *v = enumValue(args, 0, et);
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0->type == cIntType);
        llvmBuilder->CreateStore(v, out0->llValue);
        break;
    }

    case PRIM_intToEnum : {
        ensureArity(args, 2);
        EnumTypePtr et = valueToEnumType(args, 0);
        IntegerTypePtr it = (IntegerType *)cIntType.ptr();
        llvm::Value *v = integerValue(args, 1, it);
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0->type == et.ptr());
        llvmBuilder->CreateStore(v, out0->llValue);
        break;
    }

    default :
        assert(false);
        break;

    }
}



//
// codegenSharedLib, codegenExe
//

static ProcedurePtr makeInitializerProcedure() {
    CodePtr code = new Code();
    code->body = globalVarInitializers().ptr();
    IdentifierPtr name = new Identifier("%initGlobals");
    ExprPtr target = new NameRef(name);
    OverloadPtr overload = new Overload(target, code, false);
    EnvPtr env = new Env();
    overload->env = env;
    ProcedurePtr proc = new Procedure(name, PRIVATE);
    proc->overloads.push_back(overload);
    proc->env = env;
    env->entries[name->str] = proc.ptr();
    return proc;
}

static ProcedurePtr makeDestructorProcedure() {
    CodePtr code = new Code();
    code->body = globalVarDestructors().ptr();
    IdentifierPtr name = new Identifier("%destroyGlobals");
    ExprPtr target = new NameRef(name);
    OverloadPtr overload = new Overload(target, code, false);
    EnvPtr env = new Env();
    overload->env = env;
    ProcedurePtr proc = new Procedure(name, PRIVATE);
    proc->overloads.push_back(overload);
    proc->env = env;
    env->entries[name->str] = proc.ptr();
    return proc;
}

static void generateLLVMCtorsAndDtors() {
    ObjectPtr initializer = makeInitializerProcedure().ptr();
    InvokeEntryPtr entry1 = codegenCallable(initializer,
                                            vector<TypePtr>(),
                                            vector<ValueTempness>());
    entry1->llvmFunc->setLinkage(llvm::GlobalVariable::ExternalLinkage);
    codegenCWrapper(entry1, getCodeName(initializer));
    ObjectPtr destructor = makeDestructorProcedure().ptr();
    InvokeEntryPtr entry2 = codegenCallable(destructor,
                                            vector<TypePtr>(),
                                            vector<ValueTempness>());
    entry2->llvmFunc->setLinkage(llvm::GlobalVariable::ExternalLinkage);
    codegenCWrapper(entry2, getCodeName(destructor));

    // make types for llvm.global_ctors, llvm.global_dtors
    vector<const llvm::Type *> fieldTypes;
    fieldTypes.push_back(llvmIntType(32));
    const llvm::Type *funcType = entry1->llvmCWrapper->getFunctionType();
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
    structElems1.push_back(entry1->llvmCWrapper);
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
    structElems2.push_back(entry2->llvmCWrapper);
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
    generateLLVMCtorsAndDtors();

    for (unsigned i = 0; i < module->topLevelItems.size(); ++i) {
        TopLevelItemPtr x = module->topLevelItems[i];
        if (x->objKind == EXTERNAL_PROCEDURE) {
            ExternalProcedurePtr y = (ExternalProcedure *)x.ptr();
            if (y->body.ptr())
                codegenExternalProcedure(y);
        }
    }
}

void codegenExe(ModulePtr module)
{
    generateLLVMCtorsAndDtors();

    IdentifierPtr main = new Identifier("main");
    IdentifierPtr argc = new Identifier("argc");
    IdentifierPtr argv = new Identifier("argv");

    BlockPtr mainBody = new Block();

    CallPtr initCmdLine = new Call(kernelNameRef("initializeCommandLine"));
    initCmdLine->args.push_back(new NameRef(argc));
    initCmdLine->args.push_back(new NameRef(argv));
    mainBody->statements.push_back(new ExprStatement(initCmdLine.ptr()));

    CallPtr mainCall = new Call(kernelNameRef("callMain"));
    mainCall->args.push_back(new NameRef(main));

    vector<ExprPtr> exprs;
    exprs.push_back(mainCall.ptr());
    ReturnPtr ret = new Return(RETURN_VALUE, exprs);
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
                              vector<ExprPtr>());

    entryProc->env = module->env;

    codegenExternalProcedure(entryProc);
}
