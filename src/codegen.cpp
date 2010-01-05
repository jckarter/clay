#include "clay.hpp"
#include "invokeutil2.hpp"



//
// declarations
//

struct CValue;
typedef Pointer<CValue> CValuePtr;

struct CValue : public Object {
    TypePtr type;
    llvm::Value *llval;
    CValue(TypePtr type, llvm::Value *llval)
        : Object(CVALUE), type(type), llval(llval) {}
};

llvm::IRBuilder<> *builder;

llvm::Value *
codegen(ExprPtr expr,
        EnvPtr env,
        llvm::Value *outPtr);

llvm::Value *
codegenIndexing(ObjectPtr obj,
                ArgListPtr args,
                llvm::Value *outPtr);

llvm::Value *
codegenInvoke(ObjectPtr obj,
              ArgListPtr args,
              llvm::Value *outPtr);

void
codegenValue(ValuePtr v,
             llvm::Value *outPtr);

static
llvm::Value *
numericConstant(ValuePtr v);



//
// codegen
//

llvm::Value *
codegen(ExprPtr expr,
        EnvPtr env,
        llvm::Value *outPtr)
{
    LocationContext loc(expr->location);

    switch (expr->objKind) {

    case BOOL_LITERAL :
    case INT_LITERAL :
    case FLOAT_LITERAL : {
        ValuePtr v = evaluateToStatic(expr, env);
        llvm::Value *val = numericConstant(v);
        builder->CreateStore(val, outPtr);
        return outPtr;
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
            return outPtr;
        }
        else if (y->objKind == CVALUE) {
            CValue *z = (CValue *)y.ptr();
            assert(outPtr == NULL);
            return z->llval;
        }
        else {
            ValuePtr z = coToValue(y);
            codegenValue(z, outPtr);
            return outPtr;
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
        if ((indexable->type == compilerObjectType)
            && indexable->isStatic)
        {
            ObjectPtr indexable2 = lower(evaluateToStatic(x->expr, env));
            ArgListPtr args = new ArgList(x->args, env);
            return codegenIndexing(indexable2, args, outPtr);
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
        return codegen(x->converted, env, outPtr);;
    }

    default :
        assert(false);

    }

    return NULL;
}



//
// codegenValue
//

void
codegenValue(ValuePtr v,
             llvm::Value *outPtr)
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
