#include "clay.hpp"

EValuePtr evalOneAsRef(ExprPtr expr, EnvPtr env);
void evalOneInto(ExprPtr expr, EnvPtr env, EValuePtr out);

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
// evalOneAsRef
//

EValuePtr evalOneAsRef(ExprPtr expr, EnvPtr env)
{
    PValuePtr pv = analyzeOne(expr, env);
    if (pv->isTemp) {
        EValuePtr ev = evalAllocValue(pv->type);
        evalOne(expr, env, ev);
        return ev;
    }
    else {
        EValuePtr evPtr = evalAllocValue(pointerType(pv->type));
        evalOne(expr, env, evPtr);
        return derefValue(evPtr);
    }
}



//
// evalOneInto
//

void evalOneInto(ExprPtr expr, EnvPtr env, EValuePtr out)
{
    PValuePtr pv = analyzeOne(expr, env);
    if (pv->isTemp) {
        evalOne(expr, env, out);
    }
    else {
        EValuePtr evPtr = evalAllocValue(pointerType(pv->type));
        evalOne(expr, env, evPtr);
        evalValueCopy(out, derefValue(evPtr));
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
        assert(out->size() == 1);
        if (pv->isTemp) {
            EValuePtr ev = out->values[0];
            evalOneInto(x->expr1, env, ev);
            if (evalToBoolFlag(ev)) {
                evalValueDestroy(ev);
                PValuePtr pv2 = analyzeOne(x->expr2, env);
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
        assert(out->size() == 1);
        if (pv->isTemp) {
            EValuePtr ev = out->values[0];
            evalOneInto(x->expr1, env, ev);
            if (!evalToBoolFlag(ev)) {
                evalValueDestroy(ev);
                PValuePtr pv2 = analyzeOne(x->expr2, env);
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

void evalStaticObject(ObjectPtr x, MultiEValuePtr out)
{
}

void evalValueHolder(ValueHolderPtr x, MultiEValuePtr out)
{
}

void evalIndexingExpr(ExprPtr indexable,
                      const vector<ExprPtr> &args,
                      EnvPtr env,
                      MultiEValuePtr out)
{
}

void evalFieldRefExpr(ExprPtr base,
                      IdentifierPtr name,
                      EnvPtr env,
                      MultiEValuePtr out)
{
}

void evalCallExpr(ExprPtr callable,
                  const vector<ExprPtr> &args,
                  EnvPtr env,
                  MultiEValuePtr out)
{
}

void evalCallValue(EValuePtr callable,
                   MultiEValuePtr args,
                   MultiEValuePtr out)
{
}

void evalCallPointer(EValuePtr x,
                     MultiEValuePtr args,
                     MultiEValuePtr out)
{
}
