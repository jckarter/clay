#include "clay.hpp"


ValueHolder::ValueHolder(TypePtr type)
    : Object(VALUE_HOLDER), type(type)
{
    this->buf = (char *)malloc(typeSize(type));
}

ValueHolder::~ValueHolder()
{
    // FIXME: call clay 'destroy'
    free(this->buf);
}

// FIXME: this doesn't handle arbitrary values (need to call clay)
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
        return memcmp(a1->buf, b1->buf, n) == 0;
    }

    default :
        return a == b;
    }
}

// FIXME: this doesn't handle arbitrary values (need to call clay)
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

vector<TypePtr> evaluateTypeTuple(ExprPtr expr, EnvPtr env)
{
    PValuePtr x = analyzeValue(expr, env);
    if (!x)
        error(expr, "recursion during static evaluation");

    switch (x->type->typeKind) {

    case TUPLE_TYPE : {
        TupleType *y = (TupleType *)x->type.ptr();
        vector<TypePtr> types;
        bool ok = true;
        for (unsigned i = 0; i < y->elementTypes.size(); ++i) {
            if (y->elementTypes[i]->typeKind != STATIC_TYPE) {
                ok = false;
                break;
            }
            StaticType *z = (StaticType *)y->elementTypes[i].ptr();
            if (z->obj->objKind != TYPE) {
                ok = false;
                break;
            }
            types.push_back((Type *)z->obj.ptr());
        }
        if (ok)
            return types;
        break;
    }

    case STATIC_TYPE : {
        StaticType *y = (StaticType *)x->type.ptr();
        if (y->obj->objKind == TYPE) {
            vector<TypePtr> types;
            types.push_back((Type *)y->obj.ptr());
            return types;
        }
        break;
    }

    }

    error(expr, "expecting zero or more types");
    return vector<TypePtr>();
}

void evaluateReturnSpecs(const vector<ReturnSpecPtr> &returnSpecs,
                         EnvPtr env,
                         vector<bool> &returnIsRef,
                         vector<TypePtr> &returnTypes)
{
    returnIsRef.clear();
    returnTypes.clear();
    for (unsigned i = 0; i < returnSpecs.size(); ++i) {
        ReturnSpecPtr x = returnSpecs[i];
        returnIsRef.push_back(x->byRef);
        returnTypes.push_back(evaluateType(x->type, env));
    }
}

MultiStaticPtr evaluateExprStatic(ExprPtr expr, EnvPtr env)
{
    MultiPValuePtr mpv = analyzeMultiValue(expr, env);
    vector<ValueHolderPtr> valueHolders;
    MultiEValuePtr mev = new MultiEValue();
    for (unsigned i = 0; i < mpv->size(); ++i) {
        TypePtr t = mpv->values[i]->type;
        ValueHolderPtr vh = new ValueHolder(t);
        valueHolders.push_back(vh);
        EValuePtr ev = new EValue(t, (char *)vh->buf);
        mev->values.push_back(ev);
    }
    int marker = evalMarkStack();
    evalIntoValues(expr, env, mev);
    evalDestroyAndPopStack(marker);
    MultiStaticPtr ms = new MultiStatic();
    for (unsigned i = 0; i < valueHolders.size(); ++i) {
        Type *t = (Type *)valueHolders[i]->type.ptr();
        if (t->typeKind == STATIC_TYPE) {
            StaticType *st = (StaticType *)t;
            ms->values.push_back(st->obj);
        }
        else {
            ms->values.push_back(valueHolders[i].ptr());
        }
    }
    return ms;
}

ObjectPtr evaluateOneStatic(ExprPtr expr, EnvPtr env)
{
    MultiStaticPtr ms = evaluateExprStatic(expr, env);
    if (ms->size() != 1)
        arityError(expr, 1, ms->size());
    return ms->values[0];
}

MultiStaticPtr evaluateMultiStatic(const vector<ExprPtr> &exprs, EnvPtr env)
{
    MultiStaticPtr out = new MultiStatic();
    for (unsigned i = 0; i < exprs.size(); ++i) {
        ExprPtr x = exprs[i];
        if (x->exprKind == VAR_ARGS_REF) {
            MultiStaticPtr y = evaluateExprStatic(x, env);
            out->add(y);
        }
        else {
            ObjectPtr y = evaluateOneStatic(x, env);
            out->add(y);
        }
    }
    return out;
}

TypePtr evaluateType(ExprPtr expr, EnvPtr env)
{
    ObjectPtr v = evaluateOneStatic(expr, env);
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
    ObjectPtr v = evaluateOneStatic(expr, env);
    if (v->objKind != IDENTIFIER)
        error(expr, "expecting an identifier value");
    return (Identifier *)v.ptr();
}

int evaluateInt(ExprPtr expr, EnvPtr env)
{
    ObjectPtr v = evaluateOneStatic(expr, env);
    if (v->objKind != VALUE_HOLDER)
        error(expr, "expecting an Int value");
    ValueHolderPtr vh = (ValueHolder *)v.ptr();
    if (vh->type != cIntType)
        error(expr, "expecting an Int value");
    return *(int *)vh->buf;
}

size_t evaluateSizeT(ExprPtr expr, EnvPtr env)
{
    ObjectPtr v = evaluateOneStatic(expr, env);
    if (v->objKind != VALUE_HOLDER)
        error(expr, "expecting a SizeT value");
    ValueHolderPtr vh = (ValueHolder *)v.ptr();
    if (vh->type != cSizeTType)
        error(expr, "expecting a SizeT value");
    return *(size_t *)vh->buf;
}

ptrdiff_t evaluatePtrDiffT(ExprPtr expr, EnvPtr env)
{
    ObjectPtr v = evaluateOneStatic(expr, env);
    if (v->objKind != VALUE_HOLDER)
        error(expr, "expecting a PtrDiffT value");
    ValueHolderPtr vh = (ValueHolder *)v.ptr();
    if (vh->type != cSizeTType)
        error(expr, "expecting a PtrDiffT value");
    return *(ptrdiff_t *)vh->buf;
}

bool evaluateBool(ExprPtr expr, EnvPtr env)
{
    ObjectPtr v = evaluateOneStatic(expr, env);
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
    evalInvoke(dest->type.ptr(), vector<ExprPtr>(), new Env(),
               new MultiEValue(dest));
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
    evalInvoke(dest->type.ptr(), args, new Env(), new MultiEValue(dest));
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

void evalIntoValues(ExprPtr expr, EnvPtr env, MultiEValuePtr out)
{
    MultiPValuePtr mpv = analyzeMultiValue(expr, env);
    if (mpv->size() != out->size())
        arityError(expr, out->size(), mpv->size());
    MultiEValuePtr out2 = new MultiEValue();
    for (unsigned i = 0; i < mpv->size(); ++i) {
        if (mpv->values[i]->isTemp)
            out2->values.push_back(out->values[i]);
        else
            out2->values.push_back(NULL);
    }
    evalExpr(expr, env, out2);
    for (unsigned i = 0; i < mpv->size(); ++i) {
        if (!mpv->values[i]->isTemp) {
            assert(out->values[i].ptr());
            assert(out2->values[i].ptr());
            evalValueCopy(out->values[i], out2->values[i]);
        }
    }
}

void evalIntoOne(ExprPtr expr, EnvPtr env, EValuePtr out)
{
    evalIntoValues(expr, env, new MultiEValue(out));
}

EValuePtr evalOne(ExprPtr expr, EnvPtr env, EValuePtr out)
{
    PValuePtr pv = analyzeValue(expr, env);
    if (pv->isTemp) {
        assert(out.ptr());
        assert(out->type == pv->type);
        evalIntoOne(expr, env, out);
        return out;
    }
    else {
        assert(!out);
        MultiEValuePtr mev = new MultiEValue(NULL);
        evalExpr(expr, env, mev);
        assert(mev->values[0].ptr());
        return mev->values[0];
    }
}

EValuePtr evalOneAsRef(ExprPtr expr, EnvPtr env)
{
    PValuePtr pv = analyzeValue(expr, env);
    EValuePtr out;
    if (pv->isTemp)
        out = evalAllocValue(pv->type);
    else
        out = NULL;
    return evalOne(expr, env, out);
}

void evalExpr(ExprPtr expr, EnvPtr env, MultiEValuePtr out)
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
        break;
    }

    case CHAR_LITERAL : {
        CharLiteral *x = (CharLiteral *)expr.ptr();
        if (!x->desugared)
            x->desugared = desugarCharLiteral(x->value);
        evalExpr(x->desugared, env, out);
        break;
    }

    case STRING_LITERAL : {
        StringLiteral *x = (StringLiteral *)expr.ptr();
        TypePtr type = arrayType(int8Type, x->value.size());
        EValuePtr ev = new EValue(type, const_cast<char *>(x->value.data()));
        vector<ExprPtr> args;
        args.push_back(new ObjectExpr(ev.ptr()));
        evalInvoke(kernelName("String"), args, env, out);
        break;
    }

    case NAME_REF : {
        NameRef *x = (NameRef *)expr.ptr();
        ObjectPtr y = lookupEnv(env, x->name);
        if (y->objKind == EXPRESSION)
            evalExpr((Expr *)y.ptr(), env, out);
        else
            evalStaticObject(y, out);
        break;
    }

    case TUPLE : {
        Tuple *x = (Tuple *)expr.ptr();
        vector<ExprPtr> args2;
        bool expanded = expandVarArgs(x->args, env, args2);
        if (!expanded && (args2.size() == 1))
            evalExpr(args2[0], env, out);
        else
            evalInvoke(primName("tuple"), args2, env, out);
        break;
    }

    case ARRAY : {
        Array *x = (Array *)expr.ptr();
        vector<ExprPtr> args2;
        expandVarArgs(x->args, env, args2);
        evalInvoke(primName("array"), args2, env, out);
        break;
    }

    case INDEXING : {
        Indexing *x = (Indexing *)expr.ptr();
        ObjectPtr y = analyzeOne(x->expr, env);
        assert(y.ptr());
        vector<ExprPtr> args2;
        expandVarArgs(x->args, env, args2);
        if (y->objKind == PVALUE) {
            EValuePtr ev = evalOneAsRef(x->expr, env);
            vector<ExprPtr> args3;
            args3.push_back(new ObjectExpr(ev.ptr()));
            args3.insert(args3.end(), args2.begin(), args2.end());
            ObjectPtr op = kernelName("index");
            evalInvoke(op, args3, env, out);
        }
        else {
            ObjectPtr obj = analyzeIndexing(y, args2, env);
            evalStaticObject(obj, out);
        }
        break;
    }

    case CALL : {
        Call *x = (Call *)expr.ptr();
        ObjectPtr y = analyzeOne(x->expr, env);
        assert(y.ptr());
        vector<ExprPtr> args2;
        expandVarArgs(x->args, env, args2);
        if (y->objKind == PVALUE) {
            PValue *z = (PValue *)y.ptr();
            if (z->type->typeKind == STATIC_TYPE) {
                StaticType *t = (StaticType *)z->type.ptr();
                evalInvoke(t->obj, args2, env, out);
            }
            else {
                EValuePtr ev = evalOneAsRef(x->expr, env);
                evalInvokeValue(ev, args2, env, out);
            }
        }
        else {
            evalInvoke(y, args2, env, out);
        }
        break;
    }

    case FIELD_REF : {
        FieldRef *x = (FieldRef *)expr.ptr();
        ObjectPtr y = analyzeOne(x->expr, env);
        if (y->objKind == PVALUE) {
            PValue *z = (PValue *)y.ptr();
            if (z->type->typeKind == STATIC_TYPE) {
                StaticType *t = (StaticType *)z->type.ptr();
                ExprPtr inner = new FieldRef(new ObjectExpr(t->obj), x->name);
                evalExpr(inner, env, out);
            }
            else {
                EValuePtr ev = evalOneAsRef(x->expr, env);
                vector<ExprPtr> args2;
                args2.push_back(new ObjectExpr(ev.ptr()));
                args2.push_back(new ObjectExpr(x->name.ptr()));
                ObjectPtr prim = kernelName("fieldRef");
                evalInvoke(prim, args2, env, out);
            }
        }
        else if (y->objKind == MODULE_HOLDER) {
            ModuleHolderPtr z = (ModuleHolder *)y.ptr();
            ObjectPtr obj = lookupModuleMember(z, x->name);
            evalStaticObject(obj, out);
        }
        else {
            error("invalid member access operation");
        }
        break;
    }

    case TUPLE_REF : {
        TupleRef *x = (TupleRef *)expr.ptr();
        vector<ExprPtr> args;
        args.push_back(x->expr);
        args.push_back(new ObjectExpr(sizeTToValueHolder(x->index).ptr()));
        ObjectPtr prim = primName("tupleRef");
        evalInvoke(prim, args, env, out);
        break;
    }

    case UNARY_OP : {
        UnaryOp *x = (UnaryOp *)expr.ptr();
        if (!x->desugared)
            x->desugared = desugarUnaryOp(x);
        evalExpr(x->desugared, env, out);
        break;
    }

    case BINARY_OP : {
        BinaryOp *x = (BinaryOp *)expr.ptr();
        if (!x->desugared)
            x->desugared = desugarBinaryOp(x);
        evalExpr(x->desugared, env, out);
        break;
    }

    case AND : {
        And *x = (And *)expr.ptr();
        PValuePtr pv = analyzeValue(expr, env);
        assert(out->size() == 1);
        if (pv->isTemp) {
            assert(out->values[0].ptr());
            EValuePtr ev1 = out->values[0];
            evalIntoOne(x->expr1, env, ev1);
            if (evalToBoolFlag(ev1)) {
                evalValueDestroy(ev1);
                PValuePtr pv2 = analyzeValue(x->expr2, env);
                if (pv2->type != pv->type)
                    error("type mismatch in 'and' expression");
                evalIntoOne(x->expr2, env, ev1);
            }
        }
        else {
            assert(!out->values[0]);
            EValuePtr ev = evalOne(x->expr1, env, NULL);
            if (evalToBoolFlag(ev)) {
                PValuePtr pv2 = analyzeValue(x->expr2, env);
                if (pv2->type != pv->type)
                    error("type mismatch in 'and' expression");
                ev = evalOne(x->expr2, env, NULL);
            }
            out->values[0] = ev;
        }
        break;
    }

    case OR : {
        Or *x = (Or *)expr.ptr();
        PValuePtr pv = analyzeValue(expr, env);
        assert(out->size() == 1);
        if (pv->isTemp) {
            assert(out->values[0].ptr());
            EValuePtr ev1 = out->values[0];
            evalIntoOne(x->expr1, env, ev1);
            if (!evalToBoolFlag(ev1)) {
                evalValueDestroy(ev1);
                PValuePtr pv2 = analyzeValue(x->expr2, env);
                if (pv2->type != pv->type)
                    error("type mismatch in 'or' expression");
                evalIntoOne(x->expr2, env, ev1);
            }
        }
        else {
            assert(!out->values[0]);
            EValuePtr ev = evalOne(x->expr1, env, NULL);
            if (!evalToBoolFlag(ev)) {
                PValuePtr pv2 = analyzeValue(x->expr2, env);
                if (pv2->type != pv->type)
                    error("type mismatch in 'or' expression");
                ev = evalOne(x->expr2, env, NULL);
            }
            out->values[0] = ev;
        }
        break;
    }

    case LAMBDA : {
        Lambda *x = (Lambda *)expr.ptr();
        if (!x->initialized)
            initializeLambda(x, env);
        evalExpr(x->converted, env, out);
        break;
    }

    case VAR_ARGS_REF :
        error("invalid use of ...");
        break;

    case NEW : {
        New *x = (New *)expr.ptr();
        if (!x->desugared)
            x->desugared = desugarNew(x);
        evalExpr(x->desugared, env, out);
        break;
    }

    case STATIC_EXPR : {
        StaticExpr *x = (StaticExpr *)expr.ptr();
        if (!x->desugared)
            x->desugared = desugarStaticExpr(x);
        evalExpr(x->desugared, env, out);
        break;
    }

    case FOREIGN_EXPR : {
        ForeignExpr *x = (ForeignExpr *)expr.ptr();
        evalExpr(x->expr, x->getEnv(), out);
        break;
    }

    case OBJECT_EXPR : {
        ObjectExpr *x = (ObjectExpr *)expr.ptr();
        evalStaticObject(x->obj, out);
        break;
    }

    default :
        assert(false);

    }
}



//
// evalStaticObject
//

void evalStaticObject(ObjectPtr x, MultiEValuePtr out)
{
    switch (x->objKind) {
    case ENUM_MEMBER : {
        EnumMember *y = (EnumMember *)x.ptr();
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0.ptr());
        assert(out0->type == y->type);
        *((int *)out0->addr) = y->index;
        break;
    }
    case GLOBAL_VARIABLE : {
        GlobalVariable *y = (GlobalVariable *)x.ptr();
        if (!y->llGlobal)
            codegenGlobalVariable(y);
        void *ptr = llvmEngine->getPointerToGlobal(y->llGlobal);
        assert(out->size() == 1);
        assert(!out->values[0]);
        out->values[0] = new EValue(y->type, (char *)ptr);
        break;
    }
    case EXTERNAL_VARIABLE : {
        ExternalVariable *y = (ExternalVariable *)x.ptr();
        if (!y->llGlobal)
            codegenExternalVariable(y);
        void *ptr = llvmEngine->getPointerToGlobal(y->llGlobal);
        assert(out->size() == 1);
        assert(!out->values[0]);
        out->values[0] = new EValue(y->type2, (char *)ptr);
        break;
    }
    case EXTERNAL_PROCEDURE : {
        ExternalProcedure *y = (ExternalProcedure *)x.ptr();
        if (!y->llvmFunc)
            codegenExternal(y);
        void *funcPtr = llvmEngine->getPointerToGlobal(y->llvmFunc);
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0.ptr());
        assert(out0->type == y->ptrType);
        *((void **)out0->addr) = funcPtr;
        break;
    }
    case STATIC_GLOBAL : {
        StaticGlobal *y = (StaticGlobal *)x.ptr();
        evalExpr(y->expr, y->env, out);
        break;
    }
    case VALUE_HOLDER : {
        ValueHolder *y = (ValueHolder *)x.ptr();
        evalValueHolder(y, out);
        break;
    }
    case EVALUE : {
        EValue *y = (EValue *)x.ptr();
        assert(out->size() == 1);
        assert(!out->values[0]);
        out->values[0] = y;
        break;
    }
    case TYPE :
    case PRIM_OP :
    case PROCEDURE :
    case RECORD :
    case MODULE_HOLDER :
    case IDENTIFIER :
        assert(out->size() == 1);
        assert(out->values[0].ptr());
        assert(out->values[0]->type == staticType(x));
        break;
    default :
        error("invalid static object");
        break;
    }
}



//
// evalValueHolder
//

void evalValueHolder(ValueHolderPtr v, MultiEValuePtr out)
{
    assert(out->size() == 1);
    EValuePtr out0 = out->values[0];
    assert(out0.ptr());
    assert(v->type == out0->type);

    switch (v->type->typeKind) {
    case BOOL_TYPE :
    case INTEGER_TYPE :
    case FLOAT_TYPE :
        memcpy(out0->addr, v->buf, typeSize(v->type));
        break;
    default :
        assert(false);
    }
}



//
// evalInvokeValue
//

void evalInvokeValue(EValuePtr x,
                     const vector<ExprPtr> &args,
                     EnvPtr env,
                     MultiEValuePtr out)
{
    switch (x->type->typeKind) {

    case CODE_POINTER_TYPE : {
        // TODO: implement it
        error("invoking a CodePointer not yet supported in evaluator");
        break;
    }

    case CCODE_POINTER_TYPE : {
        // TODO: implement it
        error("invoking a CCodePointer not yet supported in evaluator");
        break;
    }

    default : {
        vector<ExprPtr> args2;
        args2.push_back(new ObjectExpr(x.ptr()));
        args2.insert(args2.end(), args.begin(), args.end());
        evalInvoke(kernelName("call"), args2, env, out);
        break;
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
    ObjectPtr y = analyzeInvoke(x, args, env);
    assert(y.ptr());
    if (y->objKind != MULTI_PVALUE)
        error("void return expected");
    MultiPValuePtr mpv = (MultiPValue *)y.ptr();
    if (mpv->size() != 0)
        error("void return expected");
    evalInvoke(x, args, env, new MultiEValue());
}

void evalInvoke(ObjectPtr x,
                const vector<ExprPtr> &args,
                EnvPtr env,
                MultiEValuePtr out)
{
    switch (x->objKind) {
    case TYPE :
    case RECORD :
    case PROCEDURE :
        evalInvokeCallable(x, args, env, out);
        break;
    case PRIM_OP :
        evalInvokePrimOp((PrimOp *)x.ptr(), args, env, out);
        break;
    default :
        error("invalid call operation");
        break;
    }
}



//
// evalInvokeCallable
//

void evalInvokeCallable(ObjectPtr x,
                        const vector<ExprPtr> &args,
                        EnvPtr env,
                        MultiEValuePtr out)
{
    vector<TypePtr> argsKey;
    vector<ValueTempness> argsTempness;
    vector<LocationPtr> argLocations;
    bool result = computeArgsKey(args, env,
                                 argsKey, argsTempness,
                                 argLocations);
    assert(result);
    if (evalInvokeSpecialCase(x, argsKey))
        return;
    InvokeStackContext invokeStackContext(x, argsKey);
    InvokeEntryPtr entry =
        codegenCallable(x, argsKey, argsTempness, argLocations);
    if (entry->inlined)
        evalInvokeInlined(entry, args, env, out);
    else
        evalInvokeCode(entry, args, env, out);
}



//
// evalInvokeSpecialCase
//

bool evalInvokeSpecialCase(ObjectPtr x,
                           const vector<TypePtr> &argsKey)
{
    switch (x->objKind) {
    case TYPE : {
        Type *y = (Type *)x.ptr();
        if (isPrimitiveType(y) && argsKey.empty())
            return true;
        break;
    }
    case PROCEDURE : {
        if ((x == kernelName("destroy")) &&
            (argsKey.size() == 1) &&
            isPrimitiveType(argsKey[0]))
        {
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

void evalInvokeCode(InvokeEntryPtr entry,
                    const vector<ExprPtr> &args,
                    EnvPtr env,
                    MultiEValuePtr out)
{
    vector<llvm::GenericValue> gvArgs;
    for (unsigned i = 0; i < args.size(); ++i) {
        EValuePtr earg = evalOneAsRef(args[i], env);
        assert(earg->type == entry->argsKey[i]);
        gvArgs.push_back(llvm::GenericValue(earg->addr));
    }
    assert(out->size() == entry->returnTypes.size());
    for (unsigned i = 0; i < entry->returnTypes.size(); ++i) {
        TypePtr t = entry->returnTypes[i];
        if (entry->returnIsRef[i]) {
            assert(!out->values[i]);
            EValuePtr eret = new EValue(t, NULL);
            gvArgs.push_back(llvm::GenericValue(&eret->addr));
            out->values[i] = eret;
        }
        else {
            EValuePtr eret = out->values[i];
            assert(eret.ptr());
            gvArgs.push_back(llvm::GenericValue(eret->addr));
        }
    }
    llvmEngine->runFunction(entry->llvmFunc, gvArgs);
}



//
// evalInvokeInlined
//

void evalInvokeInlined(InvokeEntryPtr entry,
                       const vector<ExprPtr> &args,
                       EnvPtr env,
                       MultiEValuePtr out)
{
    assert(entry->inlined);

    CodePtr code = entry->code;

    if (entry->hasVarArgs)
        assert(args.size() >= entry->fixedArgNames.size());
    else
        assert(args.size() == entry->fixedArgNames.size());

    EnvPtr bodyEnv = new Env(entry->env);

    for (unsigned i= 0; i < entry->fixedArgNames.size(); ++i) {
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

    ObjectPtr analysis = analyzeInvokeInlined(entry, args, env);
    assert(analysis.ptr());
    MultiPValuePtr mpv = analysisToMultiPValue(analysis);
    assert(mpv->size() == out->size());

    const vector<ReturnSpecPtr> &returnSpecs = entry->code->returnSpecs;
    if (!returnSpecs.empty()) {
        assert(returnSpecs.size() == mpv->size());
    }

    vector<EReturn> returns;
    for (unsigned i = 0; i < mpv->size(); ++i) {
        PValuePtr pv = mpv->values[i];
        if (pv->isTemp) {
            EValuePtr ev = out->values[i];
            assert(ev.ptr());
            assert(ev->type == pv->type);
            returns.push_back(EReturn(false, pv->type, ev));
            if (!returnSpecs.empty()) {
                ReturnSpecPtr rspec = returnSpecs[i];
                if (rspec.ptr() && rspec->name.ptr()) {
                    addLocal(bodyEnv, rspec->name, ev.ptr());
                }
            }
        }
        else {
            assert(!out->values[i]);
            EValuePtr ev = new EValue(pv->type, NULL);
            out->values[i] = ev;
            EValuePtr evPtr = new EValue(pointerType(pv->type),
                                         (char *)(&(ev->addr)));
            returns.push_back(EReturn(true, pv->type, evPtr));
        }
    }
    EvalContextPtr ctx = new EvalContext(returns);

    TerminationPtr term = evalStatement(entry->code->body, bodyEnv, ctx);
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
}



//
// evalStatement
//

TerminationPtr evalStatement(StatementPtr stmt,
                             EnvPtr env,
                             EvalContextPtr ctx)
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
                termination = evalStatement(y, env, ctx);
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
        EValuePtr evLeft = evalOneAsRef(x->left, env);
        EValuePtr evRight = evalOneAsRef(x->right, env);
        evalValueAssign(evLeft, evRight);
        evalDestroyAndPopStack(marker);
        return NULL;
    }

    case INIT_ASSIGNMENT : {
        InitAssignment *x = (InitAssignment *)stmt.ptr();
        PValuePtr pvLeft = analyzeValue(x->left, env);
        PValuePtr pvRight = analyzeValue(x->right, env);
        if (pvLeft->type != pvRight->type)
            error("type mismatch");
        if (pvLeft->isTemp)
            error(x->left, "cannot assign to a temporary");
        int marker = evalMarkStack();
        EValuePtr evLeft = evalOneAsRef(x->left, env);
        evalIntoOne(x->right, env, evLeft);
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
        return evalStatement(new ExprStatement(call.ptr()), env, ctx);
    }

    case GOTO : {
        Goto *x = (Goto *)stmt.ptr();
        return new TerminateGoto(x->labelName, x->location);
    }

    case RETURN : {
        Return *x = (Return *)stmt.ptr();
        if (x->exprs.size() != ctx->returns.size())
            arityError(ctx->returns.size(), x->exprs.size());
        int marker = evalMarkStack();
        for (unsigned i = 0; i < x->exprs.size(); ++i) {
            EReturn &y = ctx->returns[i];
            if (y.byRef) {
                if (!x->isRef[i])
                    error(x->exprs[i], "return by reference expected");
                EValuePtr eret = evalOneAsRef(x->exprs[i], env);
                if (eret->type != y.type)
                    error(x->exprs[i], "type mismatch");
                *((char **)y.value->addr) = eret->addr;
            }
            else {
                if (x->isRef[i])
                    error(x->exprs[i], "return by value expected");
                PValuePtr pret = analyzeValue(x->exprs[i], env);
                if (pret->type != y.type)
                    error(x->exprs[i], "type mismatch");
                evalIntoOne(x->exprs[i], env, y.value);
            }
        }
        evalDestroyAndPopStack(marker);
        return new TerminateReturn(x->location);
    }

    case IF : {
        If *x = (If *)stmt.ptr();
        int marker = evalMarkStack();
        EValuePtr ev = evalOneAsRef(x->condition, env);
        bool flag = evalToBoolFlag(ev);
        evalDestroyAndPopStack(marker);
        if (flag)
            return evalStatement(x->thenPart, env, ctx);
        if (x->elsePart.ptr())
            return evalStatement(x->elsePart, env, ctx);
        return NULL;
    }

    case EXPR_STATEMENT : {
        ExprStatement *x = (ExprStatement *)stmt.ptr();
        int marker = evalMarkStack();
        MultiPValuePtr mpv = analyzeMultiValue(x->expr, env);
        MultiEValuePtr mev = new MultiEValue();
        for (unsigned i = 0; i < mpv->size(); ++i) {
            PValuePtr pv = mpv->values[i];
            if (pv->isTemp)
                mev->values.push_back(evalAllocValue(pv->type));
            else
                mev->values.push_back(NULL);
        }
        evalExpr(x->expr, env, mev);
        evalDestroyAndPopStack(marker);
        return NULL;
    }

    case WHILE : {
        While *x = (While *)stmt.ptr();
        while (true) {
            int marker = evalMarkStack();
            EValuePtr ev = evalOneAsRef(x->condition, env);
            bool flag = evalToBoolFlag(ev);
            evalDestroyAndPopStack(marker);
            if (!flag) break;
            TerminationPtr term = evalStatement(x->body, env, ctx);
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
        return evalStatement(x->desugared, env, ctx);
    }

    case FOREIGN_STATEMENT : {
        ForeignStatement *x = (ForeignStatement *)stmt.ptr();
        return evalStatement(x->statement, x->getEnv(), ctx);
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
    LocationContext loc(x->location);

    switch (x->bindingKind) {

    case VAR : {
        MultiPValuePtr mpv = analyzeMultiValue(x->expr, env);
        if (mpv->size() != x->names.size())
            arityError(x->expr, x->names.size(), mpv->size());
        MultiEValuePtr mev = new MultiEValue();
        for (unsigned i = 0; i < x->names.size(); ++i) {
            EValuePtr ev = evalAllocValue(mpv->values[i]->type);
            mev->values.push_back(ev);
        }
        int marker = evalMarkStack();
        evalIntoValues(x->expr, env, mev);
        evalDestroyAndPopStack(marker);
        EnvPtr env2 = new Env(env);
        for (unsigned i = 0; i < x->names.size(); ++i)
            addLocal(env2, x->names[i], mev->values[i].ptr());
        return env2;
    }

    case REF : {
        MultiPValuePtr mpv = analyzeMultiValue(x->expr, env);
        if (mpv->size() != x->names.size())
            arityError(x->expr, x->names.size(), mpv->size());
        MultiEValuePtr mev = new MultiEValue();
        for (unsigned i = 0; i < x->names.size(); ++i) {
            PValuePtr pv = mpv->values[i];
            if (pv->isTemp) {
                EValuePtr ev = evalAllocValue(pv->type);
                mev->values.push_back(ev);
            }
            else {
                mev->values.push_back(NULL);
            }
        }
        int marker = evalMarkStack();
        evalExpr(x->expr, env, mev);
        evalDestroyAndPopStack(marker);
        EnvPtr env2 = new Env(env);
        for (unsigned i = 0; i < x->names.size(); ++i) {
            EValuePtr ev = mev->values[i];
            assert(ev.ptr());
            addLocal(env2, x->names[i], ev.ptr());
        }
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

    default : {
        assert(false);
        return NULL;
    }

    }
}



//
// _evalNumeric, _evalInteger, _evalPointer, _evalPointerLike
//

static EValuePtr _evalNumeric(ExprPtr expr, EnvPtr env, TypePtr &type)
{
    EValuePtr ev = evalOneAsRef(expr, env);
    if (type.ptr()) {
        if (ev->type != type)
            error(expr, "argument type mismatch");
    }
    else {
        switch (ev->type->typeKind) {
        case INTEGER_TYPE :
        case FLOAT_TYPE :
            break;
        default :
            error(expr, "expecting numeric type");
        }
        type = ev->type;
    }
    return ev;
}

static EValuePtr _evalInteger(ExprPtr expr, EnvPtr env, TypePtr &type)
{
    EValuePtr ev = evalOneAsRef(expr, env);
    if (type.ptr()) {
        if (ev->type != type)
            error(expr, "argument type mismatch");
    }
    else {
        if (ev->type->typeKind != INTEGER_TYPE)
            error(expr, "expecting integer type");
        type = ev->type;
    }
    return ev;
}

static EValuePtr _evalPointer(ExprPtr expr, EnvPtr env, PointerTypePtr &type)
{
    EValuePtr ev = evalOneAsRef(expr, env);
    if (type.ptr()) {
        if (ev->type != (Type *)type.ptr())
            error(expr, "argument type mismatch");
    }
    else {
        if (ev->type->typeKind != POINTER_TYPE)
            error(expr, "expecting pointer type");
        type = (PointerType *)ev->type.ptr();
    }
    return ev;
}

static EValuePtr _evalPointerLike(ExprPtr expr, EnvPtr env, TypePtr &type)
{
    EValuePtr ev = evalOneAsRef(expr, env);
    if (type.ptr()) {
        if (ev->type != type)
            error(expr, "argument type mismatch");
    }
    else {
        switch (ev->type->typeKind) {
        case POINTER_TYPE :
        case CODE_POINTER_TYPE :
        case CCODE_POINTER_TYPE :
            break;
        default :
            error(expr, "expecting a pointer or a code pointer");
        }
        type = ev->type;
    }
    return ev;
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

void evalInvokePrimOp(PrimOpPtr x,
                      const vector<ExprPtr> &args,
                      EnvPtr env,
                      MultiEValuePtr out)
{
    switch (x->primOpCode) {

    case PRIM_TypeP : {
        ensureArity(args, 1);
        ObjectPtr y = evaluateOneStatic(args[0], env);
        bool flag = y->objKind == TYPE;
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0.ptr());
        assert(out0->type == boolType);
        *((char *)out0->addr) = flag ? 1 : 0;
        break;
    }

    case PRIM_TypeSize : {
        ensureArity(args, 1);
        TypePtr t = evaluateType(args[0], env);
        ObjectPtr obj = sizeTToValueHolder(typeSize(t)).ptr();
        evalStaticObject(obj, out);
        break;
    }

    case PRIM_primitiveCopy : {
        ensureArity(args, 2);
        EValuePtr ev0 = evalOneAsRef(args[0], env);
        EValuePtr ev1 = evalOneAsRef(args[1], env);
        if (!isPrimitiveType(ev0->type))
            error(args[0], "expecting primitive type");
        if (ev0->type != ev1->type)
            error(args[1], "argument type mismatch");
        memcpy(ev0->addr, ev1->addr, typeSize(ev0->type));
        assert(out->size() == 0);
        break;
    }

    case PRIM_boolNot : {
        ensureArity(args, 1);
        EValuePtr ev = evalOneAsRef(args[0], env);
        if (ev->type != boolType)
            error(args[0], "expecting bool type");
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0.ptr());
        assert(out0->type == boolType);
        char *p = (char *)ev->addr;
        *((char *)out0->addr) = (*p == 0);
        break;
    }

    case PRIM_numericEqualsP : {
        ensureArity(args, 2);
        TypePtr t;
        EValuePtr ev0 = _evalNumeric(args[0], env, t);
        EValuePtr ev1 = _evalNumeric(args[1], env, t);
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0.ptr());
        assert(out0->type == boolType);
        binaryNumericOp<Op_numericEqualsP>(ev0, ev1, out0);
        break;
    }

    case PRIM_numericLesserP : {
        ensureArity(args, 2);
        TypePtr t;
        EValuePtr ev0 = _evalNumeric(args[0], env, t);
        EValuePtr ev1 = _evalNumeric(args[1], env, t);
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0.ptr());
        assert(out0->type == boolType);
        binaryNumericOp<Op_numericLesserP>(ev0, ev1, out0);
        break;
    }

    case PRIM_numericAdd : {
        ensureArity(args, 2);
        TypePtr t;
        EValuePtr ev0 = _evalNumeric(args[0], env, t);
        EValuePtr ev1 = _evalNumeric(args[1], env, t);
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0.ptr());
        assert(out0->type == t);
        binaryNumericOp<Op_numericAdd>(ev0, ev1, out0);
        break;
    }

    case PRIM_numericSubtract : {
        ensureArity(args, 2);
        TypePtr t;
        EValuePtr ev0 = _evalNumeric(args[0], env, t);
        EValuePtr ev1 = _evalNumeric(args[1], env, t);
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0.ptr());
        assert(out0->type == t);
        binaryNumericOp<Op_numericSubtract>(ev0, ev1, out0);
        break;
    }

    case PRIM_numericMultiply : {
        ensureArity(args, 2);
        TypePtr t;
        EValuePtr ev0 = _evalNumeric(args[0], env, t);
        EValuePtr ev1 = _evalNumeric(args[1], env, t);
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0.ptr());
        assert(out0->type == t);
        binaryNumericOp<Op_numericMultiply>(ev0, ev1, out0);
        break;
    }

    case PRIM_numericDivide : {
        ensureArity(args, 2);
        TypePtr t;
        EValuePtr ev0 = _evalNumeric(args[0], env, t);
        EValuePtr ev1 = _evalNumeric(args[1], env, t);
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0.ptr());
        assert(out0->type == t);
        binaryNumericOp<Op_numericDivide>(ev0, ev1, out0);
        break;
    }

    case PRIM_numericNegate : {
        ensureArity(args, 1);
        TypePtr t;
        EValuePtr ev = _evalNumeric(args[0], env, t);
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0.ptr());
        assert(out0->type == t);
        unaryNumericOp<Op_numericNegate>(ev, out0);
        break;
    }

    case PRIM_integerRemainder : {
        ensureArity(args, 2);
        TypePtr t;
        EValuePtr ev0 = _evalInteger(args[0], env, t);
        EValuePtr ev1 = _evalInteger(args[1], env, t);
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0.ptr());
        assert(out0->type == t);
        binaryIntegerOp<Op_integerRemainder>(ev0, ev1, out0);
        break;
    }

    case PRIM_integerShiftLeft : {
        ensureArity(args, 2);
        TypePtr t;
        EValuePtr ev0 = _evalInteger(args[0], env, t);
        EValuePtr ev1 = _evalInteger(args[1], env, t);
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0.ptr());
        assert(out0->type == t);
        binaryIntegerOp<Op_integerShiftLeft>(ev0, ev1, out0);
        break;
    }

    case PRIM_integerShiftRight : {
        ensureArity(args, 2);
        TypePtr t;
        EValuePtr ev0 = _evalInteger(args[0], env, t);
        EValuePtr ev1 = _evalInteger(args[1], env, t);
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0.ptr());
        assert(out0->type == t);
        binaryIntegerOp<Op_integerShiftRight>(ev0, ev1, out0);
        break;
    }

    case PRIM_integerBitwiseAnd : {
        ensureArity(args, 2);
        TypePtr t;
        EValuePtr ev0 = _evalInteger(args[0], env, t);
        EValuePtr ev1 = _evalInteger(args[1], env, t);
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0.ptr());
        assert(out0->type == t);
        binaryIntegerOp<Op_integerBitwiseAnd>(ev0, ev1, out0);
        break;
    }

    case PRIM_integerBitwiseOr : {
        ensureArity(args, 2);
        TypePtr t;
        EValuePtr ev0 = _evalInteger(args[0], env, t);
        EValuePtr ev1 = _evalInteger(args[1], env, t);
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0.ptr());
        assert(out0->type == t);
        binaryIntegerOp<Op_integerBitwiseOr>(ev0, ev1, out0);
        break;
    }

    case PRIM_integerBitwiseXor : {
        ensureArity(args, 2);
        TypePtr t;
        EValuePtr ev0 = _evalInteger(args[0], env, t);
        EValuePtr ev1 = _evalInteger(args[1], env, t);
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0.ptr());
        assert(out0->type == t);
        binaryIntegerOp<Op_integerBitwiseXor>(ev0, ev1, out0);
        break;
    }

    case PRIM_integerBitwiseNot : {
        ensureArity(args, 1);
        TypePtr t;
        EValuePtr ev0 = _evalInteger(args[0], env, t);
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0.ptr());
        assert(out0->type == t);
        unaryIntegerOp<Op_integerBitwiseNot>(ev0, out0);
        break;
    }

    case PRIM_numericConvert : {
        ensureArity(args, 2);
        TypePtr dest = evaluateType(args[0], env);
        TypePtr src;
        EValuePtr ev = _evalNumeric(args[1], env, src);
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0.ptr());
        assert(out0->type == dest);
        op_numericConvert(out0, ev);
        break;
    }

    case PRIM_Pointer :
        error("Pointer type constructor cannot be called");

    case PRIM_addressOf : {
        ensureArity(args, 1);
        PValuePtr pv = analyzeValue(args[0], env);
        if (pv->isTemp)
            error("cannot take address of a temporary");
        EValuePtr ev = evalOneAsRef(args[0], env);
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0.ptr());
        assert(out0->type == pointerType(ev->type));
        *((void **)out0->addr) = (void *)ev->addr;
        break;
    }

    case PRIM_pointerDereference : {
        ensureArity(args, 1);
        PointerTypePtr t;
        EValuePtr ev = _evalPointer(args[0], env, t);
        void *ptr = *((void **)ev->addr);
        assert(out->size() == 1);
        assert(!out->values[0]);
        out->values[0] = new EValue(t->pointeeType, (char *)ptr);
        break;
    }

    case PRIM_pointerEqualsP : {
        ensureArity(args, 2);
        PointerTypePtr t;
        EValuePtr v0 = _evalPointer(args[0], env, t);
        EValuePtr v1 = _evalPointer(args[1], env, t);
        bool flag = *((void **)v0->addr) == *((void **)v1->addr);
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0.ptr());
        assert(out0->type == boolType);
        *((char *)out0->addr) = flag ? 1 : 0;
        break;
    }

    case PRIM_pointerLesserP : {
        ensureArity(args, 2);
        PointerTypePtr t;
        EValuePtr v0 = _evalPointer(args[0], env, t);
        EValuePtr v1 = _evalPointer(args[1], env, t);
        bool flag = *((void **)v0->addr) < *((void **)v1->addr);
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0.ptr());
        assert(out0->type == boolType);
        *((char *)out0->addr) = flag ? 1 : 0;
        break;
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
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0.ptr());
        assert(out0->type == t.ptr());
        *((void **)out0->addr) = (void *)ptr;
        break;
    }

    case PRIM_pointerToInt : {
        ensureArity(args, 2);
        TypePtr dest = evaluateType(args[0], env);
        if (dest->typeKind != INTEGER_TYPE)
            error(args[0], "invalid integer type");
        PointerTypePtr pt;
        EValuePtr ev = _evalPointer(args[1], env, pt);
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0.ptr());
        assert(out0->type == dest);
        void *ptr = *((void **)ev->addr);
        op_pointerToInt(out0, ptr);
        break;
    }

    case PRIM_intToPointer : {
        ensureArity(args, 2);
        TypePtr pointeeType = evaluateType(args[0], env);
        TypePtr dest = pointerType(pointeeType);
        TypePtr t;
        EValuePtr ev = _evalInteger(args[1], env, t);
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0.ptr());
        assert(out0->type == dest);
        ptrdiff_t ptrInt = op_intToPtrInt(ev);
        *((void **)out0->addr) = (void *)ptrInt;
        break;
    }

    case PRIM_CodePointerP : {
        ensureArity(args, 1);
        ObjectPtr y = evaluateOneStatic(args[0], env);
        bool isCPType = false;
        if (y->objKind == TYPE) {
            Type *t = (Type *)y.ptr();
            if (t->typeKind == CODE_POINTER_TYPE)
                isCPType = true;
        }
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0.ptr());
        assert(out0->type == boolType);
        *((char *)out0->addr) = isCPType ? 1 : 0;
        break;
    }

    case PRIM_CodePointer :
        error("CodePointer type constructor cannot be called");

    case PRIM_RefCodePointer :
        error("RefCodePointer type constructor cannot be called");

    case PRIM_makeCodePointer : {
        if (args.size() < 1)
            error("incorrect number of arguments");
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
            codegenCallable(callable, argsKey, argsTempness, argLocations);
        if (entry->inlined)
            error(args[0], "cannot create pointer to inlined code");
        vector<TypePtr> argTypes = entry->fixedArgTypes;
        if (entry->hasVarArgs) {
            argTypes.insert(argTypes.end(),
                            entry->varArgTypes.begin(),
                            entry->varArgTypes.end());
        }
        TypePtr cpType = codePointerType(argTypes,
                                         entry->returnIsRef,
                                         entry->returnTypes);
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0.ptr());
        assert(out0->type == cpType);
        void *funcPtr = llvmEngine->getPointerToGlobal(entry->llvmFunc);
        *((void **)out0->addr) = funcPtr;
        break;
    }

    case PRIM_CCodePointerP : {
        ensureArity(args, 1);
        ObjectPtr y = evaluateOneStatic(args[0], env);
        bool isCCPType = false;
        if (y->objKind == TYPE) {
            Type *t = (Type *)y.ptr();
            if (t->typeKind == CCODE_POINTER_TYPE)
                isCCPType = true;
        }
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0.ptr());
        assert(out0->type == boolType);
        *((char *)out0->addr) = isCCPType ? 1 : 0;
        break;
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
            codegenCallable(callable, argsKey, argsTempness, argLocations);
        if (entry->inlined)
            error(args[0], "cannot create pointer to inlined code");
        vector<TypePtr> argTypes = entry->fixedArgTypes;
        if (entry->hasVarArgs) {
            argTypes.insert(argTypes.end(),
                            entry->varArgTypes.begin(),
                            entry->varArgTypes.end());
        }
        TypePtr returnType;
        if (entry->returnTypes.size() == 0) {
            returnType = NULL;
        }
        else if (entry->returnTypes.size() == 1) {
            if (entry->returnIsRef[0])
                error(args[0], "cannot create C compatible pointer "
                      "to return-by-reference code");
            returnType = entry->returnTypes[0];
        }
        else {
            error(args[0], "cannot create C compatible pointer "
                  "to multi-return code");
        }
        TypePtr ccpType = cCodePointerType(CC_DEFAULT,
                                           argTypes,
                                           false,
                                           returnType);
        string callableName = getCodeName(callable);
        if (!entry->llvmCWrapper)
            codegenCWrapper(entry, callableName);
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0.ptr());
        assert(out0->type == ccpType);
        void *funcPtr = llvmEngine->getPointerToGlobal(entry->llvmCWrapper);
        *((void **)out0->addr) = funcPtr;
        break;
    }

    case PRIM_pointerCast : {
        ensureArity(args, 2);
        TypePtr dest = evaluatePointerLikeType(args[0], env);
        TypePtr t;
        EValuePtr v = _evalPointerLike(args[1], env, t);
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0.ptr());
        assert(out0->type == dest);
        *((void **)out0->addr) = *((void **)v->addr);
        break;
    }

    case PRIM_Array :
        error("Array type constructor cannot be called");

    case PRIM_array : {
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0.ptr());
        assert(out0->type->typeKind == ARRAY_TYPE);
        ArrayType *t = (ArrayType *)out0->type.ptr();
        assert((int)args.size() == t->size);
        TypePtr etype = t->elementType;
        for (unsigned i = 0; i < args.size(); ++i) {
            PValuePtr parg = analyzeValue(args[i], env);
            if (parg->type != etype)
                error(args[i], "array element type mismatch");
            char *ptr = out0->addr + typeSize(etype)*i;
            evalIntoOne(args[i], env, new EValue(etype, ptr));
        }
        break;
    }

    case PRIM_arrayRef : {
        ensureArity(args, 2);
        EValuePtr earray = evalOneAsRef(args[0], env);
        if (earray->type->typeKind != ARRAY_TYPE)
            error(args[0], "expecting array type");
        ArrayType *at = (ArrayType *)earray->type.ptr();
        TypePtr indexType;
        EValuePtr iv = _evalInteger(args[1], env, indexType);
        ptrdiff_t i = op_intToPtrInt(iv);
        char *ptr = earray->addr + i*typeSize(at->elementType);
        assert(out->size() == 1);
        assert(!out->values[0]);
        out->values[0] = new EValue(at->elementType, ptr);
        break;
    }

    case PRIM_TupleP : {
        ensureArity(args, 1);
        ObjectPtr y = evaluateOneStatic(args[0], env);
        bool isTupleType = false;
        if (y->objKind == TYPE) {
            Type *t = (Type *)y.ptr();
            if (t->typeKind == TUPLE_TYPE)
                isTupleType = true;
        }
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0.ptr());
        assert(out0->type == boolType);
        *((char *)out0->addr) = isTupleType ? 1 : 0;
        break;
    }

    case PRIM_Tuple :
        error("Tuple type constructor cannot be called");

    case PRIM_TupleElementCount : {
        ensureArity(args, 1);
        TypePtr y = evaluateTupleType(args[0], env);
        TupleType *z = (TupleType *)y.ptr();
        ObjectPtr obj = sizeTToValueHolder(z->elementTypes.size()).ptr();
        evalStaticObject(obj, out);
        break;
    }

    case PRIM_TupleElementOffset : {
        ensureArity(args, 2);
        TypePtr y = evaluateTupleType(args[0], env);
        TupleType *z = (TupleType *)y.ptr();
        size_t i = evaluateSizeT(args[1], env);
        const llvm::StructLayout *layout = tupleTypeLayout(z);
        ObjectPtr obj =  sizeTToValueHolder(layout->getElementOffset(i)).ptr();
        evalStaticObject(obj, out);
        break;
    }

    case PRIM_tuple : {
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0.ptr());
        assert(out0->type->typeKind == TUPLE_TYPE);
        TupleType *tt = (TupleType *)out0->type.ptr();
        assert(args.size() == tt->elementTypes.size());
        const llvm::StructLayout *layout = tupleTypeLayout(tt);
        for (unsigned i = 0; i < args.size(); ++i) {
            PValuePtr parg = analyzeValue(args[i], env);
            if (parg->type != tt->elementTypes[i])
                error(args[i], "argument type mismatch");
            char *ptr = out0->addr + layout->getElementOffset(i);
            EValuePtr eargDest = new EValue(tt->elementTypes[i], ptr);
            evalIntoOne(args[i], env, eargDest);
        }
        break;
    }

    case PRIM_tupleRef : {
        ensureArity(args, 2);
        EValuePtr etuple = evalOneAsRef(args[0], env);
        if (etuple->type->typeKind != TUPLE_TYPE)
            error(args[0], "expecting a tuple");
        TupleType *tt = (TupleType *)etuple->type.ptr();
        const llvm::StructLayout *layout = tupleTypeLayout(tt);
        size_t i = evaluateSizeT(args[1], env);
        if (i >= tt->elementTypes.size())
            error(args[1], "tuple index out of range");
        char *ptr = etuple->addr + layout->getElementOffset(i);
        assert(out->size() == 1);
        assert(!out->values[0]);
        out->values[0] = new EValue(tt->elementTypes[i], ptr);
        break;
    }

    case PRIM_RecordP : {
        ensureArity(args, 1);
        ObjectPtr y = evaluateOneStatic(args[0], env);
        bool isRecordType = false;
        if (y->objKind == TYPE) {
            Type *t = (Type *)y.ptr();
            if (t->typeKind == RECORD_TYPE)
                isRecordType = true;
        }
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0.ptr());
        assert(out0->type == boolType);
        *((char *)out0->addr) = isRecordType ? 1 : 0;
        break;
    }

    case PRIM_RecordFieldCount : {
        ensureArity(args, 1);
        TypePtr y = evaluateRecordType(args[0], env);
        RecordType *z = (RecordType *)y.ptr();
        const vector<TypePtr> &fieldTypes = recordFieldTypes(z);
        ObjectPtr obj = sizeTToValueHolder(fieldTypes.size()).ptr();
        evalStaticObject(obj, out);
        break;
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
        evalStaticObject(obj, out);
        break;
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
        evalStaticObject(obj, out);
        break;
    }

    case PRIM_recordFieldRef : {
        ensureArity(args, 2);
        EValuePtr erec = evalOneAsRef(args[0], env);
        if (erec->type->typeKind != RECORD_TYPE)
            error(args[0], "expecting a record");
        RecordType *rt = (RecordType *)erec->type.ptr();
        const llvm::StructLayout *layout = recordTypeLayout(rt);
        const vector<TypePtr> &fieldTypes = recordFieldTypes(rt);
        size_t i = evaluateSizeT(args[1], env);
        char *ptr = erec->addr + layout->getElementOffset(i);
        assert(out->size() == 1);
        assert(!out->values[0]);
        out->values[0] = new EValue(fieldTypes[i], ptr);
        break;
    }

    case PRIM_recordFieldRefByName : {
        ensureArity(args, 2);
        EValuePtr erec = evalOneAsRef(args[0], env);
        if (erec->type->typeKind != RECORD_TYPE)
            error(args[0], "expecting a record");
        RecordType *rt = (RecordType *)erec->type.ptr();
        const llvm::StructLayout *layout = recordTypeLayout(rt);
        const vector<TypePtr> &fieldTypes = recordFieldTypes(rt);
        IdentifierPtr fname = evaluateIdentifier(args[1], env);
        const map<string, size_t> &fieldIndexMap = recordFieldIndexMap(rt);
        map<string, size_t>::const_iterator fi = fieldIndexMap.find(fname->str);
        if (fi == fieldIndexMap.end())
            error(args[1], "field not in record");
        size_t i = fi->second;
        char *ptr = erec->addr + layout->getElementOffset(i);
        assert(out->size() == 1);
        assert(!out->values[0]);
        out->values[0] = new EValue(fieldTypes[i], ptr);
        break;
    }

    case PRIM_Static :
        error("Static type constructor cannot be called");

    case PRIM_StaticName : {
        ensureArity(args, 1);
        ObjectPtr y = evaluateOneStatic(args[0], env);
        ostringstream sout;
        printName(sout, y);
        ExprPtr z = new StringLiteral(sout.str());
        evalExpr(z, env, out);
        break;
    }

    case PRIM_EnumP : {
        ensureArity(args, 1);
        ObjectPtr y = evaluateOneStatic(args[0], env);
        bool isEnumType = false;
        if (y->objKind == TYPE) {
            Type *t = (Type *)y.ptr();
            if (t->typeKind == ENUM_TYPE)
                isEnumType = true;
        }
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0.ptr());
        assert(out0->type == boolType);
        *((char *)out0->addr) = isEnumType ? 1 : 0;
        break;
    }

    case PRIM_enumToInt : {
        ensureArity(args, 1);
        EValuePtr ev = evalOneAsRef(args[0], env);
        if (ev->type->typeKind != ENUM_TYPE)
            error(args[0], "expecting enum value");
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0.ptr());
        assert(out0->type == cIntType);
        *((int *)out0->addr) = *((int *)ev->addr);
        break;
    }

    case PRIM_intToEnum : {
        ensureArity(args, 2);
        TypePtr t = evaluateEnumerationType(args[0], env);
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0.ptr());
        assert(out0->type == t);
        TypePtr t2 = cIntType;
        EValuePtr v = _evalInteger(args[1], env, t2);
        *((int *)out0->addr) = *((int *)v->addr);
        break;
    }

    default :
        assert(false);
    }
}
