#include "clay.hpp"

ValueHolder::ValueHolder(TypePtr type)
    : Object(VALUE_HOLDER), type(type)
{
    this->buf = (char *)malloc(typeSize(type));
}

ValueHolder::~ValueHolder()
{
    // TODO: call clay 'destroy'
    free(this->buf);
}

bool objectEquals(ObjectPtr a, ObjectPtr b)
{
    switch (a->objKind) {

    case IDENTIFIER : {
        if (b->objKind != IDENTIFIER)
            return false;
        Identifier *a1 = (Identifier *)a.ptr();
        Identifier *b1 = (Identifier *)b.ptr();
        return a1->str == b1->str;
    }

    case VALUE_HOLDER : {
        if (b->objKind != VALUE_HOLDER)
            return false;
        ValueHolder *a1 = (ValueHolder *)a.ptr();
        ValueHolder *b1 = (ValueHolder *)b.ptr();
        if (a1->type != b1->type)
            return false;
        int n = typeSize(a1->type);
        // TODO: call clay 'equals?'
        return memcmp(a1->buf, b1->buf, n) == 0;
    }

    default :
        return a == b;
    }
}

int objectHash(ObjectPtr a)
{
    switch (a->objKind) {

    case IDENTIFIER : {
        Identifier *b = (Identifier *)a.ptr();
        int h = 0;
        for (unsigned i = 0; i < b->str.size(); ++i)
            h += b->str[i];
        return h;
    }

    case VALUE_HOLDER : {
        ValueHolder *b = (ValueHolder *)a.ptr();
        // TODO: call clay 'hash'
        int h = 0;
        int n = typeSize(b->type);
        for (int i = 0; i < n; ++i)
            h += b->buf[i];
        return h;
    }

    default : {
        unsigned long long v = (unsigned long long)a.ptr();
        return (int)v;
    }

    }
}

ObjectPtr evaluateStatic(ExprPtr expr, EnvPtr env)
{
    ObjectPtr analysis = analyze(expr, env);
    if (!analysis)
        error(expr, "recursion during static evaluation");
    if (analysis->objKind == PVALUE) {
        PValue *pv = (PValue *)analysis.ptr();
        if (pv->type->typeKind == STATIC_TYPE) {
            StaticType *t = (StaticType *)pv->type.ptr();
            return t->obj;
        }
        ValueHolderPtr v = new ValueHolder(pv->type);
        EValuePtr out = new EValue(pv->type, (char *)v->buf);
        evalRootIntoValue(expr, env, pv, out);
        return v.ptr();
    }
    return analysis;
}

TypePtr evaluateMaybeVoidType(ExprPtr expr, EnvPtr env)
{
    ObjectPtr v = evaluateStatic(expr, env);
    switch (v->objKind) {
    case VOID_TYPE :
        return NULL;
    case TYPE :
        return (Type *)v.ptr();
    default :
        error(expr, "expecting a type");
        return NULL;
    }
}

TypePtr evaluateType(ExprPtr expr, EnvPtr env)
{
    ObjectPtr v = evaluateStatic(expr, env);
    if (v->objKind != TYPE)
        error(expr, "expecting a type");
    return (Type *)v.ptr();
}

TypePtr evaluateNumericType(ExprPtr expr, EnvPtr env)
{
    TypePtr t = evaluateType(expr, env);
    switch (t->typeKind) {
    case INTEGER_TYPE :
    case FLOAT_TYPE :
        break;
    default :
        error(expr, "expecting a numeric type");
    }
    return t;
}

TypePtr evaluateIntegerType(ExprPtr expr, EnvPtr env)
{
    TypePtr t = evaluateType(expr, env);
    if (t->typeKind != INTEGER_TYPE)
        error(expr, "expecting a integer type");
    return t;
}

TypePtr evaluatePointerLikeType(ExprPtr expr, EnvPtr env)
{
    TypePtr t = evaluateType(expr, env);
    switch (t->typeKind) {
    case POINTER_TYPE :
    case CODE_POINTER_TYPE :
    case CCODE_POINTER_TYPE :
        break;
    default :
        error(expr, "expecting a pointer or code pointer type");
    }
    return t;
}

TypePtr evaluateTupleType(ExprPtr expr, EnvPtr env)
{
    TypePtr t = evaluateType(expr, env);
    if (t->typeKind != TUPLE_TYPE)
        error(expr, "expecting a tuple type");
    return t;
}

TypePtr evaluateRecordType(ExprPtr expr, EnvPtr env)
{
    TypePtr t = evaluateType(expr, env);
    if (t->typeKind != RECORD_TYPE)
        error(expr, "expecting a record type");
    return t;
}

TypePtr evaluateEnumerationType(ExprPtr expr, EnvPtr env)
{
    TypePtr t = evaluateType(expr, env);
    if (t->typeKind != ENUM_TYPE)
        error(expr, "expecting an enumeration type");
    return t;
}

IdentifierPtr evaluateIdentifier(ExprPtr expr, EnvPtr env)
{
    ObjectPtr v = evaluateStatic(expr, env);
    if (v->objKind != IDENTIFIER)
        error(expr, "expecting an identifier value");
    return (Identifier *)v.ptr();
}

int evaluateInt(ExprPtr expr, EnvPtr env)
{
    ObjectPtr v = evaluateStatic(expr, env);
    if (v->objKind != VALUE_HOLDER)
        error(expr, "expecting an Int value");
    ValueHolderPtr vh = (ValueHolder *)v.ptr();
    if (vh->type != cIntType)
        error(expr, "expecting an Int value");
    return *(int *)vh->buf;
}

size_t evaluateSizeT(ExprPtr expr, EnvPtr env)
{
    ObjectPtr v = evaluateStatic(expr, env);
    if (v->objKind != VALUE_HOLDER)
        error(expr, "expecting a SizeT value");
    ValueHolderPtr vh = (ValueHolder *)v.ptr();
    if (vh->type != cSizeTType)
        error(expr, "expecting a SizeT value");
    return *(size_t *)vh->buf;
}

ptrdiff_t evaluatePtrDiffT(ExprPtr expr, EnvPtr env)
{
    ObjectPtr v = evaluateStatic(expr, env);
    if (v->objKind != VALUE_HOLDER)
        error(expr, "expecting a PtrDiffT value");
    ValueHolderPtr vh = (ValueHolder *)v.ptr();
    if (vh->type != cSizeTType)
        error(expr, "expecting a PtrDiffT value");
    return *(ptrdiff_t *)vh->buf;
}

bool evaluateBool(ExprPtr expr, EnvPtr env)
{
    ObjectPtr v = evaluateStatic(expr, env);
    if (v->objKind != VALUE_HOLDER)
        error(expr, "expecting a bool value");
    ValueHolderPtr vh = (ValueHolder *)v.ptr();
    if (vh->type != boolType)
        error(expr, "expecting a bool value");
    return (*(char *)vh->buf) != 0;
}

ValueHolderPtr intToValueHolder(int x)
{
    ValueHolderPtr v = new ValueHolder(cIntType);
    *(int *)v->buf = x;
    return v;
}

ValueHolderPtr sizeTToValueHolder(size_t x)
{
    ValueHolderPtr v = new ValueHolder(cSizeTType);
    *(size_t *)v->buf = x;
    return v;
}

ValueHolderPtr ptrDiffTToValueHolder(ptrdiff_t x)
{
    ValueHolderPtr v = new ValueHolder(cPtrDiffTType);
    *(ptrdiff_t *)v->buf = x;
    return v;
}

ValueHolderPtr boolToValueHolder(bool x)
{
    ValueHolderPtr v = new ValueHolder(boolType);
    *(char *)v->buf = x ? 1 : 0;
    return v;
}



//
// evaluator
//

static vector<EValuePtr> stackEValues;


//
// evaluate value ops
//

void evalValueInit(EValuePtr dest)
{
    evalInvoke(dest->type.ptr(), vector<ExprPtr>(), new Env(), dest);
}

void evalValueDestroy(EValuePtr dest)
{
    vector<ExprPtr> args;
    args.push_back(new ObjectExpr(dest.ptr()));
    evalInvokeVoid(kernelName("destroy"), args, new Env());
}

void evalValueCopy(EValuePtr dest, EValuePtr src)
{
    vector<ExprPtr> args;
    args.push_back(new ObjectExpr(src.ptr()));
    evalInvoke(dest->type.ptr(), args, new Env(), dest);
}

void evalValueAssign(EValuePtr dest, EValuePtr src)
{
    vector<ExprPtr> args;
    args.push_back(new ObjectExpr(dest.ptr()));
    args.push_back(new ObjectExpr(src.ptr()));
    evalInvokeVoid(kernelName("assign"), args, new Env());
}

bool evalToBoolFlag(EValuePtr a)
{
    if (a->type != boolType)
        error("expecting bool type");
    return *((char *)a->addr) != 0;
}



//
// evaluator temps
//

int evalMarkStack()
{
    return stackEValues.size();
}

void evalDestroyStack(int marker)
{
    int i = (int)stackEValues.size();
    assert(marker <= i);
    while (marker < i) {
        --i;
        evalValueDestroy(stackEValues[i]);
    }
}

void evalPopStack(int marker)
{
    assert(marker <= (int)stackEValues.size());
    while (marker < (int)stackEValues.size()) {
        free(stackEValues.back()->addr);
        stackEValues.pop_back();
    }
}

void evalDestroyAndPopStack(int marker)
{
    assert(marker <= (int)stackEValues.size());
    while (marker < (int)stackEValues.size()) {
        evalValueDestroy(stackEValues.back());
        free(stackEValues.back()->addr);
        stackEValues.pop_back();
    }
}

EValuePtr evalAllocValue(TypePtr t)
{
    char *buf = (char *)malloc(typeSize(t));
    EValuePtr ev = new EValue(t, buf);
    stackEValues.push_back(ev);
    return ev;
}



//
// evaluate expressions
//

void evalRootIntoValue(ExprPtr expr,
                       EnvPtr env,
                       PValuePtr pv,
                       EValuePtr out)
{
    int marker = evalMarkStack();
    evalIntoValue(expr, env, pv, out);
    evalDestroyAndPopStack(marker);
}

EValuePtr evalRootValue(ExprPtr expr, EnvPtr env, EValuePtr out)
{
    int marker = evalMarkStack();
    EValuePtr ev = evalValue(expr, env, out);
    evalDestroyAndPopStack(marker);
    return ev;
}

void evalIntoValue(ExprPtr expr, EnvPtr env, PValuePtr pv, EValuePtr out)
{
    assert(out.ptr());
    if (pv->isTemp && (pv->type == out->type)) {
        evalValue(expr, env, out);
    }
    else {
        EValuePtr ref = evalAsRef(expr, env, pv);
        evalValueCopy(out, ref);
    }
}

EValuePtr evalAsRef(ExprPtr expr, EnvPtr env, PValuePtr pv)
{
    EValuePtr result;
    if (pv->isTemp) {
        result = evalAllocValue(pv->type);
        evalValue(expr, env, result);
    }
    else {
        result = evalValue(expr, env, NULL);
    }
    return result;
}

EValuePtr evalValue(ExprPtr expr, EnvPtr env, EValuePtr out)
{
    EValuePtr result = evalMaybeVoid(expr, env, out);
    if (!result)
        error(expr, "expected non-void value");
    return result;
}

EValuePtr evalMaybeVoid(ExprPtr expr, EnvPtr env, EValuePtr out)
{
    return evalExpr(expr, env, out);
}

EValuePtr evalExpr(ExprPtr expr, EnvPtr env, EValuePtr out)
{
    LocationContext loc(expr->location);

    switch (expr->exprKind) {

    case BOOL_LITERAL :
    case INT_LITERAL :
    case FLOAT_LITERAL : {
        ObjectPtr x = analyze(expr, env);
        assert(x->objKind == VALUE_HOLDER);
        ValueHolder *y = (ValueHolder *)x.ptr();
        evalValueHolder(y, out);
        return out;
    }

    case CHAR_LITERAL : {
        CharLiteral *x = (CharLiteral *)expr.ptr();
        if (!x->desugared)
            x->desugared = desugarCharLiteral(x->value);
        return evalExpr(x->desugared, env, out);
    }

    case STRING_LITERAL : {
        StringLiteral *x = (StringLiteral *)expr.ptr();
        TypePtr type = arrayType(int8Type, x->value.size());
        EValuePtr ev = new EValue(type, const_cast<char *>(x->value.data()));
        vector<ExprPtr> args;
        args.push_back(new ObjectExpr(ev.ptr()));
        return evalInvoke(kernelName("String"), args, env, out);
    }

    case NAME_REF : {
        NameRef *x = (NameRef *)expr.ptr();
        ObjectPtr y = lookupEnv(env, x->name);
        if (y->objKind == EXPRESSION)
            return evalExpr((Expr *)y.ptr(), env, out);
        return evalStaticObject(y, out);
    }

    case RETURNED : {
        error(expr, "invalid use of 'returned' keyword");
    }

    case TUPLE : {
        Tuple *x = (Tuple *)expr.ptr();
        vector<ExprPtr> args2;
        bool expanded = expandVarArgs(x->args, env, args2);
        if (!expanded && (args2.size() == 1))
            return evalExpr(args2[0], env, out);
        return evalInvoke(primName("tuple"), args2, env, out);
    }

    case ARRAY : {
        Array *x = (Array *)expr.ptr();
        vector<ExprPtr> args2;
        expandVarArgs(x->args, env, args2);
        return evalInvoke(primName("array"), args2, env, out);
    }

    case INDEXING : {
        Indexing *x = (Indexing *)expr.ptr();
        ObjectPtr y = analyze(x->expr, env);
        assert(y.ptr());
        vector<ExprPtr> args2;
        expandVarArgs(x->args, env, args2);
        if (y->objKind == PVALUE) {
            PValue *z = (PValue *)y.ptr();
            if (z->type->typeKind == STATIC_TYPE) {
                StaticType *t = (StaticType *)z->type.ptr();
                ExprPtr inner = new Indexing(new ObjectExpr(t->obj), args2);
                return evalExpr(inner, env, out);
            }
            EValuePtr ev = evalAsRef(x->expr, env, z);
            vector<ExprPtr> args3;
            args3.push_back(new ObjectExpr(ev.ptr()));
            args3.insert(args3.end(), args2.begin(), args2.end());
            ObjectPtr op = kernelName("index");
            return evalInvoke(op, args3, env, out);
        }
        ObjectPtr obj = analyzeIndexing(y, args2, env);
        return evalStaticObject(obj, out);
    }

    case CALL : {
        Call *x = (Call *)expr.ptr();
        ObjectPtr y = analyze(x->expr, env);
        assert(y.ptr());
        vector<ExprPtr> args2;
        expandVarArgs(x->args, env, args2);
        if (y->objKind == PVALUE) {
            PValue *z = (PValue *)y.ptr();
            if (z->type->typeKind == STATIC_TYPE) {
                StaticType *t = (StaticType *)z->type.ptr();
                return evalInvoke(t->obj, args2, env, out);
            }
            EValuePtr ev = evalAsRef(x->expr, env, z);
            return evalInvokeValue(ev, args2, env, out);
        }
        return evalInvoke(y, args2, env, out);
    }

    case FIELD_REF : {
        FieldRef *x = (FieldRef *)expr.ptr();
        ObjectPtr y = analyze(x->expr, env);
        if (y->objKind == PVALUE) {
            PValue *z = (PValue *)y.ptr();
            if (z->type->typeKind == STATIC_TYPE) {
                StaticType *t = (StaticType *)z->type.ptr();
                ExprPtr inner = new FieldRef(new ObjectExpr(t->obj), x->name);
                return evalExpr(inner, env, out);
            }
            EValuePtr ev = evalAsRef(x->expr, env, z);
            vector<ExprPtr> args2;
            args2.push_back(new ObjectExpr(ev.ptr()));
            args2.push_back(new ObjectExpr(x->name.ptr()));
            ObjectPtr prim = primName("recordFieldRefByName");
            return evalInvoke(prim, args2, env, out);
        }
        if (y->objKind == MODULE_HOLDER) {
            ModuleHolderPtr z = (ModuleHolder *)y.ptr();
            ObjectPtr obj = lookupModuleMember(z, x->name);
            return evalStaticObject(obj, out);
        }
        error("invalid member access operation");
        return NULL;
    }

    case TUPLE_REF : {
        TupleRef *x = (TupleRef *)expr.ptr();
        vector<ExprPtr> args;
        args.push_back(x->expr);
        args.push_back(new ObjectExpr(sizeTToValueHolder(x->index).ptr()));
        ObjectPtr prim = primName("tupleRef");
        return evalInvoke(prim, args, env, out);
    }

    case UNARY_OP : {
        UnaryOp *x = (UnaryOp *)expr.ptr();
        if (!x->desugared)
            x->desugared = desugarUnaryOp(x);
        return evalExpr(x->desugared, env, out);
    }

    case BINARY_OP : {
        BinaryOp *x = (BinaryOp *)expr.ptr();
        if (!x->desugared)
            x->desugared = desugarBinaryOp(x);
        return evalExpr(x->desugared, env, out);
    }

    case AND : {
        And *x = (And *)expr.ptr();
        PValuePtr pv1 = analyzeValue(x->expr1, env);
        if (pv1->isTemp) {
            EValuePtr ev1 = evalValue(x->expr1, env, out);
            if (!evalToBoolFlag(ev1))
                return ev1;
            evalValueDestroy(ev1);
            PValuePtr pv2 = analyzeValue(x->expr2, env);
            if (pv2->type != pv1->type)
                error("type mismatch in 'and' expression");
            evalIntoValue(x->expr2, env, pv2, out);
            return out;
        }
        else {
            EValuePtr ev1 = evalValue(x->expr1, env, NULL);
            if (!evalToBoolFlag(ev1)) {
                if (out.ptr()) {
                    assert(out->type == ev1->type);
                    evalValueCopy(out, ev1);
                    return out;
                }
                return ev1;
            }
            PValuePtr pv2 = analyzeValue(x->expr2, env);
            if (pv2->type != pv1->type)
                error("type mismatch in 'and' expression");
            return evalValue(x->expr2, env, out);
        }
        assert(false);
    }

    case OR : {
        Or *x = (Or *)expr.ptr();
        PValuePtr pv1 = analyzeValue(x->expr1, env);
        if (pv1->isTemp) {
            EValuePtr ev1 = evalValue(x->expr1, env, out);
            if (evalToBoolFlag(ev1))
                return ev1;
            evalValueDestroy(ev1);
            PValuePtr pv2 = analyzeValue(x->expr2, env);
            if (pv2->type != pv1->type)
                error("type mismatch in 'or' expression");
            evalIntoValue(x->expr2, env, pv2, out);
            return out;
        }
        else {
            EValuePtr ev1 = evalValue(x->expr1, env, NULL);
            if (evalToBoolFlag(ev1)) {
                if (out.ptr()) {
                    assert(out->type == ev1->type);
                    evalValueCopy(out, ev1);
                    return out;
                }
                return ev1;
            }
            PValuePtr pv2 = analyzeValue(x->expr2, env);
            if (pv2->type != pv1->type)
                error("type mismatch in 'or' expression");
            return evalValue(x->expr2, env, out);
        }
        assert(false);
    }

    case LAMBDA : {
        Lambda *x = (Lambda *)expr.ptr();
        if (!x->initialized)
            initializeLambda(x, env);
        return evalExpr(x->converted, env, out);
    }

    case VAR_ARGS_REF :
        error("invalid use of ...");

    case NEW : {
        New *x = (New *)expr.ptr();
        if (!x->desugared)
            x->desugared = desugarNew(x);
        return evalExpr(x->desugared, env, out);
    }

    case SC_EXPR : {
        SCExpr *x = (SCExpr *)expr.ptr();
        return evalExpr(x->expr, x->env, out);
    }

    case OBJECT_EXPR : {
        ObjectExpr *x = (ObjectExpr *)expr.ptr();
        return evalStaticObject(x->obj, out);
    }

    default :
        assert(false);
        return NULL;

    }
}



//
// evalStaticObject
//

EValuePtr evalStaticObject(ObjectPtr x, EValuePtr out)
{
    switch (x->objKind) {
    case ENUM_MEMBER : {
        EnumMember *y = (EnumMember *)x.ptr();
        assert(out.ptr());
        assert(out->type == y->type);
        *((int *)out->addr) = y->index;
        return out;
    }
    case GLOBAL_VARIABLE : {
        GlobalVariable *y = (GlobalVariable *)x.ptr();
        if (!y->llGlobal)
            codegenGlobalVariable(y);
        void *ptr = llvmEngine->getPointerToGlobal(y->llGlobal);
        assert(!out);
        return new EValue(y->type, (char *)ptr);
    }
    case EXTERNAL_VARIABLE : {
        ExternalVariable *y = (ExternalVariable *)x.ptr();
        if (!y->llGlobal)
            codegenExternalVariable(y);
        void *ptr = llvmEngine->getPointerToGlobal(y->llGlobal);
        assert(!out);
        return new EValue(y->type2, (char *)ptr);
    }
    case EXTERNAL_PROCEDURE : {
        ExternalProcedure *y = (ExternalProcedure *)x.ptr();
        if (!y->llvmFunc)
            codegenExternal(y);
        void *funcPtr = llvmEngine->getPointerToGlobal(y->llvmFunc);
        assert(out.ptr());
        assert(out->type == y->ptrType);
        *((void **)out->addr) = funcPtr;
        return out;
    }
    case STATIC_GLOBAL : {
        StaticGlobal *y = (StaticGlobal *)x.ptr();
        return evalExpr(y->expr, y->env, out);
    }
    case VALUE_HOLDER : {
        ValueHolder *y = (ValueHolder *)x.ptr();
        evalValueHolder(y, out);
        return out;
    }
    case EVALUE : {
        EValue *y = (EValue *)x.ptr();
        assert(!out);
        return y;
    }
    case TYPE :
    case VOID_TYPE :
    case PRIM_OP :
    case PROCEDURE :
    case OVERLOADABLE :
    case RECORD :
    case MODULE_HOLDER :
    case IDENTIFIER :
        assert(out.ptr());
        assert(out->type == staticType(x));
        return out;
    default :
        error("invalid static object");
        return NULL;
    }
}



//
// evalValueHolder
//

void evalValueHolder(ValueHolderPtr v, EValuePtr out)
{
    assert(out.ptr());
    assert(v->type == out->type);

    switch (v->type->typeKind) {
    case BOOL_TYPE :
    case INTEGER_TYPE :
    case FLOAT_TYPE :
        memcpy(out->addr, v->buf, typeSize(v->type));
        break;
    default :
        assert(false);
    }
}



//
// evalInvokeValue
//

EValuePtr evalInvokeValue(EValuePtr x,
                          const vector<ExprPtr> &args,
                          EnvPtr env,
                          EValuePtr out)
{
    switch (x->type->typeKind) {

    case CODE_POINTER_TYPE : {
        // TODO: implement it
        error("invoking a CodePointer not yet supported in evaluator");
        return out;
    }

    case CCODE_POINTER_TYPE : {
        // TODO: implement it
        error("invoking a CCodePointer not yet supported in evaluator");
    }

    default : {
        vector<ExprPtr> args2;
        args2.push_back(new ObjectExpr(x.ptr()));
        args2.insert(args2.end(), args.begin(), args.end());
        return evalInvoke(kernelName("call"), args2, env, out);
    }

    }
}



//
// evalInvoke
//

void evalInvokeVoid(ObjectPtr x,
                    const vector<ExprPtr> &args,
                    EnvPtr env)
{
    EValuePtr result = evalInvoke(x, args, env, NULL);
    if (result.ptr())
        error("void expression expected");
}

EValuePtr evalInvoke(ObjectPtr x,
                     const vector<ExprPtr> &args,
                     EnvPtr env,
                     EValuePtr out)
{
    switch (x->objKind) {
    case TYPE :
    case RECORD :
    case PROCEDURE :
    case OVERLOADABLE :
        return evalInvokeCallable(x, args, env, out);
    case PRIM_OP :
        return evalInvokePrimOp((PrimOp *)x.ptr(), args, env, out);
    default :
        error("invalid call operation");
        return NULL;

    }
}



//
// evalInvokeCallable
//

EValuePtr evalInvokeCallable(ObjectPtr x,
                             const vector<ExprPtr> &args,
                             EnvPtr env,
                             EValuePtr out)
{
    const vector<bool> &isStaticFlags =
        lookupIsStaticFlags(x, args.size());
    vector<ObjectPtr> argsKey;
    vector<ValueTempness> argsTempness;
    vector<LocationPtr> argLocations;
    bool result = computeArgsKey(isStaticFlags, args, env,
                                 argsKey, argsTempness,
                                 argLocations);
    assert(result);
    if (evalInvokeSpecialCase(x, isStaticFlags, argsKey))
        return out;
    InvokeStackContext invokeStackContext(x, argsKey);
    InvokeEntryPtr entry = codegenCallable(x, isStaticFlags,
                                           argsKey, argsTempness,
                                           argLocations);
    if (entry->inlined)
        return evalInvokeInlined(entry, args, env, out);
    return evalInvokeCode(entry, args, env, out);
}



//
// evalInvokeSpecialCase
//

bool evalInvokeSpecialCase(ObjectPtr x,
                           const vector<bool> &isStaticFlags,
                           const vector<ObjectPtr> &argsKey)
{
    switch (x->objKind) {
    case TYPE : {
        Type *y = (Type *)x.ptr();
        if (isPrimitiveType(y) && isStaticFlags.empty())
            return true;
        break;
    }
    case OVERLOADABLE : {
        if ((x == kernelName("destroy")) &&
            (isStaticFlags.size() == 1) &&
            (!isStaticFlags[0]))
        {
            ObjectPtr y = argsKey[0];
            assert(y->objKind == TYPE);
            if (isPrimitiveType((Type *)y.ptr()))
                return true;
        }
        break;
    }
    }
    return false;
}



//
// evalInvokeCode
//

EValuePtr evalInvokeCode(InvokeEntryPtr entry,
                         const vector<ExprPtr> &args,
                         EnvPtr env,
                         EValuePtr out)
{
    vector<llvm::GenericValue> gvArgs;
    for (unsigned i = 0; i < args.size(); ++i) {
        if (entry->isStaticFlags[i])
            continue;
        assert(entry->argsKey[i]->objKind == TYPE);
        TypePtr argType = (Type *)entry->argsKey[i].ptr();
        PValuePtr parg = analyzeValue(args[i], env);
        assert(parg->type == argType);
        EValuePtr earg = evalAsRef(args[i], env, parg);
        gvArgs.push_back(llvm::GenericValue(earg->addr));
    }
    if (!entry->returnType) {
        assert(!out);
        llvmEngine->runFunction(entry->llvmFunc, gvArgs);
        return NULL;
    }
    else if (entry->returnIsTemp) {
        assert(out.ptr());
        assert(out->type == entry->returnType);
        gvArgs.push_back(llvm::GenericValue(out->addr));
        llvmEngine->runFunction(entry->llvmFunc, gvArgs);
        return out;
    }
    else {
        assert(!out);
        llvm::GenericValue returnGV =
            llvmEngine->runFunction(entry->llvmFunc, gvArgs);
        void *ptr = returnGV.PointerVal;
        return new EValue(entry->returnType, (char *)ptr);
    }
}



//
// evalInvokeInlined
//

EValuePtr evalInvokeInlined(InvokeEntryPtr entry,
                            const vector<ExprPtr> &args,
                            EnvPtr env,
                            EValuePtr out)
{
    assert(entry->inlined);

    CodePtr code = entry->code;

    if (entry->hasVarArgs)
        assert(args.size() >= entry->fixedArgNames.size());
    else
        assert(args.size() == entry->fixedArgNames.size());

    EnvPtr bodyEnv = new Env(entry->env);

    for (unsigned i= 0; i < entry->fixedArgNames.size(); ++i) {
        ExprPtr expr = new SCExpr(env, args[i]);
        addLocal(bodyEnv, entry->fixedArgNames[i], expr.ptr());
    }

    VarArgsInfoPtr vaInfo = new VarArgsInfo(entry->hasVarArgs);
    if (entry->hasVarArgs) {
        for (unsigned i = entry->fixedArgNames.size(); i < args.size(); ++i) {
            ExprPtr expr = new SCExpr(env, args[i]);
            vaInfo->varArgs.push_back(expr);
        }
    }
    addLocal(bodyEnv, new Identifier("%varArgs"), vaInfo.ptr());

    ObjectPtr analysis = analyzeInvokeInlined(entry, args, env);
    assert(analysis.ptr());
    TypePtr returnType;
    bool returnIsTemp = false;
    if (analysis->objKind == PVALUE) {
        PValue *pv = (PValue *)analysis.ptr();
        returnType = pv->type;
        returnIsTemp = pv->isTemp;
    }

    EValuePtr returnVal;
    void *returnRefVal;
    if (returnType.ptr()) {
        if (returnIsTemp) {
            assert(out.ptr());
            assert(out->type == returnType);
            returnVal = out;
        }
        else {
            assert(!out);
            returnVal = new EValue(pointerType(entry->returnType),
                                   (char *)(&returnRefVal));
        }
    }
    else {
        assert(!out);
    }
    ObjectPtr rinfo = new ReturnedInfo(returnType, returnIsTemp, returnVal);
    addLocal(bodyEnv, new Identifier("%returned"), rinfo);

    TerminationPtr term = evalStatement(entry->code->body, bodyEnv);
    if (term.ptr()) {
        switch (term->terminationKind) {
        case TERMINATE_RETURN :
            break;
        case TERMINATE_BREAK :
            error(term, "invalid 'break' statement");
        case TERMINATE_CONTINUE :
            error(term, "invalid 'continue' statement");
        case TERMINATE_GOTO :
            error(term, "invalid 'goto' statement");
        default :
            assert(false);
        }
    }

    if (returnType.ptr() && !returnIsTemp)
        return new EValue(returnType.ptr(), (char *)returnRefVal);
    return out;
}



//
// evalStatement
//

TerminationPtr evalStatement(StatementPtr stmt, EnvPtr env)
{
    LocationContext loc(stmt->location);

    switch (stmt->stmtKind) {
    case BLOCK : {
        Block *x = (Block *)stmt.ptr();
        int blockMarker = evalMarkStack();
        map<string,LabelInfo> labels;
        evalCollectLabels(x->statements, 0, env, labels);
        TerminationPtr termination;
        unsigned pos = 0;
        while (pos < x->statements.size()) {
            StatementPtr y = x->statements[pos];
            if (y->stmtKind == LABEL) {
            }
            else if (y->stmtKind == BINDING) {
                env = evalBinding((Binding *)y.ptr(), env);
                evalCollectLabels(x->statements, pos+1, env, labels);
            }
            else {
                termination = evalStatement(y, env);
                if (termination.ptr()) {
                    if (termination->terminationKind == TERMINATE_GOTO) {
                        TerminateGoto *z = (TerminateGoto *)termination.ptr();
                        if (labels.count(z->targetLabel->str)) {
                            LabelInfo &info = labels[z->targetLabel->str];
                            env = info.env;
                            evalDestroyAndPopStack(info.stackMarker);
                            pos = info.blockPosition;
                            continue;
                        }
                    }
                    break;
                }
            }
            ++pos;
        }
        evalDestroyAndPopStack(blockMarker);
        return termination;
    }

    case LABEL :
    case BINDING :
        error("invalid statement");

    case ASSIGNMENT : {
        Assignment *x = (Assignment *)stmt.ptr();
        PValuePtr pvLeft = analyzeValue(x->left, env);
        PValuePtr pvRight = analyzeValue(x->right, env);
        if (pvLeft->isTemp)
            error(x->left, "cannot assign to a temporary");
        int marker = evalMarkStack();
        EValuePtr evLeft = evalValue(x->left, env, NULL);
        EValuePtr evRight = evalAsRef(x->right, env, pvRight);
        evalValueAssign(evLeft, evRight);
        evalDestroyAndPopStack(marker);
        return NULL;
    }

    case INIT_ASSIGNMENT : {
        InitAssignment *x = (InitAssignment *)stmt.ptr();
        PValuePtr pvLeft = analyzeValue(x->left, env);
        PValuePtr pvRight = analyzeValue(x->right, env);
        if (pvLeft->isTemp)
            error(x->left, "cannot assign to a temporary");
        int marker = evalMarkStack();
        EValuePtr evLeft = evalValue(x->left, env, NULL);
        EValuePtr evRight = evalAsRef(x->right, env, pvRight);
        evalValueAssign(evLeft, evRight);
        evalDestroyAndPopStack(marker);
        return NULL;
    }

    case UPDATE_ASSIGNMENT : {
        UpdateAssignment *x = (UpdateAssignment *)stmt.ptr();
        PValuePtr pvLeft = analyzeValue(x->left, env);
        if (pvLeft->isTemp)
            error(x->left, "cannot assign to a temporary");
        CallPtr call = new Call(kernelNameRef(updateOperatorName(x->op)));
        call->args.push_back(x->left);
        call->args.push_back(x->right);
        return evalStatement(new ExprStatement(call.ptr()), env);
    }

    case GOTO : {
        Goto *x = (Goto *)stmt.ptr();
        return new TerminateGoto(x->labelName, x->location);
    }

    case RETURN : {
        Return *x = (Return *)stmt.ptr();
        ObjectPtr y = lookupEnv(env, new Identifier("%returned"));
        assert(y.ptr());
        ReturnedInfo *z = (ReturnedInfo *)y.ptr();
        if (!x->expr) {
            if (z->returnType.ptr())
                error("non-void return expected");
        }
        else {
            if (!z->returnType)
                error("void return expected");
            if (!z->returnIsTemp)
                error("return by reference expected");
            PValuePtr pv = analyzeValue(x->expr, env);
            if (pv->type != z->returnType)
                error("type mismatch in return");
            assert(z->evalReturnVal.ptr());
            evalRootIntoValue(x->expr, env, pv, z->evalReturnVal);
        }
        return new TerminateReturn(x->location);
    }

    case RETURN_REF : {
        ReturnRef *x = (ReturnRef *)stmt.ptr();
        ObjectPtr y = lookupEnv(env, new Identifier("%returned"));
        assert(y.ptr());
        ReturnedInfo *z = (ReturnedInfo *)y.ptr();
        if (!z->returnType)
            error("void return expected");
        if (z->returnIsTemp)
            error("return by value expected");
        PValuePtr pv = analyzeValue(x->expr, env);
        if (pv->type != env->ctx->returnType)
            error("type mismatch in return");
        if (pv->isTemp)
            error("cannot return a temporary by reference");
        EValuePtr ev = evalRootValue(x->expr, env, NULL);
        assert(z->evalReturnVal.ptr());
        *((void **)z->evalReturnVal->addr) = (void *)ev->addr;
        return new TerminateReturn(x->location);
    }

    case IF : {
        If *x = (If *)stmt.ptr();
        PValuePtr pv = analyzeValue(x->condition, env);
        int marker = evalMarkStack();
        EValuePtr ev = evalAsRef(x->condition, env, pv);
        bool flag = evalToBoolFlag(ev);
        evalDestroyAndPopStack(marker);
        if (flag)
            return evalStatement(x->thenPart, env);
        if (x->elsePart.ptr())
            return evalStatement(x->elsePart, env);
        return NULL;
    }

    case EXPR_STATEMENT : {
        ExprStatement *x = (ExprStatement *)stmt.ptr();
        ObjectPtr a = analyzeMaybeVoidValue(x->expr, env);
        int marker = evalMarkStack();
        if (a->objKind == VOID_VALUE) {
            evalMaybeVoid(x->expr, env, NULL);
        }
        else if (a->objKind == PVALUE) {
            PValuePtr pv = (PValue *)a.ptr();
            evalAsRef(x->expr, env, pv);
        }
        else {
            assert(false);
        }
        evalDestroyAndPopStack(marker);
        return NULL;
    }

    case WHILE : {
        While *x = (While *)stmt.ptr();
        PValuePtr pv = analyzeValue(x->condition, env);
        while (true) {
            int marker = evalMarkStack();
            EValuePtr ev = evalAsRef(x->condition, env, pv);
            bool flag = evalToBoolFlag(ev);
            evalDestroyAndPopStack(marker);
            if (!flag) break;
            TerminationPtr term = evalStatement(x->body, env);
            if (term.ptr()) {
                if (term->terminationKind == TERMINATE_BREAK)
                    break;
                if (term->terminationKind == TERMINATE_CONTINUE)
                    continue;
                return term;
            }
        }
        return NULL;
    }

    case BREAK :
        return new TerminateBreak(stmt->location);

    case CONTINUE :
        return new TerminateContinue(stmt->location);

    case FOR : {
        For *x = (For *)stmt.ptr();
        if (!x->desugared)
            x->desugared = desugarForStatement(x);
        return evalStatement(x->desugared, env);
    }

    case SC_STATEMENT : {
        SCStatement *x = (SCStatement *)stmt.ptr();
        return evalStatement(x->statement, x->env);
    }

    case TRY :
        error("try statement not yet supported in the evaluator");
        return NULL;

    default :
        assert(false);
        return NULL;
    }
}

void evalCollectLabels(const vector<StatementPtr> &statements,
                       unsigned startIndex,
                       EnvPtr env,
                       map<string, LabelInfo> &labels)
{
    for (unsigned i = startIndex; i < statements.size(); ++i) {
        StatementPtr x = statements[i];
        switch (x->stmtKind) {
        case LABEL : {
            Label *y = (Label *)x.ptr();
            if (labels.count(y->name->str))
                error(x, "label redefined");
            labels[y->name->str] = LabelInfo(env, evalMarkStack(), i+1);
            break;
        }
        case BINDING :
            return;
        default :
            break;
        }
    }
}

EnvPtr evalBinding(BindingPtr x, EnvPtr env)
{
    switch (x->bindingKind) {

    case VAR : {
        PValuePtr pv = analyzeValue(x->expr, env);
        EValuePtr ev = evalAllocValue(pv->type);
        evalRootIntoValue(x->expr, env, pv, ev);
        EnvPtr env2 = new Env(env);
        addLocal(env2, x->name, ev.ptr());
        return env2;
    }

    case REF : {
        PValuePtr pv = analyzeValue(x->expr, env);
        EValuePtr ev;
        if (pv->isTemp) {
            ev = evalAllocValue(ev->type);
            evalRootValue(x->expr, env, ev);
        }
        else {
            ev = evalRootValue(x->expr, env, NULL);
        }
        EnvPtr env2 = new Env(env);
        addLocal(env2, x->name, ev.ptr());
        return env2;
    }

    case STATIC : {
        ObjectPtr v = evaluateStatic(x->expr, env);
        EnvPtr env2 = new Env(env);
        addLocal(env2, x->name, v.ptr());
        return env2;
    }

    default :
        assert(false);
        return NULL;
    }
}



//
// _evalNumeric, _evalInteger, _evalPointer, _evalPointerLike
//

static EValuePtr _evalNumeric(ExprPtr expr, EnvPtr env, TypePtr &type)
{
    PValuePtr pv = analyzeValue(expr, env);
    if (type.ptr()) {
        if (pv->type != type)
            error(expr, "argument type mismatch");
    }
    else {
        switch (pv->type->typeKind) {
        case INTEGER_TYPE :
        case FLOAT_TYPE :
            break;
        default :
            error(expr, "expecting numeric type");
        }
        type = pv->type;
    }
    return evalAsRef(expr, env, pv);
}

static EValuePtr _evalInteger(ExprPtr expr, EnvPtr env, TypePtr &type)
{
    PValuePtr pv = analyzeValue(expr, env);
    if (type.ptr()) {
        if (pv->type != type)
            error(expr, "argument type mismatch");
    }
    else {
        if (pv->type->typeKind != INTEGER_TYPE)
            error(expr, "expecting integer type");
        type = pv->type;
    }
    return evalAsRef(expr, env, pv);
}

static EValuePtr _evalPointer(ExprPtr expr, EnvPtr env, PointerTypePtr &type)
{
    PValuePtr pv = analyzeValue(expr, env);
    if (type.ptr()) {
        if (pv->type != (Type *)type.ptr())
            error(expr, "argument type mismatch");
    }
    else {
        if (pv->type->typeKind != POINTER_TYPE)
            error(expr, "expecting pointer type");
        type = (PointerType *)pv->type.ptr();
    }
    return evalAsRef(expr, env, pv);
}

static EValuePtr _evalPointerLike(ExprPtr expr, EnvPtr env, TypePtr &type)
{
    PValuePtr pv = analyzeValue(expr, env);
    if (type.ptr()) {
        if (pv->type != type)
            error(expr, "argument type mismatch");
    }
    else {
        switch (pv->type->typeKind) {
        case POINTER_TYPE :
        case CODE_POINTER_TYPE :
        case CCODE_POINTER_TYPE :
            break;
        default :
            error(expr, "expecting a pointer or a code pointer");
        }
        type = pv->type;
    }
    return evalAsRef(expr, env, pv);
}



//
// binaryNumericOp
//

template <template<typename> class T>
static void binaryNumericOp(EValuePtr a, EValuePtr b, EValuePtr out)
{
    assert(a->type == b->type);
    switch (a->type->typeKind) {

    case INTEGER_TYPE : {
        IntegerType *t = (IntegerType *)a->type.ptr();
        if (t->isSigned) {
            switch (t->bits) {
            case 8 :  T<char>().eval(a, b, out); break;
            case 16 : T<short>().eval(a, b, out); break;
            case 32 : T<int>().eval(a, b, out); break;
            case 64 : T<long long>().eval(a, b, out); break;
            default : assert(false);
            }
        }
        else {
            switch (t->bits) {
            case 8 :  T<unsigned char>().eval(a, b, out); break;
            case 16 : T<unsigned short>().eval(a, b, out); break;
            case 32 : T<unsigned int>().eval(a, b, out); break;
            case 64 : T<unsigned long long>().eval(a, b, out); break;
            default : assert(false);
            }
        }
        break;
    }

    case FLOAT_TYPE : {
        FloatType *t = (FloatType *)a->type.ptr();
        switch (t->bits) {
        case 32 : T<float>().eval(a, b, out); break;
        case 64 : T<double>().eval(a, b, out); break;
        default : assert(false);
        }
        break;
    }

    default :
        assert(false);
    }
}

template <typename T>
class BinaryOpHelper {
public :
    void eval(EValuePtr a, EValuePtr b, EValuePtr out) {
        perform(*((T *)a->addr), *((T *)b->addr), out->addr);
    }
    virtual void perform(T &a, T &b, void *out) = 0;
};

template <typename T>
class Op_numericEqualsP : public BinaryOpHelper<T> {
public :
    virtual void perform(T &a, T &b, void *out) {
        *((char *)out) = (a == b) ? 1 : 0;
    }
};

template <typename T>
class Op_numericLesserP : public BinaryOpHelper<T> {
public :
    virtual void perform(T &a, T &b, void *out) {
        *((char *)out) = (a < b) ? 1 : 0;
    }
};

template <typename T>
class Op_numericAdd : public BinaryOpHelper<T> {
public :
    virtual void perform(T &a, T &b, void *out) {
        *((T *)out) = a + b;
    }
};

template <typename T>
class Op_numericSubtract : public BinaryOpHelper<T> {
public :
    virtual void perform(T &a, T &b, void *out) {
        *((T *)out) = a - b;
    }
};

template <typename T>
class Op_numericMultiply : public BinaryOpHelper<T> {
public :
    virtual void perform(T &a, T &b, void *out) {
        *((T *)out) = a * b;
    }
};

template <typename T>
class Op_numericDivide : public BinaryOpHelper<T> {
public :
    virtual void perform(T &a, T &b, void *out) {
        *((T *)out) = a / b;
    }
};



//
// unaryNumericOp
//

template <template<typename> class T>
static void unaryNumericOp(EValuePtr a, EValuePtr out)
{
    switch (a->type->typeKind) {

    case INTEGER_TYPE : {
        IntegerType *t = (IntegerType *)a->type.ptr();
        if (t->isSigned) {
            switch (t->bits) {
            case 8 :  T<char>().eval(a, out); break;
            case 16 : T<short>().eval(a, out); break;
            case 32 : T<int>().eval(a, out); break;
            case 64 : T<long long>().eval(a, out); break;
            default : assert(false);
            }
        }
        else {
            switch (t->bits) {
            case 8 :  T<unsigned char>().eval(a, out); break;
            case 16 : T<unsigned short>().eval(a, out); break;
            case 32 : T<unsigned int>().eval(a, out); break;
            case 64 : T<unsigned long long>().eval(a, out); break;
            default : assert(false);
            }
        }
        break;
    }

    case FLOAT_TYPE : {
        FloatType *t = (FloatType *)a->type.ptr();
        switch (t->bits) {
        case 32 : T<float>().eval(a, out); break;
        case 64 : T<double>().eval(a, out); break;
        default : assert(false);
        }
        break;
    }

    default :
        assert(false);
    }
}

template <typename T>
class UnaryOpHelper {
public :
    void eval(EValuePtr a, EValuePtr out) {
        perform(*((T *)a->addr), out->addr);
    }
    virtual void perform(T &a, void *out) = 0;
};

template <typename T>
class Op_numericNegate : public UnaryOpHelper<T> {
public :
    virtual void perform(T &a, void *out) {
        *((T *)out) = -a;
    }
};



//
// binaryIntegerOp
//

template <template<typename> class T>
static void binaryIntegerOp(EValuePtr a, EValuePtr b, EValuePtr out)
{
    assert(a->type == b->type);
    assert(a->type->typeKind == INTEGER_TYPE);
    IntegerType *t = (IntegerType *)a->type.ptr();
    if (t->isSigned) {
        switch (t->bits) {
        case 8 :  T<char>().eval(a, b, out); break;
        case 16 : T<short>().eval(a, b, out); break;
        case 32 : T<int>().eval(a, b, out); break;
        case 64 : T<long long>().eval(a, b, out); break;
        default : assert(false);
        }
    }
    else {
        switch (t->bits) {
        case 8 :  T<unsigned char>().eval(a, b, out); break;
        case 16 : T<unsigned short>().eval(a, b, out); break;
        case 32 : T<unsigned int>().eval(a, b, out); break;
        case 64 : T<unsigned long long>().eval(a, b, out); break;
        default : assert(false);
        }
    }
}

template <typename T>
class Op_integerRemainder : public BinaryOpHelper<T> {
    virtual void perform(T &a, T &b, void *out) {
        *((T *)out) = a % b;
    }
};

template <typename T>
class Op_integerShiftLeft : public BinaryOpHelper<T> {
    virtual void perform(T &a, T &b, void *out) {
        *((T *)out) = a << b;
    }
};

template <typename T>
class Op_integerShiftRight : public BinaryOpHelper<T> {
    virtual void perform(T &a, T &b, void *out) {
        *((T *)out) = a >> b;
    }
};

template <typename T>
class Op_integerBitwiseAnd : public BinaryOpHelper<T> {
    virtual void perform(T &a, T &b, void *out) {
        *((T *)out) = a & b;
    }
};

template <typename T>
class Op_integerBitwiseOr : public BinaryOpHelper<T> {
    virtual void perform(T &a, T &b, void *out) {
        *((T *)out) = a | b;
    }
};

template <typename T>
class Op_integerBitwiseXor : public BinaryOpHelper<T> {
    virtual void perform(T &a, T &b, void *out) {
        *((T *)out) = a ^ b;
    }
};



//
// unaryIntegerOp
//

template <template<typename> class T>
static void unaryIntegerOp(EValuePtr a, EValuePtr out)
{
    assert(a->type->typeKind == INTEGER_TYPE);
    IntegerType *t = (IntegerType *)a->type.ptr();
    if (t->isSigned) {
        switch (t->bits) {
        case 8 :  T<char>().eval(a, out); break;
        case 16 : T<short>().eval(a, out); break;
        case 32 : T<int>().eval(a, out); break;
        case 64 : T<long long>().eval(a, out); break;
        default : assert(false);
        }
    }
    else {
        switch (t->bits) {
        case 8 :  T<unsigned char>().eval(a, out); break;
        case 16 : T<unsigned short>().eval(a, out); break;
        case 32 : T<unsigned int>().eval(a, out); break;
        case 64 : T<unsigned long long>().eval(a, out); break;
        default : assert(false);
        }
    }
}


template <typename T>
class Op_integerBitwiseNot : public UnaryOpHelper<T> {
public :
    virtual void perform(T &a, void *out) {
        *((T *)out) = ~a;
    }
};



//
// op_numericConvert
//

template <typename DEST, typename SRC>
static void op_numericConvert3(EValuePtr dest, EValuePtr src)
{
    *((DEST *)dest->addr) = DEST(*((SRC *)src->addr));
}

template <typename D>
static void op_numericConvert2(EValuePtr dest, EValuePtr src)
{
    switch (src->type->typeKind) {
    case INTEGER_TYPE : {
        IntegerType *t = (IntegerType *)src->type.ptr();
        if (t->isSigned) {
            switch (t->bits) {
            case 8 : op_numericConvert3<D,char>(dest, src); break;
            case 16 : op_numericConvert3<D,short>(dest, src); break;
            case 32 : op_numericConvert3<D,int>(dest, src); break;
            case 64 : op_numericConvert3<D,long long>(dest, src); break;
            default : assert(false);
            }
        }
        else {
            switch (t->bits) {
            case 8 : op_numericConvert3<D,unsigned char>(dest, src); break;
            case 16 : op_numericConvert3<D,unsigned short>(dest, src); break;
            case 32 : op_numericConvert3<D,unsigned int>(dest, src); break;
            case 64 :
                op_numericConvert3<D,unsigned long long>(dest, src); break;
            default : assert(false);
            }
        }
        break;
    }
    case FLOAT_TYPE : {
        FloatType *t = (FloatType *)src->type.ptr();
        switch (t->bits) {
        case 32 : op_numericConvert3<D,float>(dest, src); break;
        case 64 : op_numericConvert3<D,double>(dest, src); break;
        default : assert(false);
        }
        break;
    }
    default :
        assert(false);
    }
}

static void op_numericConvert(EValuePtr dest, EValuePtr src)
{
    switch (dest->type->typeKind) {
    case INTEGER_TYPE : {
        IntegerType *t = (IntegerType *)dest->type.ptr();
        if (t->isSigned) {
            switch (t->bits) {
            case 8 : op_numericConvert2<char>(dest, src); break;
            case 16 : op_numericConvert2<short>(dest, src); break;
            case 32 : op_numericConvert2<int>(dest, src); break;
            case 64 : op_numericConvert2<long long>(dest, src); break;
            default : assert(false);
            }
        }
        else {
            switch (t->bits) {
            case 8 : op_numericConvert2<unsigned char>(dest, src); break;
            case 16 : op_numericConvert2<unsigned short>(dest, src); break;
            case 32 : op_numericConvert2<unsigned int>(dest, src); break;
            case 64 : op_numericConvert2<unsigned long long>(dest, src); break;
            default : assert(false);
            }
        }
        break;
    }
    case FLOAT_TYPE : {
        FloatType *t = (FloatType *)dest->type.ptr();
        switch (t->bits) {
        case 32 : op_numericConvert2<float>(dest, src); break;
        case 64 : op_numericConvert2<double>(dest, src); break;
        default : assert(false);
        }
        break;
    }
    default :
        assert(false);
    }
}



//
// op_intToPtrInt
//

template <typename T>
static ptrdiff_t op_intToPtrInt2(EValuePtr a)
{
    return ptrdiff_t(*((T *)a->addr));
}

static ptrdiff_t op_intToPtrInt(EValuePtr a)
{
    assert(a->type->typeKind == INTEGER_TYPE);
    IntegerType *t = (IntegerType *)a->type.ptr();
    if (t->isSigned) {
        switch (t->bits) {
        case 8 : return op_intToPtrInt2<char>(a);
        case 16 : return op_intToPtrInt2<short>(a);
        case 32 : return op_intToPtrInt2<int>(a);
        case 64 : return op_intToPtrInt2<long long>(a);
        default : assert(false);
        }
    }
    else {
        switch (t->bits) {
        case 8 : return op_intToPtrInt2<unsigned char>(a);
        case 16 : return op_intToPtrInt2<unsigned short>(a);
        case 32 : return op_intToPtrInt2<unsigned int>(a);
        case 64 : return op_intToPtrInt2<unsigned long long>(a);
        default : assert(false);
        }
    }
    return 0;
}



//
// op_pointerToInt
//

template <typename T>
static void op_pointerToInt2(EValuePtr dest, void *ptr)
{
    *((T *)dest->addr) = T(size_t(ptr));
}

static void op_pointerToInt(EValuePtr dest, void *ptr)
{
    assert(dest->type->typeKind == INTEGER_TYPE);
    IntegerType *t = (IntegerType *)dest->type.ptr();
    if (t->isSigned) {
        switch (t->bits) {
        case 8 : op_pointerToInt2<char>(dest, ptr); break;
        case 16 : op_pointerToInt2<short>(dest, ptr); break;
        case 32 : op_pointerToInt2<int>(dest, ptr); break;
        case 64 : op_pointerToInt2<long long>(dest, ptr); break;
        default : assert(false);
        }
    }
    else {
        switch (t->bits) {
        case 8 : op_pointerToInt2<unsigned char>(dest, ptr); break;
        case 16 : op_pointerToInt2<unsigned short>(dest, ptr); break;
        case 32 : op_pointerToInt2<unsigned int>(dest, ptr); break;
        case 64 : op_pointerToInt2<unsigned long long>(dest, ptr); break;
        default : assert(false);
        }
    }
}



//
// evalInvokePrimOp
//

EValuePtr evalInvokePrimOp(PrimOpPtr x,
                           const vector<ExprPtr> &args,
                           EnvPtr env,
                           EValuePtr out)
{
    switch (x->primOpCode) {

    case PRIM_Type : {
        ensureArity(args, 1);
        ObjectPtr y = analyzeMaybeVoidValue(args[0], env);
        ObjectPtr obj;
        switch (y->objKind) {
        case PVALUE : {
            PValue *z = (PValue *)y.ptr();
            obj = z->type.ptr();
            break;
        }
        case VOID_VALUE : {
            obj = voidType.ptr();
            break;
        }
        default :
            assert(false);
        }
        return evalStaticObject(obj, out);
    }

    case PRIM_TypeP : {
        ensureArity(args, 1);
        ObjectPtr y = evaluateStatic(args[0], env);
        bool flag = y->objKind == TYPE;
        assert(out.ptr());
        assert(out->type == boolType);
        *((char *)out->addr) = flag ? 1 : 0;
        return out;
    }

    case PRIM_TypeSize : {
        ensureArity(args, 1);
        TypePtr t = evaluateType(args[0], env);
        ObjectPtr obj = sizeTToValueHolder(typeSize(t)).ptr();
        return evalStaticObject(obj, out);
    }

    case PRIM_primitiveCopy : {
        ensureArity(args, 2);
        PValuePtr pv0 = analyzeValue(args[0], env);
        PValuePtr pv1 = analyzeValue(args[1], env);
        if (!isPrimitiveType(pv0->type))
            error(args[0], "expecting primitive type");
        if (pv0->type != pv1->type)
            error(args[1], "argument type mismatch");
        EValuePtr ev0 = evalAsRef(args[0], env, pv0);
        EValuePtr ev1 = evalAsRef(args[1], env, pv1);
        memcpy(ev0->addr, ev1->addr, typeSize(pv0->type));
        return NULL;
    }

    case PRIM_boolNot : {
        ensureArity(args, 1);
        PValuePtr pv = analyzeValue(args[0], env);
        if (pv->type != boolType)
            error(args[0], "expecting bool type");
        EValuePtr ev = evalAsRef(args[0], env, pv);
        assert(out.ptr());
        assert(out->type == boolType);
        char *p = (char *)ev->addr;
        *((char *)out->addr) = (*p == 0);
        return out;
    }

    case PRIM_numericEqualsP : {
        ensureArity(args, 2);
        TypePtr t;
        EValuePtr ev0 = _evalNumeric(args[0], env, t);
        EValuePtr ev1 = _evalNumeric(args[1], env, t);
        assert(out.ptr());
        assert(out->type == boolType);
        binaryNumericOp<Op_numericEqualsP>(ev0, ev1, out);
        return out;
    }

    case PRIM_numericLesserP : {
        ensureArity(args, 2);
        TypePtr t;
        EValuePtr ev0 = _evalNumeric(args[0], env, t);
        EValuePtr ev1 = _evalNumeric(args[1], env, t);
        assert(out.ptr());
        assert(out->type == boolType);
        binaryNumericOp<Op_numericLesserP>(ev0, ev1, out);
        return out;
    }

    case PRIM_numericAdd : {
        ensureArity(args, 2);
        TypePtr t;
        EValuePtr ev0 = _evalNumeric(args[0], env, t);
        EValuePtr ev1 = _evalNumeric(args[1], env, t);
        assert(out.ptr());
        assert(out->type == t);
        binaryNumericOp<Op_numericAdd>(ev0, ev1, out);
        return out;
    }

    case PRIM_numericSubtract : {
        ensureArity(args, 2);
        TypePtr t;
        EValuePtr ev0 = _evalNumeric(args[0], env, t);
        EValuePtr ev1 = _evalNumeric(args[1], env, t);
        assert(out.ptr());
        assert(out->type == t);
        binaryNumericOp<Op_numericSubtract>(ev0, ev1, out);
        return out;
    }

    case PRIM_numericMultiply : {
        ensureArity(args, 2);
        TypePtr t;
        EValuePtr ev0 = _evalNumeric(args[0], env, t);
        EValuePtr ev1 = _evalNumeric(args[1], env, t);
        assert(out.ptr());
        assert(out->type == t);
        binaryNumericOp<Op_numericMultiply>(ev0, ev1, out);
        return out;
    }

    case PRIM_numericDivide : {
        ensureArity(args, 2);
        TypePtr t;
        EValuePtr ev0 = _evalNumeric(args[0], env, t);
        EValuePtr ev1 = _evalNumeric(args[1], env, t);
        assert(out.ptr());
        assert(out->type == t);
        binaryNumericOp<Op_numericDivide>(ev0, ev1, out);
        return out;
    }

    case PRIM_numericNegate : {
        ensureArity(args, 1);
        TypePtr t;
        EValuePtr ev = _evalNumeric(args[0], env, t);
        assert(out.ptr());
        assert(out->type == t);
        unaryNumericOp<Op_numericNegate>(ev, out);
        return out;
    }

    case PRIM_integerRemainder : {
        ensureArity(args, 2);
        TypePtr t;
        EValuePtr ev0 = _evalInteger(args[0], env, t);
        EValuePtr ev1 = _evalInteger(args[1], env, t);
        assert(out.ptr());
        assert(out->type == t);
        binaryIntegerOp<Op_integerRemainder>(ev0, ev1, out);
        return out;
    }

    case PRIM_integerShiftLeft : {
        ensureArity(args, 2);
        TypePtr t;
        EValuePtr ev0 = _evalInteger(args[0], env, t);
        EValuePtr ev1 = _evalInteger(args[1], env, t);
        assert(out.ptr());
        assert(out->type == t);
        binaryIntegerOp<Op_integerShiftLeft>(ev0, ev1, out);
        return out;
    }

    case PRIM_integerShiftRight : {
        ensureArity(args, 2);
        TypePtr t;
        EValuePtr ev0 = _evalInteger(args[0], env, t);
        EValuePtr ev1 = _evalInteger(args[1], env, t);
        assert(out.ptr());
        assert(out->type == t);
        binaryIntegerOp<Op_integerShiftRight>(ev0, ev1, out);
        return out;
    }

    case PRIM_integerBitwiseAnd : {
        ensureArity(args, 2);
        TypePtr t;
        EValuePtr ev0 = _evalInteger(args[0], env, t);
        EValuePtr ev1 = _evalInteger(args[1], env, t);
        assert(out.ptr());
        assert(out->type == t);
        binaryIntegerOp<Op_integerBitwiseAnd>(ev0, ev1, out);
        return out;
    }

    case PRIM_integerBitwiseOr : {
        ensureArity(args, 2);
        TypePtr t;
        EValuePtr ev0 = _evalInteger(args[0], env, t);
        EValuePtr ev1 = _evalInteger(args[1], env, t);
        assert(out.ptr());
        assert(out->type == t);
        binaryIntegerOp<Op_integerBitwiseOr>(ev0, ev1, out);
        return out;
    }

    case PRIM_integerBitwiseXor : {
        ensureArity(args, 2);
        TypePtr t;
        EValuePtr ev0 = _evalInteger(args[0], env, t);
        EValuePtr ev1 = _evalInteger(args[1], env, t);
        assert(out.ptr());
        assert(out->type == t);
        binaryIntegerOp<Op_integerBitwiseXor>(ev0, ev1, out);
        return out;
    }

    case PRIM_integerBitwiseNot : {
        ensureArity(args, 1);
        TypePtr t;
        EValuePtr ev0 = _evalInteger(args[0], env, t);
        assert(out.ptr());
        assert(out->type == t);
        unaryIntegerOp<Op_integerBitwiseNot>(ev0, out);
        return out;
    }

    case PRIM_numericConvert : {
        ensureArity(args, 2);
        TypePtr dest = evaluateType(args[0], env);
        TypePtr src;
        EValuePtr ev = _evalNumeric(args[1], env, src);
        assert(out.ptr());
        assert(out->type == dest);
        op_numericConvert(out, ev);
        return out;
    }

    case PRIM_Pointer :
        error("Pointer type constructor cannot be called");

    case PRIM_addressOf : {
        ensureArity(args, 1);
        PValuePtr pv = analyzeValue(args[0], env);
        if (pv->isTemp)
            error("cannot take address of a temporary");
        EValuePtr ev = evalAsRef(args[0], env, pv);
        assert(out.ptr());
        assert(out->type == pointerType(ev->type));
        *((void **)out->addr) = (void *)ev->addr;
        return out;
    }

    case PRIM_pointerDereference : {
        ensureArity(args, 1);
        PointerTypePtr t;
        EValuePtr ev = _evalPointer(args[0], env, t);
        void *ptr = *((void **)ev->addr);
        assert(!out);
        return new EValue(t->pointeeType, (char *)ptr);
    }

    case PRIM_pointerEqualsP : {
        ensureArity(args, 2);
        PointerTypePtr t;
        EValuePtr v0 = _evalPointer(args[0], env, t);
        EValuePtr v1 = _evalPointer(args[1], env, t);
        bool flag = *((void **)v0->addr) == *((void **)v1->addr);
        assert(out.ptr());
        assert(out->type == boolType);
        *((char *)out->addr) = flag ? 1 : 0;
        return out;
    }

    case PRIM_pointerLesserP : {
        ensureArity(args, 2);
        PointerTypePtr t;
        EValuePtr v0 = _evalPointer(args[0], env, t);
        EValuePtr v1 = _evalPointer(args[1], env, t);
        bool flag = *((void **)v0->addr) < *((void **)v1->addr);
        assert(out.ptr());
        assert(out->type == boolType);
        *((char *)out->addr) = flag ? 1 : 0;
        return out;
    }

    case PRIM_pointerOffset : {
        ensureArity(args, 2);
        PointerTypePtr t;
        EValuePtr v0 = _evalPointer(args[0], env, t);
        TypePtr offsetT;
        EValuePtr v1 = _evalInteger(args[1], env, offsetT);
        ptrdiff_t offset = op_intToPtrInt(v1);
        char *ptr = *((char **)v0->addr);
        ptr += offset*typeSize(t->pointeeType);
        assert(out.ptr());
        assert(out->type == t.ptr());
        *((void **)out->addr) = (void *)ptr;
        return out;
    }

    case PRIM_pointerToInt : {
        ensureArity(args, 2);
        TypePtr dest = evaluateType(args[0], env);
        if (dest->typeKind != INTEGER_TYPE)
            error(args[0], "invalid integer type");
        PointerTypePtr pt;
        EValuePtr ev = _evalPointer(args[1], env, pt);
        assert(out.ptr());
        assert(out->type == dest);
        void *ptr = *((void **)ev->addr);
        op_pointerToInt(out, ptr);
        return out;
    }

    case PRIM_intToPointer : {
        ensureArity(args, 2);
        TypePtr pointeeType = evaluateType(args[0], env);
        TypePtr dest = pointerType(pointeeType);
        TypePtr t;
        EValuePtr ev = _evalInteger(args[1], env, t);
        assert(out.ptr());
        assert(out->type == dest);
        ptrdiff_t ptrInt = op_intToPtrInt(ev);
        *((void **)out->addr) = (void *)ptrInt;
        return out;
    }

    case PRIM_CodePointerP : {
        ensureArity(args, 1);
        ObjectPtr y = evaluateStatic(args[0], env);
        bool isCPType = false;
        if (y->objKind == TYPE) {
            Type *t = (Type *)y.ptr();
            if (t->typeKind == CODE_POINTER_TYPE)
                isCPType = true;
        }
        assert(out.ptr());
        assert(out->type == boolType);
        *((char *)out->addr) = isCPType ? 1 : 0;
        return out;
    }

    case PRIM_CodePointer :
        error("CodePointer type constructor cannot be called");

    case PRIM_RefCodePointer :
        error("RefCodePointer type constructor cannot be called");

    case PRIM_makeCodePointer : {
        if (args.size() < 1)
            error("incorrect number of arguments");
        ObjectPtr callable = evaluateStatic(args[0], env);
        switch (callable->objKind) {
        case TYPE :
        case RECORD :
        case PROCEDURE :
        case OVERLOADABLE :
            break;
        default :
            error(args[0], "invalid callable");
        }
        const vector<bool> &isStaticFlags =
            lookupIsStaticFlags(callable, args.size()-1);
        vector<ObjectPtr> argsKey;
        vector<ValueTempness> argsTempness;
        vector<LocationPtr> argLocations;
        for (unsigned i = 1; i < args.size(); ++i) {
            if (!isStaticFlags[i-1]) {
                TypePtr t = evaluateType(args[i], env);
                argsKey.push_back(t.ptr());
                argsTempness.push_back(LVALUE);
            }
            else {
                ObjectPtr param = evaluateStatic(args[i], env);
                argsKey.push_back(param);
            }
            argLocations.push_back(args[i]->location);
        }

        InvokeStackContext invokeStackContext(callable, argsKey);

        InvokeEntryPtr entry = codegenCallable(callable, isStaticFlags,
                                               argsKey, argsTempness,
                                               argLocations);
        if (entry->inlined)
            error(args[0], "cannot create pointer to inlined code");
        vector<TypePtr> argTypes = entry->fixedArgTypes;
        if (entry->hasVarArgs) {
            argTypes.insert(argTypes.end(),
                            entry->varArgTypes.begin(),
                            entry->varArgTypes.end());
        }
        TypePtr cpType = codePointerType(argTypes,
                                         entry->returnType,
                                         entry->returnIsTemp);
        assert(out.ptr());
        assert(out->type == cpType);
        void *funcPtr = llvmEngine->getPointerToGlobal(entry->llvmFunc);
        *((void **)out->addr) = funcPtr;
        return out;
    }

    case PRIM_CCodePointerP : {
        ensureArity(args, 1);
        ObjectPtr y = evaluateStatic(args[0], env);
        bool isCCPType = false;
        if (y->objKind == TYPE) {
            Type *t = (Type *)y.ptr();
            if (t->typeKind == CCODE_POINTER_TYPE)
                isCCPType = true;
        }
        assert(out.ptr());
        assert(out->type == boolType);
        *((char *)out->addr) = isCCPType ? 1 : 0;
        return out;
    }

    case PRIM_CCodePointer :
        error("CCodePointer type constructor cannot be called");

    case PRIM_StdCallCodePointer :
        error("StdCallCodePointer type constructor cannot be called");

    case PRIM_FastCallCodePointer :
        error("FastCallCodePointer type constructor cannot be called");

    case PRIM_makeCCodePointer : {
        if (args.size() < 1)
            error("incorrect number of arguments");
        ObjectPtr callable = evaluateStatic(args[0], env);
        switch (callable->objKind) {
        case TYPE :
        case RECORD :
        case PROCEDURE :
        case OVERLOADABLE :
            break;
        default :
            error(args[0], "invalid callable");
        }
        const vector<bool> &isStaticFlags =
            lookupIsStaticFlags(callable, args.size()-1);
        vector<ObjectPtr> argsKey;
        vector<ValueTempness> argsTempness;
        vector<LocationPtr> argLocations;
        for (unsigned i = 1; i < args.size(); ++i) {
            if (!isStaticFlags[i-1]) {
                TypePtr t = evaluateType(args[i], env);
                argsKey.push_back(t.ptr());
                argsTempness.push_back(LVALUE);
            }
            else {
                ObjectPtr param = evaluateStatic(args[i], env);
                argsKey.push_back(param);
            }
            argLocations.push_back(args[i]->location);
        }

        InvokeStackContext invokeStackContext(callable, argsKey);

        InvokeEntryPtr entry = codegenCallable(callable, isStaticFlags,
                                               argsKey, argsTempness,
                                               argLocations);
        if (entry->inlined)
            error(args[0], "cannot create pointer to inlined code");
        vector<TypePtr> argTypes = entry->fixedArgTypes;
        if (entry->hasVarArgs) {
            argTypes.insert(argTypes.end(),
                            entry->varArgTypes.begin(),
                            entry->varArgTypes.end());
        }
        TypePtr ccpType = cCodePointerType(CC_DEFAULT,
                                           argTypes,
                                           false,
                                           entry->returnType);
        string callableName = getCodeName(callable);
        if (!entry->llvmCWrapper)
            codegenCWrapper(entry, callableName);
        assert(out.ptr());
        assert(out->type == ccpType);
        void *funcPtr = llvmEngine->getPointerToGlobal(entry->llvmCWrapper);
        *((void **)out->addr) = funcPtr;
        return out;
    }

    case PRIM_pointerCast : {
        ensureArity(args, 2);
        TypePtr dest = evaluatePointerLikeType(args[0], env);
        TypePtr t;
        EValuePtr v = _evalPointerLike(args[1], env, t);
        assert(out.ptr());
        assert(out->type == dest);
        *((void **)out->addr) = *((void **)v->addr);
        return out;
    }

    case PRIM_Array :
        error("Array type constructor cannot be called");

    case PRIM_array : {
        assert(out.ptr());
        assert(out->type->typeKind == ARRAY_TYPE);
        ArrayType *t = (ArrayType *)out->type.ptr();
        assert((int)args.size() == t->size);
        TypePtr etype = t->elementType;
        for (unsigned i = 0; i < args.size(); ++i) {
            PValuePtr parg = analyzeValue(args[i], env);
            if (parg->type != etype)
                error(args[i], "array element type mismatch");
            char *ptr = out->addr + typeSize(etype)*i;
            evalIntoValue(args[i], env, parg, new EValue(etype, ptr));
        }
        return out;
    }

    case PRIM_arrayRef : {
        ensureArity(args, 2);
        PValuePtr parray = analyzeValue(args[0], env);
        if (parray->type->typeKind != ARRAY_TYPE)
            error(args[0], "expecting array type");
        ArrayType *at = (ArrayType *)parray->type.ptr();
        EValuePtr earray = evalAsRef(args[0], env, parray);
        TypePtr indexType;
        EValuePtr iv = _evalInteger(args[1], env, indexType);
        ptrdiff_t i = op_intToPtrInt(iv);
        char *ptr = earray->addr + i*typeSize(at->elementType);
        return new EValue(at->elementType, ptr);
    }

    case PRIM_TupleP : {
        ensureArity(args, 1);
        ObjectPtr y = evaluateStatic(args[0], env);
        bool isTupleType = false;
        if (y->objKind == TYPE) {
            Type *t = (Type *)y.ptr();
            if (t->typeKind == TUPLE_TYPE)
                isTupleType = true;
        }
        assert(out.ptr());
        assert(out->type == boolType);
        *((char *)out->addr) = isTupleType ? 1 : 0;
        return out;
    }

    case PRIM_Tuple :
        error("Tuple type constructor cannot be called");

    case PRIM_TupleElementCount : {
        ensureArity(args, 1);
        TypePtr y = evaluateTupleType(args[0], env);
        TupleType *z = (TupleType *)y.ptr();
        ObjectPtr obj = sizeTToValueHolder(z->elementTypes.size()).ptr();
        return evalStaticObject(obj, out);
    }

    case PRIM_TupleElementOffset : {
        ensureArity(args, 2);
        TypePtr y = evaluateTupleType(args[0], env);
        TupleType *z = (TupleType *)y.ptr();
        size_t i = evaluateSizeT(args[1], env);
        const llvm::StructLayout *layout = tupleTypeLayout(z);
        ObjectPtr obj =  sizeTToValueHolder(layout->getElementOffset(i)).ptr();
        return evalStaticObject(obj, out);
    }

    case PRIM_tuple : {
        assert(out.ptr());
        assert(out->type->typeKind == TUPLE_TYPE);
        TupleType *tt = (TupleType *)out->type.ptr();
        assert(args.size() == tt->elementTypes.size());
        const llvm::StructLayout *layout = tupleTypeLayout(tt);
        for (unsigned i = 0; i < args.size(); ++i) {
            PValuePtr parg = analyzeValue(args[i], env);
            if (parg->type != tt->elementTypes[i])
                error(args[i], "argument type mismatch");
            char *ptr = out->addr + layout->getElementOffset(i);
            EValuePtr eargDest = new EValue(tt->elementTypes[i], ptr);
            evalIntoValue(args[i], env, parg, eargDest);
        }
        return out;
    }

    case PRIM_tupleRef : {
        ensureArity(args, 2);
        PValuePtr ptuple = analyzeValue(args[0], env);
        if (ptuple->type->typeKind != TUPLE_TYPE)
            error(args[0], "expecting a tuple");
        TupleType *tt = (TupleType *)ptuple->type.ptr();
        const llvm::StructLayout *layout = tupleTypeLayout(tt);
        EValuePtr etuple = evalAsRef(args[0], env, ptuple);
        size_t i = evaluateSizeT(args[1], env);
        if (i >= tt->elementTypes.size())
            error(args[1], "tuple index out of range");
        char *ptr = etuple->addr + layout->getElementOffset(i);
        assert(!out);
        return new EValue(tt->elementTypes[i], ptr);
    }

    case PRIM_RecordP : {
        ensureArity(args, 1);
        ObjectPtr y = evaluateStatic(args[0], env);
        bool isRecordType = false;
        if (y->objKind == TYPE) {
            Type *t = (Type *)y.ptr();
            if (t->typeKind == RECORD_TYPE)
                isRecordType = true;
        }
        assert(out.ptr());
        assert(out->type == boolType);
        *((char *)out->addr) = isRecordType ? 1 : 0;
        return out;
    }

    case PRIM_RecordFieldCount : {
        ensureArity(args, 1);
        TypePtr y = evaluateRecordType(args[0], env);
        RecordType *z = (RecordType *)y.ptr();
        const vector<TypePtr> &fieldTypes = recordFieldTypes(z);
        ObjectPtr obj = sizeTToValueHolder(fieldTypes.size()).ptr();
        return evalStaticObject(obj, out);
    }

    case PRIM_RecordFieldOffset : {
        ensureArity(args, 2);
        TypePtr y = evaluateRecordType(args[0], env);
        RecordType *z = (RecordType *)y.ptr();
        size_t i = evaluateSizeT(args[1], env);
        const vector<TypePtr> &fieldTypes = recordFieldTypes(z);
        if (i >= fieldTypes.size())
            error("field index out of range");
        const llvm::StructLayout *layout = recordTypeLayout(z);
        ObjectPtr obj = sizeTToValueHolder(layout->getElementOffset(i)).ptr();
        return evalStaticObject(obj, out);
    }

    case PRIM_RecordFieldIndex : {
        ensureArity(args, 2);
        TypePtr y = evaluateRecordType(args[0], env);
        RecordType *z = (RecordType *)y.ptr();
        IdentifierPtr fname = evaluateIdentifier(args[1], env);
        const map<string, size_t> &fieldIndexMap = recordFieldIndexMap(z);
        map<string, size_t>::const_iterator fi =
            fieldIndexMap.find(fname->str);
        if (fi == fieldIndexMap.end())
            error("field not in record");
        ObjectPtr obj = sizeTToValueHolder(fi->second).ptr();
        return evalStaticObject(obj, out);
    }

    case PRIM_recordFieldRef : {
        ensureArity(args, 2);
        PValuePtr prec = analyzeValue(args[0], env);
        if (prec->type->typeKind != RECORD_TYPE)
            error(args[0], "expecting a record");
        RecordType *rt = (RecordType *)prec->type.ptr();
        const llvm::StructLayout *layout = recordTypeLayout(rt);
        const vector<TypePtr> &fieldTypes = recordFieldTypes(rt);
        EValuePtr erec = evalAsRef(args[0], env, prec);
        size_t i = evaluateSizeT(args[1], env);
        char *ptr = erec->addr + layout->getElementOffset(i);
        assert(!out);
        return new EValue(fieldTypes[i], ptr);
    }

    case PRIM_recordFieldRefByName : {
        ensureArity(args, 2);
        PValuePtr prec = analyzeValue(args[0], env);
        if (prec->type->typeKind != RECORD_TYPE)
            error(args[0], "expecting a record");
        RecordType *rt = (RecordType *)prec->type.ptr();
        const llvm::StructLayout *layout = recordTypeLayout(rt);
        const vector<TypePtr> &fieldTypes = recordFieldTypes(rt);
        EValuePtr erec = evalAsRef(args[0], env, prec);
        IdentifierPtr fname = evaluateIdentifier(args[1], env);
        const map<string, size_t> &fieldIndexMap = recordFieldIndexMap(rt);
        map<string, size_t>::const_iterator fi = fieldIndexMap.find(fname->str);
        if (fi == fieldIndexMap.end())
            error(args[1], "field not in record");
        size_t i = fi->second;
        char *ptr = erec->addr + layout->getElementOffset(i);
        assert(!out);
        return new EValue(fieldTypes[i], ptr);
    }

    case PRIM_Static :
        error("Static type constructor cannot be called");

    case PRIM_StaticName : {
        ensureArity(args, 1);
        ObjectPtr y = evaluateStatic(args[0], env);
        ostringstream sout;
        printName(sout, y);
        ExprPtr z = new StringLiteral(sout.str());
        return evalExpr(z, env, out);
    }

    case PRIM_EnumP : {
        ensureArity(args, 1);
        ObjectPtr y = evaluateStatic(args[0], env);
        bool isEnumType = false;
        if (y->objKind == TYPE) {
            Type *t = (Type *)y.ptr();
            if (t->typeKind == ENUM_TYPE)
                isEnumType = true;
        }
        assert(out.ptr());
        assert(out->type == boolType);
        *((char *)out->addr) = isEnumType ? 1 : 0;
        return out;
    }

    case PRIM_enumToInt : {
        ensureArity(args, 1);
        PValuePtr pv = analyzeValue(args[0], env);
        if (pv->type->typeKind != ENUM_TYPE)
            error(args[0], "expecting enum value");
        EValuePtr ev = evalAsRef(args[0], env, pv);
        assert(out.ptr());
        assert(out->type == cIntType);
        *((int *)out->addr) = *((int *)ev->addr);
        return out;
    }

    case PRIM_intToEnum : {
        ensureArity(args, 2);
        TypePtr t = evaluateEnumerationType(args[0], env);
        assert(out.ptr());
        assert(out->type == t);
        TypePtr t2 = cIntType;
        EValuePtr v = _evalInteger(args[1], env, t2);
        *((int *)out->addr) = *((int *)v->addr);
        return out;
    }

    default :
        assert(false);
        return NULL;
    }
}
