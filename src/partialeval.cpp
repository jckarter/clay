#include "clay.hpp"



//
// declarations
//

PValuePtr
partialIndexing(ObjectPtr obj,
                ArgListPtr args);

PValuePtr
partialInvokeRecord(RecordPtr x,
                    ArgListPtr args);

PValuePtr
partialInvokeType(TypePtr x,
                  ArgListPtr args);

PValuePtr
partialInvokeProcedure(ProcedurePtr x,
                       ArgListPtr args);

PValuePtr
partialInvokeOverloadable(OverloadablePtr x,
                          ArgListPtr args);

struct ReturnInfo2 {
    TypePtr type;
    bool byRef;
    ReturnInfo2()
        : byRef(false) {}
    void set(TypePtr type, bool byRef);
};

bool
partialEvalCodeBody(CodePtr code,
                    EnvPtr env,
                    ReturnInfo2 &rinfo);

EnvPtr
partialEvalBinding(BindingPtr x,
                   EnvPtr env);

bool
partialEvalStatement(StatementPtr stmt,
                     EnvPtr env,
                     ReturnInfo2 &rinfo);

PValuePtr
partialInvokeExternal(ExternalProcedurePtr x,
                      ArgListPtr args);

PValuePtr
partialInvokePrimOp(PrimOpPtr x,
                    ArgListPtr args);



//
// definitions
//

ArgList::ArgList(const vector<ExprPtr> &exprs,
                 EnvPtr env)
    : Object(DONT_CARE), exprs(exprs), env(env), allStatic(true),
      recursionError(false)
{
    for (unsigned i = 0; i < exprs.size(); ++i) {
        PValuePtr pv = partialEval(exprs[i], env);
        if (!pv)
            recursionError = true;
        else if (!pv->isStatic)
            allStatic = false;
        this->pvalues.push_back(pv);
    }
    this->staticValues.resize(exprs.size());
}

PValuePtr
ArgList::partialValue(int i)
{
    return this->pvalues[i];
}

TypePtr
ArgList::type(int i)
{
    return this->pvalues[i]->type;
}

ValuePtr
ArgList::staticValue(int i)
{
    if (!this->staticValues[i])
        this->staticValues[i] = evaluateToStatic(this->exprs[i], this->env);
    return this->staticValues[i];
}

TypePtr
ArgList::typeValue(int i)
{
    return valueToType(this->staticValue(i));
}

void 
ArgList::ensureArity(int n)
{
    if ((int)this->exprs.size() != n)
        error("incorrect no. of arguments");
}

bool
ArgList::unifyFormalArg(int i,
                        FormalArgPtr farg,
                        EnvPtr fenv)
{
    switch (farg->objKind) {
    case VALUE_ARG : {
        ValueArg *x = (ValueArg *)farg.ptr();
        if (!x->type)
            return true;
        PatternPtr pattern = evaluatePattern(x->type, fenv);
        return unifyType(pattern, type(i));
    }
    case STATIC_ARG : {
        StaticArg *x = (StaticArg *)farg.ptr();
        PatternPtr pattern = evaluatePattern(x->pattern, fenv);
        return unify(pattern, this->staticValue(i));
    }
    default :
        assert(false);
        return false;
    }
}

bool
ArgList::unifyFormalArgs(const vector<FormalArgPtr> &fargs,
                         EnvPtr fenv)
{
    if (this->size() != fargs.size())
        return false;
    for (unsigned i = 0; i < fargs.size(); ++i) {
        if (!this->unifyFormalArg(i, fargs[i], fenv))
            return false;
    }
    return true;
}

void 
ArgList::ensureUnifyFormalArgs(const vector<FormalArgPtr> &fargs,
                               EnvPtr fenv)
{
    ensureArity(fargs.size());
    for (unsigned i = 0; i < fargs.size(); ++i) {
        if (!this->unifyFormalArg(i, fargs[i], fenv))
            error(this->exprs[i], "non-matching argment");
    }
}



//
// partialEval
//

PValuePtr
partialEval(ExprPtr expr, EnvPtr env)
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

    default :
        assert(false);
        return NULL;
    }
}



//
// partialIndexing
//

PValuePtr
partialIndexing(ObjectPtr obj,
                ArgListPtr args)
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
partialInvoke(ObjectPtr obj,
              ArgListPtr args)
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

EnvPtr
initPatternVars(EnvPtr parentEnv,
                const vector<IdentifierPtr> &patternVars,
                vector<PatternCellPtr> &cells)
{
    EnvPtr env = new Env(parentEnv);
    for (unsigned i = 0; i < patternVars.size(); ++i) {
        cells.push_back(new PatternCell(patternVars[i], NULL));
        addLocal(env, patternVars[i], cells[i].ptr());
    }
    return env;
}

void
derefCells(const vector<PatternCellPtr> &cells,
           vector<ValuePtr> &cellValues)
{
    for (unsigned i = 0; i < cells.size(); ++i)
        cellValues.push_back(derefCell(cells[i]));
}

EnvPtr
bindPatternVars(EnvPtr parentEnv,
                const vector<IdentifierPtr> &patternVars,
                const vector<PatternCellPtr> &cells)
{
    EnvPtr env = new Env(parentEnv);
    for (unsigned i = 0; i < patternVars.size(); ++i) {
        ValuePtr v = derefCell(cells[i]);
        addLocal(env, patternVars[i], v.ptr());
    }
    return env;
}

PValuePtr
partialInvokeRecord(RecordPtr x,
                    ArgListPtr args)
{
    vector<PatternCellPtr> cells;
    EnvPtr env = initPatternVars(x->env, x->patternVars, cells);
    args->ensureUnifyFormalArgs(x->formalArgs, env);
    vector<ValuePtr> cellValues;
    derefCells(cells, cellValues);
    TypePtr t = recordType(x, cellValues);
    return new PValue(t, true, args->allStatic);
}



//
// partialInvokeType
//

PValuePtr
partialInvokeType(TypePtr x,
                  ArgListPtr args)
{
    return new PValue(x, true, args->allStatic);
}



//
// partialInvokeProcedure
//

int
hashArgs(ArgListPtr args, const vector<bool> &isStaticFlags)
{
    int h = 0;
    for (unsigned i = 0; i < isStaticFlags.size(); ++i) {
        if (!isStaticFlags[i])
            h += toCOIndex(args->type(i).ptr());
        else
            h += valueHash(args->staticValue(i));
    }
    return h;
}

bool
matchingArgs(ArgListPtr args,
             InvokeTableEntryPtr entry)
{
    const vector<bool> &isStaticFlags = entry->table->isStaticFlags;
    const vector<ObjectPtr> &argsInfo = entry->argsInfo;
    for (unsigned i = 0; i < isStaticFlags.size(); ++i) {
        if (!isStaticFlags[i]) {
            TypePtr t = (Type *)argsInfo[i].ptr();
            if (args->type(i) != t)
                return false;
        }
        else {
            ValuePtr v = args->staticValue(i);
            if (!valueEquals(v, (Value *)argsInfo[i].ptr()))
                return false;
        }
    }
    return true;
}

void
initArgsInfo(InvokeTableEntryPtr entry,
             ArgListPtr args)
{
    const vector<bool> &isStaticFlags = entry->table->isStaticFlags;
    vector<ObjectPtr> &argsInfo = entry->argsInfo;
    for (unsigned i = 0; i < isStaticFlags.size(); ++i) {
        if (!isStaticFlags[i])
            argsInfo.push_back(args->type(i).ptr());
        else
            argsInfo.push_back(cloneValue(args->staticValue(i)).ptr());
    }
}

InvokeTableEntryPtr
findMatchingEntry(InvokeTablePtr table,
                  ArgListPtr args)
{
    int h = hashArgs(args, table->isStaticFlags);
    h &= (table->data.size() - 1);
    vector<InvokeTableEntryPtr> &entries = table->data[h];
    for (unsigned i = 0; i < entries.size(); ++i) {
        if (matchingArgs(args, entries[i]))
            return entries[i];
    }
    InvokeTableEntryPtr entry = new InvokeTableEntry();
    entry->table = table;
    entries.push_back(entry);
    initArgsInfo(entry, args);
    return entry;
}

InvokeTablePtr
newInvokeTable(const vector<FormalArgPtr> &formalArgs) {
    InvokeTablePtr table = new InvokeTable();
    for (unsigned i = 0; i < formalArgs.size(); ++i) {
        bool isStatic = formalArgs[i]->objKind == STATIC_ARG;
        table->isStaticFlags.push_back(isStatic);
    }
    table->data.resize(64);
    return table;
}

void
initProcedureInvokeTable(ProcedurePtr x)
{
    x->invokeTable = newInvokeTable(x->code->formalArgs);
}

InvokeTableEntryPtr
lookupProcedureInvoke(ProcedurePtr x,
                      ArgListPtr args)
{
    InvokeTablePtr table = x->invokeTable;
    if (!table) {
        initProcedureInvokeTable(x);
        table = x->invokeTable;
    }
    args->ensureArity(x->code->formalArgs.size());
    return findMatchingEntry(table, args);
}

void
initProcedureEnv(ProcedurePtr x,
                 ArgListPtr args,
                 InvokeTableEntryPtr entry)
{
    CodePtr code = x->code;
    vector<PatternCellPtr> cells;
    EnvPtr patternEnv = initPatternVars(x->env, code->patternVars, cells);
    args->ensureUnifyFormalArgs(code->formalArgs, patternEnv);
    EnvPtr env = bindPatternVars(x->env, code->patternVars, cells);
    if (code->predicate.ptr()) {
        bool result = evaluateToBool(code->predicate, env);
        if (!result)
            error(code->predicate, "procedure predicate failed");
    }
    entry->env = env;
    entry->code = code;
}

EnvPtr
bindPartialDynamicArgs(InvokeTableEntryPtr entry,
                       ArgListPtr args)
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
partialInvokeProcedure(ProcedurePtr x,
                       ArgListPtr args)
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

    ReturnInfo2 rinfo;
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

void
initOverloadableInvokeTables(OverloadablePtr x)
{
    for (unsigned i = 0; i < x->overloads.size(); ++i) {
        const vector<FormalArgPtr> &formalArgs =
            x->overloads[i]->code->formalArgs;
        unsigned nArgs = formalArgs.size();
        if (x->invokeTables.size() <= nArgs)
            x->invokeTables.resize(nArgs + 1);
        if (!x->invokeTables[nArgs]) {
            x->invokeTables[nArgs] = newInvokeTable(formalArgs);
        }
        else {
            InvokeTablePtr table = x->invokeTables[nArgs];
            for (unsigned j = 0; j < formalArgs.size(); ++j) {
                if (table->isStaticFlags[j]) {
                    if (formalArgs[j]->objKind != STATIC_ARG)
                        error(formalArgs[j], "expecting static argument");
                }
                else {
                    if (formalArgs[j]->objKind == STATIC_ARG)
                        error(formalArgs[j], "expecting non static argument");
                }
            }
        }
    }
}

InvokeTableEntryPtr
lookupOverloadableInvoke(OverloadablePtr x,
                         ArgListPtr args)
{
    if (x->invokeTables.empty())
        initOverloadableInvokeTables(x);
    if (x->invokeTables.size() <= args->size())
        error("no matching overload");
    InvokeTablePtr table = x->invokeTables[args->size()];
    if (!table)
        error("no matching overload");
    return findMatchingEntry(table, args);
}

void
initOverloadableEnv(OverloadablePtr x,
                    ArgListPtr args,
                    InvokeTableEntryPtr entry)
{
    for (unsigned i = 0; i < x->overloads.size(); ++i) {
        OverloadPtr y = x->overloads[i];
        CodePtr code = y->code;
        vector<PatternCellPtr> cells;
        EnvPtr patternEnv = initPatternVars(y->env, code->patternVars, cells);
        if (!args->unifyFormalArgs(code->formalArgs, patternEnv))
            continue;
        EnvPtr env = bindPatternVars(y->env, code->patternVars, cells);
        if (code->predicate.ptr()) {
            bool result = evaluateToBool(code->predicate, env);
            if (!result)
                continue;
        }
        entry->env = env;
        entry->code = code;
        return;
    }
    error("no matching overload");
}

PValuePtr
partialInvokeOverloadable(OverloadablePtr x,
                          ArgListPtr args)
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

    ReturnInfo2 rinfo;
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
ReturnInfo2::set(TypePtr type,
                 bool byRef)
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
partialEvalCodeBody(CodePtr code,
                    EnvPtr env,
                    ReturnInfo2 &rinfo)
{
    bool result = partialEvalStatement(code->body, env, rinfo);
    if (!result)
        return false;
    if (!rinfo.type)
        rinfo.set(voidType, false);
    return true;
}

bool
partialEvalStatement(StatementPtr stmt,
                     EnvPtr env,
                     ReturnInfo2 &rinfo)
{
    LocationContext loc(stmt->location);

    switch (stmt->objKind) {
    case BLOCK : {
        Block *x = (Block *)stmt.ptr();
        ReturnInfo2 rinfo2;
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
            PValuePtr result = partialEval(x->expr, env);
            if (!result)
                return false;
            rinfo.set(result->type, false);
        }
        break;
    }
    case RETURN_REF : {
        ReturnRef *x = (ReturnRef *)stmt.ptr();
        PValuePtr right = partialEval(x->expr, env);
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
partialEvalBinding(BindingPtr x,
                   EnvPtr env)
{
    EnvPtr env2 = new Env(env);
    switch (x->bindingKind) {
    case VAR :
    case REF : {
        PValuePtr right = partialEval(x->expr, env);
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
partialInvokeExternal(ExternalProcedurePtr x,
                      ArgListPtr args)
{
    if (!x->llvmFunc)
        initExternalProcedure(x);
    return new PValue(x->returnType2, false, args->allStatic);
}



//
// partialInvokePrimOp
//

PValuePtr
partialInvokePrimOp(PrimOpPtr x,
                    ArgListPtr args)
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
    case PRIM_primitiveEqualsP :
        return new PValue(boolType, true, args->allStatic);
    case PRIM_primitiveHash :
        return new PValue(int32Type, true, args->allStatic);

    case PRIM_BoolTypeP :
        return new PValue(boolType, true, args->allStatic);
    case PRIM_boolNot :
        return new PValue(boolType, true, args->allStatic);
    case PRIM_boolTruth :
        return new PValue(boolType, true, args->allStatic);

    case PRIM_IntegerTypeP :
        return new PValue(boolType, true, args->allStatic);
    case PRIM_SignedIntegerTypeP :
        return new PValue(boolType, true, args->allStatic);
    case PRIM_FloatTypeP :
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

    case PRIM_numericConvert :
        args->ensureArity(2);
        return new PValue(args->typeValue(0), true, args->allStatic);

    case PRIM_VoidTypeP :
        return new PValue(boolType, true, args->allStatic);

    case PRIM_CompilerObjectTypeP :
        return new PValue(boolType, true, args->allStatic);

    case PRIM_PointerTypeP :
        return new PValue(boolType, true, args->allStatic);
    case PRIM_PointerType :
        return new PValue(compilerObjectType, true, args->allStatic);
    case PRIM_Pointer :
        error("Pointer type constructor cannot be invoked");
        break;
    case PRIM_PointeeType :
        return new PValue(compilerObjectType, true, args->allStatic);

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
    case PRIM_pointerCast :
        args->ensureArity(2);
        return new PValue(pointerType(args->typeValue(0)),
                          true,
                          args->allStatic);
    case PRIM_allocateMemory :
        args->ensureArity(2);
        return new PValue(pointerType(args->typeValue(0)), true, args->allStatic);
    case PRIM_freeMemory :
        return new PValue(voidType, true, args->allStatic);

    case PRIM_ArrayTypeP :
        return new PValue(boolType, true, args->allStatic);
    case PRIM_ArrayType :
        return new PValue(compilerObjectType, true, args->allStatic);
    case PRIM_Array :
        error("Array type constructor cannot be invoked");
        break;
    case PRIM_ArrayElementType :
        return new PValue(compilerObjectType, true, args->allStatic);
    case PRIM_ArraySize :
        return new PValue(int32Type, true, args->allStatic);
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
    case PRIM_TupleType :
        return new PValue(compilerObjectType, true, args->allStatic);
    case PRIM_Tuple :
        error("Tuple type constructor cannot be invoked");
        break;
    case PRIM_TupleSize :
        return new PValue(int32Type, true, args->allStatic);
    case PRIM_TupleElementType :
        return new PValue(compilerObjectType, true, args->allStatic);
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
        int i = valueToInt(args->staticValue(1));
        TupleType *t = (TupleType *)args->type(0).ptr();
        if ((i < 0) || (i >= (int)t->elementTypes.size()))
            error("tuple type index out of range");
        return new PValue(t->elementTypes[i], false, args->allStatic);
    }

    case PRIM_RecordTypeP :
        return new PValue(boolType, true, args->allStatic);
    case PRIM_RecordType :
        return new PValue(compilerObjectType, true, args->allStatic);
    case PRIM_RecordFieldCount :
        return new PValue(int32Type, true, args->allStatic);
    case PRIM_RecordFieldType :
        return new PValue(compilerObjectType, true, args->allStatic);
    case PRIM_RecordFieldOffset :
        return new PValue(int32Type, true, args->allStatic);
    case PRIM_RecordFieldIndex :
        return new PValue(int32Type, true, args->allStatic);
    case PRIM_recordFieldRef : {
        args->ensureArity(2);
        ensureRecordType(args->type(0));
        RecordType *rt = (RecordType *)args->type(0).ptr();
        int i = valueToInt(args->staticValue(1));
        const vector<TypePtr> &fieldTypes = recordFieldTypes(rt);
        if ((i < 0) || (i >= (int)fieldTypes.size()))
            error("field index out of range");
        return new PValue(fieldTypes[i], false, args->allStatic);
    }
    case PRIM_recordFieldRefByName : {
        args->ensureArity(2);
        ensureRecordType(args->type(0));
        RecordType *t = (RecordType *)args->type(0).ptr();
        ObjectPtr obj = valueToCO(args->staticValue(1));
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
    case PRIM_recordInit :
        return new PValue(voidType, true, args->allStatic);
    case PRIM_recordDestroy :
        return new PValue(voidType, true, args->allStatic);
    case PRIM_recordCopy :
        return new PValue(voidType, true, args->allStatic);
    case PRIM_recordAssign :
        return new PValue(voidType, true, args->allStatic);
    case PRIM_recordEqualsP :
        return new PValue(boolType, true, args->allStatic);
    case PRIM_recordHash :
        return new PValue(int32Type, true, args->allStatic);
    default :
        assert(false);
    }
    return NULL;
}
