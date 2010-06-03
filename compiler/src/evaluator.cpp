#include "clay.hpp"

EValuePtr evalOneAsRef(ExprPtr expr, EnvPtr env);
MultiEValuePtr evalMultiAsRef(const vector<ExprPtr> &exprs, EnvPtr env);
MultiEValuePtr evalExprAsRef(ExprPtr expr, EnvPtr env);
void evalOneInto(ExprPtr expr, EnvPtr env, EValuePtr out);
void evalMultiInto(const vector<ExprPtr> &exprs, EnvPtr env, MultiEValuePtr out);
void evalExprInto(ExprPtr expr, EnvPtr env, MultiEValuePtr out);

void evalMulti(const vector<ExprPtr> &exprs, EnvPtr env, MultiEValuePtr out);
void evalOne(ExprPtr expr, EnvPtr env, EValuePtr out);
void evalExpr(ExprPtr expr, EnvPtr env, MultiEValuePtr out);
void evalStaticObject(ObjectPtr x, MultiEValuePtr out);
void evalValueHolder(ValueHolderPtr x, MultiEValuePtr out);
void evalIndexingExpr(ExprPtr indexable,
                      const vector<ExprPtr> &args,
                      EnvPtr env,
                      MultiEValuePtr out);
void evalFieldRefExpr(ExprPtr base,
                      IdentifierPtr name,
                      EnvPtr env,
                      MultiEValuePtr out);
void evalCallExpr(ExprPtr callable,
                  const vector<ExprPtr> &args,
                  EnvPtr env,
                  MultiEValuePtr out);
void evalCallValue(EValuePtr callable,
                   MultiEValuePtr args,
                   MultiEValuePtr out);
void evalCallPointer(EValuePtr x,
                     MultiEValuePtr args,
                     MultiEValuePtr out);
void evalCallCode(InvokeEntryPtr entry,
                  MultiEValuePtr args,
                  MultiEValuePtr out);
void evalCallInlined(InvokeEntryPtr entry,
                     const vector<ExprPtr> &args,
                     EnvPtr env,
                     MultiEValuePtr out);

enum TerminationKind {
    TERMINATE_RETURN,
    TERMINATE_BREAK,
    TERMINATE_CONTINUE,
    TERMINATE_GOTO
};

struct Termination : public Object {
    TerminationKind terminationKind;
    LocationPtr location;
    Termination(TerminationKind terminationKind, LocationPtr location)
        : Object(DONT_CARE), terminationKind(terminationKind),
          location(location) {}
};
typedef Pointer<Termination> TerminationPtr;

struct TerminateReturn : Termination {
    TerminateReturn(LocationPtr location)
        : Termination(TERMINATE_RETURN, location) {}
};

struct TerminateBreak : Termination {
    TerminateBreak(LocationPtr location)
        : Termination(TERMINATE_BREAK, location) {}
};

struct TerminateContinue : Termination {
    TerminateContinue(LocationPtr location)
        : Termination(TERMINATE_CONTINUE, location) {}
};

struct TerminateGoto : Termination {
    IdentifierPtr targetLabel;
    TerminateGoto(IdentifierPtr targetLabel, LocationPtr location)
        : Termination(TERMINATE_GOTO, location) {}
};

struct LabelInfo {
    EnvPtr env;
    int stackMarker;
    int blockPosition;
    LabelInfo() {}
    LabelInfo(EnvPtr env, int stackMarker, int blockPosition)
        : env(env), stackMarker(stackMarker), blockPosition(blockPosition) {}
};

struct EReturn {
    bool byRef;
    TypePtr type;
    EValuePtr value;
    EReturn(bool byRef, TypePtr type, EValuePtr value)
        : byRef(byRef), type(type), value(value) {}
};

struct EvalContext : public Object {
    vector<EReturn> returns;
    EvalContext(const vector<EReturn> &returns)
        : Object(DONT_CARE), returns(returns) {}
};
typedef Pointer<EvalContext> EvalContextPtr;

TerminationPtr evalStatement(StatementPtr stmt,
                             EnvPtr env,
                             EvalContextPtr ctx);

void evalCollectLabels(const vector<StatementPtr> &statements,
                       unsigned startIndex,
                       EnvPtr env,
                       map<string, LabelInfo> &labels);
EnvPtr evalBinding(BindingPtr x, EnvPtr env);

void evalPrimOpExpr(PrimOpPtr x,
                    const vector<ExprPtr> &args,
                    EnvPtr env,
                    MultiEValuePtr out);
void evalPrimOp(PrimOpPtr x, MultiEValuePtr args, MultiEValuePtr out);



//
// ValueHolder constructor and destructor
//

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



//
// objectEquals
//

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

    case PVALUE : {
        if (b->objKind != PVALUE)
            return false;
        PValue *a1 = (PValue *)a.ptr();
        PValue *b1 = (PValue *)b.ptr();
        return (a1->type == b1->type) && (a1->isTemp == b1->isTemp);
    }

    default :
        return a == b;
    }
}



//
// objectHash
//

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

    case PVALUE : {
        PValue *b = (PValue *)a.ptr();
        int h = objectHash(b->type.ptr());
        h *= b->isTemp ? 11 : 23;
        return h;
    }

    default : {
        unsigned long long v = (unsigned long long)a.ptr();
        return (int)v;
    }

    }
}



//
// evaluator wrappers
//

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
    MultiPValuePtr mpv = analyzeExpr(expr, env);
    assert(mpv.ptr());
    vector<ValueHolderPtr> valueHolders;
    MultiEValuePtr mev = new MultiEValue();
    for (unsigned i = 0; i < mpv->size(); ++i) {
        TypePtr t = mpv->values[i]->type;
        ValueHolderPtr vh = new ValueHolder(t);
        valueHolders.push_back(vh);
        EValuePtr ev = new EValue(t, vh->buf);
        mev->add(ev);
    }
    int marker = evalMarkStack();
    evalExprInto(expr, env, mev);
    evalDestroyAndPopStack(marker);
    MultiStaticPtr ms = new MultiStatic();
    for (unsigned i = 0; i < valueHolders.size(); ++i) {
        Type *t = (Type *)valueHolders[i]->type.ptr();
        if (t->typeKind == STATIC_TYPE) {
            StaticType *st = (StaticType *)t;
            ms->add(st->obj);
        }
        else {
            ms->add(valueHolders[i].ptr());
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
// utility procs
//

static EValuePtr staticEValue(ObjectPtr obj)
{
    TypePtr t = staticType(obj);
    return new EValue(t, NULL);
}

static EValuePtr kernelEValue(const string &name)
{
    return staticEValue(kernelName(name));
}

static EValuePtr derefValue(EValuePtr evPtr)
{
    assert(evPtr->type->typeKind == POINTER_TYPE);
    PointerType *pt = (PointerType *)evPtr->type.ptr();
    char *addr = *((char **)evPtr->addr);
    return new EValue(pt->pointeeType, addr);
}



//
// evaluate value ops
//

void evalValueInit(EValuePtr dest)
{
    evalCallValue(staticEValue(dest->type.ptr()),
                  new MultiEValue(),
                  new MultiEValue(dest));
}

void evalValueDestroy(EValuePtr dest)
{
    evalCallValue(kernelEValue("destroy"),
                  new MultiEValue(dest),
                  new MultiEValue());
}

void evalValueCopy(EValuePtr dest, EValuePtr src)
{
    evalCallValue(staticEValue(dest->type.ptr()),
                  new MultiEValue(src),
                  new MultiEValue(dest));
}

void evalValueAssign(EValuePtr dest, EValuePtr src)
{
    MultiEValuePtr args = new MultiEValue(dest);
    args->add(src);
    evalCallValue(kernelEValue("assign"),
                  args,
                  new MultiEValue());
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
// evalOneAsRef, evalMultiAsRef, evalExprAsRef
//

EValuePtr evalOneAsRef(ExprPtr expr, EnvPtr env)
{
    MultiEValuePtr mev = evalExprAsRef(expr, env);
    ensureArity(mev, 1);
    return mev->values[0];
}

MultiEValuePtr evalMultiAsRef(const vector<ExprPtr> &exprs, EnvPtr env)
{
    MultiEValuePtr out = new MultiEValue();
    for (unsigned i = 0; i < exprs.size(); ++i) {
        MultiEValuePtr mev = evalExprAsRef(exprs[i], env);
        out->add(mev);
    }
    return out;
}

MultiEValuePtr evalExprAsRef(ExprPtr expr, EnvPtr env)
{
    MultiPValuePtr mpv = analyzeExpr(expr, env);
    assert(mpv.ptr());
    MultiEValuePtr mev = new MultiEValue();
    for (unsigned i = 0; i < mpv->size(); ++i) {
        PValuePtr pv = mpv->values[i];
        if (pv->isTemp) {
            EValuePtr ev = evalAllocValue(pv->type);
            mev->add(ev);
        }
        else {
            EValuePtr evPtr = evalAllocValue(pointerType(pv->type));
            mev->add(evPtr);
        }
    }
    evalExpr(expr, env, mev);
    MultiEValuePtr out = new MultiEValue();
    for (unsigned i = 0; i < mpv->size(); ++i) {
        if (mpv->values[i]->isTemp)
            out->add(mev->values[i]);
        else
            out->add(derefValue(mev->values[i]));
    }
    return out;
}



//
// evalOneInto, evalMultiInto, evalExprInto
//

void evalOneInto(ExprPtr expr, EnvPtr env, EValuePtr out)
{
    PValuePtr pv = analyzeOne(expr, env);
    assert(pv.ptr());
    if (pv->isTemp) {
        evalOne(expr, env, out);
    }
    else {
        EValuePtr evPtr = evalAllocValue(pointerType(pv->type));
        evalOne(expr, env, evPtr);
        evalValueCopy(out, derefValue(evPtr));
    }
}


void evalMultiInto(const vector<ExprPtr> &exprs, EnvPtr env, MultiEValuePtr out)
{
    unsigned j = 0;
    for (unsigned i = 0; i < exprs.size(); ++i) {
        ExprPtr x = exprs[i];
        MultiPValuePtr mpv = analyzeExpr(x, env);
        assert(mpv.ptr());
        if (x->exprKind == VAR_ARGS_REF) {
            assert(j + mpv->size() <= out->size());
            MultiEValuePtr out2 = new MultiEValue();
            for (unsigned k = 0; k < mpv->size(); ++k)
                out2->add(out->values[j + k]);
            evalExprInto(x, env, out2);
            j += mpv->size();
        }
        else {
            if (mpv->size() != 1)
                arityError(x, 1, mpv->size());
            assert(j < out->size());
            evalOneInto(x, env, out->values[j]);
            ++j;
        }
    }
    assert(j == out->size());
}

void evalExprInto(ExprPtr expr, EnvPtr env, MultiEValuePtr out)
{
    MultiPValuePtr mpv = analyzeExpr(expr, env);
    assert(mpv.ptr());
    assert(out->size() == mpv->size());
    MultiEValuePtr mev = new MultiEValue();
    for (unsigned i = 0; i < mpv->size(); ++i) {
        PValuePtr pv = mpv->values[i];
        if (pv->isTemp) {
            mev->add(out->values[i]);
        }
        else {
            EValuePtr evPtr = evalAllocValue(pointerType(pv->type));
            mev->add(evPtr);
        }
    }
    evalExpr(expr, env, mev);
    for (unsigned i = 0; i < mpv->size(); ++i) {
        if (!mpv->values[i]->isTemp) {
            evalValueCopy(out->values[i], derefValue(mev->values[i]));
        }
    }
}



//
// evalMulti
//

void evalMulti(const vector<ExprPtr> &exprs, EnvPtr env, MultiEValuePtr out)
{
    unsigned j = 0;
    for (unsigned i = 0; i < exprs.size(); ++i) {
        ExprPtr x = exprs[i];
        MultiPValuePtr mpv = analyzeExpr(x, env);
        assert(mpv.ptr());
        if (x->exprKind == VAR_ARGS_REF) {
            assert(j + mpv->size() <= out->size());
            MultiEValuePtr out2 = new MultiEValue();
            for (unsigned k = 0; k < mpv->size(); ++k)
                out2->add(out->values[j + k]);
            evalExpr(x, env, out2);
            j += mpv->size();
        }
        else {
            if (mpv->size() != 1)
                arityError(x, 1, mpv->size());
            assert(j < out->size());
            evalOne(x, env, out->values[j]);
            ++j;
        }
    }
    assert(j == out->size());
}



//
// evalOne
//

void evalOne(ExprPtr expr, EnvPtr env, EValuePtr out)
{
    evalExpr(expr, env, new MultiEValue(out));
}



//
// evalExpr
//

void evalExpr(ExprPtr expr, EnvPtr env, MultiEValuePtr out)
{
    LocationContext loc(expr->location);

    switch (expr->exprKind) {

    case BOOL_LITERAL : {
        BoolLiteral *x = (BoolLiteral *)expr.ptr();
        ValueHolderPtr y = boolToValueHolder(x->value);
        evalValueHolder(y, out);
        break;
    }

    case INT_LITERAL : {
        IntLiteral *x = (IntLiteral *)expr.ptr();
        ValueHolderPtr y = parseIntLiteral(x);
        evalValueHolder(y, out);
        break;
    }

    case FLOAT_LITERAL : {
        FloatLiteral *x = (FloatLiteral *)expr.ptr();
        ValueHolderPtr y = parseFloatLiteral(x);
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
        evalCallValue(kernelEValue("String"), new MultiEValue(ev), out);
        break;
    }

    case NAME_REF : {
        NameRef *x = (NameRef *)expr.ptr();
        ObjectPtr y = lookupEnv(env, x->name);
        if (y->objKind == EXPRESSION) {
            ExprPtr z = (Expr *)y.ptr();
            evalExpr(z, env, out);
        }
        else if (y->objKind == MULTI_EXPR) {
            MultiExprPtr z = (MultiExpr *)y.ptr();
            evalMulti(z->values, env, out);
        }
        else {
            evalStaticObject(y, out);
        }
        break;
    }

    case TUPLE : {
        Tuple *x = (Tuple *)expr.ptr();
        if ((x->args.size() == 1) &&
            (x->args[0]->exprKind != VAR_ARGS_REF))
        {
            evalExpr(x->args[0], env, out);
        }
        else {
            evalCallExpr(primNameRef("tuple"), x->args, env, out);
        }
        break;
    }

    case ARRAY : {
        Array *x = (Array *)expr.ptr();
        evalCallExpr(primNameRef("array"), x->args, env, out);
        break;
    }

    case INDEXING : {
        Indexing *x = (Indexing *)expr.ptr();
        evalIndexingExpr(x->expr, x->args, env, out);
        break;
    }

    case CALL : {
        Call *x = (Call *)expr.ptr();
        evalCallExpr(x->expr, x->args, env, out);
        break;
    }

    case FIELD_REF : {
        FieldRef *x = (FieldRef *)expr.ptr();
        evalFieldRefExpr(x->expr, x->name, env, out);
        break;
    }

    case TUPLE_REF : {
        TupleRef *x = (TupleRef *)expr.ptr();
        EValuePtr ev = evalOneAsRef(x->expr, env);
        ValueHolderPtr vh = sizeTToValueHolder(x->index);
        MultiEValuePtr args = new MultiEValue(ev);
        args->add(staticEValue(vh.ptr()));
        evalCallValue(kernelEValue("tupleRef"), args, out);
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
        PValuePtr pv = analyzeOne(expr, env);
        assert(pv.ptr());
        assert(out->size() == 1);
        if (pv->isTemp) {
            EValuePtr ev = out->values[0];
            evalOneInto(x->expr1, env, ev);
            if (evalToBoolFlag(ev)) {
                evalValueDestroy(ev);
                PValuePtr pv2 = analyzeOne(x->expr2, env);
                assert(pv2.ptr());
                if (pv2->type != pv->type)
                    error("type mismatch in 'and' expression");
                evalOneInto(x->expr2, env, ev);
            }
        }
        else {
            EValuePtr evPtr = out->values[0];
            evalOne(x->expr1, env, evPtr);
            if (evalToBoolFlag(derefValue(evPtr))) {
                PValuePtr pv2 = analyzeOne(x->expr2, env);
                assert(pv2.ptr());
                if (pv2->type != pv->type)
                    error("type mismatch in 'and' expression");
                evalOne(x->expr1, env, evPtr);
            }
        }
        break;
    }

    case OR : {
        Or *x = (Or *)expr.ptr();
        PValuePtr pv = analyzeOne(expr, env);
        assert(pv.ptr());
        assert(out->size() == 1);
        if (pv->isTemp) {
            EValuePtr ev = out->values[0];
            evalOneInto(x->expr1, env, ev);
            if (!evalToBoolFlag(ev)) {
                evalValueDestroy(ev);
                PValuePtr pv2 = analyzeOne(x->expr2, env);
                assert(pv2.ptr());
                if (pv2->type != pv->type)
                    error("type mismatch in 'or' expression");
                evalOneInto(x->expr2, env, ev);
            }
        }
        else {
            EValuePtr evPtr = out->values[0];
            evalOne(x->expr2, env, evPtr);
            if (evalToBoolFlag(derefValue(evPtr))) {
                PValuePtr pv2 = analyzeOne(x->expr2, env);
                assert(pv2.ptr());
                if (pv2->type != pv->type)
                    error("type mismatch in 'or' expression");
                evalOne(x->expr2, env, evPtr);
            }
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

    case VAR_ARGS_REF : {
        IdentifierPtr ident = new Identifier("%varArgs");
        ident->location = expr->location;
        ExprPtr nameRef = new NameRef(ident);
        nameRef->location = expr->location;
        evalExpr(nameRef, env, out);
        break;
    }

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
        EValuePtr out0 = out->values[0];
        assert(out0->type == pointerType(y->type));
        *((void **)out0->addr) = ptr;
        break;
    }

    case EXTERNAL_VARIABLE : {
        ExternalVariable *y = (ExternalVariable *)x.ptr();
        if (!y->llGlobal)
            codegenExternalVariable(y);
        void *ptr = llvmEngine->getPointerToGlobal(y->llGlobal);
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type == pointerType(y->type2));
        *((void **)out0->addr) = ptr;
        break;
    }

    case EXTERNAL_PROCEDURE : {
        ExternalProcedure *y = (ExternalProcedure *)x.ptr();
        if (!y->llvmFunc)
            codegenExternalProcedure(y);
        void *funcPtr = llvmEngine->getPointerToGlobal(y->llvmFunc);
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
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

    case MULTI_STATIC : {
        MultiStatic *y = (MultiStatic *)x.ptr();
        assert(y->size() == out->size());
        for (unsigned i = 0; i < y->size(); ++i)
            evalStaticObject(y->values[i], new MultiEValue(out->values[i]));
        break;
    }

    case TYPE :
    case PRIM_OP :
    case PROCEDURE :
    case RECORD :
    case MODULE_HOLDER :
    case IDENTIFIER : {
        assert(out->size() == 1);
        assert(out->values[0]->type == staticType(x));
        break;
    }

    case EVALUE : {
        EValue *y = (EValue *)x.ptr();
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type == pointerType(y->type));
        *((void **)out0->addr) = (void *)y->addr;
        break;
    }

    case MULTI_EVALUE : {
        MultiEValue *y = (MultiEValue *)x.ptr();
        assert(out->size() == y->size());
        for (unsigned i = 0; i < y->size(); ++i) {
            EValuePtr ev = y->values[i];
            EValuePtr outi = out->values[i];
            assert(outi->type == pointerType(ev->type));
            *((void **)outi->addr) = (void *)ev->addr;
        }
        break;
    }

    case PATTERN :
        error("pattern variable cannot be used as value");
        break;

    default :
        error("invalid static object");
        break;
    }
}



//
// evalValueHolder
//

void evalValueHolder(ValueHolderPtr x, MultiEValuePtr out)
{
    assert(out->size() == 1);
    EValuePtr out0 = out->values[0];
    assert(out0->type == x->type);

    switch (x->type->typeKind) {

    case BOOL_TYPE :
    case INTEGER_TYPE :
    case FLOAT_TYPE :
        memcpy(out0->addr, x->buf, typeSize(x->type));
        break;

    case STATIC_TYPE :
        break;

    default :
        evalValueCopy(out0, new EValue(x->type, x->buf));
        break;

    }
}



//
// evalIndexingExpr
//

void evalIndexingExpr(ExprPtr indexable,
                      const vector<ExprPtr> &args,
                      EnvPtr env,
                      MultiEValuePtr out)
{
    MultiPValuePtr mpv = analyzeIndexingExpr(indexable, args, env);
    assert(mpv.ptr());
    assert(mpv->size() == out->size());
    bool allTempStatics = true;
    for (unsigned i = 0; i < mpv->size(); ++i) {
        PValuePtr pv = mpv->values[i];
        if (pv->isTemp)
            assert(out->values[i]->type == pv->type);
        else
            assert(out->values[i]->type == pointerType(pv->type));
        if ((pv->type->typeKind != STATIC_TYPE) || !pv->isTemp) {
            allTempStatics = false;
        }
    }
    if (allTempStatics) {
        // takes care of type constructors
        return;
    }
    vector<ExprPtr> args2;
    args2.push_back(indexable);
    args2.insert(args2.end(), args.begin(), args.end());
    evalCallExpr(kernelNameRef("index"), args2, env, out);
}



//
// evalFieldRefExpr
//

void evalFieldRefExpr(ExprPtr base,
                      IdentifierPtr name,
                      EnvPtr env,
                      MultiEValuePtr out)
{
    MultiPValuePtr mpv = analyzeFieldRefExpr(base, name, env);
    assert(mpv.ptr());
    assert(mpv->size() == out->size());
    bool allTempStatics = true;
    for (unsigned i = 0; i < mpv->size(); ++i) {
        PValuePtr pv = mpv->values[i];
        if (pv->isTemp)
            assert(out->values[i]->type == pv->type);
        else
            assert(out->values[i]->type == pointerType(pv->type));
        if ((pv->type->typeKind != STATIC_TYPE) || !pv->isTemp) {
            allTempStatics = false;
        }
    }
    if (allTempStatics) {
        // takes care of module member access
        return;
    }
    vector<ExprPtr> args2;
    args2.push_back(base);
    args2.push_back(new ObjectExpr(name.ptr()));
    evalCallExpr(kernelNameRef("fieldRef"), args2, env, out);
}



//
// evalCallExpr
//

void evalCallExpr(ExprPtr callable,
                  const vector<ExprPtr> &args,
                  EnvPtr env,
                  MultiEValuePtr out)
{
    PValuePtr pv = analyzeOne(callable, env);
    assert(pv.ptr());

    switch (pv->type->typeKind) {
    case CODE_POINTER_TYPE :
    case CCODE_POINTER_TYPE : {
        EValuePtr ev = evalOneAsRef(callable, env);
        MultiEValuePtr mev = evalMultiAsRef(args, env);
        evalCallPointer(ev, mev, out);
        return;
    }
    }

    if (pv->type->typeKind != STATIC_TYPE) {
        vector<ExprPtr> args2;
        args2.push_back(callable);
        args2.insert(args2.end(), args.begin(), args.end());
        evalCallExpr(kernelNameRef("call"), args2, env, out);
        return;
    }

    StaticType *st = (StaticType *)pv->type.ptr();
    ObjectPtr obj = st->obj;

    switch (obj->objKind) {

    case TYPE :
    case RECORD :
    case PROCEDURE : {
        MultiPValuePtr mpv = analyzeMulti(args, env);
        assert(mpv.ptr());
        vector<TypePtr> argsKey;
        vector<ValueTempness> argsTempness;
        computeArgsKey(mpv, argsKey, argsTempness);
        InvokeStackContext invokeStackContext(obj, argsKey);
        InvokeEntryPtr entry = analyzeCallable(obj, argsKey, argsTempness);
        if (entry->inlined) {
            evalCallInlined(entry, args, env, out);
        }
        else {
            assert(entry->analyzed);
            MultiEValuePtr mev = evalMultiAsRef(args, env);
            evalCallCode(entry, mev, out);
        }
        break;
    }

    case PRIM_OP : {
        PrimOpPtr x = (PrimOp *)obj.ptr();
        evalPrimOpExpr(x, args, env, out);
        break;
    }

    default :
        error("invalid call expression");
        break;

    }
}



//
// evalCallValue
//

void evalCallValue(EValuePtr callable,
                   MultiEValuePtr args,
                   MultiEValuePtr out)
{
    switch (callable->type->typeKind) {
    case CODE_POINTER_TYPE :
    case CCODE_POINTER_TYPE :
        evalCallPointer(callable, args, out);
        return;
    }

    if (callable->type->typeKind != STATIC_TYPE) {
        MultiEValuePtr args2 = new MultiEValue(callable);
        args2->add(args);
        evalCallValue(kernelEValue("call"), args2, out);
        return;
    }

    StaticType *st = (StaticType *)callable->type.ptr();
    ObjectPtr obj = st->obj;

    switch (obj->objKind) {

    case TYPE :
    case RECORD :
    case PROCEDURE : {
        MultiPValuePtr mpv = new MultiPValue();
        for (unsigned i = 0; i < args->size(); ++i)
            mpv->add(new PValue(args->values[i]->type, false));
        vector<TypePtr> argsKey;
        vector<ValueTempness> argsTempness;
        computeArgsKey(mpv, argsKey, argsTempness);
        InvokeStackContext invokeStackContext(obj, argsKey);
        InvokeEntryPtr entry = analyzeCallable(obj, argsKey, argsTempness);
        if (entry->inlined)
            error("call to inlined code is not allowed in this context");
        assert(entry->analyzed);
        evalCallCode(entry, args, out);
        break;
    }

    case PRIM_OP : {
        PrimOpPtr x = (PrimOp *)obj.ptr();
        evalPrimOp(x, args, out);
        break;
    }

    default :
        error("invalid call operation");
        break;

    }
}



//
// evalCallPointer
//

void evalCallPointer(EValuePtr x,
                     MultiEValuePtr args,
                     MultiEValuePtr out)
{
    error("invoking a code pointer not yet supported in evaluator");
}



//
// evalCallCode
//

void evalCallCode(InvokeEntryPtr entry,
                  MultiEValuePtr args,
                  MultiEValuePtr out)
{
    if (!entry->llvmFunc)
        codegenCodeBody(entry, getCodeName(entry->callable));
    assert(entry->llvmFunc);

    vector<llvm::GenericValue> gvArgs;

    for (unsigned i = 0; i < args->size(); ++i) {
        EValuePtr earg = args->values[i];
        assert(earg->type == entry->argsKey[i]);
        gvArgs.push_back(llvm::GenericValue(earg->addr));
    }

    assert(out->size() == entry->returnTypes.size());
    for (unsigned i = 0; i < entry->returnTypes.size(); ++i) {
        TypePtr t = entry->returnTypes[i];
        if (entry->returnIsRef[i]) {
            assert(t == pointerType(out->values[i]->type));
            gvArgs.push_back(llvm::GenericValue(out->values[i].ptr()));
        }
        else {
            assert(t == out->values[i]->type);
            gvArgs.push_back(llvm::GenericValue(out->values[i].ptr()));
        }
    }
    llvmEngine->runFunction(entry->llvmFunc, gvArgs);
}



//
// evalCallInlined
//

void evalCallInlined(InvokeEntryPtr entry,
                     const vector<ExprPtr> &args,
                     EnvPtr env,
                     MultiEValuePtr out)
{
    assert(entry->inlined);
    if (entry->hasVarArgs)
        assert(args.size() >= entry->fixedArgNames.size());
    else
        assert(args.size() == entry->fixedArgNames.size());

    EnvPtr bodyEnv = new Env(entry->env);

    for (unsigned i = 0; i < entry->fixedArgNames.size(); ++i) {
        ExprPtr expr = new ForeignExpr(env, args[i]);
        addLocal(bodyEnv, entry->fixedArgNames[i], expr.ptr());
    }

    if (entry->hasVarArgs) {
        MultiExprPtr varArgs = new MultiExpr();
        for (unsigned i = entry->fixedArgNames.size(); i < args.size(); ++i) {
            ExprPtr expr = new ForeignExpr(env, args[i]);
            varArgs->add(expr);
        }
        addLocal(bodyEnv, new Identifier("%varArgs"), varArgs.ptr());
    }

    MultiPValuePtr mpv = analyzeCallInlined(entry, args, env);
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
            assert(ev->type == pv->type);
            returns.push_back(EReturn(false, pv->type, ev));
            if (!returnSpecs.empty() && returnSpecs[i]->name.ptr()) {
                addLocal(bodyEnv, returnSpecs[i]->name, ev.ptr());
            }
        }
        else {
            EValuePtr evPtr = out->values[i];
            assert(evPtr->type == pointerType(pv->type));
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
                            termination = NULL;
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
        return NULL;

    case ASSIGNMENT : {
        Assignment *x = (Assignment *)stmt.ptr();
        PValuePtr pvLeft = analyzeOne(x->left, env);
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
        PValuePtr pvLeft = analyzeOne(x->left, env);
        PValuePtr pvRight = analyzeOne(x->right, env);
        if (pvLeft->type != pvRight->type)
            error("type mismatch");
        if (pvLeft->isTemp)
            error(x->left, "cannot assign to a temporary");
        int marker = evalMarkStack();
        EValuePtr evLeft = evalOneAsRef(x->left, env);
        evalOneInto(x->right, env, evLeft);
        evalDestroyAndPopStack(marker);
        return NULL;
    }

    case UPDATE_ASSIGNMENT : {
        UpdateAssignment *x = (UpdateAssignment *)stmt.ptr();
        PValuePtr pvLeft = analyzeOne(x->left, env);
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
            PValuePtr pret = analyzeOne(x->exprs[i], env);
            if (pret->type != y.type)
                error(x->exprs[i], "type mismatch");
            if (y.byRef) {
                if (!x->isRef[i])
                    error(x->exprs[i], "return by reference expected");
                if (pret->isTemp)
                    error(x->exprs[i], "cannot return a "
                          "temporary by reference");
                evalOne(x->exprs[i], env, y.value);
            }
            else {
                if (x->isRef[i])
                    error(x->exprs[i], "return by value expected");
                evalOneInto(x->exprs[i], env, y.value);
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
        evalExprAsRef(x->expr, env);
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

    case BREAK : {
        return new TerminateBreak(stmt->location);
    }

    case CONTINUE : {
        return new TerminateContinue(stmt->location);
    }

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

    case TRY : {
        error("try statement not yet supported in the evaluator");
        return NULL;
    }

    default :
        assert(false);
        return NULL;

    }
}



//
// evalCollectLabels
//

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



//
// evalBinding
//

EnvPtr evalBinding(BindingPtr x, EnvPtr env)
{
    LocationContext loc(x->location);

    switch (x->bindingKind) {

    case VAR : {
        MultiPValuePtr mpv = analyzeExpr(x->expr, env);
        if (mpv->size() != x->names.size())
            arityError(x->expr, x->names.size(), mpv->size());
        MultiEValuePtr mev = new MultiEValue();
        for (unsigned i = 0; i < x->names.size(); ++i) {
            EValuePtr ev = evalAllocValue(mpv->values[i]->type);
            mev->add(ev);
        }
        int marker = evalMarkStack();
        evalExprInto(x->expr, env, mev);
        evalDestroyAndPopStack(marker);
        EnvPtr env2 = new Env(env);
        for (unsigned i = 0; i < x->names.size(); ++i)
            addLocal(env2, x->names[i], mev->values[i].ptr());
        return env2;
    }

    case REF : {
        MultiPValuePtr mpv = analyzeExpr(x->expr, env);
        if (mpv->size() != x->names.size())
            arityError(x->expr, x->names.size(), mpv->size());
        MultiEValuePtr mev = new MultiEValue();
        for (unsigned i = 0; i < x->names.size(); ++i) {
            PValuePtr pv = mpv->values[i];
            if (pv->isTemp) {
                EValuePtr ev = evalAllocValue(pv->type);
                mev->add(ev);
            }
            else {
                EValuePtr evPtr = evalAllocValue(pointerType(pv->type));
                mev->add(evPtr);
            }
        }
        int marker = evalMarkStack();
        evalExpr(x->expr, env, mev);
        evalDestroyAndPopStack(marker);
        EnvPtr env2 = new Env(env);
        for (unsigned i = 0; i < x->names.size(); ++i) {
            if (mpv->values[i]->isTemp) {
                EValuePtr ev = mev->values[i];
                addLocal(env2, x->names[i], ev.ptr());
            }
            else {
                EValuePtr evPtr = mev->values[i];
                addLocal(env2, x->names[i], derefValue(evPtr).ptr());
            }
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
// evalPrimOpExpr
//

void evalPrimOpExpr(PrimOpPtr x,
                    const vector<ExprPtr> &args,
                    EnvPtr env,
                    MultiEValuePtr out)
{
    switch (x->primOpCode) {

    case PRIM_array :
    case PRIM_tuple : {
        MultiPValuePtr mpv = analyzePrimOpExpr(x, args, env);
        assert(mpv->size() == 1);
        TypePtr t = mpv->values[0]->type;
        ExprPtr callable = new ObjectExpr(t.ptr());
        evalCallExpr(callable, args, env, out);
        break;
    }

    default :
        MultiEValuePtr mev = evalMultiAsRef(args, env);
        evalPrimOp(x, mev, out);
        break;

    }
}



//
// valueToStatic, valueToStaticSizeT
// valueToType, valueToNumericType, valueToIntegerType,
// valueToPointerLikeType, valueToTupleType, valueToRecordType,
// valueToEnumType, valueToIdentifier
//

static ObjectPtr valueToStatic(EValuePtr ev)
{
    if (ev->type->typeKind != STATIC_TYPE)
        return NULL;
    StaticType *st = (StaticType *)ev->type.ptr();
    return st->obj;
}

static ObjectPtr valueToStatic(MultiEValuePtr args, unsigned index)
{
    ObjectPtr obj = valueToStatic(args->values[index]);
    if (!obj)
        argumentError(index, "expecting a static value");
    return obj;
}

static size_t valueToStaticSizeT(MultiEValuePtr args, unsigned index)
{
    ObjectPtr obj = valueToStatic(args->values[index]);
    if (!obj || (obj->objKind != VALUE_HOLDER))
        argumentError(index, "expecting a static SizeT value");
    ValueHolder *vh = (ValueHolder *)obj.ptr();
    if (vh->type != cSizeTType)
        argumentError(index, "expecting a static SizeT value");
    return *((size_t *)vh->buf);
}

static TypePtr valueToType(MultiEValuePtr args, unsigned index)
{
    ObjectPtr obj = valueToStatic(args->values[index]);
    if (!obj || (obj->objKind != TYPE))
        argumentError(index, "expecting a type");
    return (Type *)obj.ptr();
}

static TypePtr valueToNumericType(MultiEValuePtr args, unsigned index)
{
    TypePtr t = valueToType(args, index);
    switch (t->typeKind) {
    case INTEGER_TYPE :
    case FLOAT_TYPE :
        return t;
    default :
        argumentError(index, "expecting a numeric type");
        return NULL;
    }
}

static IntegerTypePtr valueToIntegerType(MultiEValuePtr args, unsigned index)
{
    TypePtr t = valueToType(args, index);
    if (t->typeKind != INTEGER_TYPE)
        argumentError(index, "expecting an integer type");
    return (IntegerType *)t.ptr();
}

static TypePtr valueToPointerLikeType(MultiEValuePtr args, unsigned index)
{
    TypePtr t = valueToType(args, index);
    switch (t->typeKind) {
    case POINTER_TYPE :
    case CODE_POINTER_TYPE :
    case CCODE_POINTER_TYPE :
        break;
    default :
        argumentError(index, "expecting a pointer or code-pointer type");
    }
    return t;
}

static TupleTypePtr valueToTupleType(MultiEValuePtr args, unsigned index)
{
    TypePtr t = valueToType(args, index);
    if (t->typeKind != TUPLE_TYPE)
        argumentError(index, "expecting a tuple type");
    return (TupleType *)t.ptr();
}

static RecordTypePtr valueToRecordType(MultiEValuePtr args, unsigned index)
{
    TypePtr t = valueToType(args, index);
    if (t->typeKind != RECORD_TYPE)
        argumentError(index, "expecting a tuple type");
    return (RecordType *)t.ptr();
}

static EnumTypePtr valueToEnumType(MultiEValuePtr args, unsigned index)
{
    TypePtr t = valueToType(args, index);
    if (t->typeKind != ENUM_TYPE)
        argumentError(index, "expecting a tuple type");
    return (EnumType *)t.ptr();
}

static IdentifierPtr valueToIdentifier(MultiEValuePtr args, unsigned index)
{
    ObjectPtr obj = valueToStatic(args->values[index]);
    if (!obj || (obj->objKind != IDENTIFIER))
        argumentError(index, "expecting identifier value");
    return (Identifier *)obj.ptr();
}



//
// numericValue, integerValue, pointerValue, pointerLikeValue,
// arrayValue, tupleValue, recordValue, enumValue
//

static EValuePtr numericValue(MultiEValuePtr args, unsigned index,
                              TypePtr &type)
{
    EValuePtr ev = args->values[index];
    if (type.ptr()) {
        if (ev->type != type)
            argumentError(index, "argument type mismatch");
    }
    else {
        switch (ev->type->typeKind) {
        case INTEGER_TYPE :
        case FLOAT_TYPE :
            break;
        default :
            argumentError(index, "expecting value of numeric type");
        }
        type = ev->type;
    }
    return ev;
}

static EValuePtr integerValue(MultiEValuePtr args, unsigned index,
                              IntegerTypePtr &type)
{
    EValuePtr ev = args->values[index];
    if (type.ptr()) {
        if (ev->type != type.ptr())
            argumentError(index, "argument type mismatch");
    }
    else {
        if (ev->type->typeKind != INTEGER_TYPE)
            argumentError(index, "expecting value of integer type");
        type = (IntegerType *)ev->type.ptr();
    }
    return ev;
}

static EValuePtr pointerValue(MultiEValuePtr args, unsigned index,
                              PointerTypePtr &type)
{
    EValuePtr ev = args->values[index];
    if (type.ptr()) {
        if (ev->type != (Type *)type.ptr())
            argumentError(index, "argument type mismatch");
    }
    else {
        if (ev->type->typeKind != POINTER_TYPE)
            argumentError(index, "expecting value of pointer type");
        type = (PointerType *)ev->type.ptr();
    }
    return ev;
}

static EValuePtr pointerLikeValue(MultiEValuePtr args, unsigned index,
                                  TypePtr &type)
{
    EValuePtr ev = args->values[index];
    if (type.ptr()) {
        if (ev->type != type)
            argumentError(index, "argument type mismatch");
    }
    else {
        switch (ev->type->typeKind) {
        case POINTER_TYPE :
        case CODE_POINTER_TYPE :
        case CCODE_POINTER_TYPE :
            break;
        default :
            argumentError(index, "expecting a value of "
                          "pointer or code-pointer type");
        }
        type = ev->type;
    }
    return ev;
}

static EValuePtr arrayValue(MultiEValuePtr args, unsigned index,
                            ArrayTypePtr &type)
{
    EValuePtr ev = args->values[index];
    if (type.ptr()) {
        if (ev->type != (Type *)type.ptr())
            argumentError(index, "argument type mismatch");
    }
    else {
        if (ev->type->typeKind != ARRAY_TYPE)
            argumentError(index, "expecting a value of array type");
        type = (ArrayType *)ev->type.ptr();
    }
    return ev;
}

static EValuePtr tupleValue(MultiEValuePtr args, unsigned index,
                            TupleTypePtr &type)
{
    EValuePtr ev = args->values[index];
    if (type.ptr()) {
        if (ev->type != (Type *)type.ptr())
            argumentError(index, "argument type mismatch");
    }
    else {
        if (ev->type->typeKind != TUPLE_TYPE)
            argumentError(index, "expecting a value of tuple type");
        type = (TupleType *)ev->type.ptr();
    }
    return ev;
}

static EValuePtr recordValue(MultiEValuePtr args, unsigned index,
                            RecordTypePtr &type)
{
    EValuePtr ev = args->values[index];
    if (type.ptr()) {
        if (ev->type != (Type *)type.ptr())
            argumentError(index, "argument type mismatch");
    }
    else {
        if (ev->type->typeKind != RECORD_TYPE)
            argumentError(index, "expecting a value of record type");
        type = (RecordType *)ev->type.ptr();
    }
    return ev;
}

static EValuePtr enumValue(MultiEValuePtr args, unsigned index,
                           EnumTypePtr &type)
{
    EValuePtr ev = args->values[index];
    if (type.ptr()) {
        if (ev->type != (Type *)type.ptr())
            argumentError(index, "argument type mismatch");
    }
    else {
        if (ev->type->typeKind != ENUM_TYPE)
            argumentError(index, "expecting a value of enum type");
        type = (EnumType *)ev->type.ptr();
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
// evalPrimOp
//

void evalPrimOp(PrimOpPtr x, MultiEValuePtr args, MultiEValuePtr out)
{
    switch (x->primOpCode) {

    case PRIM_TypeP : {
        ensureArity(args, 1);
        ObjectPtr obj = valueToStatic(args->values[0]);
        bool isType = (obj.ptr() && (obj->objKind == TYPE));
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type == boolType);
        *((char *)out0->addr) = isType ? 1 : 0;
        break;
    }

    case PRIM_TypeSize : {
        ensureArity(args, 1);
        TypePtr t = valueToType(args, 0);
        ValueHolderPtr vh = sizeTToValueHolder(typeSize(t));
        evalStaticObject(vh.ptr(), out);
        break;
    }

    case PRIM_primitiveCopy : {
        ensureArity(args, 2);
        EValuePtr ev0 = args->values[0];
        EValuePtr ev1 = args->values[1];
        if (!isPrimitiveType(ev0->type))
            argumentError(0, "expecting a value of primitive type");
        if (ev0->type != ev1->type)
            argumentError(1, "argument type mismatch");
        memcpy(ev0->addr, ev1->addr, typeSize(ev0->type));
        assert(out->size() == 0);
        break;
    }

    case PRIM_boolNot : {
        ensureArity(args, 1);
        EValuePtr ev = args->values[0];
        if (ev->type != boolType)
            argumentError(0, "expecting a value of bool type");
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type == boolType);
        char *p = (char *)ev->addr;
        *((char *)out0->addr) = (*p == 0);
        break;
    }

    case PRIM_numericEqualsP : {
        ensureArity(args, 2);
        TypePtr t;
        EValuePtr ev0 = numericValue(args, 0, t);
        EValuePtr ev1 = numericValue(args, 1, t);
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type == boolType);
        binaryNumericOp<Op_numericEqualsP>(ev0, ev1, out0);
        break;
    }

    case PRIM_numericLesserP : {
        ensureArity(args, 2);
        TypePtr t;
        EValuePtr ev0 = numericValue(args, 0, t);
        EValuePtr ev1 = numericValue(args, 1, t);
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type == boolType);
        binaryNumericOp<Op_numericLesserP>(ev0, ev1, out0);
        break;
    }

    case PRIM_numericAdd : {
        ensureArity(args, 2);
        TypePtr t;
        EValuePtr ev0 = numericValue(args, 0, t);
        EValuePtr ev1 = numericValue(args, 1, t);
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type == t);
        binaryNumericOp<Op_numericAdd>(ev0, ev1, out0);
        break;
    }

    case PRIM_numericSubtract : {
        ensureArity(args, 2);
        TypePtr t;
        EValuePtr ev0 = numericValue(args, 0, t);
        EValuePtr ev1 = numericValue(args, 1, t);
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type == t);
        binaryNumericOp<Op_numericSubtract>(ev0, ev1, out0);
        break;
    }

    case PRIM_numericMultiply : {
        ensureArity(args, 2);
        TypePtr t;
        EValuePtr ev0 = numericValue(args, 0, t);
        EValuePtr ev1 = numericValue(args, 1, t);
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type == t);
        binaryNumericOp<Op_numericMultiply>(ev0, ev1, out0);
        break;
    }

    case PRIM_numericDivide : {
        ensureArity(args, 2);
        TypePtr t;
        EValuePtr ev0 = numericValue(args, 0, t);
        EValuePtr ev1 = numericValue(args, 1, t);
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type == t);
        binaryNumericOp<Op_numericDivide>(ev0, ev1, out0);
        break;
    }

    case PRIM_numericNegate : {
        ensureArity(args, 1);
        TypePtr t;
        EValuePtr ev = numericValue(args, 0, t);
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type == t);
        unaryNumericOp<Op_numericNegate>(ev, out0);
        break;
    }

    case PRIM_integerRemainder : {
        ensureArity(args, 2);
        IntegerTypePtr t;
        EValuePtr ev0 = integerValue(args, 0, t);
        EValuePtr ev1 = integerValue(args, 1, t);
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type == t.ptr());
        binaryIntegerOp<Op_integerRemainder>(ev0, ev1, out0);
        break;
    }

    case PRIM_integerShiftLeft : {
        ensureArity(args, 2);
        IntegerTypePtr t;
        EValuePtr ev0 = integerValue(args, 0, t);
        EValuePtr ev1 = integerValue(args, 1, t);
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type == t.ptr());
        binaryIntegerOp<Op_integerShiftLeft>(ev0, ev1, out0);
        break;
    }

    case PRIM_integerShiftRight : {
        ensureArity(args, 2);
        IntegerTypePtr t;
        EValuePtr ev0 = integerValue(args, 0, t);
        EValuePtr ev1 = integerValue(args, 1, t);
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type == t.ptr());
        binaryIntegerOp<Op_integerShiftRight>(ev0, ev1, out0);
        break;
    }

    case PRIM_integerBitwiseAnd : {
        ensureArity(args, 2);
        IntegerTypePtr t;
        EValuePtr ev0 = integerValue(args, 0, t);
        EValuePtr ev1 = integerValue(args, 1, t);
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type == t.ptr());
        binaryIntegerOp<Op_integerBitwiseAnd>(ev0, ev1, out0);
        break;
    }

    case PRIM_integerBitwiseOr : {
        ensureArity(args, 2);
        IntegerTypePtr t;
        EValuePtr ev0 = integerValue(args, 0, t);
        EValuePtr ev1 = integerValue(args, 1, t);
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type == t.ptr());
        binaryIntegerOp<Op_integerBitwiseOr>(ev0, ev1, out0);
        break;
    }

    case PRIM_integerBitwiseXor : {
        ensureArity(args, 2);
        IntegerTypePtr t;
        EValuePtr ev0 = integerValue(args, 0, t);
        EValuePtr ev1 = integerValue(args, 1, t);
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type == t.ptr());
        binaryIntegerOp<Op_integerBitwiseXor>(ev0, ev1, out0);
        break;
    }

    case PRIM_integerBitwiseNot : {
        ensureArity(args, 1);
        IntegerTypePtr t;
        EValuePtr ev = integerValue(args, 0, t);
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type == t.ptr());
        unaryIntegerOp<Op_integerBitwiseNot>(ev, out0);
        break;
    }

    case PRIM_numericConvert : {
        ensureArity(args, 2);
        TypePtr dest = valueToNumericType(args, 0);
        TypePtr src;
        EValuePtr ev = numericValue(args, 1, src);
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type == dest);
        op_numericConvert(out0, ev);
        break;
    }

    case PRIM_Pointer :
        error("Pointer type constructor cannot be called");

    case PRIM_addressOf : {
        ensureArity(args, 1);
        EValuePtr ev = args->values[0];
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type == pointerType(ev->type));
        *((void **)out0->addr) = (void *)ev->addr;
        break;
    }

    case PRIM_pointerDereference : {
        ensureArity(args, 1);
        PointerTypePtr t;
        EValuePtr ev = pointerValue(args, 0, t);
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type == t.ptr());
        *((void **)out0->addr) = *((void **)ev->addr);
        break;
    }

    case PRIM_pointerEqualsP : {
        ensureArity(args, 2);
        PointerTypePtr t;
        EValuePtr ev0 = pointerValue(args, 0, t);
        EValuePtr ev1 = pointerValue(args, 1, t);
        bool flag = *((void **)ev0->addr) == *((void **)ev1->addr);
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type == boolType);
        *((char *)out0->addr) = flag ? 1 : 0;
        break;
    }

    case PRIM_pointerLesserP : {
        ensureArity(args, 2);
        PointerTypePtr t;
        EValuePtr ev0 = pointerValue(args, 0, t);
        EValuePtr ev1 = pointerValue(args, 1, t);
        bool flag = *((void **)ev0->addr) < *((void **)ev1->addr);
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type == boolType);
        *((char *)out0->addr) = flag ? 1 : 0;
        break;
    }

    case PRIM_pointerOffset : {
        ensureArity(args, 2);
        PointerTypePtr t;
        EValuePtr v0 = pointerValue(args, 0, t);
        IntegerTypePtr offsetT;
        EValuePtr v1 = integerValue(args, 1, offsetT);
        ptrdiff_t offset = op_intToPtrInt(v1);
        char *ptr = *((char **)v0->addr);
        ptr += offset*typeSize(t->pointeeType);
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type == t.ptr());
        *((void **)out0->addr) = (void *)ptr;
        break;
    }

    case PRIM_pointerToInt : {
        ensureArity(args, 2);
        IntegerTypePtr dest = valueToIntegerType(args, 0);
        PointerTypePtr pt;
        EValuePtr ev = pointerValue(args, 1, pt);
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type == dest.ptr());
        void *ptr = *((void **)ev->addr);
        op_pointerToInt(out0, ptr);
        break;
    }

    case PRIM_intToPointer : {
        ensureArity(args, 2);
        TypePtr pointeeType = valueToType(args, 0);
        TypePtr dest = pointerType(pointeeType);
        IntegerTypePtr t;
        EValuePtr ev = integerValue(args, 1, t);
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type == dest);
        ptrdiff_t ptrInt = op_intToPtrInt(ev);
        *((void **)out0->addr) = (void *)ptrInt;
        break;
    }

    case PRIM_CodePointerP : {
        ensureArity(args, 1);
        bool isCodePointerType = false;
        ObjectPtr obj = valueToStatic(args->values[0]);
        if (obj.ptr() && (obj->objKind == TYPE)) {
            Type *t = (Type *)obj.ptr();
            if (t->typeKind == CODE_POINTER_TYPE)
                isCodePointerType = true;
        }
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type == boolType);
        *((char *)out0->addr) = isCodePointerType ? 1 : 0;
        break;
    }

    case PRIM_CodePointer :
        error("CodePointer type constructor cannot be called");

    case PRIM_RefCodePointer :
        error("RefCodePointer type constructor cannot be called");

    case PRIM_makeCodePointer : {
        if (args->size() < 1)
            arityError2(1, args->size());
        ObjectPtr callable = valueToStatic(args, 0);
        switch (callable->objKind) {
        case TYPE :
        case RECORD :
        case PROCEDURE :
            break;
        default :
            argumentError(0, "invalid callable");
        }
        vector<TypePtr> argsKey;
        vector<ValueTempness> argsTempness;
        for (unsigned i = 1; i < args->size(); ++i) {
            TypePtr t = valueToType(args, i);
            argsKey.push_back(t);
            argsTempness.push_back(LVALUE);
        }

        InvokeStackContext invokeStackContext(callable, argsKey);

        InvokeEntryPtr entry =
            analyzeCallable(callable, argsKey, argsTempness);
        if (entry->inlined)
            argumentError(0, "cannot create pointer to inlined code");
        assert(entry->analyzed);
        if (!entry->llvmFunc)
            codegenCodeBody(entry, getCodeName(entry->callable));
        assert(entry->llvmFunc);
        void *funcPtr = llvmEngine->getPointerToGlobal(entry->llvmFunc);
        TypePtr cpType = codePointerType(argsKey,
                                         entry->returnIsRef,
                                         entry->returnTypes);
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type == cpType);
        *((void **)out0->addr) = funcPtr;
        break;
    }

    case PRIM_CCodePointerP : {
        ensureArity(args, 1);
        bool isCCodePointerType = false;
        ObjectPtr obj = valueToStatic(args->values[0]);
        if (obj.ptr() && (obj->objKind == TYPE)) {
            Type *t = (Type *)obj.ptr();
            if (t->typeKind == CCODE_POINTER_TYPE)
                isCCodePointerType = true;
        }
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type == boolType);
        *((char *)out0->addr) = isCCodePointerType ? 1 : 0;
        break;
    }

    case PRIM_CCodePointer :
        error("CCodePointer type constructor cannot be called");

    case PRIM_StdCallCodePointer :
        error("StdCallCodePointer type constructor cannot be called");

    case PRIM_FastCallCodePointer :
        error("FastCallCodePointer type constructor cannot be called");

    case PRIM_makeCCodePointer : {
        if (args->size() < 1)
            arityError2(1, args->size());
        ObjectPtr callable = valueToStatic(args, 0);
        switch (callable->objKind) {
        case TYPE :
        case RECORD :
        case PROCEDURE :
            break;
        default :
            argumentError(0, "invalid callable");
        }
        vector<TypePtr> argsKey;
        vector<ValueTempness> argsTempness;
        for (unsigned i = 1; i < args->size(); ++i) {
            TypePtr t = valueToType(args, i);
            argsKey.push_back(t);
            argsTempness.push_back(LVALUE);
        }

        InvokeStackContext invokeStackContext(callable, argsKey);

        InvokeEntryPtr entry =
            analyzeCallable(callable, argsKey, argsTempness);
        if (entry->inlined)
            argumentError(0, "cannot create pointer to inlined code");
        assert(entry->analyzed);
        if (!entry->llvmFunc)
            codegenCodeBody(entry, getCodeName(entry->callable));
        assert(entry->llvmFunc);
        if (!entry->llvmCWrapper)
            codegenCWrapper(entry, getCodeName(entry->callable));
        assert(entry->llvmCWrapper);
        TypePtr returnType;
        if (entry->returnTypes.size() == 0) {
            returnType = NULL;
        }
        else if (entry->returnTypes.size() == 1) {
            if (entry->returnIsRef[0])
                argumentError(0, "cannot create C compatible pointer "
                              " to return-by-reference code");
            returnType = entry->returnTypes[0];
        }
        else {
            argumentError(0, "cannot create C compatible pointer "
                          "to multi-return code");
        }
        void *funcPtr = llvmEngine->getPointerToGlobal(entry->llvmCWrapper);
        TypePtr ccpType = cCodePointerType(CC_DEFAULT,
                                           argsKey,
                                           false,
                                           returnType);
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type == ccpType);
        *((void **)out0->addr) = funcPtr;
        break;
    }

    case PRIM_pointerCast : {
        ensureArity(args, 2);
        TypePtr dest = valueToPointerLikeType(args, 0);
        TypePtr src;
        EValuePtr ev = pointerLikeValue(args, 1, src);
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type == dest);
        *((void **)out0->addr) = *((void **)ev->addr);
        break;
    }

    case PRIM_Array :
        error("Array type constructor cannot be called");

    case PRIM_array : {
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type->typeKind == ARRAY_TYPE);
        ArrayType *t = (ArrayType *)out0->type.ptr();
        assert((int)args->size() == t->size);
        TypePtr etype = t->elementType;
        for (unsigned i = 0; i < args->size(); ++i) {
            EValuePtr earg = args->values[i];
            if (earg->type != etype)
                argumentError(i, "array element type mismatch");
            char *ptr = out0->addr + typeSize(etype)*i;
            evalValueCopy(new EValue(etype, ptr), earg);
        }
        break;
    }

    case PRIM_arrayRef : {
        ensureArity(args, 2);
        ArrayTypePtr at;
        EValuePtr earray = arrayValue(args, 0, at);
        IntegerTypePtr indexType;
        EValuePtr iv = integerValue(args, 1, indexType);
        ptrdiff_t i = op_intToPtrInt(iv);
        char *ptr = earray->addr + i*typeSize(at->elementType);
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type == pointerType(at->elementType));
        *((void **)out0->addr) = (void *)ptr;
        break;
    }

    case PRIM_TupleP : {
        ensureArity(args, 1);
        bool isTupleType = false;
        ObjectPtr obj = valueToStatic(args->values[0]);
        if (obj.ptr() && (obj->objKind == TYPE)) {
            Type *t = (Type *)obj.ptr();
            if (t->typeKind == TUPLE_TYPE)
                isTupleType = true;
        }
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type == boolType);
        *((char *)out0->addr) = isTupleType ? 1 : 0;
        break;
    }

    case PRIM_Tuple :
        error("Tuple type constructor cannot be called");

    case PRIM_TupleElementCount : {
        ensureArity(args, 1);
        TupleTypePtr t = valueToTupleType(args, 0);
        ValueHolderPtr vh = sizeTToValueHolder(t->elementTypes.size());
        evalStaticObject(vh.ptr(), out);
        break;
    }

    case PRIM_TupleElementOffset : {
        ensureArity(args, 2);
        TupleTypePtr t = valueToTupleType(args, 0);
        size_t i = valueToStaticSizeT(args, 1);
        if (i >= t->elementTypes.size())
            argumentError(1, "tuple element index out of range");
        const llvm::StructLayout *layout = tupleTypeLayout(t.ptr());
        ValueHolderPtr vh = sizeTToValueHolder(layout->getElementOffset(i));
        evalStaticObject(vh.ptr(), out);
        break;
    }

    case PRIM_tuple : {
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type->typeKind == TUPLE_TYPE);
        TupleType *t = (TupleType *)out0->type.ptr();
        assert(args->size() == t->elementTypes.size());
        const llvm::StructLayout *layout = tupleTypeLayout(t);
        for (unsigned i = 0; i < args->size(); ++i) {
            EValuePtr earg = args->values[i];
            if (earg->type != t->elementTypes[i])
                argumentError(i, "argument type mismatch");
            char *ptr = out0->addr + layout->getElementOffset(i);
            EValuePtr eargDest = new EValue(t->elementTypes[i], ptr);
            evalValueCopy(eargDest, earg);
        }
        break;
    }

    case PRIM_tupleRef : {
        ensureArity(args, 2);
        TupleTypePtr tt;
        EValuePtr etuple = tupleValue(args, 0, tt);
        size_t i = valueToStaticSizeT(args, 1);
        if (i >= tt->elementTypes.size())
            argumentError(1, "tuple element index out of range");
        const llvm::StructLayout *layout = tupleTypeLayout(tt.ptr());
        char *ptr = etuple->addr + layout->getElementOffset(i);
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type == pointerType(tt->elementTypes[i]));
        *((void **)out0->addr) = (void *)ptr;
        break;
    }

    case PRIM_RecordP : {
        ensureArity(args, 1);
        bool isRecordType = false;
        ObjectPtr obj = valueToStatic(args->values[0]);
        if (obj.ptr() && (obj->objKind == TYPE)) {
            Type *t = (Type *)obj.ptr();
            if (t->typeKind == RECORD_TYPE)
                isRecordType = true;
        }
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type == boolType);
        *((char *)out0->addr) = isRecordType ? 1 : 0;
        break;
    }

    case PRIM_RecordFieldCount : {
        ensureArity(args, 1);
        RecordTypePtr rt = valueToRecordType(args, 0);
        const vector<TypePtr> &fieldTypes = recordFieldTypes(rt);
        ValueHolderPtr vh = sizeTToValueHolder(fieldTypes.size());
        evalStaticObject(vh.ptr(), out);
        break;
    }

    case PRIM_RecordFieldOffset : {
        ensureArity(args, 2);
        RecordTypePtr rt = valueToRecordType(args, 0);
        size_t i = valueToStaticSizeT(args, 1);
        const vector<TypePtr> &fieldTypes = recordFieldTypes(rt);
        if (i >= fieldTypes.size())
            argumentError(1, "record field index out of range");
        const llvm::StructLayout *layout = recordTypeLayout(rt.ptr());
        ValueHolderPtr vh = sizeTToValueHolder(layout->getElementOffset(i));
        evalStaticObject(vh.ptr(), out);
        break;
    }

    case PRIM_RecordFieldIndex : {
        ensureArity(args, 2);
        RecordTypePtr rt = valueToRecordType(args, 0);
        IdentifierPtr fname = valueToIdentifier(args, 1);
        const map<string, size_t> &fieldIndexMap = recordFieldIndexMap(rt);
        map<string, size_t>::const_iterator fi =
            fieldIndexMap.find(fname->str);
        if (fi == fieldIndexMap.end())
            argumentError(1, "field not found in record");
        ValueHolderPtr vh = sizeTToValueHolder(fi->second);
        evalStaticObject(vh.ptr(), out);
        break;
    }

    case PRIM_recordFieldRef : {
        ensureArity(args, 2);
        RecordTypePtr rt;
        EValuePtr erec = recordValue(args, 0, rt);
        size_t i = valueToStaticSizeT(args, 1);
        const vector<TypePtr> &fieldTypes = recordFieldTypes(rt);
        if (i >= fieldTypes.size())
            argumentError(1, "record field index out of range");
        const llvm::StructLayout *layout = recordTypeLayout(rt.ptr());
        char *ptr = erec->addr + layout->getElementOffset(i);
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type == pointerType(fieldTypes[i]));
        *((void **)out0->addr) = (void *)ptr;
        break;
    }

    case PRIM_recordFieldRefByName : {
        ensureArity(args, 2);
        RecordTypePtr rt;
        EValuePtr erec = recordValue(args, 0, rt);
        IdentifierPtr fname = valueToIdentifier(args, 1);
        const map<string, size_t> &fieldIndexMap = recordFieldIndexMap(rt);
        map<string,size_t>::const_iterator fi =
            fieldIndexMap.find(fname->str);
        if (fi == fieldIndexMap.end())
            argumentError(1, "field not found in record");
        size_t index = fi->second;
        const vector<TypePtr> &fieldTypes = recordFieldTypes(rt);
        const llvm::StructLayout *layout = recordTypeLayout(rt.ptr());
        char *ptr = erec->addr + layout->getElementOffset(index);
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type == pointerType(fieldTypes[index]));
        *((void **)out0->addr) = (void *)ptr;
        break;
    }

    case PRIM_Static :
        error("Static type constructor cannot be called");

    case PRIM_StaticName : {
        ensureArity(args, 1);
        ObjectPtr obj = valueToStatic(args, 0);
        ostringstream sout;
        printName(sout, obj);
        ExprPtr z = new StringLiteral(sout.str());
        evalExpr(z, new Env(), out);
        break;
    }

    case PRIM_EnumP : {
        ensureArity(args, 1);
        bool isEnumType = false;
        ObjectPtr obj = valueToStatic(args->values[0]);
        if (obj.ptr() && (obj->objKind == TYPE)) {
            Type *t = (Type *)obj.ptr();
            if (t->typeKind == ENUM_TYPE)
                isEnumType = true;
        }
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type == boolType);
        *((char *)out0->addr) = isEnumType ? 1 : 0;
        break;
    }

    case PRIM_enumToInt : {
        ensureArity(args, 1);
        EnumTypePtr et;
        EValuePtr ev = enumValue(args, 0, et);
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type == cIntType);
        *((int *)out0->addr) = *((int *)ev->addr);
        break;
    }

    case PRIM_intToEnum : {
        ensureArity(args, 2);
        EnumTypePtr et = valueToEnumType(args, 0);
        IntegerTypePtr it = (IntegerType *)cIntType.ptr();
        EValuePtr ev = integerValue(args, 1, it);
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type == et.ptr());
        *((int *)out0->addr) = *((int *)ev->addr);
        break;
    }

    default :
        assert(false);

    }
}
