#include "clay.hpp"
#include "invokeutil2.hpp"



//
// declarations
//

llvm::Function *llvmFunction;
llvm::IRBuilder<> *initBuilder;
llvm::IRBuilder<> *builder;


// value operations codegen

void codegenValueInit(CValuePtr dest);
void codegenValueDestroy(CValuePtr dest);
void codegenValueCopy(CValuePtr dest, CValuePtr src);
void codegenValueAssign(CValuePtr dest, CValuePtr src);
llvm::Value *codegenToBool(CValuePtr a);


// codegen temps

vector<CValuePtr> stackCValues;

int markStack();
void destroyStack(int marker);
void popStack(int marker);
void sweepStack(int marker);

CValuePtr codegenAllocValue(TypePtr t);


// codegen

CValuePtr
codegenRoot(ExprPtr expr, EnvPtr env, llvm::Value *outPtr);


// codegenValue

void codegenValue(ValuePtr v, llvm::Value *outPtr);

static
llvm::Value *
numericConstant(ValuePtr v);


// codegenInvoke

void
codegenInvokeVoid(ObjectPtr obj, ArgListPtr args);

CValuePtr
codegenInvokeRecord(RecordPtr x, ArgListPtr args, llvm::Value *outPtr);

CValuePtr
codegenInvokeType(TypePtr x, ArgListPtr args, llvm::Value *outPtr);

CValuePtr
codegenInvokeProcedure(ProcedurePtr x, ArgListPtr args, llvm::Value *outPtr);

CValuePtr
codegenInvokeOverloadable(OverloadablePtr x,
                          ArgListPtr args,
                          llvm::Value *outPtr);

CValuePtr
codegenInvokeCode(InvokeTableEntryPtr entry,
                  ArgListPtr args,
                  llvm::Value *outPtr);

struct JumpTarget {
    llvm::BasicBlock *block;
    int stackMarker;
    JumpTarget() : block(NULL), stackMarker(-1) {}
    JumpTarget(llvm::BasicBlock *block, int stackMarker)
        : block(block), stackMarker(stackMarker) {}
};

struct CodeContext {
    InvokeTableEntryPtr entry;
    llvm::Value *returnOutPtr;
    int stackMarker;
    map<string, JumpTarget> labels;
    vector<JumpTarget> breaks;
    vector<JumpTarget> continues;
    CodeContext(InvokeTableEntryPtr entry,
                llvm::Value *returnOutPtr,
                int stackMarker)
        : entry(entry), returnOutPtr(returnOutPtr),
          stackMarker(stackMarker) {}
};

void codegenCode(InvokeTableEntryPtr entry, IdentifierPtr name);

void codegenStatement(StatementPtr stmt, EnvPtr env, CodeContext &ctx);

void
codegenCollectLabels(const vector<StatementPtr> &statements,
                     unsigned startIndex,
                     CodeContext &ctx);

EnvPtr codegenBinding(BindingPtr x, EnvPtr env);

CValuePtr
codegenInvokeExternal(ExternalProcedurePtr x,
                      ArgListPtr args,
                      llvm::Value *outPtr);

CValuePtr
codegenInvokePrimOp(PrimOpPtr x, ArgListPtr args, llvm::Value *outPtr);



//
// codegen value ops
//

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
// codegen temps
//

int markStack()
{
    return stackCValues.size();
}

void destroyStack(int marker)
{
    int i = (int)stackCValues.size();
    assert(marker <= i);
    while (marker < i) {
        --i;
        codegenValueDestroy(stackCValues[i]);
    }
}

void popStack(int marker)
{
    assert(marker <= (int)stackCValues.size());
    while (marker < (int)stackCValues.size())
        stackCValues.pop_back();
}

void sweepStack(int marker)
{
    assert(marker <= (int)stackCValues.size());
    while (marker < (int)stackCValues.size()) {
        codegenValueDestroy(stackCValues.back());
        stackCValues.pop_back();
    }
}

CValuePtr codegenAllocValue(TypePtr t)
{
    const llvm::Type *llt = llvmType(t);
    llvm::Value *llval = initBuilder->CreateAlloca(llt);
    CValuePtr cv = new CValue(t, llval);
    stackCValues.push_back(cv);
    return cv;
}



//
// codegen
//

CValuePtr
codegenRoot(ExprPtr expr, EnvPtr env, llvm::Value *outPtr)
{
    pushTempBlock();
    int marker = markStack();
    CValuePtr cv = codegen(expr, env, outPtr);
    sweepStack(marker);
    popTempBlock();
    return cv;
}

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
        PValuePtr indexable = partialEvalRoot(x->expr, env);
        if (indexable->type == compilerObjectType) {
            error("invalid indexing operation");
        }
        error("invalid indexing operation");
        return NULL;
    }

    case CALL : {
        Call *x = (Call *)expr.ptr();
        PValuePtr callable = partialEvalRoot(x->expr, env);
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
        PValuePtr pv1 = partialEvalRoot(x->expr1, env);
        PValuePtr pv2 = partialEvalRoot(x->expr2, env);
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
        PValuePtr pv1 = partialEvalRoot(x->expr1, env);
        PValuePtr pv2 = partialEvalRoot(x->expr2, env);
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
    switch (obj->objKind) {
    case RECORD :
        return codegenInvokeRecord((Record *)obj.ptr(), args, outPtr);
    case TYPE :
        return codegenInvokeType((Type *)obj.ptr(), args, outPtr);
    case PROCEDURE :
        return codegenInvokeProcedure((Procedure *)obj.ptr(), args, outPtr);
    case OVERLOADABLE :
        return codegenInvokeOverloadable((Overloadable *)obj.ptr(),
                                         args,
                                         outPtr);
    case EXTERNAL_PROCEDURE :
        return codegenInvokeExternal((ExternalProcedure *)obj.ptr(),
                                     args,
                                     outPtr);
    case PRIM_OP :
        return codegenInvokePrimOp((PrimOp *)obj.ptr(), args, outPtr);
    }
    error("invalid operation");
    return NULL;
}



//
// codegenInvokeRecord
//

CValuePtr
codegenInvokeRecord(RecordPtr x, ArgListPtr args, llvm::Value *outPtr)
{
    vector<PatternCellPtr> cells;
    EnvPtr env = initPatternVars(x->env, x->patternVars, cells);
    args->ensureUnifyFormalArgs(x->formalArgs, env);
    vector<ValuePtr> cellValues;
    derefCells(cells, cellValues);
    TypePtr t = recordType(x, cellValues);
    args = args->removeStaticArgs(x->formalArgs);
    return codegenInvokeType(t, args, outPtr);
}



//
// codegenInvokeType
//
// support constructors for array types, tuple types, and record types.
// support default constructor, copy constructor for all types
//

CValuePtr
codegenInvokeType(TypePtr x, ArgListPtr args, llvm::Value *outPtr)
{
    CValuePtr result = new CValue(x, outPtr);

    if (args->size() == 0) {
        codegenValueInit(result);
        return result;
    }

    if ((args->size() == 1) && (args->type(0) == x)) {
        if (args->isTemp(0))
            args->codegen(0, outPtr);
        else
            codegenValueCopy(result, args->codegen(0, NULL));
        return result;
    }

    switch (x->typeKind) {

    case ARRAY_TYPE : {
        ArrayType *y = (ArrayType *)x.ptr();
        args->ensureArity(y->size);
        TypePtr etype = y->elementType;
        for (unsigned i = 0; i < args->size(); ++i) {
            ensureSameType(args->type(i), etype);
            llvm::Value *ptr =
                builder->CreateConstGEP2_32(outPtr, 0, i);
            if (args->isTemp(i)) {
                args->codegen(i, ptr);
            }
            else {
                CValuePtr dest = new CValue(etype, ptr);
                CValuePtr src = args->codegen(i, NULL);
                codegenValueCopy(dest, src);
            }
        }
        return result;
    }

    case TUPLE_TYPE : {
        TupleType *y = (TupleType *)x.ptr();
        args->ensureArity(y->elementTypes.size());
        for (unsigned i = 0; i < args->size(); ++i) {
            ensureSameType(args->type(i), y->elementTypes[i]);
            llvm::Value *ptr =
                builder->CreateConstGEP2_32(outPtr, 0, i);
            if (args->isTemp(i)) {
                args->codegen(i, ptr);
            }
            else {
                CValuePtr dest = new CValue(y->elementTypes[i], ptr);
                CValuePtr src = args->codegen(i, NULL);
                codegenValueCopy(dest, src);
            }
        }
        return result;
    }

    case RECORD_TYPE : {
        RecordType *y = (RecordType *)x.ptr();
        const vector<TypePtr> &fieldTypes = recordFieldTypes(y);
        args->ensureArity(fieldTypes.size());
        for (unsigned i = 0; i < args->size(); ++i) {
            ensureSameType(args->type(i), fieldTypes[i]);
            llvm::Value *ptr =
                builder->CreateConstGEP2_32(outPtr, 0, i);
            if (args->isTemp(i)) {
                args->codegen(i, ptr);
            }
            else {
                CValuePtr dest = new CValue(fieldTypes[i], ptr);
                CValuePtr src = args->codegen(i, NULL);
                codegenValueCopy(dest, src);
            }
        }
        return result;
    }

    }

    error("invalid constructor");
    return NULL;
}



//
// codegenInvokeProcedure, codegenInvokeOverloadable
//

CValuePtr
codegenInvokeProcedure(ProcedurePtr x, ArgListPtr args, llvm::Value *outPtr)
{
    InvokeTableEntryPtr entry = lookupProcedureInvoke(x, args);
    assert(entry->env.ptr() && entry->code.ptr());
    assert(entry->returnType.ptr());
    if (!entry->llFunc)
        codegenCode(entry, x->name);
    return codegenInvokeCode(entry, args, outPtr);
}

CValuePtr
codegenInvokeOverloadable(OverloadablePtr x,
                          ArgListPtr args,
                          llvm::Value *outPtr)
{
    InvokeTableEntryPtr entry = lookupOverloadableInvoke(x, args);
    assert(entry->env.ptr() && entry->code.ptr());
    assert(entry->returnType.ptr());
    if (!entry->llFunc)
        codegenCode(entry, x->name);
    return codegenInvokeCode(entry, args, outPtr);
}

CValuePtr
codegenInvokeCode(InvokeTableEntryPtr entry,
                  ArgListPtr args,
                  llvm::Value *outPtr)
{
    args = args->removeStaticArgs(entry->code->formalArgs);
    vector<llvm::Value *> llArgs;
    for (unsigned i = 0; i < args->size(); ++i) {
        CValuePtr carg;
        if (args->isTemp(i)) {
            carg = codegenAllocValue(args->type(i));
            args->codegen(i, carg->llval);
        }
        else {
            carg = args->codegen(i, NULL);
        }
        llArgs.push_back(carg->llval);
    }
    if (entry->returnType == voidType) {
        builder->CreateCall(entry->llFunc, llArgs.begin(), llArgs.end());
        return new CValue(voidType, NULL);
    }
    else if (entry->returnByRef) {
        llvm::Value *result =
            builder->CreateCall(entry->llFunc, llArgs.begin(), llArgs.end());
        return new CValue(entry->returnType, result);
    }
    else {
        llArgs.push_back(outPtr);
        builder->CreateCall(entry->llFunc, llArgs.begin(), llArgs.end());
        return new CValue(entry->returnType, outPtr);
    }
}



//
// codegenCode
//

void codegenCode(InvokeTableEntryPtr entry, IdentifierPtr name)
{
    vector<IdentifierPtr> argNames;
    vector<TypePtr> argTypes;
    vector<const llvm::Type *> llArgTypes;

    for (unsigned i = 0; i < entry->argsInfo.size(); ++i) {
        if (entry->table->isStaticFlags[i])
            continue;

        assert(entry->code->formalArgs[i]->objKind == VALUE_ARG);
        ValueArg *va = (ValueArg *)entry->code->formalArgs[i].ptr();
        argNames.push_back(va->name);

        assert(entry->argsInfo[i]->objKind == TYPE);
        Type *t = (Type *)entry->argsInfo[i].ptr();
        argTypes.push_back(t);

        llArgTypes.push_back(llvmType(pointerType(t)));
    }

    const llvm::Type *llReturnType;
    if (entry->returnType == voidType) {
        llReturnType = llvmType(voidType);
    }
    else if (entry->returnByRef) {
        llReturnType = llvmType(pointerType(entry->returnType));
    }
    else {
        llReturnType = llvmType(voidType);
        llArgTypes.push_back(llvmType(pointerType(entry->returnType)));
    }

    llvm::FunctionType *llFuncType =
        llvm::FunctionType::get(llReturnType, llArgTypes, false);

    llvm::Function *llFunc =
        llvm::Function::Create(llFuncType,
                               llvm::Function::InternalLinkage,
                               "clay_" + name->str,
                               llvmModule);
    entry->llFunc = llFunc;

    EnvPtr env = new Env(entry->env);

    llvm::Function::arg_iterator ai = llFunc->arg_begin();
    for (unsigned i = 0; i < argNames.size(); ++i, ++ai) {
        llvm::Argument *llArgValue = &(*ai);
        llArgValue->setName(argNames[i]->str);
        CValuePtr cvalue = new CValue(argTypes[i], llArgValue);
        addLocal(env, argNames[i], cvalue.ptr());
    }
    llvm::Argument *outPtr = NULL;
    if ((entry->returnType != voidType) && !entry->returnByRef)
        outPtr = &(*ai);

    llvm::BasicBlock *initBlock =
        llvm::BasicBlock::Create(llvm::getGlobalContext(),
                                 "initBlock",
                                 llFunc);
    llvm::BasicBlock *codeBlock =
        llvm::BasicBlock::Create(llvm::getGlobalContext(),
                                 "codeBlock",
                                 llFunc);

    llvm::IRBuilder<> *savedInitBuilder = initBuilder;
    llvm::IRBuilder<> *savedBuilder = builder;

    initBuilder = new llvm::IRBuilder<>(initBlock);
    builder = new llvm::IRBuilder<>(codeBlock);

    CodeContext ctx(entry, outPtr, markStack());

    codegenStatement(entry->code->body, env, ctx);

    builder->CreateRetVoid();

    initBuilder->CreateBr(codeBlock);

    delete initBuilder;
    delete builder;

    initBuilder = savedInitBuilder;
    builder = savedBuilder;
}



//
// codegenStatement
//

void codegenStatement(StatementPtr stmt, EnvPtr env, CodeContext &ctx)
{
    LocationContext loc(stmt->location);

    switch (stmt->objKind) {

    case BLOCK : {
        Block *x = (Block *)stmt.ptr();
        int blockMarker = markStack();
        codegenCollectLabels(x->statements, 0, ctx);
        for (unsigned i = 0; i < x->statements.size(); ++i) {
            StatementPtr y = x->statements[i];
            if (y->objKind == BINDING) {
                env = codegenBinding((Binding *)y.ptr(), env);
                codegenCollectLabels(x->statements, i+1, ctx);
            }
            else if (y->objKind == LABEL) {
                Label *z = (Label *)y.ptr();
                map<string, JumpTarget>::iterator li =
                    ctx.labels.find(z->name->str);
                assert(li != ctx.labels.end());
                const JumpTarget &jt = li->second;
                builder->CreateBr(jt.block);
                builder->SetInsertPoint(jt.block);
            }
            else {
                codegenStatement(y, env, ctx);
            }
        }
        sweepStack(blockMarker);
        break;
    }

    case LABEL :
    case BINDING :
        error("invalid statement");

    case ASSIGNMENT : {
        Assignment *x = (Assignment *)stmt.ptr();
        PValuePtr pvLeft = partialEvalRoot(x->left, env);
        PValuePtr pvRight = partialEvalRoot(x->right, env);
        if (pvLeft->isTemp)
            error(x->left, "cannot assign to a temporary");
        pushTempBlock();
        int marker = markStack();
        CValuePtr cvLeft = codegen(x->left, env, NULL);
        CValuePtr cvRight;
        if (pvRight->isTemp) {
            cvRight = codegenAllocValue(pvRight->type);
            codegen(x->right, env, cvRight->llval);
        }
        else {
            cvRight = codegen(x->right, env, NULL);
        }
        codegenValueAssign(cvLeft, cvRight);
        sweepStack(marker);
        popTempBlock();
        break;
    }

    case GOTO : {
        Goto *x = (Goto *)stmt.ptr();
        map<string, JumpTarget>::iterator li =
            ctx.labels.find(x->labelName->str);
        if (li == ctx.labels.end())
            error("goto label not found");
        const JumpTarget &jt = li->second;
        destroyStack(jt.stackMarker);
        builder->CreateBr(jt.block);
        break;
    }

    case RETURN : {
        Return *x = (Return *)stmt.ptr();
        TypePtr rt = ctx.entry->returnType;
        bool byRef = ctx.entry->returnByRef;
        if (rt == voidType) {
            if (x->expr.ptr())
                error("void return expected");
            destroyStack(ctx.stackMarker);
            builder->CreateRetVoid();
        }
        else {
            if (!x->expr)
                error("non-void return expected");
            if (byRef)
                error("return by reference expected");
            PValuePtr pv = partialEvalRoot(x->expr, env);
            if (pv->type != rt)
                error("type mismatch in return");
            if (pv->isTemp) {
                codegenRoot(x->expr, env, ctx.returnOutPtr);
            }
            else {
                pushTempBlock();
                int marker = markStack();
                CValuePtr ref = codegen(x->expr, env, NULL);
                CValuePtr retV =
                    new CValue(ctx.entry->returnType, ctx.returnOutPtr);
                codegenValueCopy(retV, ref);
                sweepStack(marker);
                popTempBlock();
            }
            destroyStack(ctx.stackMarker);
            builder->CreateRetVoid();
        }
        break;
    }

    case RETURN_REF : {
        ReturnRef *x = (ReturnRef *)stmt.ptr();
        TypePtr rt = ctx.entry->returnType;
        bool byRef = ctx.entry->returnByRef;
        if (rt == voidType)
            error("void return expected");
        if (!byRef)
            error("return by value expected");
        PValuePtr pv = partialEvalRoot(x->expr, env);
        if (pv->type != rt)
            error("type mismatch in returnref");
        if (pv->isTemp)
            error("cannot return a temporary by reference");
        CValuePtr ref = codegenRoot(x->expr, env, NULL);
        destroyStack(ctx.stackMarker);
        builder->CreateRet(ref->llval);
        break;
    }

    case IF : {
        If *x = (If *)stmt.ptr();
        PValuePtr pv = partialEvalRoot(x->condition, env);
        pushTempBlock();
        int marker = markStack();
        CValuePtr cv;
        if (pv->isTemp) {
            cv = codegenAllocValue(pv->type);
            codegen(x->condition, env, cv->llval);
        }
        else {
            cv = codegen(x->condition, env, NULL);
        }
        llvm::Value *cond = codegenToBool(cv);
        sweepStack(marker);
        popTempBlock();

        llvm::BasicBlock *trueBlock =
            llvm::BasicBlock::Create(llvm::getGlobalContext(),
                                     "ifTrue",
                                     llvmFunction);
        llvm::BasicBlock *falseBlock =
            llvm::BasicBlock::Create(llvm::getGlobalContext(),
                                     "ifTrue",
                                     llvmFunction);
        llvm::BasicBlock *mergeBlock =
            llvm::BasicBlock::Create(llvm::getGlobalContext(),
                                     "ifTrue",
                                     llvmFunction);

        builder->CreateCondBr(cond, trueBlock, falseBlock);

        builder->SetInsertPoint(trueBlock);
        codegenStatement(x->thenPart, env, ctx);
        builder->CreateBr(mergeBlock);

        builder->SetInsertPoint(falseBlock);
        if (x->elsePart.ptr())
            codegenStatement(x->elsePart, env, ctx);
        builder->CreateBr(mergeBlock);

        builder->SetInsertPoint(mergeBlock);

        break;
    }

    case EXPR_STATEMENT : {
        ExprStatement *x = (ExprStatement *)stmt.ptr();
        PValuePtr pv = partialEvalRoot(x->expr, env);
        pushTempBlock();
        int marker = markStack();
        llvm::Value *outPtr = NULL;
        if ((pv->type != voidType) && (pv->isTemp))
            outPtr = codegenAllocValue(pv->type)->llval;
        codegen(x->expr, env, outPtr);
        sweepStack(marker);
        popTempBlock();
        break;
    }

    case WHILE : {
        While *x = (While *)stmt.ptr();

        llvm::BasicBlock *whileBegin =
            llvm::BasicBlock::Create(llvm::getGlobalContext(),
                                     "whileBegin",
                                     llvmFunction);
        llvm::BasicBlock *whileBody =
            llvm::BasicBlock::Create(llvm::getGlobalContext(),
                                     "whileBody",
                                     llvmFunction);
        llvm::BasicBlock *whileEnd =
            llvm::BasicBlock::Create(llvm::getGlobalContext(),
                                     "whileEnd",
                                     llvmFunction);

        builder->CreateBr(whileBegin);
        builder->SetInsertPoint(whileBegin);

        PValuePtr pv = partialEvalRoot(x->condition, env);
        pushTempBlock();
        int marker = markStack();
        CValuePtr cv;
        if (pv->isTemp) {
            cv = codegenAllocValue(pv->type);
            codegen(x->condition, env, cv->llval);
        }
        else {
            cv = codegen(x->condition, env, NULL);
        }
        llvm::Value *cond = codegenToBool(cv);
        sweepStack(marker);
        popTempBlock();

        builder->CreateCondBr(cond, whileBody, whileEnd);

        ctx.breaks.push_back(JumpTarget(whileEnd, markStack()));
        ctx.continues.push_back(JumpTarget(whileBegin, markStack()));
        builder->SetInsertPoint(whileBody);
        codegenStatement(x->body, env, ctx);
        builder->CreateBr(whileBegin);
        ctx.breaks.pop_back();
        ctx.continues.pop_back();

        builder->SetInsertPoint(whileEnd);
        break;
    }

    case BREAK : {
        if (ctx.breaks.empty())
            error("invalid break statement");
        const JumpTarget &jt = ctx.breaks.back();
        destroyStack(jt.stackMarker);
        builder->CreateBr(jt.block);
        break;
    }

    case CONTINUE : {
        if (ctx.continues.empty())
            error("invalid continue statement");
        const JumpTarget &jt = ctx.breaks.back();
        destroyStack(jt.stackMarker);
        builder->CreateBr(jt.block);
        break;
    }

    case FOR : {
        For *x = (For *)stmt.ptr();
        if (!x->converted)
            x->converted = convertForStatement(x);
        codegenStatement(x->converted, env, ctx);
        break;
    }

    default :
        assert(false);

    }
}

void
codegenCollectLabels(const vector<StatementPtr> &statements,
                     unsigned startIndex,
                     CodeContext &ctx)
{
    for (unsigned i = 0; i < statements.size(); ++i) {
        StatementPtr x = statements[i];

        switch (x->objKind) {

        case LABEL : {
            Label *y = (Label *)x.ptr();
            map<string, JumpTarget>::iterator li =
                ctx.labels.find(y->name->str);
            if (li != ctx.labels.end())
                error(x, "label redefined");
            llvm::BasicBlock *bb =
                llvm::BasicBlock::Create(llvm::getGlobalContext(),
                                         y->name->str,
                                         llvmFunction);
            ctx.labels[y->name->str] = JumpTarget(bb, markStack());
            break;
        }

        case BINDING :
            return;

        }
    }
}

EnvPtr codegenBinding(BindingPtr x, EnvPtr env)
{
    EnvPtr env2 = new Env(env);

    switch (x->bindingKind) {

    case VAR : {
        PValuePtr pv = partialEvalRoot(x->expr, env);
        CValuePtr cv = codegenAllocValue(pv->type);
        if (pv->isTemp) {
            codegenRoot(x->expr, env, cv->llval);
        }
        else {
            pushTempBlock();
            int marker = markStack();
            CValuePtr ref = codegen(x->expr, env, NULL);
            codegenValueCopy(cv, ref);
            sweepStack(marker);
            popTempBlock();
        }
        addLocal(env2, x->name, cv.ptr());
        break;
    }

    case REF : {
        PValuePtr pv = partialEvalRoot(x->expr, env);
        CValuePtr cv;
        if (pv->isTemp) {
            cv = codegenAllocValue(pv->type);
            codegenRoot(x->expr, env, cv->llval);
        }
        else {
            cv = codegenRoot(x->expr, env, NULL);
        }
        addLocal(env2, x->name, cv.ptr());
        break;
    }

    case STATIC : {
        ValuePtr v = evaluateToStatic(x->expr, env);
        addLocal(env2, x->name, v.ptr());
        break;
    }

    default :
        assert(false);
    }

    return env2;
}



//
// codegenInvokeExternal
//

CValuePtr
codegenInvokeExternal(ExternalProcedurePtr x,
                      ArgListPtr args,
                      llvm::Value *outPtr)
{
    if (!x->llvmFunc)
        initExternalProcedure(x);
    args->ensureArity(x->args.size());
    vector<llvm::Value *> llArgs;
    for (unsigned i = 0; i < args->size(); ++i) {
        CValuePtr carg;
        if (args->isTemp(i)) {
            carg = codegenAllocValue(args->type(i));
            args->codegen(i, carg->llval);
        }
        else {
            carg = args->codegen(i, NULL);
        }
        if (carg->type != x->args[i]->type2)
            error(args->exprs[i], "argument type mismatch");
        llArgs.push_back(carg->llval);
    }
    if (x->returnType2 != voidType) {
        assert(outPtr != NULL);
        llArgs.push_back(outPtr);
    }
    else {
        assert(outPtr == NULL);
    }
    builder->CreateCall(x->llvmFunc, llArgs.begin(), llArgs.end());

    return new CValue(x->returnType2, outPtr);
}
