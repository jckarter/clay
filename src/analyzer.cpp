#include "clay.hpp"



//
// analyzeValue
//

ObjectPtr analyzeMaybeVoidValue(ExprPtr expr, EnvPtr env)
{
    ObjectPtr v = analyze(expr, env);
    if (!v)
        return NULL;

    switch (v->objKind) {

    case VALUE_HOLDER : {
        ValueHolder *x = (ValueHolder *)v.ptr();
        return new PValue(x->type, true);
    }

    case PVALUE :
        return (PValue *)v.ptr();

    case VOID_VALUE :
        return voidValue.ptr();

    default :
        error(expr, "expecting a value or void");
        return NULL;
    }
}

PValuePtr analyzeValue(ExprPtr expr, EnvPtr env)
{
    ObjectPtr v = analyze(expr, env);
    if (!v)
        return NULL;

    switch (v->objKind) {

    case VALUE_HOLDER : {
        ValueHolder *x = (ValueHolder *)v.ptr();
        return new PValue(x->type, true);
    }

    case PVALUE :
        return (PValue *)v.ptr();

    default :
        error(expr, "expecting a value");
        return NULL;
    }
}

PValuePtr analyzePointerValue(ExprPtr expr, EnvPtr env)
{
    PValuePtr pv = analyzeValue(expr, env);
    if (!pv)
        return NULL;
    if (pv->type->typeKind != POINTER_TYPE)
        error(expr, "expecting a pointer value");
    return pv;
}

PValuePtr analyzeArrayValue(ExprPtr expr, EnvPtr env)
{
    PValuePtr pv = analyzeValue(expr, env);
    if (!pv)
        return NULL;
    if (pv->type->typeKind != ARRAY_TYPE)
        error(expr, "expecting an array value");
    return pv;
}

PValuePtr analyzeTupleValue(ExprPtr expr, EnvPtr env)
{
    PValuePtr pv = analyzeValue(expr, env);
    if (!pv)
        return NULL;
    if (pv->type->typeKind != TUPLE_TYPE)
        error(expr, "expecting a tuple value");
    return pv;
}

PValuePtr analyzeRecordValue(ExprPtr expr, EnvPtr env)
{
    PValuePtr pv = analyzeValue(expr, env);
    if (!pv)
        return NULL;
    if (pv->type->typeKind != RECORD_TYPE)
        error(expr, "expecting a record value");
    return pv;
}



//
// analyze
//

ObjectPtr analyze(ExprPtr expr, EnvPtr env)
{
    LocationContext loc(expr->location);

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
        ObjectPtr y = lookupEnv(env, x->name);
        switch (y->objKind) {
        case STATIC_GLOBAL : {
            StaticGlobal *z = (StaticGlobal *)y.ptr();
            if (!z->result) {
                if (z->analyzing)
                    return false;
                z->analyzing = true;
                z->result = evaluateStatic(z->expr, z->env);
                z->analyzing = false;
            }
            return z->result;
        }
        case VALUE_HOLDER : {
            ValueHolder *z = (ValueHolder *)y.ptr();
            return new PValue(z->type, true);
        }
        case CVALUE : {
            CValue *z = (CValue *)y.ptr();
            return new PValue(z->type, false);
        }
        }
        return y;
    }

    case RETURNED : {
        ObjectPtr y = lookupEnv(env, new Identifier("%returned"));
        if (!y)
            error("'returned' requires explicit return type declaration");
        ReturnedInfo *z = (ReturnedInfo *)y.ptr();
        if (!z->returnType)
            error("'returned' cannot be used when returning void");
        if (!z->returnIsTemp)
            error("'returned' cannot be used when returning by reference");
        return new PValue(z->returnType, false);
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

    case LAMBDA : {
        Lambda *x = (Lambda *)expr.ptr();
        if (!x->initialized)
            initializeLambda(x, env);
        return analyze(x->converted, env);
    }

    case SC_EXPR : {
        SCExpr *x = (SCExpr *)expr.ptr();
        return analyze(x->expr, x->env);
    }

    case OBJECT_EXPR : {
        ObjectExpr *x = (ObjectExpr *)expr.ptr();
        return x->obj;
    }

    case CVALUE_EXPR : {
        CValueExpr *x = (CValueExpr *)expr.ptr();
        return new PValue(x->cv->type, false);
    }

    default :
        assert(false);
        return NULL;

    }
}



//
// analyzeIndexing
//

ObjectPtr analyzeIndexing(ObjectPtr x, const vector<ExprPtr> &args, EnvPtr env)
{
    switch (x->objKind) {

    case VALUE_HOLDER : {
        ValueHolder *y = (ValueHolder *)x.ptr();
        return analyzeIndexing(new PValue(y->type, true), args, env);
    }

    case PVALUE : {
        vector<ExprPtr> args2;
        args2.push_back(new ObjectExpr(x));
        args2.insert(args2.end(), args.begin(), args.end());
        ObjectPtr z = kernelName("index");
        return analyzeInvoke(z, args2, env);
    }

    case PRIM_OP : {
        PrimOpPtr y = (PrimOp *)x.ptr();

        switch (y->primOpCode) {

        case PRIM_Pointer : {
            ensureArity(args, 1);
            TypePtr t = evaluateType(args[0], env);
            return pointerType(t).ptr();
        }

        case PRIM_CodePointer : {
            if (args.size() < 1)
                error("atleast one argument required for"
                      " code pointer types.");
            vector<TypePtr> types;
            for (unsigned i = 0; i+1 < args.size(); ++i)
                types.push_back(evaluateType(args[i], env));
            TypePtr returnType = evaluateMaybeVoidType(args.back(), env);
            return codePointerType(types, returnType, true).ptr();
        }

        case PRIM_RefCodePointer : {
            if (args.size() < 1)
                error("atleast one argument required for"
                      " code pointer types.");
            vector<TypePtr> types;
            for (unsigned i = 0; i+1 < args.size(); ++i)
                types.push_back(evaluateType(args[i], env));
            TypePtr returnType = evaluateMaybeVoidType(args.back(), env);
            return codePointerType(types, returnType, false).ptr();
        }

        case PRIM_CCodePointer : {
            if (args.size() < 1)
                error("atleast one argument required for"
                      " code pointer types.");
            vector<TypePtr> types;
            for (unsigned i = 0; i+1 < args.size(); ++i)
                types.push_back(evaluateType(args[i], env));
            TypePtr returnType = evaluateMaybeVoidType(args.back(), env);
            return cCodePointerType(types, returnType).ptr();
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



//
// analyzeInvoke
//

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
    case EXTERNAL_PROCEDURE :
        return analyzeInvokeExternal((ExternalProcedure *)x.ptr(), args, env);
    case STATIC_PROCEDURE : {
        StaticProcedurePtr y = (StaticProcedure *)x.ptr();
        return analyzeInvokeStaticProcedure(y, args, env);
    }
    case STATIC_OVERLOADABLE : {
        StaticOverloadablePtr y = (StaticOverloadable *)x.ptr();
        return analyzeInvokeStaticOverloadable(y, args, env);
    }
    case PVALUE :
        return analyzeInvokeValue((PValue *)x.ptr(), args, env);
    case PRIM_OP :
        return analyzeInvokePrimOp((PrimOp *)x.ptr(), args, env);
    }
    error("invalid call operation");
    return NULL;
}



//
// analyzeInvokeType
//

ObjectPtr analyzeInvokeType(TypePtr x, const vector<ExprPtr> &args, EnvPtr env)
{
    const vector<bool> &isStaticFlags =
        lookupIsStaticFlags(x.ptr(), args.size());
    vector<ObjectPtr> argsKey;
    vector<LocationPtr> argLocations;
    if (!computeArgsKey(isStaticFlags, args, env, argsKey, argLocations))
        return NULL;
    InvokeEntryPtr entry =
        analyzeConstructor(x, isStaticFlags, argsKey, argLocations);
    if (!entry->analyzed)
        return NULL;
    return entry->analysis;
}

InvokeEntryPtr analyzeConstructor(TypePtr x,
                                  const vector<bool> &isStaticFlags,
                                  const vector<ObjectPtr> &argsKey,
                                  const vector<LocationPtr> &argLocations)
{
    InvokeEntryPtr entry = lookupInvoke(x.ptr(), isStaticFlags, argsKey);
    if (entry->analyzed || entry->analyzing)
        return entry;
    entry->analyzing = true;

    unsigned i = 0;
    for (; i < x->overloads.size(); ++i) {
        OverloadPtr y = x->overloads[i];
        MatchResultPtr result = matchInvoke(y->code, y->env, argsKey,
                                            y->target, x.ptr());
        if (result->matchCode == MATCH_SUCCESS) {
            entry->code = clone(y->code);
            MatchSuccess *z = (MatchSuccess *)result.ptr();
            entry->staticArgs = z->staticArgs;
            entry->argTypes = z->argTypes;
            entry->argNames = z->argNames;
            entry->env = z->env;
            break;
        }
    }
    if (i < x->overloads.size()) {
        analyzeCodeBody(entry);
    }
    else {
        verifyBuiltinConstructor(x, isStaticFlags, argsKey, argLocations);
        entry->isBuiltin = true;
        entry->analysis = new PValue(x, true);
        entry->returnType = x;
        entry->returnIsTemp = true;
        entry->analyzed = true;
    }

    entry->analyzing = false;
    return entry;
}



//
// analyzeInvokeRecord
//

ObjectPtr analyzeInvokeRecord(RecordPtr x,
                              const vector<ExprPtr> &args,
                              EnvPtr env)
{
    const vector<bool> &isStaticFlags =
        lookupIsStaticFlags(x.ptr(), args.size());
    vector<ObjectPtr> argsKey;
    vector<LocationPtr> argLocations;
    if (!computeArgsKey(isStaticFlags, args, env, argsKey, argLocations))
        return NULL;
    InvokeEntryPtr entry =
        analyzeRecordConstructor(x, isStaticFlags, argsKey, argLocations);
    if (!entry->analyzed)
        return NULL;
    return entry->analysis;
}

InvokeEntryPtr
analyzeRecordConstructor(RecordPtr x,
                         const vector<bool> &isStaticFlags,
                         const vector<ObjectPtr> &argsKey,
                         const vector<LocationPtr> &argLocations)
{
    InvokeEntryPtr entry = lookupInvoke(x.ptr(), isStaticFlags, argsKey);
    if (entry->analyzed || entry->analyzing)
        return entry;
    entry->analyzing = true;

    unsigned i = 0;
    for (; i < x->overloads.size(); ++i) {
        OverloadPtr y = x->overloads[i];
        MatchResultPtr result = matchInvoke(y->code, y->env, argsKey,
                                            NULL, NULL);
        if (result->matchCode == MATCH_SUCCESS) {
            entry->code = clone(y->code);
            MatchSuccess *z = (MatchSuccess *)result.ptr();
            entry->staticArgs = z->staticArgs;
            entry->argTypes = z->argTypes;
            entry->argNames = z->argNames;
            entry->env = z->env;
            break;
        }
    }
    if (i < x->overloads.size()) {
        analyzeCodeBody(entry);
    }
    else {
        error("no matching record constructor");
    }

    entry->analyzing = false;
    return entry;
}



//
// analyzeInvokeProcedure, analyzeInvokeOverloadable
//

ObjectPtr analyzeInvokeProcedure(ProcedurePtr x,
                                 const vector<ExprPtr> &args,
                                 EnvPtr env)
{
    const vector<bool> &isStaticFlags =
        lookupIsStaticFlags(x.ptr(), args.size());
    vector<ObjectPtr> argsKey;
    vector<LocationPtr> argLocations;
    if (!computeArgsKey(isStaticFlags, args, env, argsKey, argLocations))
        return NULL;
    InvokeEntryPtr entry =
        analyzeProcedure(x, isStaticFlags, argsKey, argLocations);
    if (!entry->analyzed)
        return NULL;
    return entry->analysis;
}

InvokeEntryPtr analyzeProcedure(ProcedurePtr x,
                                const vector<bool> &isStaticFlags,
                                const vector<ObjectPtr> &argsKey,
                                const vector<LocationPtr> &argLocations)
{
    InvokeEntryPtr entry = lookupInvoke(x.ptr(), isStaticFlags, argsKey);
    if (entry->analyzed || entry->analyzing)
        return entry;

    entry->analyzing = true;

    MatchResultPtr result = matchInvoke(x->code, x->env, argsKey, NULL, NULL);
    if (result->matchCode != MATCH_SUCCESS)
        signalMatchError(result, argLocations);

    entry->code = clone(x->code);
    MatchSuccess *y = (MatchSuccess *)result.ptr();
    entry->staticArgs = y->staticArgs;
    entry->argTypes = y->argTypes;
    entry->argNames = y->argNames;
    entry->env = y->env;

    analyzeCodeBody(entry);

    entry->analyzing = false;
    return entry;
}

ObjectPtr analyzeInvokeOverloadable(OverloadablePtr x,
                                    const vector<ExprPtr> &args,
                                    EnvPtr env)
{
    const vector<bool> &isStaticFlags =
        lookupIsStaticFlags(x.ptr(), args.size());
    vector<ObjectPtr> argsKey;
    vector<LocationPtr> argLocations;
    if (!computeArgsKey(isStaticFlags, args, env, argsKey, argLocations))
        return NULL;
    InvokeEntryPtr entry =
        analyzeOverloadable(x, isStaticFlags, argsKey, argLocations);
    if (!entry->analyzed)
        return NULL;
    return entry->analysis;
}

InvokeEntryPtr analyzeOverloadable(OverloadablePtr x,
                                   const vector<bool> &isStaticFlags,
                                   const vector<ObjectPtr> &argsKey,
                                   const vector<LocationPtr> &argLocations)
{
    InvokeEntryPtr entry = lookupInvoke(x.ptr(), isStaticFlags, argsKey);
    if (entry->analyzed || entry->analyzing)
        return entry;
    entry->analyzing = true;

    unsigned i = 0;
    for (; i < x->overloads.size(); ++i) {
        OverloadPtr y = x->overloads[i];
        MatchResultPtr result = matchInvoke(y->code, y->env, argsKey,
                                            NULL, NULL);
        if (result->matchCode == MATCH_SUCCESS) {
            entry->code = clone(y->code);
            MatchSuccess *z = (MatchSuccess *)result.ptr();
            entry->staticArgs = z->staticArgs;
            entry->argTypes = z->argTypes;
            entry->argNames = z->argNames;
            entry->env = z->env;
            break;
        }
    }
    if (i < x->overloads.size()) {
        analyzeCodeBody(entry);
    }
    else {
        error("no matching overload");
    }

    entry->analyzing = false;
    return entry;
}

void analyzeCodeBody(InvokeEntryPtr entry) {
    assert(!entry->analyzed);
    ObjectPtr result;
    CodePtr code = entry->code;
    if (code->returnType.ptr()) {
        TypePtr retType = evaluateMaybeVoidType(code->returnType, entry->env);
        if (!retType)
            result = voidValue.ptr();
        else
            result = new PValue(retType, !code->returnRef);
    }
    else {
        EnvPtr bodyEnv = new Env(entry->env);
        for (unsigned i = 0; i < entry->argNames.size(); ++i) {
            PValuePtr parg = new PValue(entry->argTypes[i], false);
            addLocal(bodyEnv, entry->argNames[i], parg.ptr());
        }
        addLocal(bodyEnv, new Identifier("%returned"), NULL);
        bool ok = analyzeStatement(code->body, bodyEnv, result);
        if (!result && !ok)
            return;
        if (!result)
            result = voidValue.ptr();
    }
    entry->analysis = result;
    entry->analyzed = true;
    if (result->objKind == PVALUE) {
        PValuePtr pret = (PValue *)result.ptr();
        entry->returnType = pret->type;
        entry->returnIsTemp = pret->isTemp;
    }
    else {
        assert(result->objKind == VOID_VALUE);
    }
}



//
// analyzeStatement
//

bool analyzeStatement(StatementPtr stmt, EnvPtr env, ObjectPtr &result)
{
    LocationContext loc(stmt->location);

    switch (stmt->objKind) {

    case BLOCK : {
        Block *x = (Block *)stmt.ptr();
        for (unsigned i = 0; i < x->statements.size(); ++i) {
            StatementPtr y = x->statements[i];
            if (y->objKind == BINDING) {
                env = analyzeBinding((Binding *)y.ptr(), env);
                if (!env)
                    return false;
            }
            else if (!analyzeStatement(y, env, result)) {
                return false;
            }
        }
        return true;
    }

    case LABEL :
    case BINDING :
    case ASSIGNMENT :
    case INIT_ASSIGNMENT :
    case UPDATE_ASSIGNMENT :
    case GOTO :
        return true;

    case RETURN : {
        Return *x = (Return *)stmt.ptr();
        if (!x->expr) {
            if (!result)
                result = voidValue.ptr();
            else if (result->objKind != VOID_VALUE)
                error("mismatching return kind");
        }
        else {
            PValuePtr pv = analyzeValue(x->expr, env);
            if (!pv)
                return false;
            PValuePtr y = new PValue(pv->type, true);
            if (!result)
                result = y.ptr();
            else if (result->objKind != PVALUE)
                error("mismatching return kind");
            PValue *prev = (PValue *)result.ptr();
            if (y->type != prev->type)
                error("mismatching return type");
            if (!prev->isTemp)
                error("mismatching return by ref & by value");
        }
        return true;
    }

    case RETURN_REF : {
        ReturnRef *x = (ReturnRef *)stmt.ptr();
        PValuePtr y = analyzeValue(x->expr, env);
        if (!y)
            return false;
        if (y->isTemp)
            error("cannot return a temporary by reference");
        if (!result)
            result = y.ptr();
        else if (result->objKind != PVALUE)
            error("mismatching return kind");
        PValue *prev = (PValue *)result.ptr();
        if (y->type != prev->type)
            error("mismatching return type");
        if (prev->isTemp)
            error("mismatching return by ref & by value");
        return true;
    }

    case IF : {
        If *x = (If *)stmt.ptr();
        bool thenResult = analyzeStatement(x->thenPart, env, result);
        bool elseResult = true;
        if (x->elsePart.ptr())
            elseResult = analyzeStatement(x->elsePart, env, result);
        return thenResult || elseResult;
    }

    case EXPR_STATEMENT :
        return true;

    case WHILE : {
        While *x = (While *)stmt.ptr();
        analyzeStatement(x->body, env, result);
        return true;
    }

    case BREAK :
    case CONTINUE :
        return true;

    case FOR : {
        For *x = (For *)stmt.ptr();
        if (!x->desugared)
            x->desugared = desugarForStatement(x);
        return analyzeStatement(x->desugared, env, result);
    }

    default :
        assert(false);
        return false;
    }
}

EnvPtr analyzeBinding(BindingPtr x, EnvPtr env)
{
    switch (x->bindingKind) {

    case VAR :
    case REF : {
        PValuePtr right = analyzeValue(x->expr, env);
        if (!right)
            return NULL;
        EnvPtr env2 = new Env(env);
        addLocal(env2, x->name, new PValue(right->type, false));
        return env2;
    }

    case STATIC : {
        ObjectPtr right = evaluateStatic(x->expr, env);
        if (!right)
            return NULL;
        EnvPtr env2 = new Env(env);
        addLocal(env2, x->name, right.ptr());
        return env2;
    }

    default :
        assert(false);
        return NULL;

    }
}



//
// analyzeInvokeExternal
//

ObjectPtr analyzeInvokeExternal(ExternalProcedurePtr x,
                                const vector<ExprPtr> &args,
                                EnvPtr env)
{
    if (!x->analyzed) {
        for (unsigned i = 0; i < x->args.size(); ++i) {
            ExternalArgPtr y = x->args[i];
            y->type2 = evaluateType(y->type, x->env);
        }
        x->returnType2 = evaluateMaybeVoidType(x->returnType, x->env);
        x->analyzed = true;
    }
    if (!x->returnType2)
        return voidValue.ptr();
    return new PValue(x->returnType2, true);
}



//
// analyzeInvokeStaticProcedure, analyzeInvokeStaticOverloadable
//

ObjectPtr analyzeInvokeStaticProcedure(StaticProcedurePtr x,
                                       const vector<ExprPtr> &argExprs,
                                       EnvPtr env)
{
    vector<ObjectPtr> args;
    vector<LocationPtr> argLocations;
    for (unsigned i = 0; i < argExprs.size(); ++i) {
        args.push_back(evaluateStatic(argExprs[i], env));
        argLocations.push_back(argExprs[i]->location);
    }

    StaticInvokeEntryPtr entry = lookupStaticInvoke(x.ptr(), args);
    if (!entry->result) {
        if (entry->analyzing)
            return NULL;
        entry->analyzing = true;

        MatchResultPtr result = matchStaticInvoke(x->code, x->env, args);
        if (result->matchCode != MATCH_SUCCESS)
            signalMatchError(result, argLocations);
        MatchSuccess *y = (MatchSuccess *)result.ptr();
        entry->result = evaluateStatic(x->code->body, y->env);

        entry->analyzing = false;
    }
    return entry->result;
}

ObjectPtr analyzeInvokeStaticOverloadable(StaticOverloadablePtr x,
                                          const vector<ExprPtr> &argExprs,
                                          EnvPtr env)
{
    vector<ObjectPtr> args;
    vector<LocationPtr> argLocations;
    for (unsigned i = 0; i < argExprs.size(); ++i) {
        args.push_back(evaluateStatic(argExprs[i], env));
        argLocations.push_back(argExprs[i]->location);
    }

    StaticInvokeEntryPtr entry = lookupStaticInvoke(x.ptr(), args);
    if (!entry->result) {
        if (entry->analyzing)
            return NULL;
        entry->analyzing = true;

        unsigned i = 0;
        for (; i < x->overloads.size(); ++i) {
            StaticOverloadPtr y = x->overloads[i];
            MatchResultPtr result = matchStaticInvoke(y->code, y->env, args);
            if (result->matchCode == MATCH_SUCCESS) {
                MatchSuccess *z = (MatchSuccess *)result.ptr();
                entry->result = evaluateStatic(y->code->body, z->env);
                break;
            }
        }
        if (i == x->overloads.size())
            error("no matching static overload");

        entry->analyzing = false;
    }
    return entry->result;
}

ObjectPtr analyzeInvokeValue(PValuePtr x, 
                             const vector<ExprPtr> &args,
                             EnvPtr env)
{
    switch (x->type->typeKind) {

    case CODE_POINTER_TYPE : {
        CodePointerType *y = (CodePointerType *)x->type.ptr();
        if (!y->returnType)
            return voidValue.ptr();
        return new PValue(y->returnType, y->returnIsTemp);
    }

    case CCODE_POINTER_TYPE : {
        CCodePointerType *y = (CCodePointerType *)x->type.ptr();
        if (!y->returnType)
            return voidValue.ptr();
        return new PValue(y->returnType, true);
    }

    default : {
        vector<ExprPtr> args2;
        args2.push_back(new ObjectExpr(x.ptr()));
        args2.insert(args2.end(), args.begin(), args.end());
        return analyzeInvoke(kernelName("call"), args2, env);
    }

    }
}

ObjectPtr analyzeInvokePrimOp(PrimOpPtr x,
                              const vector<ExprPtr> &args,
                              EnvPtr env)
{
    switch (x->primOpCode) {

    case PRIM_TypeOf : {
        ensureArity(args, 1);
        ObjectPtr y = analyzeMaybeVoidValue(args[0], env);
        if (!y)
            return NULL;
        switch (y->objKind) {
        case PVALUE : {
            PValue *z = (PValue *)y.ptr();
            return z->type.ptr();
        }
        case VOID_VALUE :
            return voidType.ptr();
        default :
            assert(false);
        }
        return NULL;
    }

    case PRIM_TypeP : {
        ensureArity(args, 1);
        ObjectPtr obj = evaluateStatic(args[0], env);
        return boolToValueHolder(obj->objKind == TYPE).ptr();
    }

    case PRIM_TypeSize : {
        ensureArity(args, 1);
        TypePtr t = evaluateType(args[0], env);
        return intToValueHolder(typeSize(t)).ptr();
    }

    case PRIM_primitiveCopy :
        return voidValue.ptr();

    case PRIM_boolNot :
        return new PValue(boolType, true);

    case PRIM_numericEqualsP :
        return new PValue(boolType, true);

    case PRIM_numericLesserP :
        return new PValue(boolType, true);

    case PRIM_numericAdd :
    case PRIM_numericSubtract :
    case PRIM_numericMultiply :
    case PRIM_numericDivide : {
        ensureArity(args, 2);
        PValuePtr y = analyzeValue(args[0], env);
        if (!y)
            return NULL;
        return new PValue(y->type, true);
    }

    case PRIM_numericNegate : {
        ensureArity(args, 1);
        PValuePtr y = analyzeValue(args[0], env);
        if (!y)
            return NULL;
        return new PValue(y->type, true);
    }

    case PRIM_integerRemainder :
    case PRIM_integerShiftLeft :
    case PRIM_integerShiftRight :
    case PRIM_integerBitwiseAnd :
    case PRIM_integerBitwiseOr :
    case PRIM_integerBitwiseXor : {
        ensureArity(args, 2);
        PValuePtr y = analyzeValue(args[0], env);
        if (!y)
            return NULL;
        return new PValue(y->type, true);
    }

    case PRIM_integerBitwiseNot : {
        ensureArity(args, 1);
        PValuePtr y = analyzeValue(args[0], env);
        if (!y)
            return NULL;
        return new PValue(y->type, true);
    }

    case PRIM_numericConvert : {
        ensureArity(args, 2);
        TypePtr t = evaluateNumericType(args[0], env);
        return new PValue(t, true);
    }

    case PRIM_Pointer :
        error("Pointer type constructor cannot be called");

    case PRIM_addressOf : {
        ensureArity(args, 1);
        PValuePtr y = analyzeValue(args[0], env);
        if (!y)
            return NULL;
        return new PValue(pointerType(y->type), true);
    }

    case PRIM_pointerDereference : {
        ensureArity(args, 1);
        PValuePtr y = analyzePointerValue(args[0], env);
        if (!y)
            return NULL;
        PointerType *t = (PointerType *)y->type.ptr();
        return new PValue(t->pointeeType, false);
    }

    case PRIM_pointerToInt : {
        ensureArity(args, 2);
        TypePtr t = evaluateIntegerType(args[0], env);
        return new PValue(t, true);
    }

    case PRIM_intToPointer : {
        ensureArity(args, 2);
        TypePtr t = evaluateType(args[0], env);
        return new PValue(pointerType(t), true);
    }

    case PRIM_CodePointerTypeP : {
        ensureArity(args, 1);
        ObjectPtr y = evaluateStatic(args[0], env);
        bool isCPType = false;
        if (y->objKind == TYPE) {
            Type *t = (Type *)y.ptr();
            if (t->typeKind == CODE_POINTER_TYPE)
                isCPType = true;
        }
        return boolToValueHolder(isCPType).ptr();
    }

    case PRIM_CodePointer :
        error("CodePointer type constructor cannot be called");

    case PRIM_RefCodePointer :
        error("RefCodePointer type constructor cannot be called");

    case PRIM_makeCodePointer : {
        if (args.size() < 1)
            error("incorrect no. of parameters");
        ObjectPtr callable = evaluateStatic(args[0], env);
        switch (callable->objKind) {
        case PROCEDURE :
        case OVERLOADABLE :
            break;
        default :
            error(args[0], "invalid procedure/overloadable");
        }
        const vector<bool> &isStaticFlags =
            lookupIsStaticFlags(callable, args.size()-1);
        vector<ObjectPtr> argsKey;
        vector<LocationPtr> argLocations;
        for (unsigned i = 1; i < args.size(); ++i) {
            if (!isStaticFlags[i-1]) {
                TypePtr t = evaluateType(args[i], env);
                argsKey.push_back(t.ptr());
            }
            else {
                ObjectPtr param = evaluateStatic(args[i], env);
                argsKey.push_back(param);
            }
            argLocations.push_back(args[i]->location);
        }
        InvokeEntryPtr entry;
        switch (callable->objKind) {
        case PROCEDURE : {
            Procedure *y = (Procedure *)callable.ptr();
            entry = analyzeProcedure(y, isStaticFlags,
                                     argsKey, argLocations);
            break;
        }
        case OVERLOADABLE : {
            Overloadable *y = (Overloadable *)callable.ptr();
            entry = analyzeOverloadable(y, isStaticFlags,
                                        argsKey, argLocations);
            break;
        }
        default :
            assert(false);
        }
        if (!entry->analyzed)
            return NULL;
        TypePtr cpType = codePointerType(entry->argTypes, entry->returnType,
                                         entry->returnIsTemp);
        return new PValue(cpType, true);
    }

    case PRIM_CCodePointerTypeP : {
        ensureArity(args, 1);
        ObjectPtr y = evaluateStatic(args[0], env);
        bool isCCPType = false;
        if (y->objKind == TYPE) {
            Type *t = (Type *)y.ptr();
            if (t->typeKind == CCODE_POINTER_TYPE)
                isCCPType = true;
        }
        return boolToValueHolder(isCCPType).ptr();
    }

    case PRIM_CCodePointer :
        error("CCodePointer type constructor cannot be called");

    case PRIM_makeCCodePointer : {
        if (args.size() < 1)
            error("incorrect no. of parameters");
        ObjectPtr callable = evaluateStatic(args[0], env);
        switch (callable->objKind) {
        case PROCEDURE :
        case OVERLOADABLE :
            break;
        default :
            error(args[0], "invalid procedure/overloadable");
        }
        const vector<bool> &isStaticFlags =
            lookupIsStaticFlags(callable, args.size()-1);
        vector<ObjectPtr> argsKey;
        vector<LocationPtr> argLocations;
        for (unsigned i = 1; i < args.size(); ++i) {
            if (!isStaticFlags[i-1]) {
                TypePtr t = evaluateType(args[i], env);
                argsKey.push_back(t.ptr());
            }
            else {
                ObjectPtr param = evaluateStatic(args[i], env);
                argsKey.push_back(param);
            }
            argLocations.push_back(args[i]->location);
        }
        InvokeEntryPtr entry;
        switch (callable->objKind) {
        case PROCEDURE : {
            Procedure *y = (Procedure *)callable.ptr();
            entry = analyzeProcedure(y, isStaticFlags,
                                     argsKey, argLocations);
            break;
        }
        case OVERLOADABLE : {
            Overloadable *y = (Overloadable *)callable.ptr();
            entry = analyzeOverloadable(y, isStaticFlags,
                                        argsKey, argLocations);
            break;
        }
        default :
            assert(false);
        }
        if (!entry->analyzed)
            return NULL;
        TypePtr ccpType = cCodePointerType(entry->argTypes, entry->returnType);
        return new PValue(ccpType, true);
    }

    case PRIM_pointerCast : {
        ensureArity(args, 2);
        TypePtr t = evaluatePointerLikeType(args[0], env);
        return new PValue(t, true);
    }

    case PRIM_Array :
        error("Array type constructor cannot be called");

    case PRIM_array : {
        if (args.size() < 1)
            error("atleast one element required for creating an array");
        PValuePtr y = analyzeValue(args[0], env);
        if (!y)
            return NULL;
        TypePtr z = arrayType(y->type, args.size());
        return new PValue(z, true);
    }

    case PRIM_arrayRef : {
        ensureArity(args, 2);
        PValuePtr y = analyzeArrayValue(args[0], env);
        if (!y)
            return NULL;
        ArrayType *z = (ArrayType *)y->type.ptr();
        return new PValue(z->elementType, false);
    }

    case PRIM_TupleTypeP : {
        ensureArity(args, 1);
        ObjectPtr y = evaluateStatic(args[0], env);
        bool isTupleType = false;
        if (y->objKind == TYPE) {
            Type *t = (Type *)y.ptr();
            if (t->typeKind == TUPLE_TYPE)
                isTupleType = true;
        }
        return boolToValueHolder(isTupleType).ptr();
    }

    case PRIM_Tuple :
        error("Tuple type constructor cannot be called");

    case PRIM_TupleElementCount : {
        ensureArity(args, 1);
        TypePtr y = evaluateTupleType(args[0], env);
        TupleType *z = (TupleType *)y.ptr();
        return intToValueHolder(z->elementTypes.size()).ptr();
    }

    case PRIM_TupleElementOffset : {
        ensureArity(args, 2);
        TypePtr y = evaluateTupleType(args[0], env);
        TupleType *z = (TupleType *)y.ptr();
        int i = evaluateInt(args[1], env);
        const llvm::StructLayout *layout = tupleTypeLayout(z);
        return intToValueHolder(layout->getElementOffset(i)).ptr();
    }

    case PRIM_tuple : {
        if (args.size() < 2)
            error("tuples require atleast two elements");
        vector<TypePtr> elementTypes;
        for (unsigned i = 0; i < args.size(); ++i) {
            PValuePtr y = analyzeValue(args[i], env);
            if (!y)
                return NULL;
            elementTypes.push_back(y->type);
        }
        return new PValue(tupleType(elementTypes), true);
    }

    case PRIM_tupleRef : {
        ensureArity(args, 2);
        PValuePtr y = analyzeTupleValue(args[0], env);
        if (!y)
            return NULL;
        int i = evaluateInt(args[1], env);
        TupleType *z = (TupleType *)y->type.ptr();
        if ((i < 0) || (i >= (int)z->elementTypes.size()))
            error(args[1], "tuple index out of range");
        return new PValue(z->elementTypes[i], false);
    }

    case PRIM_RecordTypeP : {
        ensureArity(args, 1);
        ObjectPtr y = evaluateStatic(args[0], env);
        bool isRecordType = false;
        if (y->objKind == TYPE) {
            Type *t = (Type *)y.ptr();
            if (t->typeKind == RECORD_TYPE)
                isRecordType = true;
        }
        return boolToValueHolder(isRecordType).ptr();
    }

    case PRIM_RecordFieldCount : {
        ensureArity(args, 1);
        TypePtr y = evaluateRecordType(args[0], env);
        RecordType *z = (RecordType *)y.ptr();
        const vector<TypePtr> &fieldTypes = recordFieldTypes(z);
        return intToValueHolder(fieldTypes.size()).ptr();
    }

    case PRIM_RecordFieldOffset : {
        ensureArity(args, 2);
        TypePtr y = evaluateRecordType(args[0], env);
        RecordType *z = (RecordType *)y.ptr();
        int i = evaluateInt(args[1], env);
        const vector<TypePtr> &fieldTypes = recordFieldTypes(z);
        if ((i < 0) || (i >= (int)fieldTypes.size()))
            error("field index out of range");
        const llvm::StructLayout *layout = recordTypeLayout(z);
        return intToValueHolder(layout->getElementOffset(i)).ptr();
    }

    case PRIM_RecordFieldIndex : {
        ensureArity(args, 2);
        TypePtr y = evaluateRecordType(args[0], env);
        RecordType *z = (RecordType *)y.ptr();
        IdentifierPtr fname = evaluateIdentifier(args[1], env);
        const map<string, int> &fieldIndexMap = recordFieldIndexMap(z);
        map<string, int>::const_iterator fi = fieldIndexMap.find(fname->str);
        if (fi == fieldIndexMap.end())
            error("field not in record");
        return intToValueHolder(fi->second).ptr();
    }

    case PRIM_recordFieldRef : {
        ensureArity(args, 2);
        PValuePtr y = analyzeRecordValue(args[0], env);
        if (!y)
            return NULL;
        RecordType *z = (RecordType *)y->type.ptr();
        int i = evaluateInt(args[1], env);
        const vector<TypePtr> &fieldTypes = recordFieldTypes(z);
        if ((i < 0) || (i >= (int)fieldTypes.size()))
            error("field index out of range");
        return new PValue(fieldTypes[i], false);
    }

    case PRIM_recordFieldRefByName : {
        ensureArity(args, 2);
        PValuePtr y = analyzeRecordValue(args[0], env);
        if (!y)
            return NULL;
        RecordType *z = (RecordType *)y->type.ptr();
        IdentifierPtr fname = evaluateIdentifier(args[1], env);
        const map<string, int> &fieldIndexMap = recordFieldIndexMap(z);
        map<string, int>::const_iterator fi = fieldIndexMap.find(fname->str);
        if (fi == fieldIndexMap.end())
            error(args[1], "field not in record");
        const vector<TypePtr> &fieldTypes = recordFieldTypes(z);
        return new PValue(fieldTypes[fi->second], false);
    }

    default :
        assert(false);
        return NULL;
    }
}
