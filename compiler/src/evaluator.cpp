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
        if (pv->type->typeKind == STATIC_OBJECT_TYPE) {
            StaticObjectType *t = (StaticObjectType *)pv->type.ptr();
            return t->obj;
        }
        ValueHolderPtr v = new ValueHolder(pv->type);
        evaluateIntoValueHolder(expr, env, v);
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
// evaluateIntoValueHolder
//

void evaluateIntoValueHolder(ExprPtr expr, EnvPtr env, ValueHolderPtr v)
{
    CodePtr code = new Code();
    code->body = new Return(expr);
    IdentifierPtr name = new Identifier("clay_eval_temp");
    ProcedurePtr proc = new Procedure(name, PRIVATE, code, false);
    proc->env = env;
    InvokeEntryPtr entry = codegenCallable(proc.ptr(),
                                           vector<bool>(),
                                           vector<ObjectPtr>(),
                                           vector<ValueTempness>(),
                                           vector<LocationPtr>());
    assert(!entry->inlined);
    if (entry->returnType != v->type)
        error(expr, "type mismatch in evaluating");

    vector<llvm::GenericValue> gvArgs;
    gvArgs.push_back(llvm::GenericValue(v->buf));
    llvmEngine->runFunction(entry->llvmFunc, gvArgs);
}
