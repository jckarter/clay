#include "clay.hpp"



//
// declarations
//

struct Analysis;
typedef Ptr<Analysis> AnalysisPtr;

struct Analysis : public Object {
    TypePtr type;
    bool isTemp;
    bool isStatic;
    ExprPtr expr;
    EnvPtr env;

    Analysis(TypePtr type, bool isTemp, bool isStatic)
        : Object(ANALYSIS), type(type), isTemp(isTemp),
          isStatic(isStatic) {}
};

vector<AnalysisPtr> analyzeList(const vector<ExprPtr> &exprList, EnvPtr env);
AnalysisPtr analyze(ExprPtr expr, EnvPtr env);
AnalysisPtr analyze2(ExprPtr expr, EnvPtr env);

AnalysisPtr analyzeIndexing(ObjectPtr obj, const vector<AnalysisPtr> &args);
AnalysisPtr analyzeInvoke(ObjectPtr obj, const vector<AnalysisPtr> &args);

AnalysisPtr analyzeInvokeRecord(RecordPtr x, const vector<AnalysisPtr> &args);

bool analyzeMatchArg(AnalysisPtr arg, FormalArgPtr farg, EnvPtr fenv);

AnalysisPtr analyzeInvokeProcedure(ProcedurePtr x,
                                   const vector<AnalysisPtr> &args);

AnalysisPtr analyzeInvokeOverloadable(OverloadablePtr x,
                                      const vector<AnalysisPtr> &args);

AnalysisPtr analyzeInvokeExternalProcedure(ExternalProcedurePtr x,
                                           const vector<AnalysisPtr> &args);

AnalysisPtr analyzeInvokeType(TypePtr x, const vector<AnalysisPtr> &args);

AnalysisPtr analyzeInvokePrimOp(PrimOpPtr x, const vector<AnalysisPtr> &args);



//
// helper procs
//

static AnalysisPtr staticTemp(TypePtr t) {
    return new Analysis(t, true, true);
}

static bool allStatic(const vector<AnalysisPtr> &args) {
    for (unsigned i = 0; i < args.size(); ++i) {
        if (!args[i]->isStatic)
            return false;
    }
    return true;
}

static void ensureStaticArgs(const vector<AnalysisPtr> &args) {
    for (unsigned i = 0; i < args.size(); ++i) {
        if (!args[i]->isStatic)
            fmtError("static value expected at argument %d", (i+1));
    }
}



//
// analyze
//

vector<AnalysisPtr> analyzeList(const vector<ExprPtr> &exprList, EnvPtr env) {
    vector<AnalysisPtr> out;
    for (unsigned i = 0; i < exprList.size(); ++i)
        out.push_back(analyze(exprList[i], env));
    return out;
}

AnalysisPtr analyze(ExprPtr expr, EnvPtr env) {
    LocationContext loc(expr->location);
    AnalysisPtr result = analyze2(expr, env);
    result->expr = expr;
    result->env = env;
    return result;
}

AnalysisPtr analyze2(ExprPtr expr, EnvPtr env) {
    switch (expr->objKind) {

    case BOOL_LITERAL :
        return staticTemp(boolType);

    case INT_LITERAL : {
        IntLiteral *x = (IntLiteral *)expr.raw();
        if (x->suffix == "i8")
            return staticTemp(int8Type);
        else if (x->suffix == "i16")
            return staticTemp(int16Type);
        else if ((x->suffix == "i32") || x->suffix.empty())
            return staticTemp(int32Type);
        else if (x->suffix == "i64")
            return staticTemp(int64Type);
        else if (x->suffix == "u8")
            return staticTemp(uint8Type);
        else if (x->suffix == "u16")
            return staticTemp(uint16Type);
        else if (x->suffix == "u32")
            return staticTemp(uint32Type);
        else if (x->suffix == "u64")
            return staticTemp(uint64Type);
        else if (x->suffix == "f32")
            return staticTemp(float32Type);
        else if (x->suffix == "f64")
            return staticTemp(float64Type);
        error("invalid literal suffix: " + x->suffix);
        return NULL;
    }

    case FLOAT_LITERAL : {
        FloatLiteral *x = (FloatLiteral *)expr.raw();
        if (x->suffix == "f32")
            return staticTemp(float32Type);
        else if ((x->suffix == "f64") || x->suffix.empty())
            return staticTemp(float64Type);
        error("invalid float literal suffix: " + x->suffix);
        return NULL;
    }

    case CHAR_LITERAL : {
        CharLiteral *x = (CharLiteral *)expr.raw();
        if (!x->converted)
            x->converted = convertCharLiteral(x->value);
        return analyze(x->converted, env);
    }

    case STRING_LITERAL : {
        StringLiteral *x = (StringLiteral *)expr.raw();
        if (!x->converted)
            x->converted = convertStringLiteral(x->value);
        return analyze(x->converted, env);
    }

    case NAME_REF : {
        NameRef *x = (NameRef *)expr.raw();
        ObjectPtr y = lookupEnv(env, x->name);
        if (y->objKind == VALUE) {
            Value *z = (Value *)y.raw();
            return staticTemp(z->type);
        }
        else if (y->objKind == ANALYSIS) {
            Analysis *z = (Analysis *)y.raw();
            return new Analysis(z->type, false, false);
        }
        return staticTemp(compilerObjectType);
    }

    case TUPLE : {
        Tuple *x = (Tuple *)expr.raw();
        if (!x->converted)
            x->converted = convertTuple(x);
        return analyze(x->converted, env);
    }

    case ARRAY : {
        Array *x = (Array *)expr.raw();
        if (!x->converted)
            x->converted = convertArray(x);
        return analyze(x->converted, env);
    }

    case INDEXING : {
        Indexing *x = (Indexing *)expr.raw();
        AnalysisPtr indexable = analyze(x->expr, env);
        if (indexable->isStatic) {
            ObjectPtr indexable2 = lower(evaluateToStatic(x->expr, env));
            return analyzeIndexing(indexable2, analyzeList(x->args, env));
        }
        error("invalid indexing operation");
        return NULL;
    }

    case CALL : {
        Call *x = (Call *)expr.raw();
        AnalysisPtr callable = analyze(x->expr, env);
        if (callable->isStatic) {
            ObjectPtr callable2 = lower(evaluateToStatic(x->expr, env));
            return analyzeInvoke(callable2, analyzeList(x->args, env));
        }
        error("invalid call operation");
        return NULL;
    }

    case FIELD_REF : {
        FieldRef *x = (FieldRef *)expr.raw();
        ValuePtr name = coToValue(x->name.raw());
        vector<ExprPtr> args;
        args.push_back(x->expr);
        args.push_back(new ValueExpr(name));
        return analyzeInvoke(primName("recordFieldRefByName"),
                             analyzeList(args, env));
    }

    case TUPLE_REF : {
        TupleRef *x = (TupleRef *)expr.raw();
        ValuePtr index = intToValue(x->index);
        vector<ExprPtr> args;
        args.push_back(x->expr);
        args.push_back(new ValueExpr(index));
        return analyzeInvoke(primName("tupleRef"),
                             analyzeList(args, env));
    }

    case UNARY_OP : {
        UnaryOp *x = (UnaryOp *)expr.raw();
        if (!x->converted)
            x->converted = convertUnaryOp(x);
        return analyze(x->converted, env);
    }

    case BINARY_OP : {
        BinaryOp *x = (BinaryOp *)expr.raw();
        if (!x->converted)
            x->converted = convertBinaryOp(x);
        return analyze(x->converted, env);
    }

    case AND : {
        And *x = (And *)expr.raw();
        AnalysisPtr a1 = analyze(x->expr1, env);
        AnalysisPtr a2 = analyze(x->expr2, env);
        if (a1->type != a2->type)
            error("type mismatch in 'and' expression");
        AnalysisPtr result = new Analysis(a1->type,
                                          a1->isTemp || a2->isTemp,
                                          a1->isStatic && a2->isStatic);
        return result;
    }

    case OR : {
        Or *x = (Or *)expr.raw();
        AnalysisPtr a1 = analyze(x->expr1, env);
        AnalysisPtr a2 = analyze(x->expr2, env);
        if (a1->type != a2->type)
            error("type mismatch in 'or' expression");
        AnalysisPtr result = new Analysis(a1->type,
                                          a1->isTemp || a2->isTemp,
                                          a1->isStatic && a2->isStatic);
        return result;
    }

    case SC_EXPR : {
        SCExpr *x = (SCExpr *)expr.raw();
        return analyze(x->expr, x->env);
    }

    case VALUE_EXPR : {
        ValueExpr *x = (ValueExpr *)expr.raw();
        return staticTemp(x->value->type);
    }

    default :
        assert(false);
        return NULL;
    }
}



//
// analyzeIndexing
//

AnalysisPtr analyzeIndexing(ObjectPtr obj, const vector<AnalysisPtr> &args) {
    switch (obj->objKind) {
    case RECORD : {
        Record *x = (Record *)obj.raw();
        ensureArity(args, x->patternVars.size());
        ensureStaticArgs(args);
        return staticTemp(compilerObjectType);
    }
    case PRIM_OP : {
        PrimOp *x = (PrimOp *)obj.raw();
        switch (x->primOpCode) {
        case PRIM_Pointer :
            ensureArity(args, 1);
            ensureStaticArgs(args);
            return staticTemp(compilerObjectType);
        case PRIM_Array :
            ensureArity(args, 2);
            ensureStaticArgs(args);
            return staticTemp(compilerObjectType);
        case PRIM_Tuple :
            ensureStaticArgs(args);
            return staticTemp(compilerObjectType);
        }
        break;
    }
    }
    error("invalid indexing operation");
    return NULL;
}



//
// analyzeInvoke
//

AnalysisPtr analyzeInvoke(ObjectPtr obj, const vector<AnalysisPtr> &args) {
    switch (obj->objKind) {
    case RECORD :
        return analyzeInvokeRecord((Record *)obj.raw(), args);
    case PROCEDURE :
        return analyzeInvokeProcedure((Procedure *)obj.raw(), args);
    case OVERLOADABLE :
        return analyzeInvokeOverloadable((Overloadable *)obj.raw(), args);
    case EXTERNAL_PROCEDURE : {
        ExternalProcedurePtr x = (ExternalProcedure *)obj.raw();
        return analyzeInvokeExternalProcedure(x, args);
    }
    case TYPE :
        return analyzeInvokeType((Type *)obj.raw(), args);
    case PRIM_OP :
        return analyzeInvokePrimOp((PrimOp *)obj.raw(), args);
    }
    error("invalid operation");
    return NULL;
}



//
// analyzeInvokeRecord
//

AnalysisPtr analyzeInvokeRecord(RecordPtr x, const vector<AnalysisPtr> &args) {
    ensureArity(args, x->formalArgs.size());
    EnvPtr env = new Env(x->env);
    vector<PatternCellPtr> cells;
    for (unsigned i = 0; i < x->patternVars.size(); ++i) {
        cells.push_back(new PatternCell(x->patternVars[i], NULL));
        addLocal(env, x->patternVars[i], cells[i].raw());
    }
    for (unsigned i = 0; i < args.size(); ++i) {
        FormalArgPtr farg = x->formalArgs[i];
        if (!analyzeMatchArg(args[i], x->formalArgs[i], env))
            fmtError("mismatch at argument %d", i+1);
    }
    vector<ValuePtr> cellValues;
    for (unsigned i = 0; i < cells.size(); ++i)
        cellValues.push_back(derefCell(cells[i]));
    TypePtr t = recordType(x, cellValues);
    return new Analysis(t, true, allStatic(args));
}



//
// analyzeMatchArg
//

bool analyzeMatchArg(AnalysisPtr arg, FormalArgPtr farg, EnvPtr env) {
    switch (farg->objKind) {
    case VALUE_ARG : {
        ValueArg *x = (ValueArg *)farg.raw();
        if (!x->type)
            return true;
        PatternPtr pattern = evaluatePattern(x->type, env);
        return unifyType(pattern, arg->type);
    }
    case STATIC_ARG : {
        StaticArg *x = (StaticArg *)farg.raw();
        PatternPtr pattern = evaluatePattern(x->pattern, env);
        ValuePtr y = evaluateToStatic(arg->expr, arg->env);
        return unify(pattern, y);
    }
    }
    assert(false);
    return false;
}
