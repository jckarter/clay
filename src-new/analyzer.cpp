#include "clay.hpp"

ObjectPtr analyze(ExprPtr expr, EnvPtr env)
{
    switch (expr->objKind) {

    case BOOL_LITERAL :
        return new PValue(boolType, true);

    case INT_LITERAL : {
        IntLiteral *x = (IntLiteral *)expr.ptr();
        TypePtr t;
        if (x->suffix == "i8")
            t = int8Type;
        else if (x->suffix == "i16")
            t = int16Type;
        else if ((x->suffix == "i32") || x->suffix.empty())
            t = int32Type;
        else if (x->suffix == "i64")
            t = int64Type;
        else if (x->suffix == "u8")
            t = uint8Type;
        else if (x->suffix == "u16")
            t = uint16Type;
        else if (x->suffix == "u32")
            t = uint32Type;
        else if (x->suffix == "u64")
            t = uint64Type;
        else if (x->suffix == "f32")
            t = float32Type;
        else if (x->suffix == "f64")
            t = float64Type;
        else
            error("invalid literal suffix: " + x->suffix);
        return new PValue(t, true);
    }

    case FLOAT_LITERAL : {
        FloatLiteral *x = (FloatLiteral *)expr.ptr();
        TypePtr t;
        if (x->suffix == "f32")
            t = float32Type;
        else if ((x->suffix == "f64") || x->suffix.empty())
            t = float64Type;
        else
            error("invalid float literal suffix: " + x->suffix);
        return new PValue(t, true);
    }

    case CHAR_LITERAL : {
        CharLiteral *x = (CharLiteral *)expr.ptr();
        if (!x->desugared)
            x->desugared = desugarCharLiteral(x->value);
        return analyze(x->desugared, env);
    }

    case STRING_LITERAL : {
        StringLiteral *x = (StringLiteral *)expr.ptr();
        if (!x->desugared)
            x->desugared = desugarStringLiteral(x->value);
        return analyze(x->desugared, env);
    }

    case NAME_REF : {
        NameRef *x = (NameRef *)expr.ptr();
        return lookupEnv(env, x->name);
    }

    case TUPLE : {
        Tuple *x = (Tuple *)expr.ptr();
        if (!x->desugared)
            x->desugared = desugarTuple(x);
        return analyze(x->desugared, env);
    }

    case ARRAY : {
        Array *x = (Array *)expr.ptr();
        if (!x->desugared)
            x->desugared = desugarArray(x);
        return analyze(x->desugared, env);
    }

    case INDEXING : {
        Indexing *x = (Indexing *)expr.ptr();
        ObjectPtr indexable = analyze(x->expr, env);
        if (!indexable)
            return NULL;
        return analyzeIndexing(indexable, x->args, env);
    }

    case CALL : {
        Call *x = (Call *)expr.ptr();
        ObjectPtr callable = analyze(x->expr, env);
        if (!callable)
            return NULL;
        return analyzeInvoke(callable, x->args, env);
    }

    default :
        assert(false);
        return NULL;

    }
}

ObjectPtr analyzeIndexing(ObjectPtr x, const vector<ExprPtr> &args, EnvPtr env)
{
    switch (x->objKind) {

    case PRIM_OP : {
        PrimOpPtr y = (PrimOp *)x.ptr();

        switch (y->primOpCode) {

        case PRIM_Pointer : {
            ensureArity(args, 1);
            TypePtr t = evaluateType(args[0], env);
            return pointerType(t).ptr();
        }

        case PRIM_FunctionPointer : {
            if (args.size() < 1)
                error("atleast one argument required for"
                      " function pointer types.");
            vector<TypePtr> types;
            for (unsigned i = 0; i < args.size(); ++i)
                types.push_back(evaluateType(args[i], env));
            TypePtr returnType = types.back();
            types.pop_back();
            return functionPointerType(types, returnType).ptr();
        }

        case PRIM_Array : {
            ensureArity(args, 2);
            TypePtr t = evaluateType(args[0], env);
            int n = evaluateInt(args[1], env);
            return arrayType(t, n).ptr();
        }

        case PRIM_Tuple : {
            if (args.size() < 2)
                error("atleast two arguments required for tuple types");
            vector<TypePtr> types;
            for (unsigned i = 0; i < args.size(); ++i)
                types.push_back(evaluateType(args[i], env));
            return tupleType(types).ptr();
        }

        }
    }

    case RECORD : {
        RecordPtr y = (Record *)x.ptr();
        ensureArity(args, y->patternVars.size());
        vector<ObjectPtr> params;
        for (unsigned i = 0; i < args.size(); ++i)
            params.push_back(evaluateStatic(args[i], env));
        return recordType(y, params).ptr();
    }

    }
    error("invalid indexing operation");
    return NULL;
}

ObjectPtr analyzeInvoke(ObjectPtr x, const vector<ExprPtr> &args, EnvPtr env)
{
    switch (x->objKind) {
    case TYPE :
        return analyzeInvokeType((Type *)x.ptr(), args, env);
    case RECORD :
        return analyzeInvokeRecord((Record *)x.ptr(), args, env);
    case PROCEDURE :
        return analyzeInvokeProcedure((Procedure *)x.ptr(), args, env);
    case OVERLOADABLE :
        return analyzeInvokeOverloadable((Overloadable *)x.ptr(), args, env);
    case PRIM_OP :
        return analyzeInvokePrimOp((PrimOp *)x.ptr(), args, env);
    case PVALUE :
        return analyzeInvokeValue((PValue *)x.ptr(), args, env);
    }
    error("invalid call operation");
    return NULL;
}
