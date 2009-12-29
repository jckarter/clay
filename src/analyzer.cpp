#include "clay.hpp"
#include "invokeutil.hpp"



//
// declarations
//

bool analyzeList(const vector<ExprPtr> &exprList, EnvPtr env,
                 vector<AnalysisPtr> &result);
AnalysisPtr analyze2(ExprPtr expr, EnvPtr env);

ReturnInfoPtr analyzeIndexing(ObjectPtr obj, const vector<AnalysisPtr> &args);

ReturnInfoPtr analyzeInvokeRecord(RecordPtr x, const vector<AnalysisPtr> &args);

ReturnInfoPtr analyzeInvokeType(TypePtr x, const vector<AnalysisPtr> &args);

ReturnInfoPtr analyzeInvokeProcedure(ProcedurePtr x,
                                     const vector<AnalysisPtr> &args);

ReturnInfoPtr analyzeInvokeOverloadable(OverloadablePtr x,
                                        const vector<AnalysisPtr> &args);

bool analyzeCodeBody(CodePtr code, EnvPtr env, ReturnInfoPtr rinfo);
bool analyzeStatement(StatementPtr stmt, EnvPtr env, ReturnInfoPtr rinfo);
EnvPtr analyzeBinding(BindingPtr x, EnvPtr env);

ReturnInfoPtr analyzeInvokeExternal(ExternalProcedurePtr x,
                                    const vector<AnalysisPtr> &args);

ReturnInfoPtr analyzeInvokePrimOp(PrimOpPtr x, const vector<AnalysisPtr> &args);



//
// Analysis::evaluate
//

ValuePtr Analysis::evaluate() const {
    if (this->value.ptr())
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
        IntLiteral *x = (IntLiteral *)expr.ptr();
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
        FloatLiteral *x = (FloatLiteral *)expr.ptr();
        if (x->suffix == "f32")
            return staticTemp(float32Type);
        else if ((x->suffix == "f64") || x->suffix.empty())
            return staticTemp(float64Type);
        error("invalid float literal suffix: " + x->suffix);
        return NULL;
    }

    case CHAR_LITERAL : {
        CharLiteral *x = (CharLiteral *)expr.ptr();
        if (!x->converted)
            x->converted = convertCharLiteral(x->value);
        return analyze(x->converted, env);
    }

    case STRING_LITERAL : {
        StringLiteral *x = (StringLiteral *)expr.ptr();
        if (!x->converted)
            x->converted = convertStringLiteral(x->value);
        return analyze(x->converted, env);
    }

    case NAME_REF : {
        NameRef *x = (NameRef *)expr.ptr();
        ObjectPtr y = lookupEnv(env, x->name);
        if (y->objKind == VALUE) {
            Value *z = (Value *)y.ptr();
            return staticTemp(z->type);
        }
        else if (y->objKind == ANALYSIS) {
            Analysis *z = (Analysis *)y.ptr();
            return new Analysis(z->type, false, false);
        }
        return staticTemp(compilerObjectType);
    }

    case TUPLE : {
        Tuple *x = (Tuple *)expr.ptr();
        if (!x->converted)
            x->converted = convertTuple(x);
        return analyze(x->converted, env);
    }

    case ARRAY : {
        Array *x = (Array *)expr.ptr();
        if (!x->converted)
            x->converted = convertArray(x);
        return analyze(x->converted, env);
    }

    case INDEXING : {
        Indexing *x = (Indexing *)expr.ptr();
        AnalysisPtr indexable = analyze(x->expr, env);
        if (!indexable)
            return NULL;
        if (indexable->isStatic) {
            ObjectPtr indexable2 = lower(evaluateToStatic(x->expr, env));
            vector<AnalysisPtr> args;
            if (!analyzeList(x->args, env, args))
                return NULL;
            ReturnInfoPtr rinfo = analyzeIndexing(indexable2, args);
            if (!rinfo)
                return NULL;
            return new Analysis(rinfo->type, !rinfo->isRef, allStatic(args));
        }
        error("invalid indexing operation");
        return NULL;
    }

    case CALL : {
        Call *x = (Call *)expr.ptr();
        AnalysisPtr callable = analyze(x->expr, env);
        if (!callable)
            return NULL;
        if (callable->isStatic) {
            ObjectPtr callable2 = lower(evaluateToStatic(x->expr, env));
            vector<AnalysisPtr> args;
            if (!analyzeList(x->args, env, args))
                return NULL;
            ReturnInfoPtr rinfo = analyzeInvoke(callable2, args);
            if (!rinfo)
                return NULL;
            return new Analysis(rinfo->type, !rinfo->isRef, allStatic(args));
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
        vector<AnalysisPtr> args2;
        if (!analyzeList(args, env, args2))
            return NULL;
        ObjectPtr prim = primName("recordFieldRefByName");
        ReturnInfoPtr rinfo = analyzeInvoke(prim, args2);
        if (!rinfo)
            return NULL;
        return new Analysis(rinfo->type, !rinfo->isRef, allStatic(args2));
    }

    case TUPLE_REF : {
        TupleRef *x = (TupleRef *)expr.ptr();
        ValuePtr index = intToValue(x->index);
        vector<ExprPtr> args;
        args.push_back(x->expr);
        args.push_back(new ValueExpr(index));
        vector<AnalysisPtr> args2;
        if (!analyzeList(args, env, args2))
            return NULL;
        ReturnInfoPtr rinfo = analyzeInvoke(primName("tupleRef"), args2);
        if (!rinfo)
            return NULL;
        return new Analysis(rinfo->type, !rinfo->isRef, allStatic(args2));
    }

    case UNARY_OP : {
        UnaryOp *x = (UnaryOp *)expr.ptr();
        if (!x->converted)
            x->converted = convertUnaryOp(x);
        return analyze(x->converted, env);
    }

    case BINARY_OP : {
        BinaryOp *x = (BinaryOp *)expr.ptr();
        if (!x->converted)
            x->converted = convertBinaryOp(x);
        return analyze(x->converted, env);
    }

    case AND : {
        And *x = (And *)expr.ptr();
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
        Or *x = (Or *)expr.ptr();
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
        SCExpr *x = (SCExpr *)expr.ptr();
        return analyze(x->expr, x->env);
    }

    case VALUE_EXPR : {
        ValueExpr *x = (ValueExpr *)expr.ptr();
        return staticTemp(x->value->type);
    }

    default :
        assert(false);
        return NULL;
    }
}



//
// ReturnInfo
//

void ReturnInfo::set(TypePtr type, bool isRef) {
    if (!this->type) {
        this->type = type;
        this->isRef = isRef;
    }
    else if ((this->type != type) || (this->isRef != isRef)) {
        error("return type mismatch");
    }
}



//
// analyzeIndexing
//

ReturnInfoPtr analyzeIndexing(ObjectPtr obj, const vector<AnalysisPtr> &args) {
    switch (obj->objKind) {
    case RECORD :
        return new ReturnInfo(compilerObjectType, false);
    case PRIM_OP : {
        PrimOp *x = (PrimOp *)obj.ptr();
        switch (x->primOpCode) {
        case PRIM_Pointer :
        case PRIM_Array :
        case PRIM_Tuple :
            return new ReturnInfo(compilerObjectType, false);
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

ReturnInfoPtr analyzeInvoke(ObjectPtr obj, const vector<AnalysisPtr> &args) {
    switch (obj->objKind) {
    case RECORD :
        return analyzeInvokeRecord((Record *)obj.ptr(), args);
    case TYPE :
        return analyzeInvokeType((Type *)obj.ptr(), args);
    case PROCEDURE :
        return analyzeInvokeProcedure((Procedure *)obj.ptr(), args);
    case OVERLOADABLE :
        return analyzeInvokeOverloadable((Overloadable *)obj.ptr(), args);
    case EXTERNAL_PROCEDURE : {
        ExternalProcedurePtr x = (ExternalProcedure *)obj.ptr();
        return analyzeInvokeExternal(x, args);
    }
    case PRIM_OP :
        return analyzeInvokePrimOp((PrimOp *)obj.ptr(), args);
    }
    error("invalid operation");
    return NULL;
}



//
// analyzeInvokeRecord
//

ReturnInfoPtr analyzeInvokeRecord(RecordPtr x, const vector<AnalysisPtr> &args) {
    ensureArity(args, x->formalArgs.size());
    EnvPtr env = new Env(x->env);
    vector<PatternCellPtr> cells;
    for (unsigned i = 0; i < x->patternVars.size(); ++i) {
        cells.push_back(new PatternCell(x->patternVars[i], NULL));
        addLocal(env, x->patternVars[i], cells[i].ptr());
    }
    for (unsigned i = 0; i < args.size(); ++i) {
        FormalArgPtr farg = x->formalArgs[i];
        if (!matchFormalArg(args[i], x->formalArgs[i], env))
            fmtError("mismatch at argument %d", i+1);
    }
    vector<ValuePtr> cellValues;
    for (unsigned i = 0; i < cells.size(); ++i)
        cellValues.push_back(derefCell(cells[i]));
    TypePtr t = recordType(x, cellValues);
    return new ReturnInfo(t, false);
}



//
// analyzeInvokeType
//

ReturnInfoPtr analyzeInvokeType(TypePtr x, const vector<AnalysisPtr> &args) {
    return new ReturnInfo(x, false);
}



//
// analyzeInvokeProcedure, analyzeInvokeOverloadable
//

ReturnInfoPtr analyzeInvokeProcedure(ProcedurePtr x,
                                     const vector<AnalysisPtr> &args) {
    InvokeTableEntry *entry = lookupProcedureInvoke(x, args);
    if (entry->returnType.ptr())
        return new ReturnInfo(entry->returnType, entry->returnByRef);
    if (entry->analyzing)
        return NULL;
    entry->analyzing = true;
    MatchInvokeResultPtr result = matchInvoke(x->code, x->env, args);
    if (result->resultKind == MATCH_INVOKE_SUCCESS) {
        MatchInvokeSuccess *y = (MatchInvokeSuccess *)result.ptr();
        EnvPtr env = bindValueArgs(y->env, args, x->code);
        ReturnInfoPtr rinfo = new ReturnInfo();
        bool flag = analyzeCodeBody(x->code, env, rinfo);
        entry->analyzing = false;
        if (!flag)
            error("recursive type propagation");
        entry->returnType = rinfo->type;
        entry->returnByRef = rinfo->isRef;
        return rinfo;
    }
    entry->analyzing = false;
    signalMatchInvokeError(result);
    return NULL;
}

ReturnInfoPtr analyzeInvokeOverloadable(OverloadablePtr x,
                                        const vector<AnalysisPtr> &args) {
    InvokeTableEntry *entry = lookupOverloadableInvoke(x, args);
    if (entry->returnType.ptr())
        return new ReturnInfo(entry->returnType, entry->returnByRef);
    if (entry->analyzing)
        return NULL;
    entry->analyzing = true;
    for (unsigned i = 0; i < x->overloads.size(); ++i) {
        OverloadPtr y = x->overloads[i];
        MatchInvokeResultPtr result = matchInvoke(y->code, y->env, args);
        if (result->resultKind == MATCH_INVOKE_SUCCESS) {
            MatchInvokeSuccess *z = (MatchInvokeSuccess *)result.ptr();
            EnvPtr env = bindValueArgs(z->env, args, y->code);
            ReturnInfoPtr rinfo = new ReturnInfo();
            bool flag = analyzeCodeBody(y->code, env, rinfo);
            entry->analyzing = false;
            if (!flag)
                error("recursive type propagation");
            entry->returnType = rinfo->type;
            entry->returnByRef = rinfo->isRef;
            return rinfo;
        }
    }
    entry->analyzing = false;
    error("no matching overload");
    return NULL;
}

bool analyzeCodeBody(CodePtr code, EnvPtr env, ReturnInfoPtr rinfo) {
    bool result = analyzeStatement(code->body, env, rinfo);
    if (!result)
        return false;
    if (!rinfo->type)
        rinfo->set(voidType, false);
    return true;
}

bool analyzeStatement(StatementPtr stmt, EnvPtr env, ReturnInfoPtr rinfo) {
    LocationContext loc(stmt->location);
    switch (stmt->objKind) {
    case BLOCK : {
        Block *x = (Block *)stmt.ptr();
        ReturnInfoPtr rinfo2 = new ReturnInfo();
        bool recursive = false;
        for (unsigned i = 0; i < x->statements.size(); ++i) {
            StatementPtr y = x->statements[i];
            if (y->objKind == BINDING) {
                env = analyzeBinding((Binding *)y.ptr(), env);
                if (!env) {
                    recursive = true;
                    break;
                }
            }
            else {
                bool result = analyzeStatement(y, env, rinfo2);
                if (!result) {
                    recursive = true;
                    break;
                }
            }
        }
        if (recursive && !rinfo2->type)
            return false;
        if (rinfo2->type.ptr())
            rinfo->set(rinfo2->type, rinfo2->isRef);
        break;
    }
    case LABEL :
    case BINDING :
    case ASSIGNMENT :
    case GOTO :
        break;
    case RETURN : {
        Return *x = (Return *)stmt.ptr();
        if (!x->expr) {
            rinfo->set(voidType, false);
        }
        else {
            AnalysisPtr result = analyze(x->expr, env);
            if (!result)
                return false;
            rinfo->set(result->type, false);
        }
        break;
    }
    case RETURN_REF : {
        ReturnRef *x = (ReturnRef *)stmt.ptr();
        AnalysisPtr result = analyze(x->expr, env);
        if (!result)
            return false;
        rinfo->set(result->type, true);
        break;
    }
    case IF : {
        If *x = (If *)stmt.ptr();
        bool thenResult = analyzeStatement(x->thenPart, env, rinfo);
        bool elseResult = true;
        if (x->elsePart.ptr())
            elseResult = analyzeStatement(x->elsePart, env, rinfo);
        return thenResult || elseResult;
    }
    case EXPR_STATEMENT :
        break;
    case WHILE : {
        While *x = (While *)stmt.ptr();
        analyzeStatement(x->body, env, rinfo);
        break;
    }
    case BREAK :
    case CONTINUE :
        break;
    case FOR : {
        For *x = (For *)stmt.ptr();
        if (!x->converted)
            x->converted = convertForStatement(x);
        return analyzeStatement(x->converted, env, rinfo);
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
        addLocal(env2, x->name, right.ptr());
        break;
    }
    case STATIC : {
        ValuePtr right = evaluateToStatic(x->expr, env);
        addLocal(env2, x->name, right.ptr());
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

ReturnInfoPtr analyzeInvokeExternal(ExternalProcedurePtr x,
                                    const vector<AnalysisPtr> &args) {
    if (!x->llvmFunc)
        initExternalProcedure(x);
    return new ReturnInfo(x->returnType2, false);
}



//
// analyzeInvokePrimOp
//

ReturnInfoPtr analyzeInvokePrimOp(PrimOpPtr x,
                                  const vector<AnalysisPtr> &args) {
    switch (x->primOpCode) {
    case PRIM_TypeP :
        return new ReturnInfo(boolType, false);
    case PRIM_TypeSize :
        return new ReturnInfo(int32Type, false);
    case PRIM_primitiveInit :
        return new ReturnInfo(voidType, false);
    case PRIM_primitiveDestroy :
        return new ReturnInfo(voidType, false);
    case PRIM_primitiveCopy :
        return new ReturnInfo(voidType, false);
    case PRIM_primitiveAssign :
        return new ReturnInfo(voidType, false);
    case PRIM_primitiveEqualsP :
        return new ReturnInfo(boolType, false);
    case PRIM_primitiveHash :
        return new ReturnInfo(int32Type, false);

    case PRIM_BoolTypeP :
        return new ReturnInfo(boolType, false);
    case PRIM_boolNot :
        return new ReturnInfo(boolType, false);
    case PRIM_boolTruth :
        return new ReturnInfo(boolType, false);

    case PRIM_IntegerTypeP :
        return new ReturnInfo(boolType, false);
    case PRIM_SignedIntegerTypeP :
        return new ReturnInfo(boolType, false);
    case PRIM_FloatTypeP :
        return new ReturnInfo(boolType, false);
    case PRIM_numericEqualsP :
        return new ReturnInfo(boolType, false);
    case PRIM_numericLesserP :
        return new ReturnInfo(boolType, false);
    case PRIM_numericAdd :
        ensureArity(args, 2);
        return new ReturnInfo(args[0]->type, false);
    case PRIM_numericSubtract :
        ensureArity(args, 2);
        return new ReturnInfo(args[0]->type, false);
    case PRIM_numericMultiply :
        ensureArity(args, 2);
        return new ReturnInfo(args[0]->type, false);
    case PRIM_numericDivide :
        ensureArity(args, 2);
        return new ReturnInfo(args[0]->type, false);
    case PRIM_numericNegate :
        ensureArity(args, 1);
        return new ReturnInfo(args[0]->type, false);

    case PRIM_integerRemainder :
        ensureArity(args, 2);
        return new ReturnInfo(args[0]->type, false);
    case PRIM_integerShiftLeft :
        ensureArity(args, 2);
        return new ReturnInfo(args[0]->type, false);
    case PRIM_integerShiftRight :
        ensureArity(args, 2);
        return new ReturnInfo(args[0]->type, false);
    case PRIM_integerBitwiseAnd :
        ensureArity(args, 2);
        return new ReturnInfo(args[0]->type, false);
    case PRIM_integerBitwiseOr :
        ensureArity(args, 2);
        return new ReturnInfo(args[0]->type, false);
    case PRIM_integerBitwiseXor :
        ensureArity(args, 2);
        return new ReturnInfo(args[0]->type, false);

    case PRIM_numericConvert :
        ensureArity(args, 2);
        return new ReturnInfo(args[0]->evaluateType(), false);

    case PRIM_VoidTypeP :
        return new ReturnInfo(boolType, false);

    case PRIM_CompilerObjectTypeP :
        return new ReturnInfo(boolType, false);

    case PRIM_PointerTypeP :
        return new ReturnInfo(boolType, false);
    case PRIM_PointerType :
        return new ReturnInfo(compilerObjectType, false);
    case PRIM_Pointer :
        error("Pointer type constructor cannot be invoked");
        break;
    case PRIM_PointeeType :
        return new ReturnInfo(compilerObjectType, false);

    case PRIM_addressOf :
        ensureArity(args, 1);
        return new ReturnInfo(pointerType(args[0]->type), false);
    case PRIM_pointerDereference : {
        ensureArity(args, 1);
        ensurePointerType(args[0]->type);
        PointerType *t = (PointerType *)args[0]->type.ptr();
        return new ReturnInfo(t->pointeeType, true);
    }
    case PRIM_pointerToInt :
        ensureArity(args, 2);
        return new ReturnInfo(args[0]->evaluateType(), false);
    case PRIM_intToPointer :
        ensureArity(args, 2);
        return new ReturnInfo(pointerType(args[0]->evaluateType()), false);
    case PRIM_pointerCast :
        ensureArity(args, 2);
        return new ReturnInfo(pointerType(args[0]->evaluateType()), false);
    case PRIM_allocateMemory :
        ensureArity(args, 2);
        return new ReturnInfo(pointerType(args[0]->evaluateType()), false);
    case PRIM_freeMemory :
        return new ReturnInfo(voidType, false);

    case PRIM_ArrayTypeP :
        return new ReturnInfo(boolType, false);
    case PRIM_ArrayType :
        return new ReturnInfo(compilerObjectType, false);
    case PRIM_Array :
        error("Array type constructor cannot be invoked");
        break;
    case PRIM_ArrayElementType :
        return new ReturnInfo(compilerObjectType, false);
    case PRIM_ArraySize :
        return new ReturnInfo(int32Type, false);
    case PRIM_array : {
        if (args.empty())
            error("atleast one argument required for creating an array");
        TypePtr elementType = args[0]->type;
        int n = (int)args.size();
        return new ReturnInfo(arrayType(elementType, n), false);
    }
    case PRIM_arrayRef : {
        ensureArity(args, 2);
        ensureArrayType(args[0]->type);
        ArrayType *t = (ArrayType *)args[0]->type.ptr();
        return new ReturnInfo(t->elementType, true);
    }

    case PRIM_TupleTypeP :
        return new ReturnInfo(boolType, false);
    case PRIM_TupleType :
        return new ReturnInfo(compilerObjectType, false);
    case PRIM_Tuple :
        error("Tuple type constructor cannot be invoked");
        break;
    case PRIM_TupleSize :
        return new ReturnInfo(int32Type, false);
    case PRIM_TupleElementType :
        return new ReturnInfo(compilerObjectType, false);
    case PRIM_TupleElementOffset :
        return new ReturnInfo(int32Type, false);
    case PRIM_tuple : {
        if (args.size() < 2)
            error("tuples need atleast two elements");
        vector<TypePtr> elementTypes;
        for (unsigned i = 0; i < args.size(); ++i)
            elementTypes.push_back(args[i]->type);
        return new ReturnInfo(tupleType(elementTypes), false);
    }
    case PRIM_tupleRef : {
        ensureArity(args, 2);
        ensureTupleType(args[0]->type);
        int i = valueToInt(args[1]->evaluate());
        TupleType *t = (TupleType *)args[0]->type.ptr();
        if ((i < 0) || (i >= (int)t->elementTypes.size()))
            error("tuple type index out of range");
        return new ReturnInfo(t->elementTypes[i], true);
    }

    case PRIM_RecordTypeP :
        return new ReturnInfo(boolType, false);
    case PRIM_RecordType :
        return new ReturnInfo(compilerObjectType, false);
    case PRIM_RecordFieldCount :
        return new ReturnInfo(int32Type, false);
    case PRIM_RecordFieldType :
        return new ReturnInfo(compilerObjectType, false);
    case PRIM_RecordFieldOffset :
        return new ReturnInfo(int32Type, false);
    case PRIM_RecordFieldIndex :
        return new ReturnInfo(int32Type, false);
    case PRIM_recordFieldRef : {
        ensureArity(args, 2);
        ensureRecordType(args[0]->type);
        RecordType *rt = (RecordType *)args[0]->type.ptr();
        int i = valueToInt(args[1]->evaluate());
        const vector<TypePtr> &fieldTypes = recordFieldTypes(rt);
        if ((i < 0) || (i >= (int)fieldTypes.size()))
            error("field index out of range");
        return new ReturnInfo(fieldTypes[i], true);
    }
    case PRIM_recordFieldRefByName : {
        ensureArity(args, 2);
        ensureRecordType(args[0]->type);
        RecordType *t = (RecordType *)args[0]->type.ptr();
        ObjectPtr obj = valueToCO(args[1]->evaluate());
        if (obj->objKind != IDENTIFIER)
            error("expecting an identifier value");
        Identifier *name = (Identifier *)obj.ptr();
        const map<string, int> &fieldIndexMap = recordFieldIndexMap(t);
        map<string, int>::const_iterator fi = fieldIndexMap.find(name->str);
        if (fi == fieldIndexMap.end())
            error("field not in record");
        int i = fi->second;
        const vector<TypePtr> &fieldTypes = recordFieldTypes(t);
        return new ReturnInfo(fieldTypes[i], true);
    }
    case PRIM_recordInit :
        return new ReturnInfo(voidType, false);
    case PRIM_recordDestroy :
        return new ReturnInfo(voidType, false);
    case PRIM_recordCopy :
        return new ReturnInfo(voidType, false);
    case PRIM_recordAssign :
        return new ReturnInfo(voidType, false);
    case PRIM_recordEqualsP :
        return new ReturnInfo(boolType, false);
    case PRIM_recordHash :
        return new ReturnInfo(int32Type, false);
    default :
        assert(false);
    }
    return NULL;
}
