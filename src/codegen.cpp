#include "clay.hpp"
#include "invokeutil2.hpp"



//
// declarations
//

llvm::Function *llvmFunction;
llvm::IRBuilder<> *initBuilder;
llvm::IRBuilder<> *builder;


// value operations codegen

CValuePtr codegenAllocValue(TypePtr t);
void codegenValueInit(CValuePtr dest);
void codegenValueDestroy(CValuePtr dest);
void codegenValueCopy(CValuePtr dest, CValuePtr src);
void codegenValueAssign(CValuePtr dest, CValuePtr src);
llvm::Value *codegenToBool(CValuePtr a);

CValuePtr
codegen(ExprPtr expr, EnvPtr env, llvm::Value *outPtr);

void
codegenInvokeVoid(ObjectPtr obj, ArgListPtr args);

CValuePtr
codegenInvoke(ObjectPtr obj, ArgListPtr args, llvm::Value *outPtr);

void codegenValue(ValuePtr v, llvm::Value *outPtr);

static
llvm::Value *
numericConstant(ValuePtr v);



//
// codegen value ops
//

CValuePtr codegenAllocValue(TypePtr t)
{
    const llvm::Type *llt = llvmType(t);
    llvm::Value *llval = initBuilder->CreateAlloca(llt);
    return new CValue(t, llval);
}

void codegenValueInit(CValuePtr dest)
{
    vector<ExprPtr> exprs;
    exprs.push_back(new CValueExpr(dest));
    ArgListPtr args = new ArgList(exprs, new Env());
    codegenInvokeVoid(coreName("init"), args);
}

void codegenValueDestroy(CValuePtr dest)
{
    vector<ExprPtr> exprs;
    exprs.push_back(new CValueExpr(dest));
    ArgListPtr args = new ArgList(exprs, new Env());
    codegenInvokeVoid(coreName("destroy"), args);
}

void codegenValueCopy(CValuePtr dest, CValuePtr src)
{
    vector<ExprPtr> exprs;
    exprs.push_back(new CValueExpr(dest));
    exprs.push_back(new CValueExpr(src));
    ArgListPtr args = new ArgList(exprs, new Env());
    codegenInvokeVoid(coreName("copy"), args);
}

void codegenValueAssign(CValuePtr dest, CValuePtr src)
{
    vector<ExprPtr> exprs;
    exprs.push_back(new CValueExpr(dest));
    exprs.push_back(new CValueExpr(src));
    ArgListPtr args = new ArgList(exprs, new Env());
    codegenInvokeVoid(coreName("assign"), args);
}

llvm::Value *codegenToBool(CValuePtr a)
{
    vector<ExprPtr> exprs;
    exprs.push_back(new CValueExpr(a));
    ArgListPtr args = new ArgList(exprs, new Env());
    ObjectPtr callable = primName("boolTruth");
    PValuePtr pv = partialInvoke(callable, args);
    ensureBoolType(pv->type);
    CValuePtr result;
    if (pv->isTemp) {
        result = codegenAllocValue(boolType);
        codegenInvoke(callable, args, result->llval);
    }
    else {
        result = codegenInvoke(callable, args, NULL);
    }
    llvm::Value *b1 = builder->CreateLoad(result->llval);
    llvm::Value *zero = llvm::ConstantInt::get(llvmType(boolType), 0);
    llvm::Value *flag1 = builder->CreateICmpNE(b1, zero);
    return flag1;
}



//
// codegen
//

CValuePtr
codegen(ExprPtr expr, EnvPtr env, llvm::Value *outPtr)
{
    LocationContext loc(expr->location);

    switch (expr->objKind) {

    case BOOL_LITERAL :
    case INT_LITERAL :
    case FLOAT_LITERAL : {
        ValuePtr v = evaluateToStatic(expr, env);
        llvm::Value *val = numericConstant(v);
        builder->CreateStore(val, outPtr);
        return new CValue(v->type, outPtr);
    }

    case CHAR_LITERAL : {
        CharLiteral *x = (CharLiteral *)expr.ptr();
        if (!x->converted)
            x->converted = convertCharLiteral(x->value);
        return codegen(x->converted, env, outPtr);
    }

    case STRING_LITERAL : {
        StringLiteral *x = (StringLiteral *)expr.ptr();
        if (!x->converted)
            x->converted = convertStringLiteral(x->value);
        return codegen(x->converted, env, outPtr);
    }

    case NAME_REF : {
        NameRef *x = (NameRef *)expr.ptr();
        ObjectPtr y = lookupEnv(env, x->name);
        if (y->objKind == VALUE) {
            Value *z = (Value *)y.ptr();
            codegenValue(z, outPtr);
            return new CValue(z->type, outPtr);
        }
        else if (y->objKind == CVALUE) {
            CValue *z = (CValue *)y.ptr();
            assert(outPtr == NULL);
            return z;
        }
        else {
            ValuePtr z = coToValue(y);
            codegenValue(z, outPtr);
            return new CValue(z->type, outPtr);
        }
    }

    case TUPLE : {
        Tuple *x = (Tuple *)expr.ptr();
        if (!x->converted)
            x->converted = convertTuple(x);
        return codegen(x->converted, env, outPtr);
    }

    case ARRAY : {
        Array *x = (Array *)expr.ptr();
        if (!x->converted)
            x->converted = convertArray(x);
        return codegen(x->converted, env, outPtr);
    }

    case INDEXING : {
        Indexing *x = (Indexing *)expr.ptr();
        PValuePtr indexable = partialEval(x->expr, env);
        if (indexable->type == compilerObjectType) {
            error("codegen for type creation not-supported");
        }
        error("invalid indexing operation");
        return NULL;
    }

    case CALL : {
        Call *x = (Call *)expr.ptr();
        PValuePtr callable = partialEval(x->expr, env);
        if ((callable->type == compilerObjectType)
            && callable->isStatic)
        {
            ObjectPtr callable2 = lower(evaluateToStatic(x->expr, env));
            ArgListPtr args = new ArgList(x->args, env);
            return codegenInvoke(callable2, args, outPtr);
        }
        error("invalid call operation");
        return NULL;
    }

    case FIELD_REF : {
        FieldRef *x = (FieldRef *)expr.ptr();
        ValuePtr name = coToValue(x->name.ptr());
        vector<ExprPtr> args;
        args.push_back(x->expr);
        args.push_back(new ValueExpr(name));
        ObjectPtr prim = primName("recordFieldRefByName");
        return codegenInvoke(prim, new ArgList(args, env), outPtr);
    }

    case TUPLE_REF : {
        TupleRef *x = (TupleRef *)expr.ptr();
        ValuePtr index = intToValue(x->index);
        vector<ExprPtr> args;
        args.push_back(x->expr);
        args.push_back(new ValueExpr(index));
        ObjectPtr prim = primName("tupleRef");
        return codegenInvoke(prim, new ArgList(args, env), outPtr);
    }

    case UNARY_OP : {
        UnaryOp *x = (UnaryOp *)expr.ptr();
        if (!x->converted)
            x->converted = convertUnaryOp(x);
        return codegen(x->converted, env, outPtr);
    }

    case BINARY_OP : {
        BinaryOp *x = (BinaryOp *)expr.ptr();
        if (!x->converted)
            x->converted = convertBinaryOp(x);
        return codegen(x->converted, env, outPtr);
    }

    case AND : {
        And *x = (And *)expr.ptr();
        PValuePtr pv1 = partialEval(x->expr1, env);
        PValuePtr pv2 = partialEval(x->expr2, env);
        if (pv1->type != pv2->type)
            error("type mismatch in 'and' expression");
        llvm::BasicBlock *trueBlock =
            llvm::BasicBlock::Create(llvm::getGlobalContext(),
                                     "andTrue1",
                                     llvmFunction);
        llvm::BasicBlock *falseBlock =
            llvm::BasicBlock::Create(llvm::getGlobalContext(),
                                     "andFalse1",
                                     llvmFunction);
        llvm::BasicBlock *mergeBlock =
            llvm::BasicBlock::Create(llvm::getGlobalContext(),
                                     "andMerge",
                                     llvmFunction);
        CValuePtr result;
        if (pv1->isTemp) {
            result = codegen(x->expr1, env, outPtr);
            llvm::Value *flag1 = codegenToBool(result);
            builder->CreateCondBr(flag1, trueBlock, falseBlock);

            builder->SetInsertPoint(trueBlock);
            codegenValueDestroy(result);
            if (pv2->isTemp) {
                result = codegen(x->expr2, env, outPtr);
            }
            else {
                CValuePtr ref2 = codegen(x->expr2, env, NULL);
                codegenValueCopy(result, ref2);
            }
            builder->CreateBr(mergeBlock);

            builder->SetInsertPoint(falseBlock);
            builder->CreateBr(mergeBlock);

            builder->SetInsertPoint(mergeBlock);
        }
        else {
            if (pv2->isTemp) {
                result = new CValue(pv1->type, outPtr);
                CValuePtr ref1 = codegen(x->expr1, env, NULL);
                llvm::Value *flag1 = codegenToBool(ref1);
                builder->CreateCondBr(flag1, trueBlock, falseBlock);

                builder->SetInsertPoint(trueBlock);
                codegen(x->expr2, env, outPtr);
                builder->CreateBr(mergeBlock);

                builder->SetInsertPoint(falseBlock);
                codegenValueCopy(result, ref1);
                builder->CreateBr(mergeBlock);

                builder->SetInsertPoint(mergeBlock);
            }
            else {
                CValuePtr ref1 = codegen(x->expr1, env, NULL);
                llvm::Value *flag1 = codegenToBool(ref1);
                builder->CreateCondBr(flag1, trueBlock, falseBlock);

                builder->SetInsertPoint(trueBlock);
                CValuePtr ref2 = codegen(x->expr2, env, NULL);
                builder->CreateBr(mergeBlock);
                trueBlock = builder->GetInsertBlock();

                builder->SetInsertPoint(falseBlock);
                builder->CreateBr(mergeBlock);

                builder->SetInsertPoint(mergeBlock);
                llvm::PHINode *phi = builder->CreatePHI(llvmType(ref1->type));
                phi->addIncoming(ref1->llval, falseBlock);
                phi->addIncoming(ref2->llval, trueBlock);
                result = new CValue(pv1->type, phi);
            }
        }
        return result;
    }

    case OR : {
        Or *x = (Or *)expr.ptr();
        PValuePtr pv1 = partialEval(x->expr1, env);
        PValuePtr pv2 = partialEval(x->expr2, env);
        if (pv1->type != pv2->type)
            error("type mismatch in 'or' expression");
        llvm::BasicBlock *trueBlock =
            llvm::BasicBlock::Create(llvm::getGlobalContext(),
                                     "orTrue1",
                                     llvmFunction);
        llvm::BasicBlock *falseBlock =
            llvm::BasicBlock::Create(llvm::getGlobalContext(),
                                     "orFalse1",
                                     llvmFunction);
        llvm::BasicBlock *mergeBlock =
            llvm::BasicBlock::Create(llvm::getGlobalContext(),
                                     "orMerge",
                                     llvmFunction);
        CValuePtr result;
        if (pv1->isTemp) {
            result = codegen(x->expr1, env, outPtr);
            llvm::Value *flag1 = codegenToBool(result);
            builder->CreateCondBr(flag1, trueBlock, falseBlock);

            builder->SetInsertPoint(trueBlock);
            builder->CreateBr(mergeBlock);

            builder->SetInsertPoint(falseBlock);
            codegenValueDestroy(result);
            if (pv2->isTemp) {
                result = codegen(x->expr2, env, outPtr);
            }
            else {
                CValuePtr ref2 = codegen(x->expr2, env, NULL);
                codegenValueCopy(result, ref2);
            }
            builder->CreateBr(mergeBlock);

            builder->SetInsertPoint(mergeBlock);
        }
        else {
            if (pv2->isTemp) {
                result = new CValue(pv1->type, outPtr);
                CValuePtr ref1 = codegen(x->expr1, env, NULL);
                llvm::Value *flag1 = codegenToBool(ref1);
                builder->CreateCondBr(flag1, trueBlock, falseBlock);

                builder->SetInsertPoint(trueBlock);
                codegenValueCopy(result, ref1);
                builder->CreateBr(mergeBlock);

                builder->SetInsertPoint(falseBlock);
                codegen(x->expr2, env, outPtr);
                builder->CreateBr(mergeBlock);

                builder->SetInsertPoint(mergeBlock);
            }
            else {
                CValuePtr ref1 = codegen(x->expr1, env, NULL);
                llvm::Value *flag1 = codegenToBool(ref1);
                builder->CreateCondBr(flag1, trueBlock, falseBlock);

                builder->SetInsertPoint(trueBlock);
                builder->CreateBr(mergeBlock);

                builder->SetInsertPoint(falseBlock);
                CValuePtr ref2 = codegen(x->expr2, env, NULL);
                builder->CreateBr(mergeBlock);
                falseBlock = builder->GetInsertBlock();

                builder->SetInsertPoint(mergeBlock);
                const llvm::Type *ptrType = llvmType(pointerType(pv1->type));
                llvm::PHINode *phi = builder->CreatePHI(ptrType);
                phi->addIncoming(ref1->llval, trueBlock);
                phi->addIncoming(ref2->llval, falseBlock);
                result = new CValue(pv1->type, phi);
            }
        }
        return result;
    }

    case SC_EXPR : {
        SCExpr *x = (SCExpr *)expr.ptr();
        return codegen(x->expr, x->env, outPtr);
    }

    case VALUE_EXPR : {
        ValueExpr *x = (ValueExpr *)expr.ptr();
        codegenValue(x->value, outPtr);
        return new CValue(x->value->type, outPtr);
    }

    case CVALUE_EXPR : {
        CValueExpr *x = (CValueExpr *)expr.ptr();
        return x->cvalue;
    }

    default :
        assert(false);

    }

    return NULL;
}



//
// codegenInvokeVoid
//

void
codegenInvokeVoid(ObjectPtr obj, ArgListPtr args)
{
    PValuePtr pv = partialInvoke(obj, args);
    ensureVoidType(pv->type);
    codegenInvoke(obj, args, NULL);
}



//
// codegenInvoke
//

CValuePtr
codegenInvoke(ObjectPtr obj, ArgListPtr args, llvm::Value *outPtr)
{
}



//
// codegenValue
//

void codegenValue(ValuePtr v, llvm::Value *outPtr)
{
    switch (v->type->typeKind) {

    case BOOL_TYPE : {
        const llvm::Type *llt = llvmType(v->type);
        int bv = valueToBool(v) ? 1 : 0;
        llvm::Value *llv = llvm::ConstantInt::get(llt, bv);
        builder->CreateStore(llv, outPtr);
        break;
    }

    case INTEGER_TYPE :
    case FLOAT_TYPE : {
        llvm::Value *llv = numericConstant(v);
        builder->CreateStore(llv, outPtr);
        break;
    }

    case COMPILER_OBJECT_TYPE : {
        const llvm::Type *llt = llvmType(v->type);
        int i = *((int *)v->buf);
        llvm::Value *llv = llvm::ConstantInt::get(llt, i);
        builder->CreateStore(llv, outPtr);
        break;
    }

    default :
        assert(false);
    }
}



//
// numericConstant
//

template <typename T>
llvm::Value *
_sintConstant(ValuePtr v)
{
    return llvm::ConstantInt::getSigned(llvmType(v->type), *((T *)v->buf));
}

template <typename T>
llvm::Value *
_uintConstant(ValuePtr v)
{
    return llvm::ConstantInt::get(llvmType(v->type), *((T *)v->buf));
}

static
llvm::Value *
numericConstant(ValuePtr v) 
{
    llvm::Value *val = NULL;
    switch (v->type->typeKind) {
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
                val = _sintConstant<long>(v);
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
                val = _uintConstant<unsigned long>(v);
                break;
            case 64 :
                val = _uintConstant<unsigned long long>(v);
                break;
            default :
                assert(false);
            }
        }
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
    }
    default :
        assert(false);
    }
    return val;
}
