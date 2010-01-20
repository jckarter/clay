#include "clay.hpp"
#include "invokeutil2.hpp"



//
// declarations
//

llvm::Function *llvmFunction;
llvm::IRBuilder<> *initBuilder;
llvm::IRBuilder<> *builder;


// llvm util
llvm::BasicBlock *newBasicBlock(const char *name);

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


// codegenAsRef, codegenAsValue

CValuePtr
codegenAsRef(ExprPtr expr, EnvPtr env, PValuePtr pv);

CValuePtr
codegenAsValue(ExprPtr expr, EnvPtr env, PValuePtr pv, llvm::Value *outPtr);


// codegen

CValuePtr
codegenRoot(ExprPtr expr, EnvPtr env, llvm::Value *outPtr);

// codegenValue

CValuePtr
codegenValue(ValuePtr v, llvm::Value *outPtr);

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
// llvm util
//

llvm::BasicBlock *newBasicBlock(const char *name)
{
    return llvm::BasicBlock::Create(llvm::getGlobalContext(),
                                    name,
                                    llvmFunction);
}



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
// codegenAsRef, codegenAsValue
//

CValuePtr
codegenAsRef(ExprPtr expr, EnvPtr env, PValuePtr pv)
{
    CValuePtr result;
    if (pv->isTemp) {
        result = codegenAllocValue(pv->type);
        codegen(expr, env, result->llval);
    }
    else {
        result = codegen(expr, env, NULL);
    }
    return result;
}

CValuePtr
codegenAsValue(ExprPtr expr, EnvPtr env, PValuePtr pv, llvm::Value *outPtr)
{
    CValuePtr result = new CValue(pv->type, outPtr);
    if (pv->isTemp) {
        codegen(expr, env, outPtr);
    }
    else {
        CValuePtr ref = codegen(expr, env, NULL);
        codegenValueCopy(result, ref);
    }
    return result;
}



//
// ArgList codegen methods
//

CValuePtr ArgList::codegen(int i, llvm::Value *outPtr)
{
    return ::codegen(this->exprs[i], this->env, outPtr);
}

CValuePtr ArgList::codegenAsRef(int i)
{
    return ::codegenAsRef(this->exprs[i], this->env, this->pvalue(i));
}

CValuePtr ArgList::codegenAsValue(int i, llvm::Value *outPtr)
{
    return ::codegenAsValue(this->exprs[i], this->env,
                            this->pvalue(i), outPtr);
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
            return codegenValue(z, outPtr);
        }
        else if (y->objKind == CVALUE) {
            CValue *z = (CValue *)y.ptr();
            assert(outPtr == NULL);
            return z;
        }
        else {
            ValuePtr z = coToValue(y);
            return codegenValue(z, outPtr);
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
        if (callable->type == compilerObjectType) {
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
        llvm::BasicBlock *trueBlock = newBasicBlock("andTrue1");
        llvm::BasicBlock *falseBlock = newBasicBlock("andFalse1");
        llvm::BasicBlock *mergeBlock = newBasicBlock("andMerge");
        CValuePtr result;
        if (pv1->isTemp) {
            result = codegen(x->expr1, env, outPtr);
            llvm::Value *flag1 = codegenToBool(result);
            builder->CreateCondBr(flag1, trueBlock, falseBlock);

            builder->SetInsertPoint(trueBlock);
            codegenValueDestroy(result);
            result = codegenAsValue(x->expr2, env, pv2, outPtr);
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
        llvm::BasicBlock *trueBlock = newBasicBlock("orTrue1");
        llvm::BasicBlock *falseBlock = newBasicBlock("orFalse1");
        llvm::BasicBlock *mergeBlock = newBasicBlock("orMerge");
        CValuePtr result;
        if (pv1->isTemp) {
            result = codegen(x->expr1, env, outPtr);
            llvm::Value *flag1 = codegenToBool(result);
            builder->CreateCondBr(flag1, trueBlock, falseBlock);

            builder->SetInsertPoint(trueBlock);
            builder->CreateBr(mergeBlock);

            builder->SetInsertPoint(falseBlock);
            codegenValueDestroy(result);
            result = codegenAsValue(x->expr2, env, pv2, outPtr);
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
        return codegenValue(x->value, outPtr);
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

CValuePtr codegenValue(ValuePtr v, llvm::Value *outPtr)
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

    return new CValue(v->type, outPtr);
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
        args->codegenAsValue(0, outPtr);
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
            args->codegenAsValue(i, ptr);
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
            args->codegenAsValue(i, ptr);
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
            args->codegenAsValue(i, ptr);
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
        CValuePtr carg = args->codegenAsRef(i);
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

    llvm::Function *savedLLVMFunction = llvmFunction;
    llvm::IRBuilder<> *savedInitBuilder = initBuilder;
    llvm::IRBuilder<> *savedBuilder = builder;

    llvmFunction = llFunc;

    llvm::BasicBlock *initBlock = newBasicBlock("initBlock");
    llvm::BasicBlock *codeBlock = newBasicBlock("codeBlock");

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
    llvmFunction = savedLLVMFunction;
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
        CValuePtr cvRight = codegenAsRef(x->right, env, pvRight);
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

            pushTempBlock();
            int marker = markStack();
            codegenAsValue(x->expr, env, pv, ctx.returnOutPtr);
            sweepStack(marker);
            popTempBlock();

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
        CValuePtr cv = codegenAsRef(x->condition, env, pv);
        llvm::Value *cond = codegenToBool(cv);
        sweepStack(marker);
        popTempBlock();

        llvm::BasicBlock *trueBlock = newBasicBlock("ifTrue");
        llvm::BasicBlock *falseBlock = newBasicBlock("ifFalse");
        llvm::BasicBlock *mergeBlock = newBasicBlock("ifMerge");

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

        llvm::BasicBlock *whileBegin = newBasicBlock("whileBegin");
        llvm::BasicBlock *whileBody = newBasicBlock("whileBody");
        llvm::BasicBlock *whileEnd = newBasicBlock("whileEnd");

        builder->CreateBr(whileBegin);
        builder->SetInsertPoint(whileBegin);

        PValuePtr pv = partialEvalRoot(x->condition, env);
        pushTempBlock();
        int marker = markStack();
        CValuePtr cv = codegenAsRef(x->condition, env, pv);
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
            llvm::BasicBlock *bb = newBasicBlock(y->name->str.c_str());
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

        pushTempBlock();
        int marker = markStack();
        codegenAsValue(x->expr, env, pv, cv->llval);
        sweepStack(marker);
        popTempBlock();

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
        CValuePtr carg = args->codegenAsRef(i);
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



//
// codegenPrimOp
//

ValuePtr evaluatePrimOp(PrimOpPtr x, ArgListPtr args)
{
    vector<ValuePtr> argValues;
    for (unsigned i = 0; i < args->size(); ++i)
        argValues.push_back(args->value(i));
    return invoke(x.ptr(), argValues);
}

CValuePtr
codegenInvokePrimOp(PrimOpPtr x, ArgListPtr args, llvm::Value *outPtr)
{
    switch(x->primOpCode) {

    case PRIM_TypeP : {
        return codegenValue(evaluatePrimOp(x, args), outPtr);
    }

    case PRIM_TypeSize : {
        return codegenValue(evaluatePrimOp(x, args), outPtr);
    }

    case PRIM_primitiveInit : {
        args->ensureArity(1);
        return new CValue(voidType, NULL);
    }

    case PRIM_primitiveDestroy : {
        args->ensureArity(1);
        return new CValue(voidType, NULL);
    }

    case PRIM_primitiveCopy :
    case PRIM_primitiveAssign :
    {
        args->ensureArity(2);
        ensurePrimitiveType(args->type(0));
        ensureSameType(args->type(0), args->type(1));
        CValuePtr dest = args->codegenAsRef(0);
        CValuePtr src = args->codegenAsRef(1);
        llvm::Value *v = builder->CreateLoad(src->llval);
        builder->CreateStore(v, dest->llval);
        return new CValue(voidType, NULL);
    }

    case PRIM_boolNot : {
        args->ensureArity(1);
        ensureBoolType(args->type(0));
        CValuePtr cv = args->codegenAsRef(0);
        llvm::Value *v = builder->CreateLoad(cv->llval);
        llvm::Value *zero = llvm::ConstantInt::get(llvmType(boolType), 0);
        llvm::Value *flag = builder->CreateICmpEQ(v, zero);
        llvm::Value *result = builder->CreateZExt(flag, llvmType(boolType));
        builder->CreateStore(result, outPtr);
        return new CValue(boolType, outPtr);
    }

    case PRIM_boolTruth : {
        args->ensureArity(1);
        ensureBoolType(args->type(0));
        CValuePtr cv = args->codegenAsRef(0);
        llvm::Value *v = builder->CreateLoad(cv->llval);
        llvm::Value *zero = llvm::ConstantInt::get(llvmType(boolType), 0);
        llvm::Value *flag = builder->CreateICmpNE(v, zero);
        llvm::Value *result = builder->CreateZExt(flag, llvmType(boolType));
        builder->CreateStore(result, outPtr);
        return new CValue(boolType, outPtr);
    }

    case PRIM_numericEqualsP : {
        args->ensureArity(2);
        ensureNumericType(args->type(0));
        ensureSameType(args->type(0), args->type(1));
        CValuePtr a = args->codegenAsRef(0);
        CValuePtr b = args->codegenAsRef(1);
        llvm::Value *v1 = builder->CreateLoad(a->llval);
        llvm::Value *v2 = builder->CreateLoad(b->llval);
        llvm::Value *flag;
        switch (args->type(0)->typeKind) {
        case INTEGER_TYPE :
            flag = builder->CreateICmpEQ(v1, v2);
            break;
        case FLOAT_TYPE :
            flag = builder->CreateFCmpUEQ(v1, v2);
            break;
        default :
            assert(false);
        }
        llvm::Value *result = builder->CreateZExt(flag, llvmType(boolType));
        builder->CreateStore(result, outPtr);
        return new CValue(boolType, outPtr);
    }

    case PRIM_numericLesserP : {
        args->ensureArity(2);
        ensureNumericType(args->type(0));
        ensureSameType(args->type(0), args->type(1));
        CValuePtr a = args->codegenAsRef(0);
        CValuePtr b = args->codegenAsRef(1);
        llvm::Value *v1 = builder->CreateLoad(a->llval);
        llvm::Value *v2 = builder->CreateLoad(b->llval);
        llvm::Value *flag;
        switch (args->type(0)->typeKind) {
        case INTEGER_TYPE : {
            IntegerType *t = (IntegerType *)args->type(0).ptr();
            if (t->isSigned)
                flag = builder->CreateICmpSLT(v1, v2);
            else
                flag = builder->CreateICmpULT(v1, v2);
            break;
        }
        case FLOAT_TYPE :
            flag = builder->CreateFCmpUEQ(v1, v2);
            break;
        default :
            assert(false);
        }
        llvm::Value *result = builder->CreateZExt(flag, llvmType(boolType));
        builder->CreateStore(result, outPtr);
        return new CValue(boolType, outPtr);
    }

    case PRIM_numericAdd : {
        args->ensureArity(2);
        ensureNumericType(args->type(0));
        ensureSameType(args->type(0), args->type(1));
        CValuePtr a = args->codegenAsRef(0);
        CValuePtr b = args->codegenAsRef(1);
        llvm::Value *v1 = builder->CreateLoad(a->llval);
        llvm::Value *v2 = builder->CreateLoad(b->llval);
        llvm::Value *result;
        switch (args->type(0)->typeKind) {
        case INTEGER_TYPE :
            result = builder->CreateAdd(v1, v2);
            break;
        case FLOAT_TYPE :
            result = builder->CreateFAdd(v1, v2);
            break;
        default :
            assert(false);
        }
        builder->CreateStore(result, outPtr);
        return new CValue(args->type(0), outPtr);
    }

    case PRIM_numericSubtract : {
        args->ensureArity(2);
        ensureNumericType(args->type(0));
        ensureSameType(args->type(0), args->type(1));
        CValuePtr a = args->codegenAsRef(0);
        CValuePtr b = args->codegenAsRef(1);
        llvm::Value *v1 = builder->CreateLoad(a->llval);
        llvm::Value *v2 = builder->CreateLoad(b->llval);
        llvm::Value *result;
        switch (args->type(0)->typeKind) {
        case INTEGER_TYPE :
            result = builder->CreateSub(v1, v2);
            break;
        case FLOAT_TYPE :
            result = builder->CreateFSub(v1, v2);
            break;
        default :
            assert(false);
        }
        builder->CreateStore(result, outPtr);
        return new CValue(args->type(0), outPtr);
    }

    case PRIM_numericMultiply : {
        args->ensureArity(2);
        ensureNumericType(args->type(0));
        ensureSameType(args->type(0), args->type(1));
        CValuePtr a = args->codegenAsRef(0);
        CValuePtr b = args->codegenAsRef(1);
        llvm::Value *v1 = builder->CreateLoad(a->llval);
        llvm::Value *v2 = builder->CreateLoad(b->llval);
        llvm::Value *result;
        switch (args->type(0)->typeKind) {
        case INTEGER_TYPE :
            result = builder->CreateMul(v1, v2);
            break;
        case FLOAT_TYPE :
            result = builder->CreateFMul(v1, v2);
            break;
        default :
            assert(false);
        }
        builder->CreateStore(result, outPtr);
        return new CValue(args->type(0), outPtr);
    }

    case PRIM_numericDivide : {
        args->ensureArity(2);
        ensureNumericType(args->type(0));
        ensureSameType(args->type(0), args->type(1));
        CValuePtr a = args->codegenAsRef(0);
        CValuePtr b = args->codegenAsRef(1);
        llvm::Value *v1 = builder->CreateLoad(a->llval);
        llvm::Value *v2 = builder->CreateLoad(b->llval);
        llvm::Value *result;
        switch (args->type(0)->typeKind) {
        case INTEGER_TYPE : {
            IntegerType *t = (IntegerType *)args->type(0).ptr();
            if (t->isSigned)
                result = builder->CreateSDiv(v1, v2);
            else
                result = builder->CreateUDiv(v1, v2);
            break;
        }
        case FLOAT_TYPE :
            result = builder->CreateFDiv(v1, v2);
            break;
        default :
            assert(false);
        }
        builder->CreateStore(result, outPtr);
        return new CValue(args->type(0), outPtr);
    }

    case PRIM_numericNegate : {
        args->ensureArity(1);
        ensureNumericType(args->type(0));
        CValuePtr a = args->codegenAsRef(0);
        llvm::Value *v = builder->CreateLoad(a->llval);
        llvm::Value *result;
        switch (args->type(0)->typeKind) {
        case INTEGER_TYPE :
            result = builder->CreateNeg(v);
            break;
        case FLOAT_TYPE :
            result = builder->CreateFNeg(v);
            break;
        default :
            assert(false);
        }
        builder->CreateStore(result, outPtr);
        return new CValue(args->type(0), outPtr);
    }

    case PRIM_integerRemainder : {
        args->ensureArity(2);
        ensureIntegerType(args->type(0));
        ensureSameType(args->type(0), args->type(1));
        CValuePtr a = args->codegenAsRef(0);
        CValuePtr b = args->codegenAsRef(1);
        llvm::Value *v1 = builder->CreateLoad(a->llval);
        llvm::Value *v2 = builder->CreateLoad(b->llval);
        llvm::Value *result;
        IntegerType *t = (IntegerType *)args->type(0).ptr();
        if (t->isSigned)
            result = builder->CreateSRem(v1, v2);
        else
            result = builder->CreateURem(v1, v2);
        builder->CreateStore(result, outPtr);
        return new CValue(args->type(0), outPtr);
    }

    case PRIM_integerShiftLeft : {
        args->ensureArity(2);
        ensureIntegerType(args->type(0));
        ensureSameType(args->type(0), args->type(1));
        CValuePtr a = args->codegenAsRef(0);
        CValuePtr b = args->codegenAsRef(1);
        llvm::Value *v1 = builder->CreateLoad(a->llval);
        llvm::Value *v2 = builder->CreateLoad(b->llval);
        llvm::Value *result = builder->CreateShl(v1, v2);
        builder->CreateStore(result, outPtr);
        return new CValue(args->type(0), outPtr);
    }

    case PRIM_integerShiftRight : {
        args->ensureArity(2);
        ensureIntegerType(args->type(0));
        ensureSameType(args->type(0), args->type(1));
        CValuePtr a = args->codegenAsRef(0);
        CValuePtr b = args->codegenAsRef(1);
        llvm::Value *v1 = builder->CreateLoad(a->llval);
        llvm::Value *v2 = builder->CreateLoad(b->llval);
        llvm::Value *result;
        IntegerType *t = (IntegerType *)args->type(0).ptr();
        if (t->isSigned)
            result = builder->CreateAShr(v1, v2);
        else
            result = builder->CreateLShr(v1, v2);
        builder->CreateStore(result, outPtr);
        return new CValue(args->type(0), outPtr);
    }

    case PRIM_integerBitwiseAnd : {
        args->ensureArity(2);
        ensureIntegerType(args->type(0));
        ensureSameType(args->type(0), args->type(1));
        CValuePtr a = args->codegenAsRef(0);
        CValuePtr b = args->codegenAsRef(1);
        llvm::Value *v1 = builder->CreateLoad(a->llval);
        llvm::Value *v2 = builder->CreateLoad(b->llval);
        llvm::Value *result = builder->CreateAnd(v1, v2);
        builder->CreateStore(result, outPtr);
        return new CValue(args->type(0), outPtr);
    }

    case PRIM_integerBitwiseOr : {
        args->ensureArity(2);
        ensureIntegerType(args->type(0));
        ensureSameType(args->type(0), args->type(1));
        CValuePtr a = args->codegenAsRef(0);
        CValuePtr b = args->codegenAsRef(1);
        llvm::Value *v1 = builder->CreateLoad(a->llval);
        llvm::Value *v2 = builder->CreateLoad(b->llval);
        llvm::Value *result = builder->CreateOr(v1, v2);
        builder->CreateStore(result, outPtr);
        return new CValue(args->type(0), outPtr);
    }

    case PRIM_integerBitwiseXor : {
        args->ensureArity(2);
        ensureIntegerType(args->type(0));
        ensureSameType(args->type(0), args->type(1));
        CValuePtr a = args->codegenAsRef(0);
        CValuePtr b = args->codegenAsRef(1);
        llvm::Value *v1 = builder->CreateLoad(a->llval);
        llvm::Value *v2 = builder->CreateLoad(b->llval);
        llvm::Value *result = builder->CreateXor(v1, v2);
        builder->CreateStore(result, outPtr);
        return new CValue(args->type(0), outPtr);
    }

    case PRIM_integerBitwiseNot : {
        args->ensureArity(1);
        ensureIntegerType(args->type(0));
        CValuePtr a = args->codegenAsRef(0);
        llvm::Value *v = builder->CreateLoad(a->llval);
        llvm::Value *result = builder->CreateNot(v);
        builder->CreateStore(result, outPtr);
        return new CValue(args->type(0), outPtr);
    }

    case PRIM_numericConvert : {
        args->ensureArity(2);
        TypePtr dest = args->typeValue(0);
        ensureNumericType(dest);
        ensureNumericType(args->type(1));
        CValuePtr a = args->codegenAsRef(1);
        llvm::Value *v = builder->CreateLoad(a->llval);
        TypePtr src = args->type(1);
        llvm::Value *result;
        if (dest == src) {
            result = v;
        }
        else if (dest->typeKind == INTEGER_TYPE) {
            IntegerType *dest2 = (IntegerType *)dest.ptr();
            if (src->typeKind == INTEGER_TYPE) {
                IntegerType *src2 = (IntegerType *)src.ptr();
                if (dest2->bits < src2->bits)
                    result = builder->CreateTrunc(v, llvmType(dest));
                else if (src2->isSigned)
                    result = builder->CreateSExt(v, llvmType(dest));
                else
                    result = builder->CreateZExt(v, llvmType(dest));
            }
            else if (src->typeKind == FLOAT_TYPE) {
                if (dest2->isSigned)
                    result = builder->CreateFPToSI(v, llvmType(dest));
                else
                    result = builder->CreateFPToUI(v, llvmType(dest));
            }
            else {
                assert(false);
            }
        }
        else if (dest->typeKind == FLOAT_TYPE) {
            FloatType *dest2 = (FloatType *)dest.ptr();
            if (src->typeKind == INTEGER_TYPE) {
                IntegerType *src2 = (IntegerType *)src.ptr();
                if (src2->isSigned)
                    result = builder->CreateSIToFP(v, llvmType(dest));
                else
                    result = builder->CreateUIToFP(v, llvmType(dest));
            }
            else if (src->typeKind == FLOAT_TYPE) {
                FloatType *src2 = (FloatType *)src.ptr();
                if (dest2->bits < src2->bits)
                    result = builder->CreateFPTrunc(v, llvmType(dest));
                else
                    result = builder->CreateFPExt(v, llvmType(dest));
            }
            else {
                assert(false);
            }
        }
        else {
            assert(false);
        }
        builder->CreateStore(result, outPtr);
        return new CValue(dest, outPtr);
    }

    case PRIM_Pointer : {
        error("Pointer type constructor cannot be invoked");
    }

    case PRIM_addressOf : {
        args->ensureArity(1);
        if (args->isTemp(0))
            error("Cannot take address of a temporary");
        CValuePtr a = args->codegenAsRef(0);
        builder->CreateStore(a->llval, outPtr);
        return new CValue(pointerType(a->type), outPtr);
    }

    case PRIM_pointerDereference : {
        args->ensureArity(1);
        ensurePointerType(args->type(0));
        CValuePtr a = args->codegenAsRef(0);
        llvm::Value *v = builder->CreateLoad(a->llval);
        PointerType *pt = (PointerType *)a->type.ptr();
        return new CValue(pt->pointeeType, v);
    }

    case PRIM_pointerToInt : {
        args->ensureArity(2);
        TypePtr dest = args->typeValue(0);
        ensureIntegerType(dest);
        ensurePointerType(args->type(1));
        CValuePtr a = args->codegenAsRef(1);
        llvm::Value *v = builder->CreateLoad(a->llval);
        llvm::Value *result = builder->CreatePtrToInt(v, llvmType(dest));
        builder->CreateStore(result, outPtr);
        return new CValue(dest, outPtr);
    }

    case PRIM_intToPointer : {
        args->ensureArity(2);
        TypePtr dest = pointerType(args->typeValue(0));
        ensureIntegerType(args->type(1));
        CValuePtr a = args->codegenAsRef(1);
        llvm::Value *v = builder->CreateLoad(a->llval);
        llvm::Value *result = builder->CreateIntToPtr(v, llvmType(dest));
        builder->CreateStore(result, outPtr);
        return new CValue(dest, outPtr);
    }

    case PRIM_pointerCast : {
        args->ensureArity(2);
        TypePtr dest = pointerType(args->typeValue(0));
        ensurePointerType(args->type(1));
        CValuePtr a = args->codegenAsRef(1);
        llvm::Value *v = builder->CreateLoad(a->llval);
        llvm::Value *result = builder->CreateBitCast(v, llvmType(dest));
        builder->CreateStore(result, outPtr);
        return new CValue(dest, outPtr);
    }

    case PRIM_allocateMemory : {
        args->ensureArity(2);
        TypePtr etype = args->typeValue(0);
        ensureIntegerType(args->type(1));
        CValuePtr a = args->codegenAsRef(1);
        llvm::Value *v = builder->CreateLoad(a->llval);
        llvm::Value *result = builder->CreateMalloc(llvmType(etype), v);
        builder->CreateStore(result, outPtr);
        return new CValue(pointerType(etype), outPtr);
    }

    case PRIM_freeMemory : {
        args->ensureArity(1);
        ensurePointerType(args->type(0));
        CValuePtr a = args->codegenAsRef(0);
        llvm::Value *v = builder->CreateLoad(a->llval);
        builder->CreateFree(v);
        return new CValue(voidType, NULL);
    }

    case PRIM_Array : {
        error("Array type constructor cannot be invoked");
    }

    case PRIM_array : {
        if (args->size() == 0)
            error("atleast one argument required for creating an array");
        TypePtr etype = args->type(0);
        int n = (int)args->size();
        for (unsigned i = 0; i < args->size(); ++i) {
            ensureSameType(args->type(i), etype);
            llvm::Value *ptr = builder->CreateConstGEP2_32(outPtr, 0, i);
            args->codegenAsValue(i, ptr);
        }
        return new CValue(arrayType(etype, n), outPtr);
    }

    case PRIM_arrayRef : {
        args->ensureArity(2);
        ensureArrayType(args->type(0));
        ensureIntegerType(args->type(1));
        CValuePtr a = args->codegenAsRef(0);
        CValuePtr b = args->codegenAsRef(1);
        llvm::Value *i = builder->CreateLoad(b->llval);
        vector<llvm::Value *> indices;
        indices.push_back(llvm::ConstantInt::get(llvmType(int32Type), 0));
        indices.push_back(i);
        llvm::Value *ptr =
            builder->CreateGEP(a->llval, indices.begin(), indices.end());
        ArrayType *at = (ArrayType *)args->type(0).ptr();
        return new CValue(at->elementType, ptr);
    }

    case PRIM_TupleTypeP : {
        return codegenValue(evaluatePrimOp(x, args), outPtr);
    }

    case PRIM_TupleElementCount : {
        return codegenValue(evaluatePrimOp(x, args), outPtr);
    }

    case PRIM_TupleElementType : {
        return codegenValue(evaluatePrimOp(x, args), outPtr);
    }

    case PRIM_TupleElementOffset : {
        return codegenValue(evaluatePrimOp(x, args), outPtr);
    }

    case PRIM_tuple : {
        if (args->size() < 2)
            error("tuples require atleast two elements");
        vector<TypePtr> elementTypes;
        for (unsigned i = 0; i < args->size(); ++i) {
            elementTypes.push_back(args->type(i));
            llvm::Value *ptr = builder->CreateConstGEP2_32(outPtr, 0, i);
            args->codegenAsValue(i, ptr);
        }
        return new CValue(tupleType(elementTypes), outPtr);
    }

    case PRIM_tupleRef : {
        args->ensureArity(2);
        ensureTupleType(args->type(0));
        int i = valueToInt(args->value(1));
        TupleType *tt = (TupleType *)args->type(0).ptr();
        if ((i < 0) || (i >= (int)tt->elementTypes.size()))
            error("tuple type index out of range");
        CValuePtr a = args->codegenAsRef(0);
        llvm::Value *result = builder->CreateConstGEP2_32(a->llval, 0, i);
        return new CValue(tt->elementTypes[i], result);
    }

    }
}
