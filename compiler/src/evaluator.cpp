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
            codegenExternal(y);
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
        // takes care of type constructors
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
    MultiPValuePtr mpv = analyzeMulti(args, env);
    assert(mpv.ptr());

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
        MultiEValuePtr mev = evalMultiAsRef(args, env);
        evalPrimOp(x, mev, out);
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

    CodePtr code = entry->code;

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
// evalPrimOp
//

void evalPrimOp(PrimOpPtr x, MultiEValuePtr args, MultiEValuePtr out)
{
}
