#include "clay.hpp"
#include "invokeutil2.hpp"



//
// declarations
//

PValuePtr
partialIndexing(ObjectPtr obj, ArgListPtr args);

PValuePtr
partialInvokeRecord(RecordPtr x, ArgListPtr args);

PValuePtr
partialInvokeType(TypePtr x, ArgListPtr args);

PValuePtr
partialInvokeProcedure(ProcedurePtr x, ArgListPtr args);

PValuePtr
partialInvokeOverloadable(OverloadablePtr x, ArgListPtr args);

struct ReturnInfo {
    TypePtr type;
    bool byRef;
    ReturnInfo()
        : byRef(false) {}
    void set(TypePtr type, bool byRef);
};

bool
partialEvalCodeBody(CodePtr code, EnvPtr env, ReturnInfo &rinfo);

EnvPtr
partialEvalBinding(BindingPtr x, EnvPtr env);

bool
partialEvalStatement(StatementPtr stmt, EnvPtr env, ReturnInfo &rinfo);

PValuePtr
partialInvokeExternal(ExternalProcedurePtr x, ArgListPtr args);

PValuePtr
partialInvokePrimOp(PrimOpPtr x, ArgListPtr args);



//
// partialEval
//

PValuePtr partialEvalRoot(ExprPtr expr, EnvPtr env)
{
    pushTempBlock();
    PValuePtr pv = partialEval(expr, env);
    popTempBlock();
    return pv;
}

PValuePtr partialEval(ExprPtr expr, EnvPtr env)
{
    LocationContext loc(expr->location);

    switch (expr->objKind) {

    case BOOL_LITERAL :
        return new PValue(boolType, true, true);

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
        return new PValue(t, true, true);
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
        return new PValue(t, true, true);
    }

    case CHAR_LITERAL : {
        CharLiteral *x = (CharLiteral *)expr.ptr();
        if (!x->converted)
            x->converted = convertCharLiteral(x->value);
        return partialEval(x->converted, env);
    }

    case STRING_LITERAL : {
        StringLiteral *x = (StringLiteral *)expr.ptr();
        if (!x->converted)
            x->converted = convertStringLiteral(x->value);
        return partialEval(x->converted, env);
    }

    case NAME_REF : {
        NameRef *x = (NameRef *)expr.ptr();
        ObjectPtr y = lookupEnv(env, x->name);
        if (y->objKind == VALUE) {
            Value *z = (Value *)y.ptr();
            return new PValue(z->type, true, true);
        }
        else if (y->objKind == PVALUE) {
            PValue *z = (PValue *)y.ptr();
            return new PValue(z->type, false, false);
        }
        else if (y->objKind == CVALUE) {
            CValue *z = (CValue *)y.ptr();
            return new PValue(z->type, false, false);
        }
        return new PValue(compilerObjectType, true, true);
    }

    case TUPLE : {
        Tuple *x = (Tuple *)expr.ptr();
        if (!x->converted)
            x->converted = convertTuple(x);
        return partialEval(x->converted, env);
    }

    case ARRAY : {
        Array *x = (Array *)expr.ptr();
        if (!x->converted)
            x->converted = convertArray(x);
        return partialEval(x->converted, env);
    }

    case INDEXING : {
        Indexing *x = (Indexing *)expr.ptr();
        PValuePtr indexable = partialEval(x->expr, env);
        if (!indexable)
            return NULL;
        if (indexable->type == compilerObjectType) {
            ObjectPtr indexable2 = lower(evaluateToStatic(x->expr, env));
            return partialIndexing(indexable2, new ArgList(x->args, env));
        }
        error("invalid indexing operation");
        return NULL;
    }

    case CALL : {
        Call *x = (Call *)expr.ptr();
        PValuePtr callable = partialEval(x->expr, env);
        if (!callable)
            return NULL;
        if (callable->type == compilerObjectType) {
            ObjectPtr callable2 = lower(evaluateToStatic(x->expr, env));
            return partialInvoke(callable2, new ArgList(x->args, env));
        }
        else if (callable->type->typeKind == FUNCTION_POINTER_TYPE) {
            FunctionPointerType *t =
                (FunctionPointerType *)callable->type.ptr();
            ArgListPtr args = new ArgList(x->args, env);
            return new PValue(t->returnType, true, args->allStatic);
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
        return partialInvoke(prim, new ArgList(args, env));
    }

    case TUPLE_REF : {
        TupleRef *x = (TupleRef *)expr.ptr();
        ValuePtr index = intToValue(x->index);
        vector<ExprPtr> args;
        args.push_back(x->expr);
        args.push_back(new ValueExpr(index));
        ObjectPtr prim = primName("tupleRef");
        return partialInvoke(prim, new ArgList(args, env));
    }

    case UNARY_OP : {
        UnaryOp *x = (UnaryOp *)expr.ptr();
        if (!x->converted)
            x->converted = convertUnaryOp(x);
        return partialEval(x->converted, env);
    }

    case BINARY_OP : {
        BinaryOp *x = (BinaryOp *)expr.ptr();
        if (!x->converted)
            x->converted = convertBinaryOp(x);
        return partialEval(x->converted, env);
    }

    case AND : {
        And *x = (And *)expr.ptr();
        PValuePtr a = partialEval(x->expr1, env);
        PValuePtr b = partialEval(x->expr2, env);
        if (!a || !b)
            return NULL;
        if (a->type != b->type)
            error("type mismatch in 'and' expression");
        return new PValue(a->type,
                          a->isTemp || b->isTemp,
                          a->isStatic && b->isStatic);
    }

    case OR : {
        Or *x = (Or *)expr.ptr();
        PValuePtr a = partialEval(x->expr1, env);
        PValuePtr b = partialEval(x->expr2, env);
        if (!a || !b)
            return NULL;
        if (a->type != b->type)
            error("type mismatch in 'or' expression");
        return new PValue(a->type,
                          a->isTemp || b->isTemp,
                          a->isStatic && b->isStatic);
    }

    case SC_EXPR : {
        SCExpr *x = (SCExpr *)expr.ptr();
        return partialEval(x->expr, x->env);
    }

    case VALUE_EXPR : {
        ValueExpr *x = (ValueExpr *)expr.ptr();
        return new PValue(x->value->type, true, true);
    }

    case CVALUE_EXPR : {
        CValueExpr *x = (CValueExpr *)expr.ptr();
        return new PValue(x->cvalue->type, false, false);
    }

    default :
        assert(false);
        return NULL;
    }
}



//
// partialIndexing
//

PValuePtr
partialIndexing(ObjectPtr obj, ArgListPtr args)
{
    if (args->recursionError)
        return NULL;
    switch (obj->objKind) {
    case RECORD :
        return new PValue(compilerObjectType, true, args->allStatic);
    case PRIM_OP : {
        PrimOp *x = (PrimOp *)obj.ptr();
        switch (x->primOpCode) {
        case PRIM_Pointer :
        case PRIM_FunctionPointer :
        case PRIM_Array :
        case PRIM_Tuple :
            return new PValue(compilerObjectType, true, args->allStatic);
        }
    }
    }
    error("invalid indexing operation");
    return NULL;
}



//
// partialInvoke
//

PValuePtr
partialInvoke(ObjectPtr obj, ArgListPtr args)
{
    if (args->recursionError)
        return NULL;
    switch (obj->objKind) {
    case RECORD :
        return partialInvokeRecord((Record *)obj.ptr(), args);
    case TYPE :
        return partialInvokeType((Type *)obj.ptr(), args);
    case PROCEDURE :
        return partialInvokeProcedure((Procedure *)obj.ptr(), args);
    case OVERLOADABLE :
        return partialInvokeOverloadable((Overloadable *)obj.ptr(), args);
    case EXTERNAL_PROCEDURE :
        return partialInvokeExternal((ExternalProcedure *)obj.ptr(), args);
    case PRIM_OP :
        return partialInvokePrimOp((PrimOp *)obj.ptr(), args);
    }
    error("invalid operation");
    return NULL;
}



//
// partialInvokeRecord
//

PValuePtr
partialInvokeRecord(RecordPtr x, ArgListPtr args)
{
    vector<PatternCellPtr> cells;
    EnvPtr env = initPatternVars(x->env, x->patternVars, cells);
    args->ensureUnifyRecordFields(x->fields, env);
    vector<ValuePtr> cellValues;
    derefCells(cells, cellValues);
    TypePtr t = recordType(x, cellValues);
    return partialInvokeType(t, args);
}



//
// partialInvokeType
//

PValuePtr
partialInvokeType(TypePtr x, ArgListPtr args)
{
    return new PValue(x, true, args->allStatic);
}



//
// partialInvokeProcedure
//

EnvPtr
bindPartialDynamicArgs(InvokeTableEntryPtr entry, ArgListPtr args)
{
    EnvPtr env = new Env(entry->env);
    const vector<FormalArgPtr> &formalArgs = entry->code->formalArgs;
    const vector<bool> &isStaticFlags = entry->table->isStaticFlags;
    for (unsigned i = 0; i < isStaticFlags.size(); ++i) {
        if (!isStaticFlags[i]) {
            PValuePtr parg = new PValue(args->type(i), false, false);
            ValueArg *x = (ValueArg *)formalArgs[i].ptr();
            addLocal(env, x->name, parg.ptr());
        }
    }
    return env;
}

PValuePtr
partialInvokeProcedure(ProcedurePtr x, ArgListPtr args)
{
    InvokeTableEntryPtr entry = lookupProcedureInvoke(x, args);
    if (entry->returnType.ptr())
        return new PValue(entry->returnType,
                          !entry->returnByRef,
                          args->allStatic);

    if (entry->analyzing)
        return NULL;

    entry->analyzing = true;

    if (!entry->env)
        initProcedureEnv(x, args, entry);

    EnvPtr env = bindPartialDynamicArgs(entry, args);

    ReturnInfo rinfo;
    bool flag = partialEvalCodeBody(entry->code, env, rinfo);
    if (!flag)
        error("recursive type inference");

    entry->analyzing = false;
    entry->returnType = rinfo.type;
    entry->returnByRef = rinfo.byRef;

    return new PValue(rinfo.type, !rinfo.byRef, args->allStatic);
}



//
// partialInvokeOverloadable
//

PValuePtr
partialInvokeOverloadable(OverloadablePtr x, ArgListPtr args)
{
    InvokeTableEntryPtr entry = lookupOverloadableInvoke(x, args);
    if (entry->returnType.ptr())
        return new PValue(entry->returnType,
                          !entry->returnByRef,
                          args->allStatic);

    if (entry->analyzing)
        return NULL;

    entry->analyzing = true;

    if (!entry->env)
        initOverloadableEnv(x, args, entry);

    EnvPtr env = bindPartialDynamicArgs(entry, args);

    ReturnInfo rinfo;
    bool flag = partialEvalCodeBody(entry->code, env, rinfo);
    if (!flag)
        error("recursive type inference");

    entry->analyzing = false;
    entry->returnType = rinfo.type;
    entry->returnByRef = rinfo.byRef;

    return new PValue(rinfo.type, !rinfo.byRef, args->allStatic);
}



//
// partialEvalCodeBody, partialEvalStatement
//

void
ReturnInfo::set(TypePtr type, bool byRef)
{
    if (!this->type) {
        this->type = type;
        this->byRef = byRef;
    }
    else if ((this->type != type) || (this->byRef != byRef)) {
        error("return type mismatch");
    }
}

bool
partialEvalCodeBody(CodePtr code, EnvPtr env, ReturnInfo &rinfo)
{
    bool result = partialEvalStatement(code->body, env, rinfo);
    if (!result)
        return false;
    if (!rinfo.type)
        rinfo.set(voidType, false);
    return true;
}

bool
partialEvalStatement(StatementPtr stmt, EnvPtr env, ReturnInfo &rinfo)
{
    LocationContext loc(stmt->location);

    switch (stmt->objKind) {
    case BLOCK : {
        Block *x = (Block *)stmt.ptr();
        ReturnInfo rinfo2;
        bool recursive = false;
        for (unsigned i = 0; i < x->statements.size(); ++i) {
            StatementPtr y = x->statements[i];
            if (y->objKind == BINDING) {
                env = partialEvalBinding((Binding *)y.ptr(), env);
                if (!env) {
                    recursive = true;
                    break;
                }
            }
            else {
                bool result = partialEvalStatement(y, env, rinfo2);
                if (!result) {
                    recursive = true;
                    break;
                }
            }
        }
        if (recursive && !rinfo2.type)
            return false;
        if (rinfo2.type.ptr())
            rinfo.set(rinfo2.type, rinfo2.byRef);
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
            rinfo.set(voidType, false);
        }
        else {
            PValuePtr result = partialEvalRoot(x->expr, env);
            if (!result)
                return false;
            rinfo.set(result->type, false);
        }
        break;
    }
    case RETURN_REF : {
        ReturnRef *x = (ReturnRef *)stmt.ptr();
        PValuePtr right = partialEvalRoot(x->expr, env);
        if (!right)
            return false;
        rinfo.set(right->type, true);
        break;
    }
    case IF : {
        If *x = (If *)stmt.ptr();
        bool thenResult = partialEvalStatement(x->thenPart, env, rinfo);
        bool elseResult = true;
        if (x->elsePart.ptr())
            elseResult = partialEvalStatement(x->elsePart, env, rinfo);
        return thenResult || elseResult;
    }
    case EXPR_STATEMENT :
        break;
    case WHILE : {
        While *x = (While *)stmt.ptr();
        partialEvalStatement(x->body, env, rinfo);
        break;
    }
    case BREAK :
    case CONTINUE :
        break;
    case FOR : {
        For *x = (For *)stmt.ptr();
        if (!x->converted)
            x->converted = convertForStatement(x);
        return partialEvalStatement(x->converted, env, rinfo);
    }
    default :
        assert(false);
    }
    return true;
}

EnvPtr
partialEvalBinding(BindingPtr x, EnvPtr env)
{
    EnvPtr env2 = new Env(env);
    switch (x->bindingKind) {
    case VAR :
    case REF : {
        PValuePtr right = partialEvalRoot(x->expr, env);
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
// partialInvokeExternal
//

PValuePtr
partialInvokeExternal(ExternalProcedurePtr x, ArgListPtr args)
{
    if (!x->llvmFunc)
        initExternalProcedure(x);
    return new PValue(x->returnType2, true, args->allStatic);
}



//
// partialInvokePrimOp
//

PValuePtr
partialInvokePrimOp(PrimOpPtr x, ArgListPtr args)
{
    switch (x->primOpCode) {
    case PRIM_TypeP :
        return new PValue(boolType, true, args->allStatic);
    case PRIM_TypeSize :
        return new PValue(int32Type, true, args->allStatic);
    case PRIM_primitiveInit :
        return new PValue(voidType, true, args->allStatic);
    case PRIM_primitiveDestroy :
        return new PValue(voidType, true, args->allStatic);
    case PRIM_primitiveCopy :
        return new PValue(voidType, true, args->allStatic);
    case PRIM_primitiveAssign :
        return new PValue(voidType, true, args->allStatic);

    case PRIM_boolNot :
        return new PValue(boolType, true, args->allStatic);
    case PRIM_boolTruth :
        return new PValue(boolType, true, args->allStatic);

    case PRIM_numericEqualsP :
        return new PValue(boolType, true, args->allStatic);
    case PRIM_numericLesserP :
        return new PValue(boolType, true, args->allStatic);
    case PRIM_numericAdd :
        args->ensureArity(2);
        return new PValue(args->type(0), true, args->allStatic);
    case PRIM_numericSubtract :
        args->ensureArity(2);
        return new PValue(args->type(0), true, args->allStatic);
    case PRIM_numericMultiply :
        args->ensureArity(2);
        return new PValue(args->type(0), true, args->allStatic);
    case PRIM_numericDivide :
        args->ensureArity(2);
        return new PValue(args->type(0), true, args->allStatic);
    case PRIM_numericNegate :
        args->ensureArity(1);
        return new PValue(args->type(0), true, args->allStatic);

    case PRIM_integerRemainder :
        args->ensureArity(2);
        return new PValue(args->type(0), true, args->allStatic);
    case PRIM_integerShiftLeft :
        args->ensureArity(2);
        return new PValue(args->type(0), true, args->allStatic);
    case PRIM_integerShiftRight :
        args->ensureArity(2);
        return new PValue(args->type(0), true, args->allStatic);
    case PRIM_integerBitwiseAnd :
        args->ensureArity(2);
        return new PValue(args->type(0), true, args->allStatic);
    case PRIM_integerBitwiseOr :
        args->ensureArity(2);
        return new PValue(args->type(0), true, args->allStatic);
    case PRIM_integerBitwiseXor :
        args->ensureArity(2);
        return new PValue(args->type(0), true, args->allStatic);
    case PRIM_integerBitwiseNot :
        args->ensureArity(1);
        return new PValue(args->type(0), true, args->allStatic);

    case PRIM_numericConvert :
        args->ensureArity(2);
        return new PValue(args->typeValue(0), true, args->allStatic);

    case PRIM_Pointer :
        error("Pointer type constructor cannot be invoked");
        break;

    case PRIM_addressOf :
        args->ensureArity(1);
        return new PValue(pointerType(args->type(0)), true, args->allStatic);
    case PRIM_pointerDereference : {
        args->ensureArity(1);
        ensurePointerType(args->type(0));
        PointerType *t = (PointerType *)args->type(0).ptr();
        return new PValue(t->pointeeType, false, args->allStatic);
    }
    case PRIM_pointerToInt :
        args->ensureArity(2);
        return new PValue(args->typeValue(0), true, args->allStatic);
    case PRIM_intToPointer :
        args->ensureArity(2);
        return new PValue(pointerType(args->typeValue(0)),
                          true,
                          args->allStatic);
    case PRIM_allocateMemory :
        args->ensureArity(2);
        return new PValue(pointerType(args->typeValue(0)), true, args->allStatic);
    case PRIM_freeMemory :
        return new PValue(voidType, true, args->allStatic);

    case PRIM_FunctionPointerTypeP :
        return new PValue(boolType, true, args->allStatic);
    case PRIM_FunctionPointer :
        error("FunctionPointer type constructor cannot be invoked");
    case PRIM_makeFunctionPointer : {
        if (args->size() < 1)
            error("incorrect no. of arguments");
        ValuePtr callable = args->value(0);
        ObjectPtr y = lower(callable);
        InvokeTablePtr table = getInvokeTable(y, args->size()-1);
        const vector<bool> &isStaticFlags = table->isStaticFlags;
        vector<ExprPtr> argExprs;
        vector<TypePtr> nonStaticArgTypes;
        for (unsigned i = 0; i < isStaticFlags.size(); ++i) {
            if (!isStaticFlags[i]) {
                TypePtr t = args->typeValue(i+1);
                CValuePtr cv = new CValue(t, NULL);
                argExprs.push_back(new CValueExpr(cv));
                nonStaticArgTypes.push_back(t);
            }
            else {
                ValuePtr v = args->value(i+1);
                argExprs.push_back(new ValueExpr(v));
            }
        }
        ArgListPtr dummyArgs = new ArgList(argExprs, new Env());
        PValuePtr pv = partialInvoke(y, dummyArgs);
        TypePtr fpType = functionPointerType(nonStaticArgTypes, pv->type);
        return new PValue(fpType, true, true);
    }

    case PRIM_pointerCast :
        args->ensureArity(2);
        return new PValue(args->typeValue(0), true, args->allStatic);

    case PRIM_Array :
        error("Array type constructor cannot be invoked");
        break;
    case PRIM_array : {
        if (args->size() == 0)
            error("atleast one argument required for creating an array");
        TypePtr elementType = args->type(0);
        int n = (int)args->size();
        return new PValue(arrayType(elementType, n), true, args->allStatic);
    }
    case PRIM_arrayRef : {
        args->ensureArity(2);
        ensureArrayType(args->type(0));
        ArrayType *t = (ArrayType *)args->type(0).ptr();
        return new PValue(t->elementType, false, args->allStatic);
    }

    case PRIM_TupleTypeP :
        return new PValue(boolType, true, args->allStatic);
    case PRIM_Tuple :
        error("Tuple type constructor cannot be invoked");
        break;
    case PRIM_TupleElementCount :
        return new PValue(int32Type, true, args->allStatic);
    case PRIM_TupleElementOffset :
        return new PValue(int32Type, true, args->allStatic);
    case PRIM_tuple : {
        if (args->size() < 2)
            error("tuples need atleast two elements");
        vector<TypePtr> elementTypes;
        for (unsigned i = 0; i < args->size(); ++i)
            elementTypes.push_back(args->type(i));
        return new PValue(tupleType(elementTypes), true, args->allStatic);
    }
    case PRIM_tupleRef : {
        args->ensureArity(2);
        ensureTupleType(args->type(0));
        int i = valueToInt(args->value(1));
        TupleType *t = (TupleType *)args->type(0).ptr();
        if ((i < 0) || (i >= (int)t->elementTypes.size()))
            error("tuple type index out of range");
        return new PValue(t->elementTypes[i], false, args->allStatic);
    }

    case PRIM_RecordTypeP :
        return new PValue(boolType, true, args->allStatic);
    case PRIM_RecordFieldCount :
        return new PValue(int32Type, true, args->allStatic);
    case PRIM_RecordFieldOffset :
        return new PValue(int32Type, true, args->allStatic);
    case PRIM_RecordFieldIndex :
        return new PValue(int32Type, true, args->allStatic);
    case PRIM_recordFieldRef : {
        args->ensureArity(2);
        ensureRecordType(args->type(0));
        RecordType *rt = (RecordType *)args->type(0).ptr();
        int i = valueToInt(args->value(1));
        const vector<TypePtr> &fieldTypes = recordFieldTypes(rt);
        if ((i < 0) || (i >= (int)fieldTypes.size()))
            error("field index out of range");
        return new PValue(fieldTypes[i], false, args->allStatic);
    }
    case PRIM_recordFieldRefByName : {
        args->ensureArity(2);
        ensureRecordType(args->type(0));
        RecordType *t = (RecordType *)args->type(0).ptr();
        ObjectPtr obj = valueToCO(args->value(1));
        if (obj->objKind != IDENTIFIER)
            error("expecting an identifier value");
        Identifier *name = (Identifier *)obj.ptr();
        const map<string, int> &fieldIndexMap = recordFieldIndexMap(t);
        map<string, int>::const_iterator fi = fieldIndexMap.find(name->str);
        if (fi == fieldIndexMap.end())
            error("field not in record");
        int i = fi->second;
        const vector<TypePtr> &fieldTypes = recordFieldTypes(t);
        return new PValue(fieldTypes[i], false, args->allStatic);
    }
    default :
        assert(false);
    }
    return NULL;
}
