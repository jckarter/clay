
#include "clay.hpp"

namespace clay {

static int analysisCachingDisabled = 0;
void disableAnalysisCaching() { analysisCachingDisabled += 1; }
void enableAnalysisCaching() { analysisCachingDisabled -= 1; }

static TypePtr objectType(ObjectPtr x);

static TypePtr valueToType(MultiPValuePtr x, unsigned index);
static TypePtr valueToNumericType(MultiPValuePtr x, unsigned index);
static TypePtr valueToComplexType(MultiPValuePtr x, unsigned index);
static IntegerTypePtr valueToIntegerType(MultiPValuePtr x, unsigned index);
static TypePtr valueToPointerLikeType(MultiPValuePtr x, unsigned index);
static TypePtr valueToEnumerationType(MultiPValuePtr x, unsigned index);

static bool staticToTypeTuple(ObjectPtr x, vector<TypePtr> &out);
static void staticToTypeTuple(MultiStaticPtr x, unsigned index,
                             vector<TypePtr> &out);
static bool staticToInt(ObjectPtr x, int &out);
static int staticToInt(MultiStaticPtr x, unsigned index);
static bool staticToSizeTOrInt(ObjectPtr x, size_t &out);

static TypePtr numericTypeOfValue(MultiPValuePtr x, unsigned index);
static IntegerTypePtr integerTypeOfValue(MultiPValuePtr x, unsigned index);
static PointerTypePtr pointerTypeOfValue(MultiPValuePtr x, unsigned index);
static ArrayTypePtr arrayTypeOfValue(MultiPValuePtr x, unsigned index);
static TupleTypePtr tupleTypeOfValue(MultiPValuePtr x, unsigned index);
static ComplexTypePtr complexTypeOfValue(MultiPValuePtr x, unsigned index);
static RecordTypePtr recordTypeOfValue(MultiPValuePtr x, unsigned index);

static PValuePtr staticPValue(ObjectPtr x);



//
// utility procs
//

static TypePtr objectType(ObjectPtr x)
{
    switch (x->objKind) {

    case VALUE_HOLDER : {
        ValueHolder *y = (ValueHolder *)x.ptr();
        return y->type;
    }

    case TYPE :
    case PRIM_OP :
    case PROCEDURE :
    case RECORD :
    case VARIANT :
    case MODULE_HOLDER :
    case IDENTIFIER :
        return staticType(x);

    default :
        error("untypeable object");
        return NULL;

    }
}

ObjectPtr unwrapStaticType(TypePtr t) {
    if (t->typeKind != STATIC_TYPE)
        return NULL;
    StaticType *st = (StaticType *)t.ptr();
    return st->obj;
}

static TypePtr valueToType(MultiPValuePtr x, unsigned index)
{
    ObjectPtr obj = unwrapStaticType(x->values[index]->type);
    if (!obj)
        argumentError(index, "expecting a type");
    TypePtr t;
    if (!staticToType(obj, t))
        argumentError(index, "expecting a type");
    return t;
}

static TypePtr valueToNumericType(MultiPValuePtr x, unsigned index)
{
    TypePtr t = valueToType(x, index);
    switch (t->typeKind) {
    case INTEGER_TYPE :
    case FLOAT_TYPE :
        return t;
    default :
        argumentTypeError(index, "numeric type", t);
        return NULL;
    }
}

static TypePtr valueToComplexType(MultiPValuePtr x, unsigned index)
{
    TypePtr t = valueToType(x, index);
    if (t->typeKind != COMPLEX_TYPE)
        argumentTypeError(index, "complex type", t);
    return (ComplexType *)t.ptr();;
}

static IntegerTypePtr valueToIntegerType(MultiPValuePtr x, unsigned index)
{
    TypePtr t = valueToType(x, index);
    if (t->typeKind != INTEGER_TYPE)
        argumentTypeError(index, "integer type", t);
    return (IntegerType *)t.ptr();
}

static TypePtr valueToPointerLikeType(MultiPValuePtr x, unsigned index)
{
    TypePtr t = valueToType(x, index);
    if (!isPointerOrCodePointerType(t))
        argumentTypeError(index, "pointer or code-pointer type", t);
    return t;
}

static TypePtr valueToEnumerationType(MultiPValuePtr x, unsigned index)
{
    TypePtr t = valueToType(x, index);
    if (t->typeKind != ENUM_TYPE)
        argumentTypeError(index, "enum type", t);
    return t;
}

static bool staticToTypeTuple(ObjectPtr x, vector<TypePtr> &out)
{
    TypePtr t;
    if (staticToType(x, t)) {
        out.push_back(t);
        return true;
    }
    if (x->objKind != VALUE_HOLDER)
        return false;
    ValueHolderPtr y = (ValueHolder *)x.ptr();
    assert(y->type->typeKind != STATIC_TYPE);
    if (y->type->typeKind != TUPLE_TYPE)
        return false;
    TupleTypePtr z = (TupleType *)y->type.ptr();
    for (unsigned i = 0; i < z->elementTypes.size(); ++i) {
        ObjectPtr obj = unwrapStaticType(z->elementTypes[i]);
        if ((!obj) || (obj->objKind != TYPE))
            return false;
        out.push_back((Type *)obj.ptr());
    }
    return true;
}

static void staticToTypeTuple(MultiStaticPtr x, unsigned index,
                             vector<TypePtr> &out)
{
    if (!staticToTypeTuple(x->values[index], out))
        argumentError(index, "expecting zero-or-more types");
}

static bool staticToInt(ObjectPtr x, int &out)
{
    if (x->objKind != VALUE_HOLDER)
        return false;
    ValueHolderPtr vh = (ValueHolder *)x.ptr();
    if (vh->type != cIntType)
        return false;
    out = *((int *)vh->buf);
    return true;
}

static int staticToInt(MultiStaticPtr x, unsigned index)
{
    int out = -1;
    if (!staticToInt(x->values[index], out))
        argumentError(index, "expecting Int value");
    return out;
}

static bool staticToSizeTOrInt(ObjectPtr x, size_t &out)
{
    if (x->objKind != VALUE_HOLDER)
        return false;
    ValueHolderPtr vh = (ValueHolder *)x.ptr();
    if (vh->type == cSizeTType) {
        out = *((size_t *)vh->buf);
        return true;
    }
    else if (vh->type == cIntType) {
        int i = *((int *)vh->buf);
        out = size_t(i);
        return true;
    }
    else {
        return false;
    }
}

static TypePtr numericTypeOfValue(MultiPValuePtr x, unsigned index)
{
    TypePtr t = x->values[index]->type;
    switch (t->typeKind) {
    case INTEGER_TYPE :
    case FLOAT_TYPE :
        return t;
    default :
        argumentTypeError(index, "numeric type", t);
        return NULL;
    }
}

static ComplexTypePtr complexTypeOfValue(MultiPValuePtr x, unsigned index)
{
    TypePtr t = x->values[index]->type;
    if (t->typeKind != COMPLEX_TYPE)
        argumentTypeError(index, "complex type", t);
    return (ComplexType *)t.ptr();
}

static IntegerTypePtr integerTypeOfValue(MultiPValuePtr x, unsigned index)
{
    TypePtr t = x->values[index]->type;
    if (t->typeKind != INTEGER_TYPE)
        argumentTypeError(index, "integer type", t);
    return (IntegerType *)t.ptr();
}

static PointerTypePtr pointerTypeOfValue(MultiPValuePtr x, unsigned index)
{
    TypePtr t = x->values[index]->type;
    if (t->typeKind != POINTER_TYPE)
        argumentTypeError(index, "pointer type", t);
    return (PointerType *)t.ptr();
}

static CCodePointerTypePtr cCodePointerTypeOfValue(MultiPValuePtr x,
                                                   unsigned index)
{
    TypePtr t = x->values[index]->type;
    if (t->typeKind != CCODE_POINTER_TYPE)
        argumentTypeError(index, "c code pointer type", t);
    return (CCodePointerType *)t.ptr();
}

static ArrayTypePtr arrayTypeOfValue(MultiPValuePtr x, unsigned index)
{
    TypePtr t = x->values[index]->type;
    if (t->typeKind != ARRAY_TYPE)
        argumentTypeError(index, "array type", t);
    return (ArrayType *)t.ptr();
}

static TupleTypePtr tupleTypeOfValue(MultiPValuePtr x, unsigned index)
{
    TypePtr t = x->values[index]->type;
    if (t->typeKind != TUPLE_TYPE)
        argumentTypeError(index, "tuple type", t);
    return (TupleType *)t.ptr();
}

static RecordTypePtr recordTypeOfValue(MultiPValuePtr x, unsigned index)
{
    TypePtr t = x->values[index]->type;
    if (t->typeKind != RECORD_TYPE)
        argumentTypeError(index, "record type", t);
    return (RecordType *)t.ptr();
}

static VariantTypePtr variantTypeOfValue(MultiPValuePtr x, unsigned index)
{
    TypePtr t = x->values[index]->type;
    if (t->typeKind != VARIANT_TYPE)
        argumentTypeError(index, "variant type", t);
    return (VariantType *)t.ptr();
}

static PValuePtr staticPValue(ObjectPtr x)
{
    TypePtr t = staticType(x);
    return new PValue(t, true);
}



//
// safe analysis
//

static LocationPtr analysisErrorLocation;
static vector<CompileContextEntry> analysisErrorCompileContext;

struct ClearAnalysisError {
    ClearAnalysisError() {}
    ~ClearAnalysisError() {
        analysisErrorLocation = NULL;
        analysisErrorCompileContext.clear();
    }
};

static void updateAnalysisErrorLocation()
{
    analysisErrorLocation = topLocation();
    analysisErrorCompileContext = getCompileContext();
}

static void analysisError()
{
    setCompileContext(analysisErrorCompileContext);
    LocationContext loc(analysisErrorLocation);
    error("type propagation failed due to recursion without base case");
}

PValuePtr safeAnalyzeOne(ExprPtr expr, EnvPtr env)
{
    ClearAnalysisError clear;
    PValuePtr result = analyzeOne(expr, env);
    if (!result)
        analysisError();
    return result;
}

MultiPValuePtr safeAnalyzeMulti(ExprListPtr exprs, EnvPtr env, unsigned wantCount)
{
    ClearAnalysisError clear;
    MultiPValuePtr result = analyzeMulti(exprs, env, wantCount);
    if (!result)
        analysisError();
    return result;
}

MultiPValuePtr safeAnalyzeExpr(ExprPtr expr, EnvPtr env)
{
    ClearAnalysisError clear;
    MultiPValuePtr result = analyzeExpr(expr, env);
    if (!result)
        analysisError();
    return result;
}

MultiPValuePtr safeAnalyzeIndexingExpr(ExprPtr indexable,
                                       ExprListPtr args,
                                       EnvPtr env)
{
    ClearAnalysisError clear;
    MultiPValuePtr result = analyzeIndexingExpr(indexable, args, env);
    if (!result)
        analysisError();
    return result;
}

MultiPValuePtr safeAnalyzeMultiArgs(ExprListPtr exprs,
                                    EnvPtr env,
                                    vector<unsigned> &dispatchIndices)
{
    ClearAnalysisError clear;
    MultiPValuePtr result = analyzeMultiArgs(exprs, env, dispatchIndices);
    if (!result)
        analysisError();
    return result;
}

InvokeEntryPtr safeAnalyzeCallable(ObjectPtr x,
                                   const vector<TypePtr> &argsKey,
                                   const vector<ValueTempness> &argsTempness)
{
    ClearAnalysisError clear;
    InvokeEntryPtr entry = analyzeCallable(x, argsKey, argsTempness);
    if (!entry->callByName && !entry->analyzed)
        analysisError();
    return entry;
}

MultiPValuePtr safeAnalyzeCallByName(InvokeEntryPtr entry,
                                     ExprPtr callable,
                                     ExprListPtr args,
                                     EnvPtr env)
{
    ClearAnalysisError clear;
    MultiPValuePtr result = analyzeCallByName(entry, callable, args, env);
    if (!entry)
        analysisError();
    return result;
}

MultiPValuePtr safeAnalyzeGVarInstance(GVarInstancePtr x)
{
    ClearAnalysisError clear;
    MultiPValuePtr result = analyzeGVarInstance(x);
    if (!result)
        analysisError();
    return result;
}



//
// analyzeMulti
//

static MultiPValuePtr analyzeMulti2(ExprListPtr exprs, EnvPtr env, unsigned wantCount);

MultiPValuePtr analyzeMulti(ExprListPtr exprs, EnvPtr env, unsigned wantCount)
{
    if (analysisCachingDisabled > 0)
        return analyzeMulti2(exprs, env, wantCount);
    if (!exprs->cachedAnalysis)
        exprs->cachedAnalysis = analyzeMulti2(exprs, env, wantCount);
    return exprs->cachedAnalysis;
}

static MultiPValuePtr analyzeMulti2(ExprListPtr exprs, EnvPtr env, unsigned wantCount)
{
    MultiPValuePtr out = new MultiPValue();
    ExprPtr unpackExpr = implicitUnpackExpr(wantCount, exprs);
    if (unpackExpr != NULL) {
        MultiPValuePtr z = analyzeExpr(unpackExpr, env);
        if (!z)
            return NULL;
        out->add(z);
    } else for (unsigned i = 0; i < exprs->size(); ++i) {
        ExprPtr x = exprs->exprs[i];
        if (x->exprKind == UNPACK) {
            Unpack *y = (Unpack *)x.ptr();
            MultiPValuePtr z = analyzeExpr(y->expr, env);
            if (!z)
                return NULL;
            out->add(z);
        }
        else if (x->exprKind == PAREN) {
            MultiPValuePtr y = analyzeExpr(x, env);
            if (!y)
                return NULL;
            out->add(y);
        } else {
            PValuePtr y = analyzeOne(x, env);
            if (!y)
                return NULL;
            out->add(y);
        }
    }
    return out;
}



//
// analyzeOne
//

PValuePtr analyzeOne(ExprPtr expr, EnvPtr env)
{
    MultiPValuePtr x = analyzeExpr(expr, env);
    if (!x)
        return NULL;
    LocationContext loc(expr->location);
    ensureArity(x, 1);
    return x->values[0];
}



//
// analyzeMultiArgs, analyzeOneArg, analyzeArgExpr
//

static MultiPValuePtr analyzeMultiArgs2(ExprListPtr exprs,
                                        EnvPtr env,
                                        unsigned startIndex,
                                        vector<unsigned> &dispatchIndices);

MultiPValuePtr analyzeMultiArgs(ExprListPtr exprs,
                                EnvPtr env,
                                vector<unsigned> &dispatchIndices)
{
    if (analysisCachingDisabled > 0)
        return analyzeMultiArgs2(exprs, env, 0, dispatchIndices);
    if (!exprs->cachedAnalysis) {
        MultiPValuePtr mpv = analyzeMultiArgs2(exprs, env, 0, dispatchIndices);
        if (mpv.ptr() && dispatchIndices.empty())
            exprs->cachedAnalysis = mpv;
        return mpv;
    }
    return exprs->cachedAnalysis;
}

static MultiPValuePtr analyzeMultiArgs2(ExprListPtr exprs,
                                        EnvPtr env,
                                        unsigned startIndex,
                                        vector<unsigned> &dispatchIndices)
{
    MultiPValuePtr out = new MultiPValue();
    unsigned index = startIndex;
    for (unsigned i = 0; i < exprs->size(); ++i) {
        ExprPtr x = exprs->exprs[i];
        if (x->exprKind == UNPACK) {
            Unpack *y = (Unpack *)x.ptr();
            MultiPValuePtr z = analyzeArgExpr(y->expr, env, index, dispatchIndices);
            if (!z)
                return NULL;
            index += z->size();
            out->add(z);
        }
        else if (x->exprKind == PAREN) {
            MultiPValuePtr z = analyzeArgExpr(x, env, index, dispatchIndices);
            if (!z)
                return NULL;
            index += z->size();
            out->add(z);
        } else {
            PValuePtr y = analyzeOneArg(x, env, index, dispatchIndices);
            if (!y)
                return NULL;
            out->add(y);
            index += 1;
        }
    }
    return out;
}

PValuePtr analyzeOneArg(ExprPtr x,
                        EnvPtr env,
                        unsigned startIndex,
                        vector<unsigned> &dispatchIndices)
{
    MultiPValuePtr mpv = analyzeArgExpr(x, env, startIndex, dispatchIndices);
    if (!mpv)
        return NULL;
    LocationContext loc(x->location);
    ensureArity(mpv, 1);
    return mpv->values[0];
}

MultiPValuePtr analyzeArgExpr(ExprPtr x,
                              EnvPtr env,
                              unsigned startIndex,
                              vector<unsigned> &dispatchIndices)
{
    if (x->exprKind == DISPATCH_EXPR) {
        DispatchExpr *y = (DispatchExpr *)x.ptr();
        MultiPValuePtr mpv = analyzeExpr(y->expr, env);
        if (!mpv)
            return NULL;
        for (unsigned i = 0; i < mpv->size(); ++i)
            dispatchIndices.push_back(i + startIndex);
        return mpv;
    }
    return analyzeExpr(x, env);
}



//
// analyzeExpr
//

static MultiPValuePtr analyzeExpr2(ExprPtr expr, EnvPtr env);

MultiPValuePtr analyzeExpr(ExprPtr expr, EnvPtr env)
{
    if (analysisCachingDisabled > 0)
        return analyzeExpr2(expr, env);
    if (!expr->cachedAnalysis)
        expr->cachedAnalysis = analyzeExpr2(expr, env);
    return expr->cachedAnalysis;
}

static MultiPValuePtr analyzeExpr2(ExprPtr expr, EnvPtr env)
{
    LocationContext loc(expr->location);

    switch (expr->exprKind) {

    case BOOL_LITERAL : {
        return new MultiPValue(new PValue(boolType, true));
    }

    case INT_LITERAL : {
        IntLiteral *x = (IntLiteral *)expr.ptr();
        ValueHolderPtr v = parseIntLiteral(safeLookupModule(env), x);
        return new MultiPValue(new PValue(v->type, true));
    }

    case FLOAT_LITERAL : {
        FloatLiteral *x = (FloatLiteral *)expr.ptr();
        ValueHolderPtr v = parseFloatLiteral(safeLookupModule(env), x);
        return new MultiPValue(new PValue(v->type, true));
    }

    case CHAR_LITERAL : {
        CharLiteral *x = (CharLiteral *)expr.ptr();
        if (!x->desugared)
            x->desugared = desugarCharLiteral(x->value);
        return analyzeExpr(x->desugared, env);
    }

    case STRING_LITERAL : {
        StringLiteral *x = (StringLiteral *)expr.ptr();
        TypePtr ptrInt8Type = pointerType(int8Type);
        PValuePtr pv = new PValue(arrayType(int8Type, x->value.size()), true);
        MultiPValuePtr args = new MultiPValue();
        args->add(new PValue(ptrInt8Type, true));
        args->add(new PValue(ptrInt8Type, true));
        return analyzeCallValue(staticPValue(operator_stringLiteral()), args);
    }

    case IDENTIFIER_LITERAL : {
        IdentifierLiteral *x = (IdentifierLiteral *)expr.ptr();
        return new MultiPValue(staticPValue(x->value.ptr()));
    }

    case NAME_REF : {
        NameRef *x = (NameRef *)expr.ptr();
        ObjectPtr y = safeLookupEnv(env, x->name);
        if (y->objKind == EXPRESSION) {
            ExprPtr z = (Expr *)y.ptr();
            return analyzeExpr(z, env);
        }
        else if (y->objKind == EXPR_LIST) {
            ExprListPtr z = (ExprList *)y.ptr();
            return analyzeMulti(z, env, 0);
        }
        return analyzeStaticObject(y);
    }

    case FILE_EXPR : {
        LocationPtr location = safeLookupCallByNameLocation(env);
        string filename = location->source->fileName;
        return analyzeStaticObject(new Identifier(filename));
    }

    case LINE_EXPR : {
        return new MultiPValue(new PValue(cSizeTType, true));
    }

    case COLUMN_EXPR : {
        return new MultiPValue(new PValue(cSizeTType, true));
    }

    case ARG_EXPR : {
        ARGExpr *arg = (ARGExpr *)expr.ptr();
        ObjectPtr obj = safeLookupEnv(env, arg->name);
        if (obj->objKind == EXPRESSION) {
            Expr *expr = (Expr*)obj.ptr();
            if (expr->exprKind == FOREIGN_EXPR) {
                ForeignExpr *fexpr = (ForeignExpr*)expr;
                string argString = fexpr->expr->asString();
                return analyzeStaticObject(new Identifier(argString));
            }
        }
        error("__ARG__ may only be applied to an alias value or alias function argument");
    }

    case TUPLE : {
        Tuple *x = (Tuple *)expr.ptr();
        return analyzeCallExpr(operator_expr_tupleLiteral(),
                               x->args,
                               env);
    }

    case PAREN : {
        Paren *x = (Paren *)expr.ptr();
        return analyzeMulti(x->args, env, 0);
    }

    case INDEXING : {
        Indexing *x = (Indexing *)expr.ptr();
        return analyzeIndexingExpr(x->expr, x->args, env);
    }

    case CALL : {
        Call *x = (Call *)expr.ptr();
        return analyzeCallExpr(x->expr, x->allArgs(), env);
    }

    case FIELD_REF : {
        FieldRef *x = (FieldRef *)expr.ptr();
        PValuePtr pv = analyzeOne(x->expr, env);
        if (!pv)
            return NULL;
        if (pv->type->typeKind == STATIC_TYPE) {
            StaticType *st = (StaticType *)pv->type.ptr();
            if (st->obj->objKind == MODULE_HOLDER) {
                ModuleHolder *mh = (ModuleHolder *)st->obj.ptr();
                ObjectPtr obj = safeLookupModuleHolder(mh, x->name);
                return analyzeStaticObject(obj);
            }
        }
        if (!x->desugared)
            x->desugared = desugarFieldRef(x);
        return analyzeExpr(x->desugared, env);
    }

    case STATIC_INDEXING : {
        StaticIndexing *x = (StaticIndexing *)expr.ptr();
        if (!x->desugared)
            x->desugared = desugarStaticIndexing(x);
        return analyzeExpr(x->desugared, env);
    }

    case UNARY_OP : {
        UnaryOp *x = (UnaryOp *)expr.ptr();
        if (x->op == ADDRESS_OF) {
            PValuePtr pv = analyzeOne(x->expr, env);
            if (!pv)
                return NULL;
            if (pv->isTemp)
                error("can't take address of a temporary");
        }
        if (!x->desugared)
            x->desugared = desugarUnaryOp(x);
        return analyzeExpr(x->desugared, env);
    }

    case BINARY_OP : {
        BinaryOp *x = (BinaryOp *)expr.ptr();
        if (!x->desugared)
            x->desugared = desugarBinaryOp(x);
        return analyzeExpr(x->desugared, env);
    }

    case EVAL_EXPR : {
        EvalExpr *eval = (EvalExpr *)expr.ptr();
        // XXX compilation context
        ExprListPtr evaled = desugarEvalExpr(eval, env);
        return analyzeMulti(evaled, env, 0);
    }

    case AND : {
        PValuePtr pv = new PValue(boolType, true);
        return new MultiPValue(pv);
    }

    case OR : {
        PValuePtr pv = new PValue(boolType, true);
        return new MultiPValue(pv);
    }

    case IF_EXPR : {
        IfExpr *x = (IfExpr *)expr.ptr();
        if (!x->desugared)
            x->desugared = desugarIfExpr(x);
        return analyzeExpr(x->desugared, env);
    }

    case LAMBDA : {
        Lambda *x = (Lambda *)expr.ptr();
        if (!x->initialized)
            initializeLambda(x, env);
        return analyzeExpr(x->converted, env);
    }

    case UNPACK : {
        error("incorrect usage of unpack operator");
        return NULL;
    }

    case STATIC_EXPR : {
        StaticExpr *x = (StaticExpr *)expr.ptr();
        ObjectPtr obj = evaluateOneStatic(x->expr, env);
        TypePtr t = staticType(obj);
        PValuePtr pv = new PValue(t, true);
        return new MultiPValue(pv);
    }

    case DISPATCH_EXPR : {
        error("incorrect usage of dispatch operator");
        return NULL;
    }

    case FOREIGN_EXPR : {
        ForeignExpr *x = (ForeignExpr *)expr.ptr();
        return analyzeExpr(x->expr, x->getEnv());
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
// analyzeStaticObject
//

MultiPValuePtr analyzeStaticObject(ObjectPtr x)
{
    switch (x->objKind) {

    case ENUM_MEMBER : {
        EnumMember *y = (EnumMember *)x.ptr();
        assert(y->type->typeKind == ENUM_TYPE);
        initializeEnumType((EnumType*)y->type.ptr());

        PValuePtr pv = new PValue(y->type, true);
        return new MultiPValue(pv);
    }

    case GLOBAL_VARIABLE : {
        GlobalVariable *y = (GlobalVariable *)x.ptr();
        if (y->hasParams()) {
            TypePtr t = staticType(x);
            PValuePtr pv = new PValue(t, true);
            return new MultiPValue(pv);
        }
        GVarInstancePtr inst = defaultGVarInstance(y);
        return analyzeGVarInstance(inst);
    }

    case EXTERNAL_VARIABLE : {
        ExternalVariable *y = (ExternalVariable *)x.ptr();
        PValuePtr pv = analyzeExternalVariable(y);
        return new MultiPValue(pv);
    }

    case EXTERNAL_PROCEDURE : {
        ExternalProcedure *y = (ExternalProcedure *)x.ptr();
        analyzeExternalProcedure(y);
        PValuePtr pv = new PValue(y->ptrType, true);
        return new MultiPValue(pv);
    }

    case GLOBAL_ALIAS : {
        GlobalAlias *y = (GlobalAlias *)x.ptr();
        if (y->hasParams()) {
            TypePtr t = staticType(x);
            PValuePtr pv = new PValue(t, true);
            return new MultiPValue(pv);
        }
        evaluateToplevelPredicate(y->patternVars, y->predicate, y->env);
        return analyzeExpr(y->expr, y->env);
    }

    case VALUE_HOLDER : {
        ValueHolder *y = (ValueHolder *)x.ptr();
        PValuePtr pv = new PValue(y->type, true);
        return new MultiPValue(pv);
    }

    case MULTI_STATIC : {
        MultiStatic *y = (MultiStatic *)x.ptr();
        MultiPValuePtr mpv = new MultiPValue();
        for (unsigned i = 0; i < y->size(); ++i) {
            TypePtr t = objectType(y->values[i]);
            mpv->values.push_back(new PValue(t, true));
        }
        return mpv;
    }

    case RECORD : {
        Record *y = (Record *)x.ptr();
        ObjectPtr z;
        if (y->params.empty() && !y->varParam)
            z = recordType(y, vector<ObjectPtr>()).ptr();
        else
            z = y;
        PValuePtr pv = new PValue(staticType(z), true);
        return new MultiPValue(pv);
    }

    case VARIANT : {
        Variant *y = (Variant *)x.ptr();
        ObjectPtr z;
        if (y->params.empty() && !y->varParam)
            z = variantType(y, vector<ObjectPtr>()).ptr();
        else
            z = y;
        PValuePtr pv = new PValue(staticType(z), true);
        return new MultiPValue(pv);
    }

    case TYPE :
    case PRIM_OP :
    case PROCEDURE :
    case MODULE_HOLDER :
    case IDENTIFIER : {
        TypePtr t = staticType(x);
        PValuePtr pv = new PValue(t, true);
        return new MultiPValue(pv);
    }

    case EVALUE : {
        EValue *y = (EValue *)x.ptr();
        PValuePtr pv = new PValue(y->type, y->forwardedRValue);
        return new MultiPValue(pv);
    }

    case MULTI_EVALUE : {
        MultiEValue *y = (MultiEValue *)x.ptr();
        MultiPValuePtr z = new MultiPValue();
        for (unsigned i = 0; i < y->values.size(); ++i) {
            EValue *ev = y->values[i].ptr();
            z->values.push_back(new PValue(ev->type, ev->forwardedRValue));
        }
        return z;
    }

    case CVALUE : {
        CValue *y = (CValue *)x.ptr();
        PValuePtr pv = new PValue(y->type, y->forwardedRValue);
        return new MultiPValue(pv);
    }

    case MULTI_CVALUE : {
        MultiCValue *y = (MultiCValue *)x.ptr();
        MultiPValuePtr z = new MultiPValue();
        for (unsigned i = 0; i < y->values.size(); ++i) {
            CValue *cv = y->values[i].ptr();
            z->values.push_back(new PValue(cv->type, cv->forwardedRValue));
        }
        return z;
    }

    case PVALUE : {
        PValue *y = (PValue *)x.ptr();
        return new MultiPValue(y);
    }

    case MULTI_PVALUE : {
        MultiPValue *y = (MultiPValue *)x.ptr();
        return y;
    }

    case PATTERN :
        error("pattern variable cannot be used as value");
        return NULL;

    default :
        invalidStaticObjectError(x);
        return NULL;

    }
}



//
// lookupGVarInstance, defaultGVarInstance
// analyzeGVarIndexing, analyzeGVarInstance
//

GVarInstancePtr lookupGVarInstance(GlobalVariablePtr x,
                                   const vector<ObjectPtr> &params)
{
    if (!x->instances)
        x->instances = new ObjectTable();
    ObjectPtr &y = x->instances->lookup(params);
    if (!y) {
        y = new GVarInstance(x, params);
    }
    return (GVarInstance *)y.ptr();
}

GVarInstancePtr defaultGVarInstance(GlobalVariablePtr x)
{
    return lookupGVarInstance(x, vector<ObjectPtr>());
}

GVarInstancePtr analyzeGVarIndexing(GlobalVariablePtr x,
                                    ExprListPtr args,
                                    EnvPtr env)
{
    assert(x->hasParams());
    MultiStaticPtr params = evaluateMultiStatic(args, env);
    if (x->varParam.ptr()) {
        if (params->size() < x->params.size())
            arityError2(x->params.size(), params->size());
    }
    else {
        ensureArity(params, x->params.size());
    }
    return lookupGVarInstance(x, params->values);
}

MultiPValuePtr analyzeGVarInstance(GVarInstancePtr x)
{
    if (x->analysis.ptr())
        return x->analysis;
    CompileContextPusher pusher(x->gvar.ptr(), x->params);
    if (x->analyzing) {
        updateAnalysisErrorLocation();
        return NULL;
    }
    GlobalVariablePtr gvar = x->gvar;
    if (!x->expr)
        x->expr = clone(gvar->expr);
    if (!x->env) {
        x->env = new Env(gvar->env);
        for (unsigned i = 0; i < gvar->params.size(); ++i) {
            addLocal(x->env, gvar->params[i], x->params[i]);
        }
        if (gvar->varParam.ptr()) {
            MultiStaticPtr varParams = new MultiStatic();
            for (unsigned i = gvar->params.size(); i < x->params.size(); ++i)
                varParams->add(x->params[i]);
            addLocal(x->env, gvar->varParam, varParams.ptr());
        }
    }
    x->analyzing = true;
    evaluateToplevelPredicate(x->gvar->patternVars, x->gvar->predicate, x->env);
    PValuePtr pv = analyzeOne(x->expr, x->env);
    x->analyzing = false;
    if (!pv)
        return NULL;
    x->analysis = new MultiPValue(new PValue(pv->type, false));
    x->type = pv->type;
    return x->analysis;
}



//
// analyzeExternalVariable
//

PValuePtr analyzeExternalVariable(ExternalVariablePtr x)
{
    if (!x->type2)
        x->type2 = evaluateType(x->type, x->env);
    return new PValue(x->type2, false);
}



//
// analyzeExternalProcedure
//

void analyzeExternalProcedure(ExternalProcedurePtr x)
{
    if (x->analyzed)
        return;
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
    x->ptrType = cCodePointerType(x->callingConv, argTypes,
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
    int callingConv = -1;
    x->attrAsmLabel = "";
    for (unsigned i = 0; i < x->attributes->size(); ++i) {
        ExprPtr expr = x->attributes->exprs[i];
        if (expr->exprKind == STRING_LITERAL) {
            StringLiteral *y = (StringLiteral *)expr.ptr();
            x->attrAsmLabel = y->value;
        }
        else {
            ObjectPtr obj = evaluateOneStatic(expr, x->env);
            if (obj->objKind != PRIM_OP)
                error(expr, "invalid external function attribute");
            PrimOp *y = (PrimOp *)obj.ptr();
            switch (y->primOpCode) {
            case PRIM_AttributeStdCall : {
                if (callingConv != -1)
                    error(expr, "cannot specify more than one calling convention");
                callingConv = CC_STDCALL;
                break;
            }
            case PRIM_AttributeFastCall : {
                if (callingConv != -1)
                    error(expr, "cannot specify more than one calling convention");
                callingConv = CC_FASTCALL;
                break;
            }
            case PRIM_AttributeThisCall : {
                if (callingConv != -1)
                    error(expr, "cannot specify more than one calling convention");
                callingConv = CC_THISCALL;
                break;
            }
            case PRIM_AttributeCCall : {
                if (callingConv != -1)
                    error(expr, "cannot specify more than one calling convention");
                callingConv = CC_DEFAULT;
                break;
            }
            case PRIM_AttributeLLVMCall : {
                if (callingConv != -1)
                    error(expr, "cannot specify more than one calling convention");
                callingConv = CC_LLVM;
                break;
            }
            case PRIM_AttributeDLLImport : {
                if (x->attrDLLExport)
                    error(expr, "dllimport specified after dllexport");
                x->attrDLLImport = true;
                break;
            }
            case PRIM_AttributeDLLExport : {
                if (x->attrDLLImport)
                    error(expr, "dllexport specified after dllimport");
                x->attrDLLExport = true;
                break;
            }
            default :
                error(expr, "invalid external function attribute");
            }
        }
    }
    if (callingConv == -1)
        callingConv = CC_DEFAULT;
    x->callingConv = (CallingConv)callingConv;
}

void verifyAttributes(ExternalVariablePtr var)
{
    assert(!var->attributesVerified);
    var->attributesVerified = true;
    var->attrDLLImport = false;
    var->attrDLLExport = false;
    for (unsigned i = 0; i < var->attributes->size(); ++i) {
        ExprPtr expr = var->attributes->exprs[i];
        ObjectPtr obj = evaluateOneStatic(expr, var->env);
        if (obj->objKind != PRIM_OP)
            error(expr, "invalid external variable attribute");
        PrimOp *y = (PrimOp *)obj.ptr();
        switch (y->primOpCode) {
        case PRIM_AttributeDLLImport : {
            if (var->attrDLLExport)
                error(expr, "dllimport specified after dllexport");
            var->attrDLLImport = true;
            break;
        }
        case PRIM_AttributeDLLExport : {
            if (var->attrDLLImport)
                error(expr, "dllexport specified after dllimport");
            var->attrDLLExport = true;
            break;
        }
        default :
            error(expr, "invalid external variable attribute");
        }
    }
}

void verifyAttributes(ModulePtr mod)
{
    assert(!mod->attributesVerified);
    mod->attributesVerified = true;

    if (mod->declaration != NULL && mod->declaration->attributes != NULL) {
        for (unsigned i = 0; i < mod->declaration->attributes->size(); ++i) {
            ExprPtr expr = mod->declaration->attributes->exprs[i];
            if (expr->exprKind == STRING_LITERAL) {
                StringLiteral *y = (StringLiteral *)expr.ptr();
                mod->attrBuildFlags.push_back(y->value);
            } else {
                ObjectPtr obj = evaluateOneStatic(expr, mod->env);
                if (obj->objKind == TYPE) {
                    Type *ty = (Type*)obj.ptr();
                    if (ty->typeKind == FLOAT_TYPE) {
                        mod->attrDefaultFloatType = (FloatType*)ty;
                        continue;
                    } else if (ty->typeKind == INTEGER_TYPE) {
                        mod->attrDefaultIntegerType = (IntegerType*)ty;
                        continue;
                    }
                }

                error(expr, "invalid module attribute");
            }
        }
    }
}



//
// analyzeIndexingExpr
//

static bool isTypeConstructor(ObjectPtr x) {
    if (x->objKind == PRIM_OP) {
        PrimOpPtr y = (PrimOp *)x.ptr();
        switch (y->primOpCode) {
        case PRIM_Pointer :
        case PRIM_CodePointer :
        case PRIM_CCodePointer :
        case PRIM_VarArgsCCodePointer :
        case PRIM_StdCallCodePointer :
        case PRIM_FastCallCodePointer :
        case PRIM_ThisCallCodePointer :
        case PRIM_LLVMCodePointer :
        case PRIM_Array :
        case PRIM_Vec :
        case PRIM_Tuple :
        case PRIM_Union :
        case PRIM_Static :
            return true;
        default :
            return false;
        }
    }
    else if (x->objKind == RECORD) {
        return true;
    }
    else if (x->objKind == VARIANT) {
        return true;
    }
    else {
        return false;
    }
}

MultiPValuePtr analyzeIndexingExpr(ExprPtr indexable,
                                   ExprListPtr args,
                                   EnvPtr env)
{
    PValuePtr pv = analyzeOne(indexable, env);
    if (!pv)
        return NULL;
    ObjectPtr obj = unwrapStaticType(pv->type);
    if (obj.ptr()) {
        if (isTypeConstructor(obj)) {
            MultiStaticPtr params = evaluateMultiStatic(args, env);
            PValuePtr out = analyzeTypeConstructor(obj, params);
            return new MultiPValue(out);
        }
        if (obj->objKind == GLOBAL_ALIAS) {
            GlobalAlias *x = (GlobalAlias *)obj.ptr();
            return analyzeAliasIndexing(x, args, env);
        }
        if (obj->objKind == GLOBAL_VARIABLE) {
            GlobalVariable *x = (GlobalVariable *)obj.ptr();
            GVarInstancePtr y = analyzeGVarIndexing(x, args, env);
            return analyzeGVarInstance(y);
        }
        if (obj->objKind != VALUE_HOLDER)
            error("invalid indexing operation");
    }
    ExprListPtr args2 = new ExprList(indexable);
    args2->add(args);
    return analyzeCallExpr(operator_expr_index(), args2, env);
}



//
// unwrapByRef
//

bool unwrapByRef(TypePtr &t)
{
    if (t->typeKind == RECORD_TYPE) {
        RecordType *rt = (RecordType *)t.ptr();
        if (rt->record.ptr() == primitive_ByRef().ptr()) {
            assert(rt->params.size() == 1);
            ObjectPtr obj = rt->params[0];
            if (obj->objKind != TYPE) {
                ostringstream ostr;
                ostr << "invalid return type: " << t;
                error(ostr.str());
            }
            t = (Type *)obj.ptr();
            return true;
        }
    }
    return false;
}



//
// constructType
//

TypePtr constructType(ObjectPtr constructor, MultiStaticPtr args)
{
    if (constructor->objKind == PRIM_OP) {
        PrimOpPtr x = (PrimOp *)constructor.ptr();

        switch (x->primOpCode) {

        case PRIM_Pointer : {
            ensureArity(args, 1);
            TypePtr pointeeType = staticToType(args, 0);
            return pointerType(pointeeType);
        }

        case PRIM_CodePointer : {
            ensureArity(args, 2);
            vector<TypePtr> argTypes;
            staticToTypeTuple(args, 0, argTypes);
            vector<TypePtr> returnTypes;
            staticToTypeTuple(args, 1, returnTypes);
            vector<bool> returnIsRef(returnTypes.size(), false);
            for (unsigned i = 0; i < returnTypes.size(); ++i)
                returnIsRef[i] = unwrapByRef(returnTypes[i]);
            return codePointerType(argTypes, returnIsRef, returnTypes);
        }

        case PRIM_CCodePointer :
        case PRIM_VarArgsCCodePointer :
        case PRIM_StdCallCodePointer :
        case PRIM_FastCallCodePointer :
        case PRIM_ThisCallCodePointer :
        case PRIM_LLVMCodePointer : {
            ensureArity(args, 2);
            vector<TypePtr> argTypes;
            staticToTypeTuple(args, 0, argTypes);
            vector<TypePtr> returnTypes;
            staticToTypeTuple(args, 1, returnTypes);
            if (returnTypes.size() > 1)
                argumentError(1, "C code cannot return more than one value");
            TypePtr returnType;
            if (returnTypes.size() == 1)
                returnType = returnTypes[0];
            CallingConv cc;
            switch (x->primOpCode) {
            case PRIM_CCodePointer :
            case PRIM_VarArgsCCodePointer :
                cc = CC_DEFAULT;
                break;
            case PRIM_StdCallCodePointer :
                cc = CC_STDCALL;
                break;
            case PRIM_FastCallCodePointer :
                cc = CC_FASTCALL;
                break;
            case PRIM_ThisCallCodePointer :
                cc = CC_FASTCALL;
                break;
            case PRIM_LLVMCodePointer :
                cc = CC_LLVM;
                break;
            default :
                assert(false);
            }
            bool hasVarArgs = (x->primOpCode == PRIM_VarArgsCCodePointer);
            return cCodePointerType(cc, argTypes, hasVarArgs, returnType);
        }

        case PRIM_Array : {
            ensureArity(args, 2);
            TypePtr t = staticToType(args, 0);
            int size = staticToInt(args, 1);
            return arrayType(t, size);
        }

        case PRIM_Vec : {
            ensureArity(args, 2);
            TypePtr t = staticToType(args, 0);
            int size = staticToInt(args, 1);
            return vecType(t, size);
        }

        case PRIM_Tuple : {
            vector<TypePtr> types;
            for (unsigned i = 0; i < args->size(); ++i)
                types.push_back(staticToType(args, i));
            return tupleType(types);
        }

        case PRIM_Union : {
            vector<TypePtr> types;
            for (unsigned i = 0; i < args->size(); ++i)
                types.push_back(staticToType(args, i));
            return unionType(types);
        }

        case PRIM_Static : {
            ensureArity(args, 1);
            return staticType(args->values[0]);
        }

        default :
            assert(false);
            return NULL;
        }
    }
    else if (constructor->objKind == RECORD) {
        RecordPtr x = (Record *)constructor.ptr();
        if (x->varParam.ptr()) {
            if (args->size() < x->params.size())
                arityError2(x->params.size(), args->size());
        }
        else {
            ensureArity(args, x->params.size());
        }
        return recordType(x, args->values);
    }
    else if (constructor->objKind == VARIANT) {
        VariantPtr x = (Variant *)constructor.ptr();
        if (x->varParam.ptr()) {
            if (args->size() < x->params.size())
                arityError2(x->params.size(), args->size());
        }
        else {
            ensureArity(args, x->params.size());
        }
        return variantType(x, args->values);
    }
    else {
        assert(false);
        return NULL;
    }
}



//
// analyzeTypeConstructor
//

PValuePtr analyzeTypeConstructor(ObjectPtr obj, MultiStaticPtr args)
{
    TypePtr t = constructType(obj, args);
    return staticPValue(t.ptr());
}



//
// analyzeAliasIndexing
//

MultiPValuePtr analyzeAliasIndexing(GlobalAliasPtr x,
                                    ExprListPtr args,
                                    EnvPtr env)
{
    assert(x->hasParams());
    MultiStaticPtr params = evaluateMultiStatic(args, env);
    if (x->varParam.ptr()) {
        if (params->size() < x->params.size())
            arityError2(x->params.size(), params->size());
    }
    else {
        ensureArity(params, x->params.size());
    }
    EnvPtr bodyEnv = new Env(x->env);
    for (unsigned i = 0; i < x->params.size(); ++i) {
        addLocal(bodyEnv, x->params[i], params->values[i]);
    }
    if (x->varParam.ptr()) {
        MultiStaticPtr varParams = new MultiStatic();
        for (unsigned i = x->params.size(); i < params->size(); ++i)
            varParams->add(params->values[i]);
        addLocal(bodyEnv, x->varParam, varParams.ptr());
    }
    evaluateToplevelPredicate(x->patternVars, x->predicate, bodyEnv);
    AnalysisCachingDisabler disabler;
    return analyzeExpr(x->expr, bodyEnv);
}



//
// computeArgsKey, analyzeReturn
//

void computeArgsKey(MultiPValuePtr args,
                    vector<TypePtr> &argsKey,
                    vector<ValueTempness> &argsTempness)
{
    for (unsigned i = 0; i < args->size(); ++i) {
        PValuePtr pv = args->values[i];
        argsKey.push_back(pv->type);
        argsTempness.push_back(pv->isTemp ? TEMPNESS_RVALUE : TEMPNESS_LVALUE);
    }
}

MultiPValuePtr analyzeReturn(const vector<bool> &returnIsRef,
                             const vector<TypePtr> &returnTypes)

{
    MultiPValuePtr x = new MultiPValue();
    for (unsigned i = 0; i < returnTypes.size(); ++i) {
        PValuePtr pv = new PValue(returnTypes[i], !returnIsRef[i]);
        x->values.push_back(pv);
    }
    return x.ptr();
}



//
// analyzeCallExpr
//

MultiPValuePtr analyzeCallExpr(ExprPtr callable,
                               ExprListPtr args,
                               EnvPtr env)
{
    PValuePtr pv = analyzeOne(callable, env);
    if (!pv)
        return NULL;
    switch (pv->type->typeKind) {
    case CODE_POINTER_TYPE :
        MultiPValuePtr mpv = analyzeMulti(args, env, 0);
        if (!mpv)
            return NULL;
        return analyzeCallPointer(pv, mpv);
    }
    ObjectPtr obj = unwrapStaticType(pv->type);
    if (!obj) {
        ExprListPtr args2 = new ExprList(callable);
        args2->add(args);
        return analyzeCallExpr(operator_expr_call(), args2, env);
    }

    switch (obj->objKind) {

    case TYPE :
    case RECORD :
    case VARIANT :
    case PROCEDURE :
    case PRIM_OP : {
        if ((obj->objKind == PRIM_OP) && !isOverloadablePrimOp(obj)) {
            PrimOpPtr x = (PrimOp *)obj.ptr();
            MultiPValuePtr mpv = analyzeMulti(args, env, 0);
            if (!mpv)
                return NULL;
            return analyzePrimOp(x, mpv);
        }
        vector<unsigned> dispatchIndices;
        MultiPValuePtr mpv = analyzeMultiArgs(args, env, dispatchIndices);
        if (!mpv)
            return NULL;
        if (dispatchIndices.size() > 0) {
            return analyzeDispatch(obj, mpv, dispatchIndices);
        }
        vector<TypePtr> argsKey;
        vector<ValueTempness> argsTempness;
        computeArgsKey(mpv, argsKey, argsTempness);
        CompileContextPusher pusher(obj, argsKey);
        InvokeEntryPtr entry =
            analyzeCallable(obj, argsKey, argsTempness);
        if (entry->callByName)
            return analyzeCallByName(entry, callable, args, env);
        if (!entry->analyzed)
            return NULL;
        return analyzeReturn(entry->returnIsRef, entry->returnTypes);
    }

    default :
        error("invalid call expression");
        return NULL;

    }
}



//
// analyzeDispatch
//

PValuePtr analyzeDispatchIndex(PValuePtr pv, int tag)
{
    MultiPValuePtr args = new MultiPValue();
    args->add(pv);
    ValueHolderPtr vh = intToValueHolder(tag);
    args->add(staticPValue(vh.ptr()));

    MultiPValuePtr out =
        analyzeCallValue(staticPValue(operator_dispatchIndex()), args);
    ensureArity(out, 1);
    return out->values[0];
}

MultiPValuePtr analyzeDispatch(ObjectPtr obj,
                               MultiPValuePtr args,
                               const vector<unsigned> &dispatchIndices)
{
    if (dispatchIndices.empty())
        return analyzeCallValue(staticPValue(obj), args);

    vector<TypePtr> argsKey;
    vector<ValueTempness> argsTempness;
    computeArgsKey(args, argsKey, argsTempness);
    CompileContextPusher pusher(obj, argsKey, dispatchIndices);

    unsigned index = dispatchIndices[0];
    vector<unsigned> dispatchIndices2(dispatchIndices.begin() + 1,
                                      dispatchIndices.end());
    MultiPValuePtr prefix = new MultiPValue();
    for (unsigned i = 0; i < index; ++i)
        prefix->add(args->values[i]);
    MultiPValuePtr suffix = new MultiPValue();
    for (unsigned i = index+1; i < args->size(); ++i)
        suffix->add(args->values[i]);
    PValuePtr pvDispatch = args->values[index];
    int memberCount = dispatchTagCount(pvDispatch->type);
    MultiPValuePtr result;
    vector<TypePtr> dispatchedTypes;
    for (unsigned i = 0; i < memberCount; ++i) {
        MultiPValuePtr args2 = new MultiPValue();
        args2->add(prefix);
        PValuePtr pvDispatch2 = analyzeDispatchIndex(pvDispatch, i);
        args2->add(pvDispatch2);
        args2->add(suffix);
        MultiPValuePtr result2 = analyzeDispatch(obj, args2, dispatchIndices2);
        if (!result2)
            return NULL;
        if (!result) {
            result = result2;
        }
        else {
            bool ok = true;
            if (result->size() != result2->size()) {
                ok = false;
            }
            else {
                for (unsigned j = 0; j < result2->size(); ++j) {
                    PValuePtr pv = result->values[j];
                    PValuePtr pv2 = result2->values[j];
                    if (pv->type != pv2->type) {
                        ok = false;
                        break;
                    }
                    if (pv->isTemp != pv2->isTemp) {
                        ok = false;
                        break;
                    }
                }
            }
            if (!ok) {
                ostringstream ostr;
                ostr << "mismatching result types with dispatch";
                ostr << "\n    expected ";
                for (unsigned j = 0; j < result->size(); ++j) {
                    if (j != 0) ostr << ", ";
                    ostr << result->values[j]->type;
                }
                ostr << "\n        from dispatching to ";
                for (unsigned j = 0; j < dispatchedTypes.size(); ++j) {
                    if (j != 0) ostr << ", ";
                    ostr << dispatchedTypes[j];
                }
                ostr << "\n     but got ";
                for (unsigned j = 0; j < result->size(); ++j) {
                    if (j != 0) ostr << ", ";
                    ostr << result2->values[j]->type;
                }
                ostr << "\n        when dispatching to " << pvDispatch2->type;
                argumentError(index, ostr.str());
            }
        }
        dispatchedTypes.push_back(pvDispatch2->type);
    }
    assert(result.ptr());
    return result;
}



//
// analyzeCallValue
//

MultiPValuePtr analyzeCallValue(PValuePtr callable,
                                MultiPValuePtr args)
{
    switch (callable->type->typeKind) {
    case CODE_POINTER_TYPE :
        return analyzeCallPointer(callable, args);
    }
    ObjectPtr obj = unwrapStaticType(callable->type);
    if (!obj) {
        MultiPValuePtr args2 = new MultiPValue(callable);
        args2->add(args);
        return analyzeCallValue(staticPValue(operator_call()), args2);
    }

    switch (obj->objKind) {

    case TYPE :
    case RECORD :
    case VARIANT :
    case PROCEDURE :
    case PRIM_OP : {
        if ((obj->objKind == PRIM_OP) && !isOverloadablePrimOp(obj)) {
            PrimOpPtr x = (PrimOp *)obj.ptr();
            return analyzePrimOp(x, args);
        }
        vector<TypePtr> argsKey;
        vector<ValueTempness> argsTempness;
        computeArgsKey(args, argsKey, argsTempness);
        CompileContextPusher pusher(obj, argsKey);
        InvokeEntryPtr entry =
            analyzeCallable(obj, argsKey, argsTempness);
        if (entry->callByName) {
            ExprListPtr objectExprs = new ExprList();
            for (vector<PValuePtr>::const_iterator i = args->values.begin();
                 i != args->values.end();
                 ++i)
            {
                objectExprs->add(new ObjectExpr(i->ptr()));
            }
            ExprPtr callableObject = new ObjectExpr(callable.ptr());
            callableObject->location = topLocation();
            return analyzeCallByName(entry, callableObject, objectExprs, new Env());
        } else if (!entry->analyzed)
            return NULL;
        else
            return analyzeReturn(entry->returnIsRef, entry->returnTypes);
    }

    default :
        error("invalid call expression");
        return NULL;

    }
}



//
// analyzeCallPointer
//

MultiPValuePtr analyzeCallPointer(PValuePtr x,
                                  MultiPValuePtr /*args*/)
{
    switch (x->type->typeKind) {

    case CODE_POINTER_TYPE : {
        CodePointerType *y = (CodePointerType *)x->type.ptr();
        return analyzeReturn(y->returnIsRef, y->returnTypes);
    }

    default :
        assert(false);
        return NULL;

    }
}



//
// analyzeIsDefined, analyzeCallable
//

bool analyzeIsDefined(ObjectPtr x,
                      const vector<TypePtr> &argsKey,
                      const vector<ValueTempness> &argsTempness)
{
    MatchFailureError failures;
    InvokeEntryPtr entry = lookupInvokeEntry(x, argsKey, argsTempness, failures);

    if (!entry || !entry->code->hasBody())
        return false;
    return true;
}

InvokeEntryPtr analyzeCallable(ObjectPtr x,
                               const vector<TypePtr> &argsKey,
                               const vector<ValueTempness> &argsTempness)
{
    MatchFailureError failures;
    InvokeEntryPtr entry = lookupInvokeEntry(x, argsKey, argsTempness, failures);

    if (!entry || !entry->code->hasBody()) {
        matchFailureError(failures);
    }
    if (entry->analyzed)
        return entry;
    if (entry->analyzing) {
        updateAnalysisErrorLocation();
        return entry;
    }
    if (entry->callByName) {
        entry->code = clone(entry->origCode);
        return entry;
    }

    entry->analyzing = true;
    analyzeCodeBody(entry);
    entry->analyzing = false;

    return entry;
}



//
// analyzeCallByName
//

MultiPValuePtr analyzeCallByName(InvokeEntryPtr entry,
                                 ExprPtr callable,
                                 ExprListPtr args,
                                 EnvPtr env)
{
    assert(entry->callByName);

    CodePtr code = entry->code;
    assert(code->body.ptr());

    if (code->hasReturnSpecs()) {
        vector<bool> returnIsRef;
        vector<TypePtr> returnTypes;
        evaluateReturnSpecs(code->returnSpecs, code->varReturnSpec,
                            entry->env, returnIsRef, returnTypes);
        return analyzeReturn(returnIsRef, returnTypes);
    }

    if (entry->varArgName.ptr())
        assert(args->size() >= entry->fixedArgNames.size());
    else
        assert(args->size() == entry->fixedArgNames.size());

    EnvPtr bodyEnv = new Env(entry->env);
    bodyEnv->callByNameExprHead = callable;

    for (unsigned i = 0; i < entry->fixedArgNames.size(); ++i) {
        ExprPtr expr = foreignExpr(env, args->exprs[i]);
        addLocal(bodyEnv, entry->fixedArgNames[i], expr.ptr());
    }

    if (entry->varArgName.ptr()) {
        ExprListPtr varArgs = new ExprList();
        for (unsigned i = entry->fixedArgNames.size(); i < args->size(); ++i) {
            ExprPtr expr = foreignExpr(env, args->exprs[i]);
            varArgs->add(expr);
        }
        addLocal(bodyEnv, entry->varArgName, varArgs.ptr());
    }

    AnalysisContextPtr ctx = new AnalysisContext();
    StatementAnalysis sa = analyzeStatement(code->body, bodyEnv, ctx);
    if ((sa == SA_RECURSIVE) && (!ctx->returnInitialized))
        return NULL;
    if (ctx->returnInitialized) {
        return analyzeReturn(ctx->returnIsRef, ctx->returnTypes);
    }
    else if ((sa == SA_TERMINATED) && ctx->hasRecursivePropagation) {
        analysisError();
        return NULL;
    }
    else {
        // assume void return (zero return values)
        return new MultiPValue();
    }
}



//
// analyzeCodeBody
//

static void unifyInterfaceReturns(InvokeEntryPtr entry)
{
    if (entry->parent->interface == NULL)
        return;

    CodePtr interfaceCode = entry->parent->interface->code;
    if (!interfaceCode->returnSpecsDeclared)
        return;

    vector<bool> interfaceReturnIsRef;
    vector<TypePtr> interfaceReturnTypes;

    evaluateReturnSpecs(interfaceCode->returnSpecs,
                        interfaceCode->varReturnSpec,
                        entry->interfaceEnv,
                        interfaceReturnIsRef, interfaceReturnTypes);
    if (entry->returnTypes.size() != interfaceReturnTypes.size()) {
        ostringstream out;
        out << "interface declares " << interfaceReturnTypes.size()
            << " arguments, but got " << entry->returnTypes.size() << "\n"
            << "    interface at ";
        printFileLineCol(out, entry->parent->interface->location);
        error(entry->code, out.str());
    }
    for (unsigned i = 0; i < interfaceReturnTypes.size(); ++i) {
        if (interfaceReturnTypes[i] != entry->returnTypes[i]) {
            ostringstream out;
            out << "return value " << i+1 << ": "
                << "interface declares type " << interfaceReturnTypes[i]
                << ", but got type " << entry->returnTypes[i] << "\n"
                << "    interface at ";
            printFileLineCol(out, entry->parent->interface->location);
            error(entry->code, out.str());
        }
        if (interfaceReturnIsRef[i] && !entry->returnIsRef[i]) {
            ostringstream out;
            out << "return value " << i+1 << ": "
                << "interface declares return by reference, but got return by value\n"
                << "    interface at ";
            printFileLineCol(out, entry->parent->interface->location);
            error(entry->code, out.str());
        }
    }
}

void analyzeCodeBody(InvokeEntryPtr entry)
{
    assert(!entry->analyzed);

    CodePtr code = entry->code;
    assert(code->hasBody());

    if (code->isLLVMBody() || code->hasReturnSpecs()) {
        evaluateReturnSpecs(code->returnSpecs, code->varReturnSpec,
                            entry->env,
                            entry->returnIsRef, entry->returnTypes);
        entry->analyzed = true;
        return;
    }

    EnvPtr bodyEnv = new Env(entry->env);

    for (unsigned i = 0; i < entry->fixedArgNames.size(); ++i) {
        bool flag = entry->forwardedRValueFlags[i];
        PValuePtr parg = new PValue(entry->fixedArgTypes[i], flag);
        addLocal(bodyEnv, entry->fixedArgNames[i], parg.ptr());
    }

    if (entry->varArgName.ptr()) {
        unsigned nFixed = entry->fixedArgNames.size();
        MultiPValuePtr varArgs = new MultiPValue();
        for (unsigned i = 0; i < entry->varArgTypes.size(); ++i) {
            bool flag = entry->forwardedRValueFlags[i + nFixed];
            PValuePtr parg = new PValue(entry->varArgTypes[i], flag);
            varArgs->values.push_back(parg);
        }
        addLocal(bodyEnv, entry->varArgName, varArgs.ptr());
    }

    AnalysisContextPtr ctx = new AnalysisContext();
    StatementAnalysis sa = analyzeStatement(code->body, bodyEnv, ctx);
    if ((sa == SA_RECURSIVE) && (!ctx->returnInitialized))
        return;
    if (ctx->returnInitialized) {
        entry->returnIsRef = ctx->returnIsRef;
        entry->returnTypes = ctx->returnTypes;
    }
    else if ((sa == SA_TERMINATED) && ctx->hasRecursivePropagation) {
        analysisError();
    }

    unifyInterfaceReturns(entry);

    entry->analyzed = true;
}



//
// analyzeStatement
//

static StatementAnalysis combineStatementAnalysis(StatementAnalysis a,
                                                  StatementAnalysis b)
{
    if ((a == SA_FALLTHROUGH) || (b == SA_FALLTHROUGH))
        return SA_FALLTHROUGH;
    if ((a == SA_RECURSIVE) && (b == SA_RECURSIVE))
        return SA_RECURSIVE;
    if ((a == SA_TERMINATED) && (b == SA_TERMINATED))
        return SA_TERMINATED;
    if ((a == SA_RECURSIVE) && (b == SA_TERMINATED))
        return SA_TERMINATED;
    assert((a == SA_TERMINATED) && (b == SA_RECURSIVE));
    return SA_TERMINATED;
}

StatementAnalysis analyzeStatement(StatementPtr stmt, EnvPtr env, AnalysisContextPtr ctx);

static StatementAnalysis analyzeBlockStatement(StatementPtr stmt, EnvPtr &env, AnalysisContextPtr ctx)
{
    if (stmt->stmtKind == BINDING) {
        env = analyzeBinding((Binding *)stmt.ptr(), env);
        if (!env) {
            ctx->hasRecursivePropagation = true;
            return SA_RECURSIVE;
        }
        return SA_FALLTHROUGH;
    } else if (stmt->stmtKind == EVAL_STATEMENT) {
        EvalStatement *eval = (EvalStatement*)stmt.ptr();
        vector<StatementPtr> const &evaled = desugarEvalStatement(eval, env);
        for (unsigned i = 0; i < evaled.size(); ++i) {
            StatementAnalysis sa = analyzeBlockStatement(evaled[i], env, ctx);
            if (sa != SA_FALLTHROUGH)
                return sa;
        }
        return SA_FALLTHROUGH;
    } else
        return analyzeStatement(stmt, env, ctx);
}

StatementAnalysis analyzeStatement(StatementPtr stmt, EnvPtr env, AnalysisContextPtr ctx)
{
    LocationContext loc(stmt->location);

    switch (stmt->stmtKind) {

    case BLOCK : {
        Block *block = (Block *)stmt.ptr();
        if (!block->desugared)
            block->desugared = desugarBlock(block);
        for (unsigned i = 0; i < block->desugared->statements.size(); ++i) {
            StatementAnalysis sa = analyzeBlockStatement(block->desugared->statements[i], env, ctx);
            if (sa != SA_FALLTHROUGH)
                return sa;
        }
        return SA_FALLTHROUGH;
    }

    case LABEL :
    case BINDING :
    case ASSIGNMENT :
    case INIT_ASSIGNMENT :
    case UPDATE_ASSIGNMENT :
        return SA_FALLTHROUGH;

    case GOTO :
        return SA_TERMINATED;

    case RETURN : {
        Return *x = (Return *)stmt.ptr();
        MultiPValuePtr mpv = analyzeMulti(x->values, env, 0);
        if (!mpv) {
            ctx->hasRecursivePropagation = true;
            return SA_RECURSIVE;
        }
        if (ctx->returnInitialized) {
            ensureArity(mpv, ctx->returnTypes.size());
            for (unsigned i = 0; i < mpv->size(); ++i) {
                PValuePtr pv = mpv->values[i];
                bool byRef = returnKindToByRef(x->returnKind, pv);
                if (ctx->returnTypes[i] != pv->type)
                    argumentTypeError(i, ctx->returnTypes[i], pv->type);
                if (byRef != ctx->returnIsRef[i])
                    argumentError(i, "mismatching by-ref and "
                                  "by-value returns");
                if (byRef && pv->isTemp)
                    argumentError(i, "cannot return a temporary by reference");
            }
        }
        else {
            ctx->returnIsRef.clear();
            ctx->returnTypes.clear();
            for (unsigned i = 0; i < mpv->size(); ++i) {
                PValuePtr pv = mpv->values[i];
                bool byRef = returnKindToByRef(x->returnKind, pv);
                ctx->returnIsRef.push_back(byRef);
                if (byRef && pv->isTemp)
                    argumentError(i, "cannot return a temporary by reference");
                ctx->returnTypes.push_back(pv->type);
            }
            ctx->returnInitialized = true;
        }
        return SA_TERMINATED;
    }

    case IF : {
        If *x = (If *)stmt.ptr();
        StatementAnalysis thenResult, elseResult;
        thenResult = analyzeStatement(x->thenPart, env, ctx);
        elseResult = SA_FALLTHROUGH;
        if (x->elsePart.ptr())
            elseResult = analyzeStatement(x->elsePart, env, ctx);
        return combineStatementAnalysis(thenResult, elseResult);
    }

    case SWITCH : {
        Switch *x = (Switch *)stmt.ptr();
        if (!x->desugared)
            x->desugared = desugarSwitchStatement(x);
        return analyzeStatement(x->desugared, env, ctx);
    }

    case EVAL_STATEMENT : {
        EvalStatement *eval = (EvalStatement*)stmt.ptr();
        vector<StatementPtr> const &evaled = desugarEvalStatement(eval, env);
        for (unsigned i = 0; i < evaled.size(); ++i) {
            StatementAnalysis sa = analyzeStatement(evaled[i], env, ctx);
            if (sa != SA_FALLTHROUGH)
                return sa;
        }
        return SA_FALLTHROUGH;
    }

    case EXPR_STATEMENT :
        return SA_FALLTHROUGH;

    case WHILE : {
        While *x = (While *)stmt.ptr();
        analyzeStatement(x->body, env, ctx);
        return SA_FALLTHROUGH;
    }

    case BREAK :
    case CONTINUE :
        return SA_TERMINATED;

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
        if (!exceptionsEnabled())
            return analyzeStatement(x->tryBlock, env, ctx);
        if (!x->desugaredCatchBlock)
            x->desugaredCatchBlock = desugarCatchBlocks(x->catchBlocks);
        StatementAnalysis result1, result2;
        result1 = analyzeStatement(x->tryBlock, env, ctx);
        result2 = analyzeStatement(x->desugaredCatchBlock, env, ctx);
        return combineStatementAnalysis(result1, result2);
    }

    case THROW :
        return SA_TERMINATED;

    case STATIC_FOR : {
        StaticFor *x = (StaticFor *)stmt.ptr();
        MultiPValuePtr mpv = analyzeMulti(x->values, env, 2);
        if (!mpv) {
            ctx->hasRecursivePropagation = true;
            return SA_RECURSIVE;
        }
        initializeStaticForClones(x, mpv->size());
        for (unsigned i = 0; i < mpv->size(); ++i) {
            EnvPtr env2 = new Env(env);
            addLocal(env2, x->variable, mpv->values[i].ptr());
            StatementAnalysis sa;
            sa = analyzeStatement(x->clonedBodies[i], env2, ctx);
            if (sa != SA_FALLTHROUGH)
                return sa;
        }
        return SA_FALLTHROUGH;
    }

    case FINALLY :
    case ONERROR :
        return SA_FALLTHROUGH;

    case UNREACHABLE :
        return SA_TERMINATED;

    default :
        assert(false);
        return SA_FALLTHROUGH;
    }
}

void initializeStaticForClones(StaticForPtr x, unsigned count)
{
    if (x->clonesInitialized) {
        assert(count == x->clonedBodies.size());
    }
    else {
        if (analysisCachingDisabled == 0)
            x->clonesInitialized = true;
        x->clonedBodies.clear();
        for (unsigned i = 0; i < count; ++i)
            x->clonedBodies.push_back(clone(x->body));
    }
}

EnvPtr analyzeBinding(BindingPtr x, EnvPtr env)
{
    LocationContext loc(x->location);

    switch (x->bindingKind) {

    case VAR :
    case REF :
    case FORWARD : {
        MultiPValuePtr right = analyzeMulti(x->values, env, x->names.size());
        if (!right)
            return NULL;
        if (right->size() != x->names.size())
            arityMismatchError(x->names.size(), right->size());
        EnvPtr env2 = new Env(env);
        for (unsigned i = 0; i < right->size(); ++i) {
            PValuePtr pv = right->values[i];
            addLocal(env2, x->names[i], new PValue(pv->type, false));
        }
        return env2;
    }

    case ALIAS : {
        ensureArity(x->names, 1);
        ensureArity(x->values->exprs, 1);
        EnvPtr env2 = new Env(env);
        ExprPtr y = foreignExpr(env, x->values->exprs[0]);
        addLocal(env2, x->names[0], y.ptr());
        return env2;
    }

    default :
        assert(false);
        return NULL;

    }
}

bool returnKindToByRef(ReturnKind returnKind, PValuePtr pv)
{
    switch (returnKind) {
    case RETURN_VALUE : return false;
    case RETURN_REF : return true;
    case RETURN_FORWARD : return !pv->isTemp;
    default : assert(false); return false;
    }
}



//
// analyzePrimOp
//

static std::pair<vector<TypePtr>, InvokeEntryPtr>
invokeEntryForCallableArguments(MultiPValuePtr args)
{
    if (args->size() < 1)
        arityError2(1, args->size());
    ObjectPtr callable = unwrapStaticType(args->values[0]->type);
    if (!callable)
        argumentError(0, "static callable expected");
    switch (callable->objKind) {
    case TYPE :
    case RECORD :
    case VARIANT :
    case PROCEDURE :
        break;
    case PRIM_OP :
        if (!isOverloadablePrimOp(callable))
            argumentError(0, "invalid callable");
        break;
    default :
        argumentError(0, "invalid callable");
    }
    vector<TypePtr> argsKey;
    vector<ValueTempness> argsTempness;
    for (unsigned i = 1; i < args->size(); ++i) {
        TypePtr t = valueToType(args, i);
        argsKey.push_back(t);
        argsTempness.push_back(TEMPNESS_LVALUE);
    }

    CompileContextPusher pusher(callable, argsKey);

    return std::make_pair(
        argsKey,
        analyzeCallable(callable, argsKey, argsTempness));
}

MultiPValuePtr analyzePrimOp(PrimOpPtr x, MultiPValuePtr args)
{
    switch (x->primOpCode) {

    case PRIM_TypeP :
        return new MultiPValue(new PValue(boolType, true));

    case PRIM_TypeSize :
        return new MultiPValue(new PValue(cSizeTType, true));

    case PRIM_TypeAlignment :
        return new MultiPValue(new PValue(cSizeTType, true));

    case PRIM_StaticCallDefinedP :
        return new MultiPValue(new PValue(boolType, true));

    case PRIM_StaticCallOutputTypes : {
        std::pair<vector<TypePtr>, InvokeEntryPtr> entry =
            invokeEntryForCallableArguments(args);
        MultiPValuePtr values = new MultiPValue();
        for (size_t i = 0; i < entry.second->returnTypes.size(); ++i)
            values->add(staticPValue(entry.second->returnTypes[i].ptr()));
        return values;
    }

    case PRIM_StaticMonoP : {
        return new MultiPValue(new PValue(boolType, true));
    }

    case PRIM_StaticMonoInputTypes : {
        ensureArity(args, 1);
        ObjectPtr callable = unwrapStaticType(args->values[0]->type);
        if (!callable)
            argumentError(0, "static callable expected");

        switch (callable->objKind) {
        case PROCEDURE : {
            Procedure *proc = (Procedure*)callable.ptr();
            if (proc->mono.monoState != Procedure_MonoOverload) {
                argumentError(0, "not a static monomorphic callable");
            }

            MultiPValuePtr values = new MultiPValue();
            for (size_t i = 0; i < proc->mono.monoTypes.size(); ++i)
                values->add(staticPValue(proc->mono.monoTypes[i].ptr()));

            return values;
        }

        default :
            argumentError(0, "not a static monomorphic callable");
        }
    }

    case PRIM_bitcopy :
        return new MultiPValue();

    case PRIM_boolNot :
        return new MultiPValue(new PValue(boolType, true));

    case PRIM_numericEqualsP :
        return new MultiPValue(new PValue(boolType, true));

    case PRIM_numericLesserP :
        return new MultiPValue(new PValue(boolType, true));

    case PRIM_numericAdd :
    case PRIM_numericSubtract :
    case PRIM_numericMultiply :
    case PRIM_numericDivide : {
        ensureArity(args, 2);
        TypePtr t = numericTypeOfValue(args, 0);
        return new MultiPValue(new PValue(t, true));
    }

    case PRIM_numericNegate : {
        ensureArity(args, 1);
        TypePtr t = numericTypeOfValue(args, 0);
        return new MultiPValue(new PValue(t, true));
    }

    case PRIM_integerRemainder :
    case PRIM_integerShiftLeft :
    case PRIM_integerShiftRight :
    case PRIM_integerBitwiseAnd :
    case PRIM_integerBitwiseOr :
    case PRIM_integerBitwiseXor :
    case PRIM_integerAddChecked :
    case PRIM_integerSubtractChecked :
    case PRIM_integerMultiplyChecked :
    case PRIM_integerDivideChecked :
    case PRIM_integerRemainderChecked :
    case PRIM_integerShiftLeftChecked : {
        ensureArity(args, 2);
        IntegerTypePtr t = integerTypeOfValue(args, 0);
        return new MultiPValue(new PValue(t.ptr(), true));
    }

    case PRIM_integerBitwiseNot :
    case PRIM_integerNegateChecked : {
        ensureArity(args, 1);
        IntegerTypePtr t = integerTypeOfValue(args, 0);
        return new MultiPValue(new PValue(t.ptr(), true));
    }

    case PRIM_numericConvert : {
        ensureArity(args, 2);
        TypePtr t = valueToNumericType(args, 0);
        return new MultiPValue(new PValue(t, true));
    }

    case PRIM_integerConvertChecked : {
        ensureArity(args, 2);
        IntegerTypePtr t = valueToIntegerType(args, 0);
        return new MultiPValue(new PValue(t.ptr(), true));
    }

    case PRIM_Pointer :
        error("Pointer type constructor cannot be called");

    case PRIM_addressOf : {
        ensureArity(args, 1);
        TypePtr t = pointerType(args->values[0]->type);
        return new MultiPValue(new PValue(t, true));
    }

    case PRIM_pointerDereference : {
        ensureArity(args, 1);
        PointerTypePtr pt = pointerTypeOfValue(args, 0);
        return new MultiPValue(new PValue(pt->pointeeType, false));
    }

    case PRIM_pointerEqualsP :
    case PRIM_pointerLesserP :
        return new MultiPValue(new PValue(boolType, true));

    case PRIM_pointerOffset : {
        ensureArity(args, 2);
        PointerTypePtr pt = pointerTypeOfValue(args, 0);
        return new MultiPValue(new PValue(pt.ptr(), true));
    }

    case PRIM_pointerToInt : {
        ensureArity(args, 2);
        IntegerTypePtr t = valueToIntegerType(args, 0);
        return new MultiPValue(new PValue(t.ptr(), true));
    }

    case PRIM_intToPointer : {
        ensureArity(args, 2);
        TypePtr t = valueToType(args, 0);
        return new MultiPValue(new PValue(pointerType(t), true));
    }

    case PRIM_CodePointer :
        error("CodePointer type constructor cannot be called");

    case PRIM_makeCodePointer : {
        std::pair<vector<TypePtr>, InvokeEntryPtr> entry =
            invokeEntryForCallableArguments(args);
        if (entry.second->callByName)
            argumentError(0, "cannot create pointer to call-by-name code");
        if (!entry.second->analyzed)
            return NULL;
        TypePtr cpType = codePointerType(entry.first,
                                         entry.second->returnIsRef,
                                         entry.second->returnTypes);
        return new MultiPValue(new PValue(cpType, true));
    }

    case PRIM_CCodePointerP :
        return new MultiPValue(new PValue(boolType, true));

    case PRIM_CCodePointer :
        error("CCodePointer type constructor cannot be called");

    case PRIM_VarArgsCCodePointer :
        error("VarArgsCCodePointer type constructor cannot be called");

    case PRIM_StdCallCodePointer :
        error("StdCallCodePointer type constructor cannot be called");

    case PRIM_FastCallCodePointer :
        error("FastCallCodePointer type constructor cannot be called");

    case PRIM_ThisCallCodePointer :
        error("ThisCallCodePointer type constructor cannot be called");

    case PRIM_LLVMCodePointer :
        error("LLVMCodePointer type constructor cannot be called");

    case PRIM_makeCCodePointer : {
        std::pair<vector<TypePtr>, InvokeEntryPtr> entry =
            invokeEntryForCallableArguments(args);
        if (entry.second->callByName)
            argumentError(0, "cannot create pointer to call-by-name code");
        if (!entry.second->analyzed)
            return NULL;
        TypePtr returnType;
        if (entry.second->returnTypes.empty()) {
            returnType = NULL;
        }
        else if (entry.second->returnTypes.size() == 1) {
            if (entry.second->returnIsRef[0]) {
                argumentError(0, "cannot create c-code pointer to "
                              " return-by-reference code");
            }
            returnType = entry.second->returnTypes[0];
        }
        else {
            argumentError(0, "cannot create c-code pointer to "
                          "multi-return code");
        }
        TypePtr ccpType = cCodePointerType(CC_DEFAULT,
                                           entry.first,
                                           false,
                                           returnType);
        return new MultiPValue(new PValue(ccpType, true));
    }

    case PRIM_callCCodePointer : {
        if (args->size() < 1)
            arityError2(1, args->size());
        CCodePointerTypePtr y = cCodePointerTypeOfValue(args, 0);
        if (!y->returnType)
            return new MultiPValue();
        return new MultiPValue(new PValue(y->returnType, true));
    }

    case PRIM_bitcast : {
        ensureArity(args, 2);
        TypePtr t = valueToType(args, 0);
        return new MultiPValue(new PValue(t, false));
    }

    case PRIM_Array :
        error("Array type constructor cannot be called");

    case PRIM_arrayRef : {
        ensureArity(args, 2);
        ArrayTypePtr t = arrayTypeOfValue(args, 0);
        return new MultiPValue(new PValue(t->elementType, false));
    }

    case PRIM_arrayElements: {
        ensureArity(args, 1);
        ArrayTypePtr t = arrayTypeOfValue(args, 0);
        MultiPValuePtr mpv = new MultiPValue();
        for (unsigned i = 0; i < (unsigned)t->size; ++i)
            mpv->add(new PValue(t->elementType, false));
        return mpv;
    }

    case PRIM_Vec :
        error("Vec type constructor cannot be called");

    case PRIM_Tuple :
        error("Tuple type constructor cannot be called");

    case PRIM_TupleElementCount :
        return new MultiPValue(new PValue(cSizeTType, true));

    case PRIM_tupleRef : {
        ensureArity(args, 2);
        TupleTypePtr t = tupleTypeOfValue(args, 0);
        ObjectPtr obj = unwrapStaticType(args->values[1]->type);
        size_t i = 0;
        if (!obj || !staticToSizeTOrInt(obj, i))
            argumentError(1, "expecting static SizeT or Int value");
        if (i >= t->elementTypes.size())
            argumentIndexRangeError(1, "tuple element index",
                                    i, t->elementTypes.size());
        return new MultiPValue(new PValue(t->elementTypes[i], false));
    }

    case PRIM_tupleElements : {
        ensureArity(args, 1);
        TupleTypePtr t = tupleTypeOfValue(args, 0);
        MultiPValuePtr mpv = new MultiPValue();
        for (unsigned i = 0; i < t->elementTypes.size(); ++i)
            mpv->add(new PValue(t->elementTypes[i], false));
        return mpv;
    }

    case PRIM_Union :
        error("Union type constructor cannot be called");

    case PRIM_UnionMemberCount :
        return new MultiPValue(new PValue(cSizeTType, true));

    case PRIM_RecordP :
        return new MultiPValue(new PValue(boolType, true));

    case PRIM_RecordFieldCount :
        return new MultiPValue(new PValue(cSizeTType, true));

    case PRIM_RecordFieldName : {
        ensureArity(args, 2);
        ObjectPtr first = unwrapStaticType(args->values[0]->type);
        if (!first)
            argumentError(0, "expecting a record type");
        TypePtr t;
        if (!staticToType(first, t))
            argumentError(0, "expecting a record type");
        if (t->typeKind != RECORD_TYPE)
            argumentError(0, "expecting a record type");
        RecordType *rt = (RecordType *)t.ptr();
        ObjectPtr second = unwrapStaticType(args->values[1]->type);
        size_t i = 0;
        if (!second || !staticToSizeTOrInt(second, i))
            argumentError(1, "expecting static SizeT or Int value");
        const vector<IdentifierPtr> fieldNames = recordFieldNames(rt);
        if (i >= fieldNames.size())
            argumentIndexRangeError(1, "record field index",
                                    i, fieldNames.size());
        return new MultiPValue(staticPValue(fieldNames[i].ptr()));
    }

    case PRIM_RecordWithFieldP :
        return new MultiPValue(new PValue(boolType, true));

    case PRIM_recordFieldRef : {
        ensureArity(args, 2);
        RecordTypePtr t = recordTypeOfValue(args, 0);
        ObjectPtr obj = unwrapStaticType(args->values[1]->type);
        size_t i = 0;
        if (!obj || !staticToSizeTOrInt(obj, i))
            argumentError(1, "expecting static SizeT or Int value");
        const vector<TypePtr> &fieldTypes = recordFieldTypes(t);
        if (i >= fieldTypes.size())
            argumentIndexRangeError(1, "record field index",
                                    i, fieldTypes.size());
        return new MultiPValue(new PValue(fieldTypes[i], false));
    }

    case PRIM_recordFieldRefByName : {
        ensureArity(args, 2);
        RecordTypePtr t = recordTypeOfValue(args, 0);
        ObjectPtr obj = unwrapStaticType(args->values[1]->type);
        if (!obj || (obj->objKind != IDENTIFIER))
            argumentError(1, "expecting field name identifier");
        IdentifierPtr fname = (Identifier *)obj.ptr();
        const map<string, size_t> &fieldIndexMap = recordFieldIndexMap(t);
        map<string, size_t>::const_iterator fi =
            fieldIndexMap.find(fname->str);
        if (fi == fieldIndexMap.end()) {
            ostringstream sout;
            sout << "field not found: " << fname->str;
            argumentError(1, sout.str());
        }
        const vector<TypePtr> &fieldTypes = recordFieldTypes(t);
        return new MultiPValue(new PValue(fieldTypes[fi->second], false));
    }

    case PRIM_recordFields : {
        ensureArity(args, 1);
        RecordTypePtr t = recordTypeOfValue(args, 0);
        const vector<TypePtr> &fieldTypes = recordFieldTypes(t);
        MultiPValuePtr mpv = new MultiPValue();
        for (unsigned i = 0; i < fieldTypes.size(); ++i)
            mpv->add(new PValue(fieldTypes[i], false));
        return mpv;
    }

    case PRIM_VariantP :
        return new MultiPValue(new PValue(boolType, true));

    case PRIM_VariantMemberIndex :
        return new MultiPValue(new PValue(cSizeTType, true));

    case PRIM_VariantMemberCount :
        return new MultiPValue(new PValue(cSizeTType, true));

    case PRIM_VariantMembers : {
        ensureArity(args, 1);
        ObjectPtr typeObj = unwrapStaticType(args->values[0]->type);
        if (!typeObj)
            argumentError(0, "expecting a variant type");
        TypePtr t;
        if (!staticToType(typeObj, t))
            argumentError(0, "expecting a variant type");
        if (t->typeKind != VARIANT_TYPE)
            argumentError(0, "expecting a variant type");
        VariantType *vt = (VariantType *)t.ptr();
        const vector<TypePtr> &members = variantMemberTypes(vt);

        MultiPValuePtr mpv = new MultiPValue();
        for (vector<TypePtr>::const_iterator i = members.begin(), end = members.end();
             i != end;
             ++i)
        {
            mpv->add(staticPValue(i->ptr()));
        }
        return mpv;
    }

    case PRIM_variantRepr : {
        ensureArity(args, 1);
        VariantTypePtr t = variantTypeOfValue(args, 0);
        TypePtr reprType = variantReprType(t);
        return new MultiPValue(new PValue(reprType, false));
    }

    case PRIM_Static :
        error("Static type constructor cannot be called");

    case PRIM_ModuleName : {
        ensureArity(args, 1);
        ObjectPtr obj = unwrapStaticType(args->values[0]->type);
        if (!obj)
            argumentError(0, "expecting static object");
        ModulePtr m = staticModule(obj);
        if (!m)
            argumentError(0, "value has no associated module");
        ExprPtr z = new StringLiteral(m->moduleName);
        return analyzeExpr(z, new Env());
    }

    case PRIM_StaticName : {
        ensureArity(args, 1);
        ObjectPtr obj = unwrapStaticType(args->values[0]->type);
        if (!obj)
            argumentError(0, "expecting static object");
        ostringstream sout;
        printStaticName(sout, obj);
        ExprPtr z = new StringLiteral(sout.str());
        return analyzeExpr(z, new Env());
    }

    case PRIM_staticIntegers : {
        ensureArity(args, 1);
        ObjectPtr obj = unwrapStaticType(args->values[0]->type);
        if (!obj || (obj->objKind != VALUE_HOLDER))
            argumentError(0, "expecting a static SizeT or Int value");
        MultiPValuePtr mpv = new MultiPValue();
        ValueHolder *vh = (ValueHolder *)obj.ptr();
        if (vh->type == cIntType) {
            int count = *((int *)vh->buf);
            if (count < 0)
                argumentError(0, "negative values are not allowed");
            for (int i = 0; i < count; ++i) {
                ValueHolderPtr vhi = intToValueHolder(i);
                mpv->add(new PValue(staticType(vhi.ptr()), true));
            }
        }
        else if (vh->type == cSizeTType) {
            size_t count = *((size_t *)vh->buf);
            for (size_t i = 0; i < count; ++i) {
                ValueHolderPtr vhi = sizeTToValueHolder(i);
                mpv->add(new PValue(staticType(vhi.ptr()), true));
            }
        }
        else {
            argumentError(0, "expecting a static SizeT or Int value");
        }
        return mpv;
    }

    case PRIM_integers : {
        ensureArity(args, 1);
        ObjectPtr obj = unwrapStaticType(args->values[0]->type);
        if (!obj || (obj->objKind != VALUE_HOLDER))
            argumentError(0, "expecting a static SizeT or Int value");
        MultiPValuePtr mpv = new MultiPValue();
        ValueHolder *vh = (ValueHolder *)obj.ptr();
        if (vh->type == cIntType) {
            int count = *((int *)vh->buf);
            if (count < 0)
                argumentError(0, "negative values are not allowed");
            for (int i = 0; i < count; ++i)
                mpv->add(new PValue(cIntType, true));
        }
        else if (vh->type == cSizeTType) {
            size_t count = *((size_t *)vh->buf);
            for (int i = 0; i < count; ++i)
                mpv->add(new PValue(cSizeTType, true));
        }
        else {
            argumentError(0, "expecting a static SizeT or Int value");
        }
        return mpv;
    }

    case PRIM_staticFieldRef : {
        ensureArity(args, 2);
        ObjectPtr moduleObj = unwrapStaticType(args->values[0]->type);
        if (!moduleObj || (moduleObj->objKind != MODULE_HOLDER))
            argumentError(0, "expecting a module");
        ObjectPtr identObj = unwrapStaticType(args->values[1]->type);
        if (!identObj || (identObj->objKind != IDENTIFIER))
            argumentError(1, "expecting an identifier");
        ModuleHolder *module = (ModuleHolder *)moduleObj.ptr();
        Identifier *ident = (Identifier *)identObj.ptr();
        ObjectPtr obj = safeLookupModuleHolder(module, ident);
        return analyzeStaticObject(obj);
    }

    case PRIM_EnumP :
        return new MultiPValue(new PValue(boolType, true));

    case PRIM_EnumMemberCount :
        return new MultiPValue(new PValue(cSizeTType, true));

    case PRIM_EnumMemberName : {
        TypePtr ptrInt8Type = pointerType(int8Type);
        PValuePtr pv = new PValue(arrayType(int8Type, 1), true);
        MultiPValuePtr args = new MultiPValue();
        args->add(new PValue(ptrInt8Type, true));
        args->add(new PValue(ptrInt8Type, true));
        return analyzeCallValue(staticPValue(operator_stringLiteral()), args);
    }

    case PRIM_enumToInt :
        return new MultiPValue(new PValue(cIntType, true));

    case PRIM_intToEnum : {
        ensureArity(args, 2);
        TypePtr t = valueToEnumerationType(args, 0);
        initializeEnumType((EnumType*)t.ptr());
        return new MultiPValue(new PValue(t, true));
    }

    case PRIM_IdentifierP :
        return new MultiPValue(new PValue(boolType, true));

    case PRIM_IdentifierSize :
        return new MultiPValue(new PValue(cSizeTType, true));

    case PRIM_IdentifierConcat : {
        std::string result;
        for (unsigned i = 0; i < args->size(); ++i) {
            ObjectPtr obj = unwrapStaticType(args->values[i]->type);
            if (!obj || (obj->objKind != IDENTIFIER))
                argumentError(i, "expecting an identifier value");
            Identifier *ident = (Identifier *)obj.ptr();
            result.append(ident->str);
        }
        return analyzeStaticObject(new Identifier(result));
    }

    case PRIM_IdentifierSlice : {
        ensureArity(args, 3);
        ObjectPtr identObj = unwrapStaticType(args->values[0]->type);
        if (!identObj || (identObj->objKind != IDENTIFIER))
            argumentError(0, "expecting an identifier value");
        Identifier *ident = (Identifier *)identObj.ptr();
        ObjectPtr beginObj = unwrapStaticType(args->values[1]->type);
        ObjectPtr endObj = unwrapStaticType(args->values[2]->type);
        size_t begin = 0, end = 0;
        if (!beginObj || !staticToSizeTOrInt(beginObj, begin))
            argumentError(1, "expecting a static SizeT or Int value");
        if (!endObj || !staticToSizeTOrInt(endObj, end))
            argumentError(2, "expecting a static SizeT or Int value");
        if (end > ident->str.size()) {
            argumentIndexRangeError(2, "ending index",
                                    end, ident->str.size());
        }
        if (begin > end)
            argumentIndexRangeError(1, "starting index",
                                    begin, end);
        string result = ident->str.substr(begin, end-begin);
        return analyzeStaticObject(new Identifier(result));
    }

    case PRIM_IdentifierModuleName : {
        ensureArity(args, 1);
        ObjectPtr obj = unwrapStaticType(args->values[0]->type);
        if (!obj)
            argumentError(0, "expecting static object");
        ModulePtr m = staticModule(obj);
        if (!m)
            argumentError(0, "value has no associated module");
        return analyzeStaticObject(new Identifier(m->moduleName));
    }

    case PRIM_IdentifierStaticName : {
        ensureArity(args, 1);
        ObjectPtr obj = unwrapStaticType(args->values[0]->type);
        if (!obj)
            argumentError(0, "expecting static object");
        ostringstream sout;
        printStaticName(sout, obj);
        return analyzeStaticObject(new Identifier(sout.str()));
    }

    case PRIM_FlagP : {
        return new MultiPValue(new PValue(boolType, true));
    }

    case PRIM_Flag : {
        ensureArity(args, 1);
        ObjectPtr obj = unwrapStaticType(args->values[0]->type);
        if (obj != NULL && obj->objKind == IDENTIFIER) {
            Identifier *ident = (Identifier*)obj.ptr();

            map<string, string>::const_iterator flag = globalFlags.find(ident->str);
            string value;
            if (flag != globalFlags.end())
                value = flag->second;

            return analyzeStaticObject(new Identifier(value));
        } else
            argumentTypeError(0, "identifier", args->values[0]->type);
        return NULL;
    }

    case PRIM_atomicFence : {
        ensureArity(args, 1);
        return new MultiPValue();
    }

    case PRIM_atomicRMW : {
        // order op ptr val
        ensureArity(args, 4);
        pointerTypeOfValue(args, 2);
        return new MultiPValue(new PValue(args->values[3]->type, true));
    }

    case PRIM_atomicLoad : {
        // order ptr
        ensureArity(args, 2);
        PointerTypePtr pt = pointerTypeOfValue(args, 1);
        return new MultiPValue(new PValue(pt->pointeeType, true));
    }

    case PRIM_atomicStore : {
        // order ptr val
        ensureArity(args, 3);
        pointerTypeOfValue(args, 1);
        return new MultiPValue();
    }

    case PRIM_atomicCompareExchange : {
        // order ptr old new
        ensureArity(args, 4);
        PointerTypePtr pt = pointerTypeOfValue(args, 1);

        return new MultiPValue(new PValue(args->values[2]->type, true));
    }

    case PRIM_activeException : {
        ensureArity(args, 0);
        return new MultiPValue(new PValue(pointerType(uint8Type), true));
    }

    case PRIM_memcpy :
    case PRIM_memmove : {
        ensureArity(args, 3);
        PointerTypePtr toPt = pointerTypeOfValue(args, 0);
        PointerTypePtr fromPt = pointerTypeOfValue(args, 1);
        integerTypeOfValue(args, 2);
        return new MultiPValue();
    }

    case PRIM_countValues : {
        return new MultiPValue(new PValue(cIntType, true));
    }

    case PRIM_nthValue : {
        if (args->size() < 1)
            arityError2(1, args->size());
        ObjectPtr obj = unwrapStaticType(args->values[0]->type);
        size_t i = 0;
        if (!obj || !staticToSizeTOrInt(obj, i))
            argumentError(0, "expecting static SizeT or Int value");
        if (i+1 >= args->size())
            argumentError(0, "nthValue argument out of bounds");
        return new MultiPValue(args->values[i+1]);
    }

    case PRIM_withoutNthValue : {
        if (args->size() < 1)
            arityError2(1, args->size());
        ObjectPtr obj = unwrapStaticType(args->values[0]->type);
        size_t i = 0;
        if (!obj || !staticToSizeTOrInt(obj, i))
            argumentError(0, "expecting static SizeT or Int value");
        if (i+1 >= args->size())
            argumentError(0, "withoutNthValue argument out of bounds");
        MultiPValuePtr mpv = new MultiPValue();
        for (size_t n = 1; n < args->size(); ++n) {
            if (n == i+1)
                continue;
            mpv->add(args->values[n]);
        }
        return mpv;
    }

    case PRIM_takeValues : {
        if (args->size() < 1)
            arityError2(1, args->size());
        ObjectPtr obj = unwrapStaticType(args->values[0]->type);
        size_t i = 0;
        if (!obj || !staticToSizeTOrInt(obj, i))
            argumentError(0, "expecting static SizeT or Int value");
        if (i+1 >= args->size())
            i = args->size() - 1;
        MultiPValuePtr mpv = new MultiPValue();
        for (size_t n = 1; n < i+1; ++n)
            mpv->add(args->values[n]);
        return mpv;
    }

    case PRIM_dropValues : {
        if (args->size() < 1)
            arityError2(1, args->size());
        ObjectPtr obj = unwrapStaticType(args->values[0]->type);
        size_t i = 0;
        if (!obj || !staticToSizeTOrInt(obj, i))
            argumentError(0, "expecting static SizeT or Int value");
        if (i+1 >= args->size())
            i = args->size() - 1;
        MultiPValuePtr mpv = new MultiPValue();
        for (size_t n = i+1; n < args->size(); ++n)
            mpv->add(args->values[n]);
        return mpv;
    }

    case PRIM_LambdaRecordP : {
        ensureArity(args, 1);
        return new MultiPValue(new PValue(boolType, true));
    }

    case PRIM_LambdaSymbolP : {
        ensureArity(args, 1);
        return new MultiPValue(new PValue(boolType, true));
    }

    case PRIM_LambdaMonoP : {
        ensureArity(args, 1);
        return new MultiPValue(new PValue(boolType, true));
    }

    case PRIM_LambdaMonoInputTypes : {
        ensureArity(args, 1);
        ObjectPtr callable = unwrapStaticType(args->values[0]->type);
        if (!callable)
            argumentError(0, "lambda record type expected");

        if (callable != NULL && callable->objKind == TYPE) {
            Type *t = (Type*)callable.ptr();
            if (t->typeKind == RECORD_TYPE) {
                RecordType *r = (RecordType*)t;
                if (r->record->lambda->mono.monoState != Procedure_MonoOverload)
                    argumentError(0, "not a monomorphic lambda record type");
                vector<TypePtr> const &monoTypes = r->record->lambda->mono.monoTypes;
                MultiPValuePtr values = new MultiPValue();
                for (size_t i = 0; i < monoTypes.size(); ++i)
                    values->add(staticPValue(monoTypes[i].ptr()));

                return values;
            }
        }

        argumentError(0, "not a monomorphic lambda record type");
        return new MultiPValue();
    }

    default :
        assert(false);
        return NULL;

    }
}

}
