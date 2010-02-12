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

    case FIELD_REF : {
        FieldRef *x = (FieldRef *)expr.ptr();
        vector<ExprPtr> args;
        args.push_back(x->expr);
        args.push_back(new ObjectExpr(x->name.ptr()));
        ObjectPtr prim = primName("recordFieldRefByName");
        return analyzeInvoke(prim, args, env);
    }

    case TUPLE_REF : {
        TupleRef *x = (TupleRef *)expr.ptr();
        vector<ExprPtr> args;
        args.push_back(x->expr);
        args.push_back(new ObjectExpr(intToValueHolder(x->index).ptr()));
        ObjectPtr prim = primName("tupleRef");
        return analyzeInvoke(prim, args, env);
    }

    case UNARY_OP : {
        UnaryOp *x = (UnaryOp *)expr.ptr();
        if (!x->desugared)
            x->desugared = desugarUnaryOp(x);
        return analyze(x->desugared, env);
    }

    case BINARY_OP : {
        BinaryOp *x = (BinaryOp *)expr.ptr();
        if (!x->desugared)
            x->desugared = desugarBinaryOp(x);
        return analyze(x->desugared, env);
    }

    case AND : {
        And *x = (And *)expr.ptr();
        PValuePtr a = analyzeValue(x->expr1, env);
        PValuePtr b = analyzeValue(x->expr2, env);
        if (!a || !b)
            return NULL;
        if (a->type != b->type)
            error("type mismatch in 'and' expression");
        return new PValue(a->type, a->isTemp || b->isTemp);
    }

    case OR : {
        Or *x = (Or *)expr.ptr();
        PValuePtr a = analyzeValue(x->expr1, env);
        PValuePtr b = analyzeValue(x->expr2, env);
        if (!a || !b)
            return NULL;
        if (a->type != b->type)
            error("type mismatch in 'or' expression");
        return new PValue(a->type, a->isTemp || b->isTemp);
    }

    case SC_EXPR : {
        SCExpr *x = (SCExpr *)expr.ptr();
        return analyze(x->expr, x->env);
    }

    case OBJECT_EXPR : {
        ObjectExpr *x = (ObjectExpr *)expr.ptr();
        return x->obj;
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
            for (unsigned i = 0; i+1 < args.size(); ++i)
                types.push_back(evaluateType(args[i], env));
            ObjectPtr returnType = evaluateReturnType(args.back(), env);
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

ObjectPtr analyzeInvokeType(TypePtr x, const vector<ExprPtr> &args, EnvPtr env)
{
    return new PValue(x, true);
}

ObjectPtr analyzeInvokeRecord(RecordPtr x, const vector<ExprPtr> &args,
                              EnvPtr env)
{
    ensureArity(args, x->fields.size());
    vector<PatternCellPtr> cells;
    EnvPtr renv = new Env(x->env);
    for (unsigned i = 0; i < x->patternVars.size(); ++i) {
        PatternCellPtr cell = new PatternCell(x->patternVars[i], NULL);
        addLocal(renv, x->patternVars[i], cell.ptr());
        cells.push_back(cell);
    }
    for (unsigned i = 0; i < args.size(); ++i) {
        PatternPtr fieldType = evaluatePattern(x->fields[i]->type, renv);
        PValuePtr pv = analyzeValue(args[i], env);
        if (!unify(fieldType, pv->type.ptr()))
            error(args[i], "type mismatch");
    }
    vector<ObjectPtr> params;
    for (unsigned i = 0; i < cells.size(); ++i)
        params.push_back(derefCell(cells[i]));
    TypePtr t = recordType(x, params);
    return analyzeInvokeType(t, args, env);
}

ObjectPtr analyzeInvokeProcedure(ProcedurePtr x, const vector<ExprPtr> &args,
                                 EnvPtr env)
{
    if (!x->staticFlagsInitialized)
        initIsStaticFlags(x);
    const vector<bool> &isStaticFlags =
        lookupIsStaticFlags(x.ptr(), args.size());
    vector<ObjectPtr> argsKey;
    for (unsigned i = 0; i < args.size(); ++i) {
        if (isStaticFlags[i])
            argsKey.push_back(evaluateStatic(args[i], env));
        else
            argsKey.push_back(analyzeValue(args[i], env)->type.ptr());
    }
    // lookup invoke table entry
    // if no entry, create entry
    // if analyzed, return info
    // if analyzing return NULL
    // analyzing = true
    // analyze
    // analyzing = false
    // analyzed = true
}

ObjectPtr analyzeInvokeOverloadable(OverloadablePtr x,
                                    const vector<ExprPtr> &args, EnvPtr env)
{
    if (!x->staticFlagsInitialized)
        initIsStaticFlags(x);
    const vector<bool> &isStaticFlags =
        lookupIsStaticFlags(x.ptr(), args.size());
    vector<ObjectPtr> argsKey;
    for (unsigned i = 0; i < args.size(); ++i) {
        if (isStaticFlags[i])
            argsKey.push_back(evaluateStatic(args[i], env));
        else
            argsKey.push_back(analyzeValue(args[i], env)->type.ptr());
    }
    // lookup invoke table entry
    // if no entry, create entry
    // if analyzed return info
    // if analyzing return NULL
    // analyzing = true
    // analyze
    // analyzing = false
    // analyzed = true
}

ObjectPtr analyzeInvokePrimOp(PrimOpPtr x, const vector<ExprPtr> &args, EnvPtr env);
ObjectPtr analyzeInvokeValue(PValuePtr x, const vector<ExprPtr> &args, EnvPtr env);
