#include "clay.hpp"


void evalValueInit(EValuePtr dest);
void evalValueDestroy(EValuePtr dest);
void evalValueCopy(EValuePtr dest, EValuePtr src);
void evalValueAssign(EValuePtr dest, EValuePtr src);
bool evalToBoolFlag(EValuePtr a);

int evalMarkStack();
void evalDestroyStack(int marker);
void evalPopStack(int marker);
void evalDestroyAndPopStack(int marker);
EValuePtr evalAllocValue(TypePtr t);

void evalRootIntoValue(ExprPtr expr,
                       EnvPtr env,
                       PValuePtr pv,
                       EValuePtr out);
void evalIntoValue(ExprPtr expr, EnvPtr env, PValuePtr pv, EValuePtr out);
EValuePtr evalAsRef(ExprPtr expr, EnvPtr env, PValuePtr pv);
EValuePtr evalValue(ExprPtr expr, EnvPtr env, EValuePtr out);
EValuePtr evalMaybeVoid(ExprPtr expr, EnvPtr env, EValuePtr out);
EValuePtr evalExpr(ExprPtr expr, EnvPtr env, EValuePtr out);

EValuePtr evalStaticObject(ObjectPtr x, EValuePtr out);
void evalValueHolder(ValueHolderPtr v, EValuePtr out);

EValuePtr evalInvokeValue(EValuePtr x,
                          const vector<ExprPtr> &args,
                          EnvPtr env,
                          EValuePtr out);

void evalInvokeVoid(ObjectPtr x,
                    const vector<ExprPtr> &args,
                    EnvPtr env);
EValuePtr evalInvoke(ObjectPtr x,
                     const vector<ExprPtr> &args,
                     EnvPtr env,
                     EValuePtr out);
EValuePtr evalInvokeCallable(ObjectPtr x,
                             const vector<ExprPtr> &args,
                             EnvPtr env,
                             EValuePtr out);
bool evalInvokeSpecialCase(ObjectPtr x,
                           const vector<bool> &isStaticFlags,
                           const vector<ObjectPtr> &argsKey);
EValuePtr evalInvokeCode(InvokeEntryPtr entry,
                         const vector<ExprPtr> &args,
                         EnvPtr env,
                         EValuePtr out);
EValuePtr evalInvokeInlined(InvokeEntryPtr entry,
                            const vector<ExprPtr> &args,
                            EnvPtr env,
                            EValuePtr out);
EValuePtr evalInvokePrimOp(PrimOpPtr x,
                           const vector<ExprPtr> &args,
                           EnvPtr env,
                           EValuePtr out);

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
    case STATIC_PROCEDURE :
    case STATIC_OVERLOADABLE :
    case MODULE_HOLDER :
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

    case STATIC_PROCEDURE : {
        StaticProcedurePtr y = (StaticProcedure *)x.ptr();
        StaticInvokeEntryPtr entry = analyzeStaticProcedure(y, args, env);
        assert(entry->result.ptr());
        return evalStaticObject(entry->result, out);
    }

    case STATIC_OVERLOADABLE : {
        StaticOverloadablePtr y = (StaticOverloadable *)x.ptr();
        StaticInvokeEntryPtr entry = analyzeStaticOverloadable(y, args, env);
        assert(entry->result.ptr());
        return evalStaticObject(entry->result, out);
    }

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
    // TODO: inlined procedures are not supported yet.
    return out;
}



//
// evalInvokePrimOp
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

template <template<typename> class T>
static void evalNumericOp(EValuePtr a, EValuePtr b, EValuePtr out)
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
            case 64 : T<long long>().eval(a, b, out); break;
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
    }

    default :
        assert(false);
    }
}

template <typename T>
class EvalHelper {
public :
    void eval(EValuePtr a, EValuePtr b, EValuePtr out) {
        eval2(*((T *)a->addr), *((T *)b->addr), out->addr);
    }
    virtual void eval2(T &a, T &b, void *out) = 0;
};

template <typename T>
class Eval_numericEqualsP : public EvalHelper<T> {
public :
    virtual void eval2(T &a, T &b, void *out) {
        *((char *)out) = (a == b) ? 1 : 0;
    }
};

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
        ObjectPtr obj = boolToValueHolder(y->objKind == TYPE).ptr();
        return evalStaticObject(obj, out);
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
        evalNumericOp<Eval_numericEqualsP>(ev0, ev1, out);
        return out;
    }

    default :
        assert(false);

    }
}
