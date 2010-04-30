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
        assert(out.ptr());
        assert(out->type == y->type);
        memcpy(out->addr, y->buf, typeSize(y->type));
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
        EValuePtr ev = new EValue(type, (char *)x->value.data());
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
        if (!y->llGlobal) {
            ObjectPtr z = analyzeStaticObject(y);
            assert(z->objKind == PVALUE);
        }
    }
    }
    return out;
}



//
// evalInvokeValue
//

EValuePtr evalInvokeValue(EValuePtr x,
                          const vector<ExprPtr> &args,
                          EnvPtr env,
                          EValuePtr out)
{
    return out;
}



//
// evalInvoke
//

void evalInvokeVoid(ObjectPtr x,
                    const vector<ExprPtr> &args,
                    EnvPtr env)
{
}

EValuePtr evalInvoke(ObjectPtr x,
                     const vector<ExprPtr> &args,
                     EnvPtr env,
                     EValuePtr out)
{
    return out;
}
