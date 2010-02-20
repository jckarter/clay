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
        ValueHolderPtr v = new ValueHolder(pv->type);
        evaluateIntoValueHolder(expr, env, v);
        return v.ptr();
    }
    return analysis;
}

TypePtr evaluateType(ExprPtr expr, EnvPtr env)
{
    ObjectPtr v = evaluateStatic(expr, env);
    if (v->objKind != TYPE)
        error(expr, "expecting a type");
    return (Type *)v.ptr();
}

ObjectPtr evaluateReturnType(ExprPtr expr, EnvPtr env)
{
    ObjectPtr v = evaluateStatic(expr, env);
    switch (v->objKind) {
    case TYPE :
    case VOID_TYPE :
        break;
    default :
        error(expr, "expecting a return type");
    }
    return v;
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

TypePtr evaluatePointerOrFunctionPointerType(ExprPtr expr, EnvPtr env)
{
    TypePtr t = evaluateType(expr, env);
    switch (t->typeKind) {
    case POINTER_TYPE :
    case FUNCTION_POINTER_TYPE :
        break;
    default :
        error(expr, "expecting a pointer or function pointer type");
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
        error(expr, "expecting an Int32 value");
    ValueHolderPtr vh = (ValueHolder *)v.ptr();
    if (vh->type != int32Type)
        error(expr, "expecting an Int32 value");
    return *(int *)vh->buf;
}

bool evaluateBool(ExprPtr expr, EnvPtr env)
{
    ObjectPtr v = evaluateStatic(expr, env);
    if (v->objKind != VALUE_HOLDER)
        error(expr, "expecting a bool value");
    ValueHolderPtr vh = (ValueHolder *)v.ptr();
    if (vh->type != boolType)
        error(expr, "expecting a bool value");
    return *(bool *)vh->buf;
}

ValueHolderPtr intToValueHolder(int x)
{
    ValueHolderPtr v = new ValueHolder(int32Type);
    *(int *)v->buf = x;
    return v;
}

ValueHolderPtr boolToValueHolder(bool x)
{
    ValueHolderPtr v = new ValueHolder(int32Type);
    *(bool *)v->buf = x;
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
    ProcedurePtr proc = new Procedure(name, code);
    proc->env = env;
    InvokeEntryPtr entry = codegenProcedure(proc, vector<ObjectPtr>(),
                                            vector<LocationPtr>());
    assert(entry->analysis->objKind == PVALUE);
    PValuePtr pv = (PValue *)entry->analysis.ptr();
    if (pv->type != v->type)
        error(expr, "type mismatch in evaluating");

    vector<llvm::GenericValue> gvArgs;
    gvArgs.push_back(llvm::GenericValue(v->buf));
    llvmEngine->runFunction(entry->llvmFunc, gvArgs);
}
