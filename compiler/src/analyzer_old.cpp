#include "clay.hpp"



//
// analyze wrappers
//

PValuePtr analysisToPValue(ObjectPtr x)
{
    switch (x->objKind) {

    case VALUE_HOLDER : {
        ValueHolder *y = (ValueHolder *)x.ptr();
        return new PValue(y->type, true);
    }

    case PVALUE :
        return (PValue *)x.ptr();

    case TYPE :
    case PRIM_OP :
    case PROCEDURE :
    case RECORD :
    case MODULE_HOLDER :
    case IDENTIFIER :
        return new PValue(staticType(x), true);

    default :
        error("invalid value");
        return NULL;
    }
}

MultiPValuePtr analysisToMultiPValue(ObjectPtr x)
{
    switch (x->objKind) {
    case MULTI_PVALUE :
        return (MultiPValue *)x.ptr();
    default :
        return new MultiPValue(analysisToPValue(x));
    }
}

MultiPValuePtr analyzeMultiValue(ExprPtr expr, EnvPtr env)
{
    ObjectPtr v = analyze(expr, env);
    if (!v)
        return NULL;
    return analysisToMultiPValue(v);
}

ObjectPtr analyzeOne(ExprPtr expr, EnvPtr env)
{
    ObjectPtr v = analyze(expr, env);
    if (!v)
        return NULL;
    if (v->objKind == MULTI_PVALUE)
        error(expr, "multi-valued expression in single value context");
    return v;
}

PValuePtr analyzeValue(ExprPtr expr, EnvPtr env)
{
    ObjectPtr v = analyzeOne(expr, env);
    if (!v)
        return NULL;
    return analysisToPValue(v);
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

    switch (expr->exprKind) {

    case BOOL_LITERAL : {
        BoolLiteral *x = (BoolLiteral *)expr.ptr();
        return boolToValueHolder(x->value).ptr();
    }

    case INT_LITERAL : {
        IntLiteral *x = (IntLiteral *)expr.ptr();
        return parseIntLiteral(x).ptr();
    }

    case FLOAT_LITERAL : {
        FloatLiteral *x = (FloatLiteral *)expr.ptr();
        return parseFloatLiteral(x).ptr();
    }

    case CHAR_LITERAL : {
        CharLiteral *x = (CharLiteral *)expr.ptr();
        if (!x->desugared)
            x->desugared = desugarCharLiteral(x->value);
        return analyze(x->desugared, env);
    }

    case STRING_LITERAL : {
        StringLiteral *x = (StringLiteral *)expr.ptr();
        PValuePtr pv = new PValue(arrayType(int8Type, x->value.size()), true);
        vector<ExprPtr> args;
        args.push_back(new ObjectExpr(pv.ptr()));
        return analyzeInvoke(kernelName("stringRef"), args, env);
    }

    case NAME_REF : {
        NameRef *x = (NameRef *)expr.ptr();
        ObjectPtr y = lookupEnv(env, x->name);
        if (y->objKind == EXPRESSION)
            return analyze((Expr *)y.ptr(), env);
        return analyzeStaticObject(y);
    }

    case TUPLE : {
        Tuple *x = (Tuple *)expr.ptr();
        vector<ExprPtr> args2;
        bool expanded = expandVarArgs(x->args, env, args2);
        if (!expanded && (args2.size() == 1))
            return analyze(args2[0], env);
        return analyzeInvoke(primName("tuple"), args2, env);
    }

    case ARRAY : {
        Array *x = (Array *)expr.ptr();
        vector<ExprPtr> args2;
        expandVarArgs(x->args, env, args2);
        return analyzeInvoke(primName("array"), args2, env);
    }

    case INDEXING : {
        Indexing *x = (Indexing *)expr.ptr();
        ObjectPtr indexable = analyze(x->expr, env);
        if (!indexable)
            return NULL;
        vector<ExprPtr> args2;
        expandVarArgs(x->args, env, args2);
        return analyzeIndexing(indexable, args2, env);
    }

    case CALL : {
        Call *x = (Call *)expr.ptr();
        ObjectPtr callable = analyze(x->expr, env);
        if (!callable)
            return NULL;
        vector<ExprPtr> args2;
        expandVarArgs(x->args, env, args2);
        return analyzeInvoke(callable, args2, env);
    }

    case FIELD_REF : {
        FieldRef *x = (FieldRef *)expr.ptr();
        ObjectPtr base = analyze(x->expr, env);
        if (!base)
            return NULL;
        return analyzeFieldRef(base, x->name);
    }

    case TUPLE_REF : {
        TupleRef *x = (TupleRef *)expr.ptr();
        vector<ExprPtr> args;
        args.push_back(x->expr);
        args.push_back(new ObjectExpr(sizeTToValueHolder(x->index).ptr()));
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
        ObjectPtr first = analyze(x->expr1, env);
        if (!first)
            return NULL;
        if (first->objKind == VALUE_HOLDER) {
            ValueHolder *y = (ValueHolder *)first.ptr();
            if (y->type == boolType) {
                if (*((char *)y->buf) == 0)
                    return first;
            }
        }
        PValuePtr a = analysisToPValue(first);
        if (a->isTemp)
            return new PValue(a->type, true);
        PValuePtr b = analyzeValue(x->expr2, env);
        if (!a || !b)
            return NULL;
        if (a->type != b->type)
            error("type mismatch in 'and' expression");
        return new PValue(a->type, a->isTemp || b->isTemp);
    }

    case OR : {
        Or *x = (Or *)expr.ptr();
        ObjectPtr first = analyze(x->expr1, env);
        if (!first)
            return NULL;
        if (first->objKind == VALUE_HOLDER) {
            ValueHolder *y = (ValueHolder *)first.ptr();
            if (y->type == boolType) {
                if (*((char *)y->buf) != 0)
                    return first;
            }
        }
        PValuePtr a = analysisToPValue(first);
        if (a->isTemp)
            return new PValue(a->type, true);
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

    case VAR_ARGS_REF :
        error("invalid use of '...'");

    case NEW : {
        New *x = (New *)expr.ptr();
        if (!x->desugared)
            x->desugared = desugarNew(x);
        return analyze(x->desugared, env);
    }

    case STATIC_EXPR : {
        StaticExpr *x = (StaticExpr *)expr.ptr();
        if (!x->desugared)
            x->desugared = desugarStaticExpr(x);
        return analyze(x->desugared, env);
    }

    case FOREIGN_EXPR : {
        ForeignExpr *x = (ForeignExpr *)expr.ptr();
        return analyze(x->expr, x->getEnv());
    }

    case OBJECT_EXPR : {
        ObjectExpr *x = (ObjectExpr *)expr.ptr();
        return analyzeStaticObject(x->obj);
    }

    default :
        assert(false);
        return NULL;

    }
}



//
// expandVarArgs
//

bool expandVarArgs(const vector<ExprPtr> &args,
                   EnvPtr env,
                   vector<ExprPtr> &outArgs)
{
    bool expanded = false;
    for (unsigned i = 0; i < args.size(); ++i) {
        if (args[i]->exprKind == VAR_ARGS_REF) {
            ObjectPtr z = lookupEnv(env, new Identifier("%varArgs"));
            VarArgsInfo *vaInfo = (VarArgsInfo *)z.ptr();
            if (!vaInfo->hasVarArgs)
                error(args[i], "varargs unavailable");
            expanded = true;
            outArgs.insert(outArgs.end(),
                           vaInfo->varArgs.begin(),
                           vaInfo->varArgs.end());
        }
        else {
            outArgs.push_back(args[i]);
        }
    }
    return expanded;
}



//
// analyzeStaticObject
//

ObjectPtr analyzeStaticObject(ObjectPtr x)
{
    switch (x->objKind) {
    case ENUM_MEMBER : {
        EnumMember *y = (EnumMember *)x.ptr();
        return new PValue(y->type, true);
    }
    case GLOBAL_VARIABLE : {
        GlobalVariable *y = (GlobalVariable *)x.ptr();
        if (!y->type) {
            if (y->analyzing)
                return NULL;
            y->analyzing = true;
            PValuePtr pv = analyzeValue(y->expr, y->env);
            y->analyzing = false;
            if (!pv)
                return NULL;
            y->type = pv->type;
        }
        return new PValue(y->type, false);
    }
    case EXTERNAL_VARIABLE : {
        ExternalVariable *y = (ExternalVariable *)x.ptr();
        if (!y->type2)
            y->type2 = evaluateType(y->type, y->env);
        return new PValue(y->type2, false);
    }
    case EXTERNAL_PROCEDURE : {
        ExternalProcedure *y = (ExternalProcedure *)x.ptr();
        if (!y->analyzed)
            analyzeExternal(y);
        return new PValue(y->ptrType, true);
    }
    case STATIC_GLOBAL : {
        StaticGlobal *y = (StaticGlobal *)x.ptr();
        return analyze(y->expr, y->env);
    }
    case EVALUE : {
        EValue *y = (EValue *)x.ptr();
        return new PValue(y->type, false);
    }
    case MULTI_EVALUE : {
        MultiEValue *y = (MultiEValue *)x.ptr();
        MultiPValuePtr z = new MultiPValue();
        for (unsigned i = 0; i < y->values.size(); ++i)
            z->values.push_back(new PValue(y->values[i]->type, false));
        return z.ptr();
    }
    case CVALUE : {
        CValue *y = (CValue *)x.ptr();
        return new PValue(y->type, false);
    }
    case MULTI_CVALUE : {
        MultiCValue *y = (MultiCValue *)x.ptr();
        MultiPValuePtr z = new MultiPValue();
        for (unsigned i = 0; i < y->values.size(); ++i)
            z->values.push_back(new PValue(y->values[i]->type, false));
        return z.ptr();
    }
    case VALUE_HOLDER :
    case MULTI_STATIC :
    case PVALUE :
    case MULTI_PVALUE :

    case TYPE :
    case PRIM_OP :
    case PROCEDURE :
    case RECORD :
    case MODULE_HOLDER :
    case IDENTIFIER :
        return x;

    case PATTERN :
        error("pattern cannot be used as value");
        return NULL;

    default :
        error("invalid static object");
        return NULL;
    }
}



//
// analyzeExternal
//

void analyzeExternal(ExternalProcedurePtr x)
{
    assert(!x->analyzed);
    if (!x->attributesVerified)
        verifyAttributes(x);
    vector<TypePtr> argTypes;
    for (unsigned i = 0; i < x->args.size(); ++i) {
        ExternalArgPtr y = x->args[i];
        y->type2 = evaluateType(y->type, x->env);
        argTypes.push_back(y->type2);
    }
    if (!x->returnType)
        x->returnType2 = NULL;
    else
        x->returnType2 = evaluateType(x->returnType, x->env);
    CallingConv callingConv = CC_DEFAULT;
    if (x->attrStdCall)
        callingConv = CC_STDCALL;
    else if (x->attrFastCall)
        callingConv = CC_FASTCALL;
    x->ptrType = cCodePointerType(callingConv, argTypes,
                                  x->hasVarArgs, x->returnType2);
    x->analyzed = true;
}



//
// verifyAttributes
//

void verifyAttributes(ExternalProcedurePtr x)
{
    assert(!x->attributesVerified);
    x->attributesVerified = true;
    x->attrDLLImport = false;
    x->attrDLLExport = false;
    x->attrStdCall = false;
    x->attrFastCall = false;
    x->attrAsmLabel = "";
    for (unsigned i = 0; i < x->attributes.size(); ++i) {
        ExprPtr expr = x->attributes[i];
        if (expr->exprKind == NAME_REF) {
            const string &s = ((NameRef *)expr.ptr())->name->str;
            if (s == "dllimport") {
                if (x->attrDLLExport)
                    error(expr, "dllimport specified after dllexport");
                x->attrDLLImport = true;
            }
            else if (s == "dllexport") {
                if (x->attrDLLImport)
                    error(expr, "dllexport specified after dllimport");
                x->attrDLLExport = true;
            }
            else if (s == "stdcall") {
                if (x->attrFastCall)
                    error(expr, "stdcall specified after fastcall");
                x->attrStdCall = true;
            }
            else if (s == "fastcall") {
                if (x->attrStdCall)
                    error(expr, "fastcall specified after stdcall");
                x->attrFastCall = true;
            }
            else {
                error(expr, "invalid attribute");
            }
        }
        else if (expr->exprKind == STRING_LITERAL) {
            StringLiteral *y = (StringLiteral *)expr.ptr();
            x->attrAsmLabel = y->value;
        }
        else {
            error(expr, "invalid attribute");
        }
    }
}

void verifyAttributes(ExternalVariablePtr x)
{
    assert(!x->attributesVerified);
    x->attributesVerified = true;
    x->attrDLLImport = false;
    x->attrDLLExport = false;
    for (unsigned i = 0; i < x->attributes.size(); ++i) {
        ExprPtr expr = x->attributes[i];
        if (expr->exprKind != NAME_REF)
            error(expr, "invalid attribute");
        const string &s = ((NameRef *)expr.ptr())->name->str;
        if (s == "dllimport") {
            if (x->attrDLLExport)
                error(expr, "dllimport specified after dllexport");
            x->attrDLLImport = true;
        }
        else if (s == "dllexport") {
            if (x->attrDLLImport)
                error(expr, "dllexport specified after dllimport");
            x->attrDLLExport = true;
        }
        else {
            error(expr, "invalid attribute");
        }
    }
}



//
// analyzeFieldRef
//

ObjectPtr analyzeFieldRef(ObjectPtr x, IdentifierPtr name)
{
    switch (x->objKind) {
    case PVALUE : {
        PValuePtr y = (PValue *)x.ptr();
        if (y->type->typeKind == STATIC_TYPE) {
            StaticType *t = (StaticType *)y->type.ptr();
            return analyzeFieldRef(t->obj, name);
        }
        vector<ExprPtr> args;
        args.push_back(new ObjectExpr(x));
        args.push_back(new ObjectExpr(name.ptr()));
        ObjectPtr prim = kernelName("fieldRef");
        return analyzeInvoke(prim, args, new Env());
    }
    case MODULE_HOLDER : {
        ModuleHolderPtr y = (ModuleHolder *)x.ptr();
        ObjectPtr z = lookupModuleMember(y, name);
        return analyzeStaticObject(z);
    }
    default :
        error("invalid field access operation");
        return NULL;
    }
}



//
// analyzeIndexing
//

ObjectPtr analyzeIndexing(ObjectPtr x, const vector<ExprPtr> &args, EnvPtr env)
{
    switch (x->objKind) {

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
            ensureArity(args, 2);
            vector<TypePtr> argTypes = evaluateTypeTuple(args[0], env);
            vector<TypePtr> returnTypes = evaluateTypeTuple(args[1], env);
            vector<bool> returnIsRef(returnTypes.size(), false);
            return codePointerType(argTypes, returnIsRef, returnTypes).ptr();
        }

        case PRIM_RefCodePointer : {
            ensureArity(args, 2);
            vector<TypePtr> argTypes = evaluateTypeTuple(args[0], env);
            vector<TypePtr> returnTypes = evaluateTypeTuple(args[1], env);
            vector<bool> returnIsRef(returnTypes.size(), true);
            return codePointerType(argTypes, returnIsRef, returnTypes).ptr();
        }

        case PRIM_CCodePointer : {
            ensureArity(args, 2);
            vector<TypePtr> argTypes = evaluateTypeTuple(args[0], env);
            vector<TypePtr> returnTypes = evaluateTypeTuple(args[1], env);
            if (returnTypes.size() > 1)
                error(args[1], "C code cannot return more than one value");
            TypePtr returnType;
            if (returnTypes.size() == 1)
                returnType = returnTypes[0];
            return cCodePointerType(CC_DEFAULT, argTypes,
                                    false, returnType).ptr();
        }

        case PRIM_StdCallCodePointer : {
            ensureArity(args, 2);
            vector<TypePtr> argTypes = evaluateTypeTuple(args[0], env);
            vector<TypePtr> returnTypes = evaluateTypeTuple(args[1], env);
            if (returnTypes.size() > 1)
                error(args[1], "C code cannot return more than one value");
            TypePtr returnType;
            if (returnTypes.size() == 1)
                returnType = returnTypes[0];
            return cCodePointerType(CC_STDCALL, argTypes,
                                    false, returnType).ptr();
        }

        case PRIM_FastCallCodePointer : {
            ensureArity(args, 2);
            vector<TypePtr> argTypes = evaluateTypeTuple(args[0], env);
            vector<TypePtr> returnTypes = evaluateTypeTuple(args[1], env);
            if (returnTypes.size() > 1)
                error(args[1], "C code cannot return more than one value");
            TypePtr returnType;
            if (returnTypes.size() == 1)
                returnType = returnTypes[0];
            return cCodePointerType(CC_FASTCALL, argTypes,
                                    false, returnType).ptr();
        }

        case PRIM_Array : {
            ensureArity(args, 2);
            TypePtr t = evaluateType(args[0], env);
            int n = evaluateInt(args[1], env);
            return arrayType(t, n).ptr();
        }

        case PRIM_Tuple : {
            vector<TypePtr> types;
            for (unsigned i = 0; i < args.size(); ++i)
                types.push_back(evaluateType(args[i], env));
            return tupleType(types).ptr();
        }

        case PRIM_Static : {
            ensureArity(args, 1);
            ObjectPtr obj = evaluateOneStatic(args[0], env);
            return staticType(obj).ptr();
        }

        }
    }

    case RECORD : {
        RecordPtr y = (Record *)x.ptr();
        ensureArity(args, y->patternVars.size());
        vector<ObjectPtr> params;
        for (unsigned i = 0; i < args.size(); ++i)
            params.push_back(evaluateOneStatic(args[i], env));
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
    case RECORD :
    case PROCEDURE :
        return analyzeInvokeCallable(x, args, env);
    case PVALUE : {
        PValue *y = (PValue *)x.ptr();
        if (y->type->typeKind == STATIC_TYPE) {
            StaticType *t = (StaticType *)y->type.ptr();
            return analyzeInvoke(t->obj, args, env);
        }
        return analyzeInvokeValue(y, args, env);
    }
    case PRIM_OP :
        return analyzeInvokePrimOp((PrimOp *)x.ptr(), args, env);
    }
    error("invalid call operation");
    return NULL;
}



//
// analyzeInvokeCallable, analyzeInvokeSpecialCase, analyzeCallable
//

ObjectPtr analyzeInvokeCallable(ObjectPtr x,
                                const vector<ExprPtr> &args,
                                EnvPtr env)
{
    vector<TypePtr> argsKey;
    vector<ValueTempness> argsTempness;
    vector<LocationPtr> argLocations;
    if (!computeArgsKey(args, env, argsKey, argsTempness, argLocations))
        return NULL;
    ObjectPtr result = analyzeInvokeSpecialCase(x, argsKey);
    if (result.ptr())
        return result;
    InvokeStackContext invokeStackContext(x, argsKey);
    InvokeEntryPtr entry =
        analyzeCallable(x, argsKey, argsTempness, argLocations);
    if (entry->inlined)
        return analyzeInvokeInlined(entry, args, env);
    if (!entry->analyzed)
        return NULL;
    return entry->analysis;
}

ObjectPtr analyzeInvokeSpecialCase(ObjectPtr x,
                                   const vector<TypePtr> &argsKey)
{
    switch (x->objKind) {
    case TYPE : {
        Type *y = (Type *)x.ptr();
        if (argsKey.empty())
            return new PValue(y, true);
        break;
    }
    case PROCEDURE : {
        if ((x == kernelName("destroy")) &&
            (argsKey.size() == 1) &&
            isPrimitiveType(argsKey[0]))
        {
            return new MultiPValue();
        }
        break;
    }
    }
    return NULL;
}

static InvokeEntryPtr findNextMatchingEntry(InvokeSetPtr invokeSet,
                                            unsigned entryIndex,
                                            const vector<OverloadPtr> &overloads)
{
    if (entryIndex < invokeSet->entries.size())
        return invokeSet->entries[entryIndex];

    assert(entryIndex == invokeSet->entries.size());

    unsigned overloadIndex;
    if (invokeSet->overloadIndices.empty())
        overloadIndex = 0;
    else
        overloadIndex = invokeSet->overloadIndices.back() + 1;

    for (; overloadIndex < overloads.size(); ++overloadIndex) {
        OverloadPtr y = overloads[overloadIndex];
        MatchResultPtr result = matchInvoke(y->code, y->env,
                                            invokeSet->argsKey,
                                            y->target, invokeSet->callable);
        if (result->matchCode == MATCH_SUCCESS) {
            InvokeEntryPtr entry =
                new InvokeEntry(invokeSet->callable,
                                invokeSet->argsKey);
            invokeSet->entries.push_back(entry);
            invokeSet->overloadIndices.push_back(overloadIndex);
            entry->code = clone(y->code);
            MatchSuccess *z = (MatchSuccess *)result.ptr();
            entry->env = z->env;
            entry->fixedArgTypes = z->fixedArgTypes;
            entry->fixedArgNames = z->fixedArgNames;
            entry->hasVarArgs = z->hasVarArgs;
            entry->varArgTypes = z->varArgTypes;
            entry->inlined = y->inlined;
            return entry;
        }
    }
    return NULL;
}

static bool tempnessMatches(CodePtr code,
                            const vector<ValueTempness> &tempness)
{
    if (code->hasVarArgs)
        assert(code->formalArgs.size() <= tempness.size());
    else
        assert(code->formalArgs.size() == tempness.size());

    for (unsigned i = 0; i < code->formalArgs.size(); ++i) {
        FormalArgPtr arg = code->formalArgs[i];
        if ((tempness[i] & arg->tempness) == 0)
            return false;
    }
    return true;
}

InvokeEntryPtr analyzeCallable(ObjectPtr x,
                               const vector<TypePtr> &argsKey,
                               const vector<ValueTempness> &argsTempness,
                               const vector<LocationPtr> &argLocations)
{
    InvokeSetPtr invokeSet = lookupInvokeSet(x, argsKey);
    const vector<OverloadPtr> &overloads = callableOverloads(x);

    unsigned i = 0;
    InvokeEntryPtr entry;
    while ((entry = findNextMatchingEntry(invokeSet, i, overloads)).ptr()) {
        if (tempnessMatches(entry->code, argsTempness))
            break;
        ++i;
    }
    if (!entry)
        error("no matching operation");
    if (entry->analyzed || entry->analyzing)
        return entry;
    if (entry->inlined)
        return entry;

    entry->analyzing = true;
    analyzeCodeBody(entry);
    entry->analyzing = false;

    return entry;
}



//
// analyzeReturn
//

ObjectPtr analyzeReturn(const vector<bool> &returnIsRef,
                        const vector<TypePtr> &returnTypes)
{
    if (returnTypes.size() == 1)
        return new PValue(returnTypes[0], !returnIsRef[0]);
    MultiPValuePtr x = new MultiPValue();
    for (unsigned i = 0; i < returnTypes.size(); ++i) {
        PValuePtr pv = new PValue(returnTypes[i], !returnIsRef[i]);
        x->values.push_back(pv);
    }
    return x.ptr();
}



//
// analyzeInvokeInlined
//

ObjectPtr analyzeInvokeInlined(InvokeEntryPtr entry,
                               const vector<ExprPtr> &args,
                               EnvPtr env)
{
    assert(entry->inlined);

    CodePtr code = entry->code;

    if (!code->returnSpecs.empty()) {
        vector<bool> returnIsRef;
        vector<TypePtr> returnTypes;
        evaluateReturnSpecs(code->returnSpecs, entry->env,
                            returnIsRef, returnTypes);
        return analyzeReturn(returnIsRef, returnTypes);
    }

    if (entry->hasVarArgs)
        assert(args.size() >= entry->fixedArgNames.size());
    else
        assert(args.size() == entry->fixedArgNames.size());

    EnvPtr bodyEnv = new Env(entry->env);

    for (unsigned i = 0; i < entry->fixedArgNames.size(); ++i) {
        ExprPtr expr = new ForeignExpr(env, args[i]);
        addLocal(bodyEnv, entry->fixedArgNames[i], expr.ptr());
    }

    VarArgsInfoPtr vaInfo = new VarArgsInfo(entry->hasVarArgs);
    if (entry->hasVarArgs) {
        for (unsigned i = entry->fixedArgNames.size(); i < args.size(); ++i) {
            ExprPtr expr = new ForeignExpr(env, args[i]);
            vaInfo->varArgs.push_back(expr);
        }
    }
    addLocal(bodyEnv, new Identifier("%varArgs"), vaInfo.ptr());

    AnalysisContextPtr ctx = new AnalysisContext();
    bool ok = analyzeStatement(code->body, bodyEnv, ctx);

    if (ctx->returnInitialized)
        return analyzeReturn(ctx->returnIsRef, ctx->returnTypes);
    else if (ok)
        return new MultiPValue();
    else
        return NULL;
}



//
// analyzeCodeBody
//

void analyzeCodeBody(InvokeEntryPtr entry) {
    assert(!entry->analyzed);
    ObjectPtr result;
    CodePtr code = entry->code;
    if (!code->returnSpecs.empty()) {
        evaluateReturnSpecs(code->returnSpecs, entry->env,
                            entry->returnIsRef, entry->returnTypes);
        result = analyzeReturn(entry->returnIsRef, entry->returnTypes);
    }
    else {
        EnvPtr bodyEnv = new Env(entry->env);

        for (unsigned i = 0; i < entry->fixedArgNames.size(); ++i) {
            PValuePtr parg = new PValue(entry->fixedArgTypes[i], false);
            addLocal(bodyEnv, entry->fixedArgNames[i], parg.ptr());
        }

        VarArgsInfoPtr vaInfo = new VarArgsInfo(entry->hasVarArgs);
        if (entry->hasVarArgs) {
            for (unsigned i = 0; i < entry->varArgTypes.size(); ++i) {
                PValuePtr parg = new PValue(entry->varArgTypes[i], false);
                ExprPtr expr = new ObjectExpr(parg.ptr());
                vaInfo->varArgs.push_back(expr);
            }
        }
        addLocal(bodyEnv, new Identifier("%varArgs"), vaInfo.ptr());

        AnalysisContextPtr ctx = new AnalysisContext();
        bool ok = analyzeStatement(code->body, bodyEnv, ctx);
        if (ctx->returnInitialized) {
            result = analyzeReturn(ctx->returnIsRef, ctx->returnTypes);
            entry->returnIsRef = ctx->returnIsRef;
            entry->returnTypes = ctx->returnTypes;
        }
        else if (ok) {
            result = new MultiPValue();
            entry->returnIsRef.clear();
            entry->returnTypes.clear();
        }
        else {
            result = NULL;
        }
    }
    if (result.ptr()) {
        entry->analysis = result;
        entry->analyzed = true;
    }
}



//
// analyzeStatement
//

bool analyzeStatement(StatementPtr stmt, EnvPtr env, AnalysisContextPtr ctx)
{
    LocationContext loc(stmt->location);

    switch (stmt->stmtKind) {

    case BLOCK : {
        Block *x = (Block *)stmt.ptr();
        for (unsigned i = 0; i < x->statements.size(); ++i) {
            StatementPtr y = x->statements[i];
            if (y->stmtKind == BINDING) {
                env = analyzeBinding((Binding *)y.ptr(), env);
                if (!env)
                    return false;
            }
            else if (!analyzeStatement(y, env, ctx)) {
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
        assert(x->isRef.size() == x->exprs.size());
        if (ctx->returnInitialized) {
            if (x->exprs.size() != ctx->returnTypes.size())
                error("mismatching number of return values");
            for (unsigned i = 0; i < x->exprs.size(); ++i) {
                if (x->isRef[i] != ctx->returnIsRef[i]) {
                    error(x->exprs[i],
                          "mismatching by-ref and by-value returns");
                }
                PValuePtr pv = analyzeValue(x->exprs[i], env);
                if (!pv)
                    return false;
                if (pv->type != ctx->returnTypes[i])
                    error(x->exprs[i], "type mismatch");
                if (pv->isTemp && x->isRef[i]) {
                    error(x->exprs[i],
                          "cannot return a temporary by reference");
                }
            }
        }
        else {
            ctx->returnIsRef.clear();
            ctx->returnTypes.clear();
            for (unsigned i = 0; i < x->exprs.size(); ++i) {
                ctx->returnIsRef.push_back(x->isRef[i]);
                PValuePtr pv = analyzeValue(x->exprs[i], env);
                if (!pv)
                    return false;
                if (pv->isTemp && x->isRef[i]) {
                    error(x->exprs[i],
                          "cannot return a temporary by reference");
                }
                ctx->returnTypes.push_back(pv->type);
            }
            ctx->returnInitialized = true;
        }
        return true;
    }

    case IF : {
        If *x = (If *)stmt.ptr();
        bool thenResult = analyzeStatement(x->thenPart, env, ctx);
        bool elseResult = true;
        if (x->elsePart.ptr())
            elseResult = analyzeStatement(x->elsePart, env, ctx);
        return thenResult || elseResult;
    }

    case EXPR_STATEMENT :
        return true;

    case WHILE : {
        While *x = (While *)stmt.ptr();
        analyzeStatement(x->body, env, ctx);
        return true;
    }

    case BREAK :
    case CONTINUE :
        return true;

    case FOR : {
        For *x = (For *)stmt.ptr();
        if (!x->desugared)
            x->desugared = desugarForStatement(x);
        return analyzeStatement(x->desugared, env, ctx);
    }

    case FOREIGN_STATEMENT : {
        ForeignStatement *x = (ForeignStatement *)stmt.ptr();
        return analyzeStatement(x->statement, x->getEnv(), ctx);
    }

    case TRY : {
        Try *x = (Try *)stmt.ptr();
        bool tryResult = analyzeStatement(x->tryBlock, env, ctx);
        bool catchResult = analyzeStatement(x->catchBlock, env, ctx);
        return tryResult || catchResult; // FIXME
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
        MultiPValuePtr right = analyzeMultiValue(x->expr, env);
        if (!right)
            return NULL;
        if (right->size() != x->names.size())
            arityError(x->expr, x->names.size(), right->size());
        EnvPtr env2 = new Env(env);
        for (unsigned i = 0; i < right->size(); ++i)
            addLocal(env2, x->names[i], right->values[i].ptr());
        return env2;
    }

    case STATIC : {
        MultiStaticPtr right = evaluateExprStatic(x->expr, env);
        if (right->size() != x->names.size())
            arityError(x->expr, x->names.size(), right->size());
        EnvPtr env2 = new Env(env);
        for (unsigned i = 0; i < right->size(); ++i)
            addLocal(env2, x->names[i], right->values[i]);
        return env2;
    }

    default :
        assert(false);
        return NULL;

    }
}



//
// analyzeInvokeValue
//

ObjectPtr analyzeInvokeValue(PValuePtr x, 
                             const vector<ExprPtr> &args,
                             EnvPtr env)
{
    switch (x->type->typeKind) {

    case CODE_POINTER_TYPE : {
        CodePointerType *y = (CodePointerType *)x->type.ptr();
        return analyzeReturn(y->returnIsRef, y->returnTypes);
    }

    case CCODE_POINTER_TYPE : {
        CCodePointerType *y = (CCodePointerType *)x->type.ptr();
        if (!y->returnType)
            return new MultiPValue();
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



//
// analyzeInvokePrimOp
//

ObjectPtr analyzeInvokePrimOp(PrimOpPtr x,
                              const vector<ExprPtr> &args,
                              EnvPtr env)
{
    switch (x->primOpCode) {

    case PRIM_TypeP : {
        return new PValue(boolType, true);
    }

    case PRIM_TypeSize : {
        return new PValue(cSizeTType, true);
    }

    case PRIM_primitiveCopy :
        return new MultiPValue();

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

    case PRIM_pointerEqualsP : {
        return new PValue(boolType, true);
    }

    case PRIM_pointerLesserP : {
        return new PValue(boolType, true);
    }

    case PRIM_pointerOffset : {
        ensureArity(args, 2);
        PValuePtr y = analyzePointerValue(args[0], env);
        return new PValue(y->type, true);
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

    case PRIM_CodePointerP : {
        return new PValue(boolType, true);
    }

    case PRIM_CodePointer :
        error("CodePointer type constructor cannot be called");

    case PRIM_RefCodePointer :
        error("RefCodePointer type constructor cannot be called");

    case PRIM_makeCodePointer : {
        if (args.size() < 1)
            error("incorrect no. of parameters");
        ObjectPtr callable = evaluateOneStatic(args[0], env);
        switch (callable->objKind) {
        case TYPE :
        case RECORD :
        case PROCEDURE :
            break;
        default :
            error(args[0], "invalid callable");
        }
        vector<TypePtr> argsKey;
        vector<ValueTempness> argsTempness;
        vector<LocationPtr> argLocations;
        for (unsigned i = 1; i < args.size(); ++i) {
            TypePtr t = evaluateType(args[i], env);
            argsKey.push_back(t);
            argsTempness.push_back(LVALUE);
            argLocations.push_back(args[i]->location);
        }

        InvokeStackContext invokeStackContext(callable, argsKey);

        InvokeEntryPtr entry =
            analyzeCallable(callable, argsKey,
                            argsTempness, argLocations);
        if (entry->inlined)
            error(args[0], "cannot create pointer to inlined code");
        if (!entry->analyzed)
            return NULL;
        TypePtr cpType = codePointerType(argsKey,
                                         entry->returnIsRef,
                                         entry->returnTypes);
        return new PValue(cpType, true);
    }

    case PRIM_CCodePointerP : {
        return new PValue(boolType, true);
    }

    case PRIM_CCodePointer :
        error("CCodePointer type constructor cannot be called");

    case PRIM_StdCallCodePointer :
        error("StdCallCodePointer type constructor cannot be called");

    case PRIM_FastCallCodePointer :
        error("FastCallCodePointer type constructor cannot be called");

    case PRIM_makeCCodePointer : {
        if (args.size() < 1)
            error("incorrect no. of parameters");
        ObjectPtr callable = evaluateOneStatic(args[0], env);
        switch (callable->objKind) {
        case TYPE :
        case RECORD :
        case PROCEDURE :
            break;
        default :
            error(args[0], "invalid callable");
        }
        vector<TypePtr> argsKey;
        vector<ValueTempness> argsTempness;
        vector<LocationPtr> argLocations;
        for (unsigned i = 1; i < args.size(); ++i) {
            TypePtr t = evaluateType(args[i], env);
            argsKey.push_back(t);
            argsTempness.push_back(LVALUE);
            argLocations.push_back(args[i]->location);
        }

        InvokeStackContext invokeStackContext(callable, argsKey);

        InvokeEntryPtr entry =
            analyzeCallable(callable, argsKey,
                            argsTempness, argLocations);
        if (entry->inlined)
            error(args[0], "cannot create pointer to inlined code");
        if (!entry->analyzed)
            return NULL;
        TypePtr returnType;
        if (entry->returnTypes.empty()) {
            returnType = NULL;
        }
        else if (entry->returnTypes.size() == 1) {
            if (entry->returnIsRef[0])
                error(args[0],
                      "cannot create c-code pointer to ref-return code");
            returnType = entry->returnTypes[0];
        }
        else {
            error(args[0],
                  "cannot create c-code pointer to multi-return code");
        }
        TypePtr ccpType = cCodePointerType(CC_DEFAULT,
                                           argsKey,
                                           false,
                                           returnType);
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

    case PRIM_TupleP : {
        return new PValue(boolType, true);
    }

    case PRIM_Tuple :
        error("Tuple type constructor cannot be called");

    case PRIM_TupleElementCount : {
        return new PValue(cSizeTType, true);
    }

    case PRIM_TupleElementOffset : {
        return new PValue(cSizeTType, true);
    }

    case PRIM_tuple : {
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
        size_t i = evaluateSizeT(args[1], env);
        TupleType *z = (TupleType *)y->type.ptr();
        if (i >= z->elementTypes.size())
            error(args[1], "tuple index out of range");
        return new PValue(z->elementTypes[i], false);
    }

    case PRIM_RecordP : {
        return new PValue(boolType, true);
    }

    case PRIM_RecordFieldCount : {
        return new PValue(cSizeTType, true);
    }

    case PRIM_RecordFieldOffset : {
        return new PValue(cSizeTType, true);
    }

    case PRIM_RecordFieldIndex : {
        return new PValue(cSizeTType, true);
    }

    case PRIM_recordFieldRef : {
        ensureArity(args, 2);
        PValuePtr y = analyzeRecordValue(args[0], env);
        if (!y)
            return NULL;
        RecordType *z = (RecordType *)y->type.ptr();
        size_t i = evaluateSizeT(args[1], env);
        const vector<TypePtr> &fieldTypes = recordFieldTypes(z);
        if (i >= fieldTypes.size())
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
        const map<string, size_t> &fieldIndexMap = recordFieldIndexMap(z);
        map<string, size_t>::const_iterator fi =
            fieldIndexMap.find(fname->str);
        if (fi == fieldIndexMap.end())
            error(args[1], "field not in record");
        const vector<TypePtr> &fieldTypes = recordFieldTypes(z);
        return new PValue(fieldTypes[fi->second], false);
    }

    case PRIM_Static :
        error("Static type constructor cannot be called");

    case PRIM_StaticName : {
        ensureArity(args, 1);
        ObjectPtr y = evaluateOneStatic(args[0], env);
        ostringstream sout;
        printName(sout, y);
        ExprPtr z = new StringLiteral(sout.str());
        return analyze(z, env);
    }

    case PRIM_EnumP : {
        return new PValue(boolType, true);
    }

    case PRIM_enumToInt : {
        return new PValue(cIntType, true);
    }

    case PRIM_intToEnum : {
        ensureArity(args, 2);
        TypePtr t = evaluateEnumerationType(args[0], env);
        return new PValue(t, true);
    }

    default :
        assert(false);
        return NULL;
    }
}
