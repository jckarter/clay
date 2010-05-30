#include "clay.hpp"

void codegenValueInit(CValuePtr dest, CodegenContextPtr ctx);
void codegenValueDestroy(CValuePtr dest, CodegenContextPtr ctx);
void codegenValueCopy(CValuePtr dest, CValuePtr src, CodegenContextPtr ctx);
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
void codegenValueHolder(ValueHolderPtr x,
                        CodegenContextPtr ctx,
                        MultiCValuePtr out);

void codegenIndexingExpr(ExprPtr indexable,
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


static vector<CValuePtr> stackCValues;



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
    codegenCallValue(staticCValue(dest->type.ptr()),
                     new MultiCValue(),
                     ctx,
                     new MultiCValue(dest));
}

void codegenValueDestroy(CValuePtr dest, CodegenContextPtr ctx)
{
    codegenCallValue(kernelCValue("destroy"),
                     new MultiCValue(dest),
                     ctx,
                     new MultiCValue());
}

void codegenValueCopy(CValuePtr dest, CValuePtr src, CodegenContextPtr ctx)
{
    codegenCallValue(staticCValue(dest->type.ptr()),
                     new MultiCValue(src),
                     ctx,
                     new MultiCValue(dest));
}

void codegenValueAssign(CValuePtr dest, CValuePtr src, CodegenContextPtr ctx)
{
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

int cgMarkStack()
{
    return stackCValues.size();
}

void cgDestroyStack(int marker, CodegenContextPtr ctx)
{
    int i = (int)stackCValues.size();
    assert(marker <= i);
    while (marker < i) {
        --i;
        codegenValueDestroy(stackCValues[i], ctx);
    }
}

void cgPopStack(int marker)
{
    assert(marker <= (int)stackCValues.size());
    while (marker < (int)stackCValues.size())
        stackCValues.pop_back();
}

void cgDestroyAndPopStack(int marker, CodegenContextPtr ctx)
{
    assert(marker <= (int)stackCValues.size());
    while (marker < (int)stackCValues.size()) {
        codegenValueDestroy(stackCValues.back(), ctx);
        stackCValues.pop_back();
    }
}

CValuePtr codegenAllocValue(TypePtr t)
{
    const llvm::Type *llt = llvmType(t);
    llvm::Value *llval = llvmInitBuilder->CreateAlloca(llt);
    CValuePtr cv = new CValue(t, llval);
    stackCValues.push_back(cv);
    return cv;
}



//
// codegenOneAsRef, codegenMultiAsRef, codegenExprAsRef
//

CValuePtr codegenOneAsRef(ExprPtr expr,
                          EnvPtr env,
                          CodegenContextPtr ctx)
{
    MultiCValuePtr mcv = codegenExprAsRef(expr, env, ctx);
    ensureArity(mcv, 1);
    return mcv->values[0];
}

MultiCValuePtr codegenMultiAsRef(const vector<ExprPtr> &exprs,
                                 EnvPtr env,
                                 CodegenContextPtr ctx)
{
    MultiCValuePtr out = new MultiCValue();
    for (unsigned i = 0; i < exprs.size(); ++i) {
        MultiCValuePtr mcv = codegenExprAsRef(exprs[i], env, ctx);
        out->add(mcv);
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
        if (mpv->values[i]->isTemp)
            out->add(mcv->values[i]);
        else
            out->add(derefValue(mcv->values[i]));
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
    if (pv->isTemp) {
        codegenOne(expr, env, ctx, out);
    }
    else {
        CValuePtr cvPtr = codegenAllocValue(pointerType(pv->type));
        codegenOne(expr, env, ctx, cvPtr);
        codegenValueCopy(out, derefValue(cvPtr), ctx);
    }
}

void codegenMultiInto(const vector<ExprPtr> &exprs,
                      EnvPtr env,
                      CodegenContextPtr ctx,
                      MultiCValuePtr out)
{
    unsigned j = 0;
    for (unsigned i = 0; i < exprs.size(); ++i) {
        ExprPtr x = exprs[i];
        MultiPValuePtr mpv = analyzeExpr(x, env);
        assert(mpv.ptr());
        if (x->exprKind == VAR_ARGS_REF) {
            assert(j + mpv->size() <= out->size());
            MultiCValuePtr out2 = new MultiCValue();
            for (unsigned k = 0; k < mpv->size(); ++k)
                out2->add(out->values[j + k]);
            codegenExprInto(x, env, ctx, out2);
            j += mpv->size();
        }
        else {
            if (mpv->size() != 1)
                arityError(x, 1, mpv->size());
            assert(j < out->size());
            codegenOneInto(x, env, ctx, out->values[j]);
            ++j;
        }
    }
    assert(j == out->size());
}

void codegenExprInto(ExprPtr expr,
                     EnvPtr env,
                     CodegenContextPtr ctx,
                     MultiCValuePtr out)
{
    MultiPValuePtr mpv = analyzeExpr(expr, env);
    assert(mpv.ptr());
    assert(out->size() == mpv->size());
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
    for (unsigned i = 0; i < mpv->size(); ++i) {
        if (!mpv->values[i]->isTemp) {
            codegenValueCopy(out->values[i], derefValue(mcv->values[i]), ctx);
        }
    }
}



//
// codegenMulti, codegenOne, codegenExpr
//

void codegenMulti(const vector<ExprPtr> &exprs,
                  EnvPtr env,
                  CodegenContextPtr ctx,
                  MultiCValuePtr out)
{
    unsigned j = 0;
    for (unsigned i = 0; i < exprs.size(); ++i) {
        ExprPtr x = exprs[i];
        MultiPValuePtr mpv = analyzeExpr(x, env);
        assert(mpv.ptr());
        if (x->exprKind == VAR_ARGS_REF) {
            assert(j + mpv->size() <= out->size());
            MultiCValuePtr out2 = new MultiCValue();
            for (unsigned k = 0; k < mpv->size(); ++k)
                out2->add(out->values[j + k]);
            codegenExpr(x, env, ctx, out2);
            j += mpv->size();
        }
        else {
            if (mpv->size() != 1)
                arityError(x, 1, mpv->size());
            assert(j < out->size());
            codegenOne(x, env, ctx, out->values[j]);
            ++j;
        }
    }
    assert(j == out->size());
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
            (x->args[0]->exprKind != VAR_ARGS_REF))
        {
            codegenExpr(x->args[0], env, ctx, out);
        }
        else {
            codegenCallExpr(primNameRef("tuple"), x->args, env, ctx, out);
        }
        break;
    }

    case ARRAY : {
        Array *x = (Array *)expr.ptr();
        codegenCallExpr(primNameRef("array"), x->args, env, ctx, out);
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
        PValuePtr pv2 = analyzeOne(x->expr2, env);
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
        PValuePtr pv2 = analyzeOne(x->expr2, env);
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

    case VAR_ARGS_REF : {
        IdentifierPtr ident = new Identifier("%varArgs");
        ident->location = expr->location;
        ExprPtr nameRef = new NameRef(ident);
        nameRef->location = expr->location;
        codegenExpr(nameRef, env, ctx, out);
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
