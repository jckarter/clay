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
    mutable ValuePtr value;

    Analysis(TypePtr type, bool isTemp, bool isStatic)
        : Object(ANALYSIS), type(type), isTemp(isTemp),
          isStatic(isStatic) {}
    ValuePtr evaluate() const;
    TypePtr evaluateType() const;
};

bool analyzeList(const vector<ExprPtr> &exprList, EnvPtr env,
                 vector<AnalysisPtr> &result);
AnalysisPtr analyze(ExprPtr expr, EnvPtr env);
AnalysisPtr analyze2(ExprPtr expr, EnvPtr env);

AnalysisPtr analyzeIndexing(ObjectPtr obj, const vector<AnalysisPtr> &args);
AnalysisPtr analyzeInvoke(ObjectPtr obj, const vector<AnalysisPtr> &args);

AnalysisPtr analyzeInvokeRecord(RecordPtr x, const vector<AnalysisPtr> &args);

bool analyzeMatchArg(AnalysisPtr arg, FormalArgPtr farg, EnvPtr fenv);

AnalysisPtr analyzeInvokeType(TypePtr x, const vector<AnalysisPtr> &args);

AnalysisPtr analyzeInvokeProcedure(ProcedurePtr x,
                                   const vector<AnalysisPtr> &args);

AnalysisPtr analyzeInvokeOverloadable(OverloadablePtr x,
                                      const vector<AnalysisPtr> &args);

MatchInvokeResultPtr analyzeMatchInvokeCode(CodePtr code, EnvPtr env,
                                            const vector<AnalysisPtr> &args);
EnvPtr analyzeBindValueArgs(EnvPtr env, const vector<AnalysisPtr> &args,
                            CodePtr code);

struct ReturnCell {
    TypePtr type;
    bool isRef;
    ReturnCell()
        : type(NULL), isRef(false) {}
    void set(TypePtr type, bool isRef);
};

bool analyzeCodeBody(CodePtr code, EnvPtr env, ReturnCell &rcell);
bool analyzeStatement(StatementPtr stmt, EnvPtr env, ReturnCell &rcell);
EnvPtr analyzeBinding(BindingPtr x, EnvPtr env);

AnalysisPtr analyzeInvokeExternal(ExternalProcedurePtr x,
                                  const vector<AnalysisPtr> &args);

AnalysisPtr analyzeInvokePrimOp(PrimOpPtr x, const vector<AnalysisPtr> &args);



//
// Analysis::evaluate
//

ValuePtr Analysis::evaluate() const {
    if (this->value.raw())
        return this->value;
    this->value = evaluateToStatic(this->expr, this->env);
    return this->value;
}

TypePtr Analysis::evaluateType() const {
    return valueToType(this->evaluate());
}



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

static void ensureArgType(const vector<AnalysisPtr> &args, int i, TypePtr t) {
    if (args[i]->type != t)
        fmtError("type mismatch at argument %d", (i+1));
}



//
// analyze
//

bool analyzeList(const vector<ExprPtr> &exprList, EnvPtr env,
                 vector<AnalysisPtr> &result) {
    for (unsigned i = 0; i < exprList.size(); ++i) {
        AnalysisPtr a = analyze(exprList[i], env);
        if (!a)
            return false;
        result.push_back(a);
    }
    return true;
}

AnalysisPtr analyze(ExprPtr expr, EnvPtr env) {
    LocationContext loc(expr->location);
    AnalysisPtr result = analyze2(expr, env);
    if (!result)
        return NULL;
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
        if (!indexable)
            return NULL;
        if (indexable->isStatic) {
            ObjectPtr indexable2 = lower(evaluateToStatic(x->expr, env));
            vector<AnalysisPtr> args;
            if (!analyzeList(x->args, env, args))
                return NULL;
            return analyzeIndexing(indexable2, args);
        }
        error("invalid indexing operation");
        return NULL;
    }

    case CALL : {
        Call *x = (Call *)expr.raw();
        AnalysisPtr callable = analyze(x->expr, env);
        if (!callable)
            return NULL;
        if (callable->isStatic) {
            ObjectPtr callable2 = lower(evaluateToStatic(x->expr, env));
            vector<AnalysisPtr> args;
            if (!analyzeList(x->args, env, args))
                return NULL;
            return analyzeInvoke(callable2, args);
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
        vector<AnalysisPtr> args2;
        if (!analyzeList(args, env, args2))
            return NULL;
        return analyzeInvoke(primName("recordFieldRefByName"), args2);
    }

    case TUPLE_REF : {
        TupleRef *x = (TupleRef *)expr.raw();
        ValuePtr index = intToValue(x->index);
        vector<ExprPtr> args;
        args.push_back(x->expr);
        args.push_back(new ValueExpr(index));
        vector<AnalysisPtr> args2;
        if (!analyzeList(args, env, args2))
            return NULL;
        return analyzeInvoke(primName("tupleRef"), args2);
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
        if (!a1 || !a2)
            return NULL;
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
        if (!a1 || !a2)
            return NULL;
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
    case TYPE :
        return analyzeInvokeType((Type *)obj.raw(), args);
    case PROCEDURE :
        return analyzeInvokeProcedure((Procedure *)obj.raw(), args);
    case OVERLOADABLE :
        return analyzeInvokeOverloadable((Overloadable *)obj.raw(), args);
    case EXTERNAL_PROCEDURE : {
        ExternalProcedurePtr x = (ExternalProcedure *)obj.raw();
        return analyzeInvokeExternal(x, args);
    }
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
        return unify(pattern, arg->evaluate());
    }
    }
    assert(false);
    return false;
}



//
// analyzeInvokeType
//

AnalysisPtr analyzeInvokeType(TypePtr x, const vector<AnalysisPtr> &args) {
    if (args.size() == 0)
        return staticTemp(x);
    if ((args.size() == 1) && (args[0]->type == x))
        return new Analysis(x, true, args[0]->isStatic);
    switch (x->typeKind) {
    case ARRAY_TYPE : {
        ArrayType *y = (ArrayType *)x.raw();
        ensureArity(args, y->size);
        for (unsigned i = 0; i < args.size(); ++i)
            ensureArgType(args, i, y->elementType);
        return new Analysis(x, true, allStatic(args));
    }
    case TUPLE_TYPE : {
        TupleType *y = (TupleType *)x.raw();
        ensureArity(args, y->elementTypes.size());
        for (unsigned i = 0; i < args.size(); ++i)
            ensureArgType(args, i, y->elementTypes[i]);
        return new Analysis(x, true, allStatic(args));
    }
    case RECORD_TYPE : {
        RecordType *y = (RecordType *)x.raw();
        const vector<TypePtr> &fieldTypes = recordFieldTypes(y);
        ensureArity(args, fieldTypes.size());
        for (unsigned i = 0; i < args.size(); ++i)
            ensureArgType(args, i, fieldTypes[i]);
        return new Analysis(x, true, allStatic(args));
    }
    }
    error("invalid constructor");
    return NULL;
}



//
// analyzeInvokeProcedure, analyzeInvokeOverloadable
//

AnalysisPtr analyzeInvokeProcedure(ProcedurePtr x,
                                   const vector<AnalysisPtr> &args) {
    MatchInvokeResultPtr result =
        analyzeMatchInvokeCode(x->code, x->env, args);
    if (result->resultKind == MATCH_INVOKE_SUCCESS) {
        MatchInvokeSuccess *y = (MatchInvokeSuccess *)result.raw();
        EnvPtr env = analyzeBindValueArgs(y->env, args, x->code);
        ReturnCell rcell;
        bool flag = analyzeCodeBody(x->code, env, rcell);
        if (!flag)
            return NULL;
        return new Analysis(rcell.type, !rcell.isRef, allStatic(args));
    }
    signalMatchInvokeError(result);
    return NULL;
}

AnalysisPtr analyzeInvokeOverloadable(OverloadablePtr x,
                                      const vector<AnalysisPtr> &args) {
    for (unsigned i = 0; i < x->overloads.size(); ++i) {
        OverloadPtr y = x->overloads[i];
        MatchInvokeResultPtr result =
            analyzeMatchInvokeCode(y->code, y->env, args);
        if (result->resultKind == MATCH_INVOKE_SUCCESS) {
            MatchInvokeSuccess *z = (MatchInvokeSuccess *)result.raw();
            EnvPtr env = analyzeBindValueArgs(z->env, args, y->code);
            ReturnCell rcell;
            bool flag = analyzeCodeBody(y->code, env, rcell);
            if (!flag)
                return NULL;
            return new Analysis(rcell.type, !rcell.isRef, allStatic(args));
        }
    }
    error("no matching overload");
    return NULL;
}

MatchInvokeResultPtr analyzeMatchInvokeCode(CodePtr code, EnvPtr env,
                                            const vector<AnalysisPtr> &args) {
    if (args.size() != code->formalArgs.size())
        return new MatchInvokeArgCountError();
    EnvPtr patternEnv = new Env(env);
    vector<PatternCellPtr> cells;
    for (unsigned i = 0; i < code->patternVars.size(); ++i) {
        cells.push_back(new PatternCell(code->patternVars[i], NULL));
        addLocal(patternEnv, code->patternVars[i], cells[i].raw());
    }
    for (unsigned i = 0; i < args.size(); ++i) {
        if (!analyzeMatchArg(args[i], code->formalArgs[i], patternEnv))
            return new MatchInvokeArgMismatch(i);
    }
    EnvPtr env2 = new Env(env);
    for (unsigned i = 0; i < cells.size(); ++i) {
        ValuePtr v = derefCell(cells[i]);
        addLocal(env2, code->patternVars[i], v.raw());
    }
    if (code->predicate.raw()) {
        bool result = evaluateToBool(code->predicate, env2);
        if (!result)
            return new MatchInvokePredicateFailure();
    }
    return new MatchInvokeSuccess(env2);
}

EnvPtr analyzeBindValueArgs(EnvPtr env, const vector<AnalysisPtr> &args,
                            CodePtr code) {
    EnvPtr env2 = new Env(env);
    for (unsigned i = 0; i < args.size(); ++i) {
        FormalArgPtr farg = code->formalArgs[i];
        if (farg->objKind == VALUE_ARG) {
            ValueArg *x = (ValueArg *)farg.raw();
            AnalysisPtr arg2 = new Analysis(args[i]->type, false, false);
            addLocal(env2, x->name, arg2.raw());
        }
    }
    return env2;
}

void ReturnCell::set(TypePtr type, bool isRef) {
    if (!this->type) {
        this->type = type;
        this->isRef = isRef;
    }
    else if ((this->type != type) || (this->isRef != isRef)) {
        error("return type mismatch");
    }
}

bool analyzeCodeBody(CodePtr code, EnvPtr env, ReturnCell &rcell) {
    bool result = analyzeStatement(code->body, env, rcell);
    if (!result)
        return false;
    if (!rcell.type)
        rcell.set(voidType, false);
    return true;
}

bool analyzeStatement(StatementPtr stmt, EnvPtr env, ReturnCell &rcell) {
    LocationContext loc(stmt->location);
    switch (stmt->objKind) {
    case BLOCK : {
        Block *x = (Block *)stmt.raw();
        for (unsigned i = 0; i < x->statements.size(); ++i) {
            StatementPtr y = x->statements[i];
            if (y->objKind == BINDING) {
                env = analyzeBinding((Binding *)y.raw(), env);
                if (!env)
                    return false;
            }
            else {
                bool result = analyzeStatement(y, env, rcell);
                if (!result)
                    return false;
            }
        }
        break;
    }
    case LABEL :
    case BINDING :
    case ASSIGNMENT :
    case GOTO :
        break;
    case RETURN : {
        Return *x = (Return *)stmt.raw();
        if (!x->expr) {
            rcell.set(voidType, false);
        }
        else {
            AnalysisPtr result = analyze(x->expr, env);
            if (!result)
                return false;
            rcell.set(result->type, false);
        }
        break;
    }
    case RETURN_REF : {
        ReturnRef *x = (ReturnRef *)stmt.raw();
        AnalysisPtr result = analyze(x->expr, env);
        if (!result)
            return false;
        rcell.set(result->type, true);
        break;
    }
    case IF : {
        If *x = (If *)stmt.raw();
        bool thenResult = analyzeStatement(x->thenPart, env, rcell);
        bool elseResult = true;
        if (x->elsePart.raw())
            elseResult = analyzeStatement(x->elsePart, env, rcell);
        return thenResult || elseResult;
    }
    case EXPR_STATEMENT :
        break;
    case WHILE : {
        While *x = (While *)stmt.raw();
        analyzeStatement(x->body, env, rcell);
        break;
    }
    case BREAK :
    case CONTINUE :
        break;
    case FOR : {
        For *x = (For *)stmt.raw();
        if (!x->converted)
            x->converted = convertForStatement(x);
        return analyzeStatement(x->converted, env, rcell);
    }
    default :
        assert(false);
    }
    return true;
}

EnvPtr analyzeBinding(BindingPtr x, EnvPtr env) {
    EnvPtr env2 = new Env(env);
    switch (x->bindingKind) {
    case VAR :
    case REF : {
        AnalysisPtr right = analyze(x->expr, env);
        if (!right)
            return NULL;
        addLocal(env2, x->name, right.raw());
        break;
    }
    case STATIC : {
        ValuePtr right = evaluateToStatic(x->expr, env);
        addLocal(env2, x->name, right.raw());
        break;
    }
    default :
        assert(false);
    }
    return env2;
}



//
// analyzeInvokeExternal
//

AnalysisPtr analyzeInvokeExternal(ExternalProcedurePtr x,
                                  const vector<AnalysisPtr> &args) {
    if (!x->llvmFunc)
        initExternalProcedure(x);
    return new Analysis(x->returnType2, false, allStatic(args));
}



//
// analyzeInvokePrimOp
//

AnalysisPtr analyzeInvokePrimOp(PrimOpPtr x, const vector<AnalysisPtr> &args) {
    switch (x->primOpCode) {
    case PRIM_TypeP :
        return new Analysis(boolType, true, allStatic(args));
    case PRIM_TypeSize :
        return new Analysis(int32Type, true, allStatic(args));
    case PRIM_primitiveInit :
        return new Analysis(voidType, true, allStatic(args));
    case PRIM_primitiveDestroy :
        return new Analysis(voidType, true, allStatic(args));
    case PRIM_primitiveCopy :
        return new Analysis(voidType, true, allStatic(args));
    case PRIM_primitiveAssign :
        return new Analysis(voidType, true, allStatic(args));
    case PRIM_primitiveEqualsP :
        return new Analysis(boolType, true, allStatic(args));
    case PRIM_primitiveHash :
        return new Analysis(int32Type, true, allStatic(args));

    case PRIM_BoolTypeP :
        return new Analysis(boolType, true, allStatic(args));
    case PRIM_boolNot :
        return new Analysis(boolType, true, allStatic(args));
    case PRIM_boolTruth :
        return new Analysis(boolType, true, allStatic(args));

    case PRIM_IntegerTypeP :
        return new Analysis(boolType, true, allStatic(args));
    case PRIM_SignedIntegerTypeP :
        return new Analysis(boolType, true, allStatic(args));
    case PRIM_FloatTypeP :
        return new Analysis(boolType, true, allStatic(args));
    case PRIM_numericEqualsP :
        return new Analysis(boolType, true, allStatic(args));
    case PRIM_numericLesserP :
        return new Analysis(boolType, true, allStatic(args));
    case PRIM_numericAdd :
        ensureArity(args, 2);
        return new Analysis(args[0]->type, true, allStatic(args));
    case PRIM_numericSubtract :
        ensureArity(args, 2);
        return new Analysis(args[0]->type, true, allStatic(args));
    case PRIM_numericMultiply :
        ensureArity(args, 2);
        return new Analysis(args[0]->type, true, allStatic(args));
    case PRIM_numericDivide :
        ensureArity(args, 2);
        return new Analysis(args[0]->type, true, allStatic(args));
    case PRIM_numericNegate :
        ensureArity(args, 1);
        return new Analysis(args[0]->type, true, allStatic(args));

    case PRIM_integerRemainder :
        ensureArity(args, 2);
        return new Analysis(args[0]->type, true, allStatic(args));
    case PRIM_integerShiftLeft :
        ensureArity(args, 2);
        return new Analysis(args[0]->type, true, allStatic(args));
    case PRIM_integerShiftRight :
        ensureArity(args, 2);
        return new Analysis(args[0]->type, true, allStatic(args));
    case PRIM_integerBitwiseAnd :
        ensureArity(args, 2);
        return new Analysis(args[0]->type, true, allStatic(args));
    case PRIM_integerBitwiseOr :
        ensureArity(args, 2);
        return new Analysis(args[0]->type, true, allStatic(args));
    case PRIM_integerBitwiseXor :
        ensureArity(args, 2);
        return new Analysis(args[0]->type, true, allStatic(args));

    case PRIM_numericConvert :
        ensureArity(args, 2);
        return new Analysis(args[0]->evaluateType(), true, allStatic(args));

    case PRIM_VoidTypeP :
        return new Analysis(boolType, true, allStatic(args));

    case PRIM_CompilerObjectTypeP :
        return new Analysis(boolType, true, allStatic(args));

    case PRIM_PointerTypeP :
        return new Analysis(boolType, true, allStatic(args));
    case PRIM_PointerType :
        return new Analysis(compilerObjectType, true, allStatic(args));
    case PRIM_Pointer :
        error("Pointer type constructor cannot be invoked");
        break;
    case PRIM_PointeeType :
        return new Analysis(compilerObjectType, true, allStatic(args));

    case PRIM_addressOf :
        ensureArity(args, 1);
        return new Analysis(pointerType(args[0]->type), true, allStatic(args));
    case PRIM_pointerDereference : {
        ensureArity(args, 1);
        ensurePointerType(args[0]->type);
        PointerType *t = (PointerType *)args[0]->type.raw();
        return new Analysis(t->pointeeType, false, allStatic(args));
    }
    case PRIM_pointerToInt :
        ensureArity(args, 2);
        return new Analysis(args[0]->evaluateType(), true, allStatic(args));
    case PRIM_intToPointer :
        ensureArity(args, 2);
        return new Analysis(pointerType(args[0]->evaluateType()),
                            true, allStatic(args));
    case PRIM_pointerCast :
        ensureArity(args, 2);
        return new Analysis(pointerType(args[0]->evaluateType()),
                            true, allStatic(args));
    case PRIM_allocateMemory :
        ensureArity(args, 2);
        return new Analysis(pointerType(args[0]->evaluateType()),
                            true, allStatic(args));
    case PRIM_freeMemory :
        return new Analysis(voidType, true, allStatic(args));

    case PRIM_ArrayTypeP :
        return new Analysis(boolType, true, allStatic(args));
    case PRIM_ArrayType :
        return new Analysis(compilerObjectType, true, allStatic(args));
    case PRIM_Array :
        error("Array type constructor cannot be invoked");
        break;
    case PRIM_ArrayElementType :
        return new Analysis(compilerObjectType, true, allStatic(args));
    case PRIM_ArraySize :
        return new Analysis(int32Type, true, allStatic(args));
    case PRIM_array : {
        if (args.empty())
            error("atleast one argument required for creating an array");
        TypePtr elementType = args[0]->type;
        int n = (int)args.size();
        return new Analysis(arrayType(elementType, n), true, allStatic(args));
    }
    case PRIM_arrayRef : {
        ensureArity(args, 2);
        ensureArrayType(args[0]->type);
        ArrayType *t = (ArrayType *)args[0]->type.raw();
        return new Analysis(t->elementType, false, allStatic(args));
    }

    case PRIM_TupleTypeP :
        return new Analysis(boolType, true, allStatic(args));
    case PRIM_TupleType :
        return new Analysis(compilerObjectType, true, allStatic(args));
    case PRIM_Tuple :
        error("Tuple type constructor cannot be invoked");
        break;
    case PRIM_TupleSize :
        return new Analysis(int32Type, true, allStatic(args));
    case PRIM_TupleElementType :
        return new Analysis(compilerObjectType, true, allStatic(args));
    case PRIM_TupleElementOffset :
        return new Analysis(int32Type, true, allStatic(args));
    case PRIM_tuple : {
        if (args.size() < 2)
            error("tuples need atleast two elements");
        vector<TypePtr> elementTypes;
        for (unsigned i = 0; i < args.size(); ++i)
            elementTypes.push_back(args[i]->type);
        return new Analysis(tupleType(elementTypes), true, allStatic(args));
    }
    case PRIM_tupleRef : {
        ensureArity(args, 2);
        ensureTupleType(args[0]->type);
        int i = valueToInt(args[1]->evaluate());
        TupleType *t = (TupleType *)args[0]->type.raw();
        if ((i < 0) || (i >= (int)t->elementTypes.size()))
            error("tuple type index out of range");
        return new Analysis(t->elementTypes[i], false, allStatic(args));
    }

    case PRIM_RecordTypeP :
        return new Analysis(boolType, true, allStatic(args));
    case PRIM_RecordType :
        return new Analysis(compilerObjectType, true, allStatic(args));
    case PRIM_RecordFieldCount :
        return new Analysis(int32Type, true, allStatic(args));
    case PRIM_RecordFieldType :
        return new Analysis(compilerObjectType, true, allStatic(args));
    case PRIM_RecordFieldOffset :
        return new Analysis(int32Type, true, allStatic(args));
    case PRIM_RecordFieldIndex :
        return new Analysis(int32Type, true, allStatic(args));
    case PRIM_recordFieldRef : {
        ensureArity(args, 2);
        ensureRecordType(args[0]->type);
        RecordType *rt = (RecordType *)args[0]->type.raw();
        int i = valueToInt(args[1]->evaluate());
        const vector<TypePtr> &fieldTypes = recordFieldTypes(rt);
        if ((i < 0) || (i >= (int)fieldTypes.size()))
            error("field index out of range");
        return new Analysis(fieldTypes[i], false, allStatic(args));
    }
    case PRIM_recordFieldRefByName : {
        ensureArity(args, 2);
        ensureRecordType(args[0]->type);
        RecordType *t = (RecordType *)args[0]->type.raw();
        ObjectPtr obj = valueToCO(args[1]->evaluate());
        if (obj->objKind != IDENTIFIER)
            error("expecting an identifier value");
        Identifier *name = (Identifier *)obj.raw();
        const map<string, int> &fieldIndexMap = recordFieldIndexMap(t);
        map<string, int>::const_iterator fi = fieldIndexMap.find(name->str);
        if (fi == fieldIndexMap.end())
            error("field not in record");
        int i = fi->second;
        const vector<TypePtr> &fieldTypes = recordFieldTypes(t);
        return new Analysis(fieldTypes[i], false, allStatic(args));
    }
    case PRIM_recordInit :
        return new Analysis(voidType, true, allStatic(args));
    case PRIM_recordDestroy :
        return new Analysis(voidType, true, allStatic(args));
    case PRIM_recordCopy :
        return new Analysis(voidType, true, allStatic(args));
    case PRIM_recordAssign :
        return new Analysis(voidType, true, allStatic(args));
    case PRIM_recordEqualsP :
        return new Analysis(boolType, true, allStatic(args));
    case PRIM_recordHash :
        return new Analysis(int32Type, true, allStatic(args));
    default :
        assert(false);
    }
    return NULL;
}
