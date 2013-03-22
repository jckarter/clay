
#include "clay.hpp"
#include "evaluator.hpp"
#include "codegen.hpp"
#include "loader.hpp"
#include "operators.hpp"
#include "patterns.hpp"
#include "lambdas.hpp"
#include "analyzer.hpp"
#include "invoketables.hpp"
#include "matchinvoke.hpp"
#include "error.hpp"
#include "literals.hpp"
#include "desugar.hpp"
#include "constructors.hpp"
#include "env.hpp"
#include "clone.hpp"
#include "objects.hpp"
#include "analyzer_op.hpp"


#pragma clang diagnostic ignored "-Wcovered-switch-default"


namespace clay {


GVarInstance::GVarInstance(GlobalVariablePtr gvar,
             llvm::ArrayRef<ObjectPtr> params)
    : gvar(gvar), params(params),
      llGlobal(NULL), debugInfo(NULL), analyzing(false) {}

GVarInstance::~GVarInstance() {}


Procedure::Procedure(Module *module, IdentifierPtr name, Visibility visibility, bool privateOverload)
    : TopLevelItem(PROCEDURE, module, name, visibility),
      privateOverload(privateOverload)
{}

Procedure::Procedure(Module *module, IdentifierPtr name, Visibility visibility, bool privateOverload, OverloadPtr interface)
    : TopLevelItem(PROCEDURE, module, name, visibility),
      interface(interface), privateOverload(privateOverload)
{}

Procedure::~Procedure() {}


GlobalVariable::GlobalVariable(Module *module, IdentifierPtr name,
               Visibility visibility,
               llvm::ArrayRef<PatternVar> patternVars,
               ExprPtr predicate,
               llvm::ArrayRef<IdentifierPtr> params,
               IdentifierPtr varParam,
               ExprPtr expr)
    : TopLevelItem(GLOBAL_VARIABLE, module, name, visibility),
      patternVars(patternVars), predicate(predicate),
      params(params), varParam(varParam), expr(expr) {}

GlobalVariable::~GlobalVariable() {}



static StatementAnalysis analyzeStatement(StatementPtr stmt, EnvPtr env, AnalysisContext *ctx);
static EnvPtr analyzeBinding(BindingPtr x, EnvPtr env);


static int analysisCachingDisabled = 0;
void disableAnalysisCaching() { analysisCachingDisabled += 1; }
void enableAnalysisCaching() { analysisCachingDisabled -= 1; }

static TypePtr objectType(ObjectPtr x);

static bool staticToTypeTuple(ObjectPtr x, vector<TypePtr> &out);
static void staticToTypeTuple(MultiStaticPtr x, unsigned index,
                             vector<TypePtr> &out);
static int staticToInt(MultiStaticPtr x, unsigned index);



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
    case INTRINSIC :
    case GLOBAL_ALIAS :
    case RECORD_DECL :
    case VARIANT_DECL :
    case MODULE :
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
    for (size_t i = 0; i < z->elementTypes.size(); ++i) {
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

static bool staticToInt(ObjectPtr x, int &out, TypePtr &type)
{
    if (x->objKind != VALUE_HOLDER)
        return false;
    ValueHolderPtr vh = (ValueHolder *)x.ptr();
    if (vh->type != cIntType) {
        type = vh->type;
        return false;
    }
    out = vh->as<int>();
    return true;
}

static int staticToInt(MultiStaticPtr x, unsigned index)
{
    int out = -1;
    TypePtr type;
    if (!staticToInt(x->values[index], out, type)){
        argumentTypeError(index, "Int", type);
    }
    return out;
}

bool staticToBool(ObjectPtr x, bool &out, TypePtr &type)
{
    if (x->objKind != VALUE_HOLDER)
        return false;
    ValueHolderPtr vh = (ValueHolder *)x.ptr();
    if (vh->type != boolType) {
        type = vh->type;
        return false;
    }
    out = vh->as<bool>();
    return true;
}

bool staticToBool(MultiStaticPtr x, unsigned index)
{
    bool out = false;
    TypePtr type;
    if (!staticToBool(x->values[index], out, type))
        argumentTypeError(index, "Bool", type);
    return out;
}

bool staticToCallingConv(ObjectPtr x, CallingConv &out)
{
    if (x->objKind != PRIM_OP)
        return false;
    PrimOp *ccPrim = (PrimOp*)x.ptr();
    switch (ccPrim->primOpCode) {
    case PRIM_AttributeCCall:
        out = CC_DEFAULT;
        return true;
    case PRIM_AttributeStdCall:
        out = CC_STDCALL;
        return true;
    case PRIM_AttributeFastCall:
        out = CC_FASTCALL;
        return true;
    case PRIM_AttributeThisCall:
        out = CC_THISCALL;
        return true;
    case PRIM_AttributeLLVMCall:
        out = CC_LLVM;
        return true;
    default:
        return false;
    }
}

CallingConv staticToCallingConv(MultiStaticPtr x, unsigned index)
{
    CallingConv out;
    if (!staticToCallingConv(x->values[index], out))
        argumentError(index, "expecting calling convention attribute");
    return out;
}

//
// safe analysis
//

static Location analysisErrorLocation;
static vector<CompileContextEntry> analysisErrorCompileContext;

struct ClearAnalysisError {
    ClearAnalysisError() {}
    ~ClearAnalysisError() {
        analysisErrorLocation = Location();
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

PVData safeAnalyzeOne(ExprPtr expr, EnvPtr env)
{
    ClearAnalysisError clear;
    PVData result = analyzeOne(expr, env);
    if (!result.ok())
        analysisError();
    return result;
}

MultiPValuePtr safeAnalyzeMulti(ExprListPtr exprs, EnvPtr env, size_t wantCount)
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

InvokeEntry* safeAnalyzeCallable(ObjectPtr x,
                                   llvm::ArrayRef<TypePtr> argsKey,
                                   llvm::ArrayRef<ValueTempness> argsTempness)
{
    ClearAnalysisError clear;
    InvokeEntry* entry = analyzeCallable(x, argsKey, argsTempness);
    if (!entry->callByName && !entry->analyzed)
        analysisError();
    return entry;
}

MultiPValuePtr safeAnalyzeCallByName(InvokeEntry* entry,
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

static MultiPValuePtr analyzeMulti2(ExprListPtr exprs, EnvPtr env, size_t wantCount);

MultiPValuePtr analyzeMulti(ExprListPtr exprs, EnvPtr env, size_t wantCount)
{
    if (analysisCachingDisabled > 0)
        return analyzeMulti2(exprs, env, wantCount);
    if (!exprs->cachedAnalysis)
        exprs->cachedAnalysis = analyzeMulti2(exprs, env, wantCount);
    return exprs->cachedAnalysis;
}

static MultiPValuePtr analyzeMulti2(ExprListPtr exprs, EnvPtr env, size_t wantCount)
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
            PVData y = analyzeOne(x, env);
            if (!y.ok())
                return NULL;
            out->add(y);
        }
    }
    return out;
}



//
// analyzeOne
//

PVData analyzeOne(ExprPtr expr, EnvPtr env)
{
    MultiPValuePtr x = analyzeExpr(expr, env);
    if (!x)
        return PVData();
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
            index += unsigned(z->size());
            out->add(z);
        }
        else if (x->exprKind == PAREN) {
            MultiPValuePtr z = analyzeArgExpr(x, env, index, dispatchIndices);
            if (!z)
                return NULL;
            index += unsigned(z->size());
            out->add(z);
        } else {
            PVData y = analyzeOneArg(x, env, index, dispatchIndices);
            if (!y.ok())
                return NULL;
            out->add(y);
            index += 1;
        }
    }
    return out;
}

PVData analyzeOneArg(ExprPtr x,
                        EnvPtr env,
                        unsigned startIndex,
                        vector<unsigned> &dispatchIndices)
{
    MultiPValuePtr mpv = analyzeArgExpr(x, env, startIndex, dispatchIndices);
    if (!mpv)
        return PVData();
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

void appendArgString(Expr *expr, string *outString)
{
    ForeignExpr *fexpr;
    if (expr->exprKind == FOREIGN_EXPR)
        fexpr = (ForeignExpr*)expr;
    else if (expr->exprKind == UNPACK) {
        Unpack *uexpr = (Unpack*)expr;
        if (uexpr->expr->exprKind != FOREIGN_EXPR)
            goto notAlias;
        fexpr = (ForeignExpr *)uexpr->expr.ptr();
        *outString += "..";
    } else
        goto notAlias;

    *outString += fexpr->expr->asString();
    return;

notAlias:
    error("__ARG__ may only be applied to an alias value or alias function argument");
}

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
        return new MultiPValue(PVData(boolType, true));
    }

    case INT_LITERAL : {
        IntLiteral *x = (IntLiteral *)expr.ptr();
        ValueHolderPtr v = parseIntLiteral(safeLookupModule(env), x);
        return new MultiPValue(PVData(v->type, true));
    }

    case FLOAT_LITERAL : {
        FloatLiteral *x = (FloatLiteral *)expr.ptr();
        ValueHolderPtr v = parseFloatLiteral(safeLookupModule(env), x);
        return new MultiPValue(PVData(v->type, true));
    }

    case CHAR_LITERAL : {
        CharLiteral *x = (CharLiteral *)expr.ptr();
        if (!x->desugared)
            x->desugared = desugarCharLiteral(x->value);
        return analyzeExpr(x->desugared, env);
    }

    case STRING_LITERAL : {
        StringLiteral *x = (StringLiteral *)expr.ptr();
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
        Location location = safeLookupCallByNameLocation(env);
        string filename = location.source->fileName;
        return analyzeStaticObject(Identifier::get(filename));
    }

    case LINE_EXPR : {
        return new MultiPValue(PVData(cSizeTType, true));
    }

    case COLUMN_EXPR : {
        return new MultiPValue(PVData(cSizeTType, true));
    }

    case ARG_EXPR : {
        ARGExpr *arg = (ARGExpr *)expr.ptr();
        ObjectPtr obj = safeLookupEnv(env, arg->name);
        string argString;
        if (obj->objKind == EXPRESSION) {
            Expr *expr = (Expr*)obj.ptr();
            appendArgString(expr, &argString);
        } else if (obj->objKind == EXPR_LIST) {
            ExprList *elist = (ExprList*)obj.ptr();
            for (vector<ExprPtr>::const_iterator i = elist->exprs.begin(),
                    end = elist->exprs.end();
                 i != end;
                 ++i)
            {
                if (!argString.empty())
                    argString += ", ";

                Expr *expr = i->ptr();
                appendArgString(expr, &argString);
            }
        }
        return analyzeStaticObject(Identifier::get(argString));
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
        if (!x->desugared)
            desugarFieldRef(x, safeLookupModule(env));
        if (x->isDottedModuleName)
            return analyzeExpr(x->desugared, env);

        PVData pv = analyzeOne(x->expr, env);
        if (!pv.ok())
            return NULL;
        if (pv.type->typeKind == STATIC_TYPE) {
            StaticType *st = (StaticType *)pv.type.ptr();
            if (st->obj->objKind == MODULE) {
                Module *m = (Module *)st->obj.ptr();
                ObjectPtr obj = safeLookupPublic(m, x->name);
                return analyzeStaticObject(obj);
            }
        }

        return analyzeExpr(x->desugared, env);
    }

    case STATIC_INDEXING : {
        StaticIndexing *x = (StaticIndexing *)expr.ptr();
        if (!x->desugared)
            x->desugared = desugarStaticIndexing(x);
        return analyzeExpr(x->desugared, env);
    }

    case VARIADIC_OP : {
        VariadicOp *x = (VariadicOp *)expr.ptr();
        if (x->op == ADDRESS_OF) {
            PVData pv = analyzeOne(x->exprs->exprs.front(), env);
            if (!pv.ok())
                return NULL;
            if (pv.isTemp)
                error("can't take address of a temporary");
        }
        if (!x->desugared)
            x->desugared = desugarVariadicOp(x);
        return analyzeExpr(x->desugared, env);
    }

    case EVAL_EXPR : {
        EvalExpr *eval = (EvalExpr *)expr.ptr();
        // XXX compilation context
        ExprListPtr evaled = desugarEvalExpr(eval, env);
        return analyzeMulti(evaled, env, 0);
    }

    case AND : {
        return new MultiPValue(PVData(boolType, true));
    }

    case OR : {
        return new MultiPValue(PVData(boolType, true));
    }

    case LAMBDA : {
        Lambda *x = (Lambda *)expr.ptr();
        if (!x->initialized)
            initializeLambda(x, env);
        return analyzeExpr(x->converted, env);
    }

    case UNPACK : {
        Unpack *unpack = (Unpack *)expr.ptr();
        if (unpack->expr->exprKind != FOREIGN_EXPR)
            error("incorrect usage of unpack operator");
        return analyzeExpr(unpack->expr, env);
    }

    case STATIC_EXPR : {
        StaticExpr *x = (StaticExpr *)expr.ptr();
        ObjectPtr obj = evaluateOneStatic(x->expr, env);
        TypePtr t = staticType(obj);
        return new MultiPValue(PVData(t, true));
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

    case NEW_TYPE_DECL : {
        NewTypeDecl *y = (NewTypeDecl *)x.ptr();
        assert(y->type->typeKind == NEW_TYPE);
        initializeNewType(y->type);

        return new MultiPValue(PVData(y->type.ptr(), true));
    }

    case ENUM_MEMBER : {
        EnumMember *y = (EnumMember *)x.ptr();
        assert(y->type->typeKind == ENUM_TYPE);
        initializeEnumType((EnumType*)y->type.ptr());

        return new MultiPValue(PVData(y->type, true));
    }

    case GLOBAL_VARIABLE : {
        GlobalVariable *y = (GlobalVariable *)x.ptr();
        if (y->hasParams()) {
            TypePtr t = staticType(x);
            return new MultiPValue(PVData(t, true));
        }
        GVarInstancePtr inst = defaultGVarInstance(y);
        return analyzeGVarInstance(inst);
    }

    case EXTERNAL_VARIABLE : {
        ExternalVariable *y = (ExternalVariable *)x.ptr();
        return new MultiPValue(analyzeExternalVariable(y));
    }

    case EXTERNAL_PROCEDURE : {
        ExternalProcedure *y = (ExternalProcedure *)x.ptr();
        analyzeExternalProcedure(y);
        return new MultiPValue(PVData(y->ptrType, true));
    }

    case GLOBAL_ALIAS : {
        GlobalAlias *y = (GlobalAlias *)x.ptr();
        if (y->hasParams()) {
            TypePtr t = staticType(x);
            return new MultiPValue(PVData(t, true));
        }
        evaluatePredicate(y->patternVars, y->predicate, y->env);
        return analyzeExpr(y->expr, y->env);
    }

    case VALUE_HOLDER : {
        ValueHolder *y = (ValueHolder *)x.ptr();
        return new MultiPValue(PVData(y->type, true));
    }

    case MULTI_STATIC : {
        MultiStatic *y = (MultiStatic *)x.ptr();
        MultiPValuePtr mpv = new MultiPValue();
        for (size_t i = 0; i < y->size(); ++i) {
            TypePtr t = objectType(y->values[i]);
            mpv->values.push_back(PVData(t, true));
        }
        return mpv;
    }

    case RECORD_DECL : {
        RecordDecl *y = (RecordDecl *)x.ptr();
        ObjectPtr z;
        if (y->params.empty() && !y->varParam)
            z = recordType(y, vector<ObjectPtr>()).ptr();
        else
            z = y;
        return new MultiPValue(PVData(staticType(z), true));
    }

    case VARIANT_DECL : {
        VariantDecl *y = (VariantDecl *)x.ptr();
        ObjectPtr z;
        if (y->params.empty() && !y->varParam)
            z = variantType(y, vector<ObjectPtr>()).ptr();
        else
            z = y;
        return new MultiPValue(PVData(staticType(z), true));
    }

    case TYPE :
    case PRIM_OP :
    case PROCEDURE :
    case MODULE :
    case INTRINSIC :
    case IDENTIFIER : {
        TypePtr t = staticType(x);
        return new MultiPValue(PVData(t, true));
    }

    case EVALUE : {
        EValue *y = (EValue *)x.ptr();
        return new MultiPValue(PVData(y->type, y->forwardedRValue));
    }

    case MULTI_EVALUE : {
        MultiEValue *y = (MultiEValue *)x.ptr();
        MultiPValuePtr z = new MultiPValue();
        for (size_t i = 0; i < y->values.size(); ++i) {
            EValue *ev = y->values[i].ptr();
            z->values.push_back(PVData(ev->type, ev->forwardedRValue));
        }
        return z;
    }

    case CVALUE : {
        CValue *y = (CValue *)x.ptr();
        return new MultiPValue(PVData(y->type, y->forwardedRValue));
    }

    case MULTI_CVALUE : {
        MultiCValue *y = (MultiCValue *)x.ptr();
        MultiPValuePtr z = new MultiPValue();
        for (size_t i = 0; i < y->values.size(); ++i) {
            CValue *cv = y->values[i].ptr();
            z->add(PVData(cv->type, cv->forwardedRValue));
        }
        return z;
    }

    case PVALUE : {
        PValue *y = (PValue *)x.ptr();
        return new MultiPValue(y->data);
    }

    case MULTI_PVALUE : {
        MultiPValue *y = (MultiPValue *)x.ptr();
        return y;
    }

    case PATTERN :
    case MULTI_PATTERN :
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
                                   llvm::ArrayRef<ObjectPtr> params)
{
    if (!x->instances)
        x->instances = new ObjectTable();
    Pointer<RefCounted> &y = x->instances->lookup(params);
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
        for (size_t i = 0; i < gvar->params.size(); ++i) {
            addLocal(x->env, gvar->params[i], x->params[i]);
        }
        if (gvar->varParam.ptr()) {
            MultiStaticPtr varParams = new MultiStatic();
            for (size_t i = gvar->params.size(); i < x->params.size(); ++i)
                varParams->add(x->params[i]);
            addLocal(x->env, gvar->varParam, varParams.ptr());
        }
    }
    x->analyzing = true;
    evaluatePredicate(x->gvar->patternVars, x->gvar->predicate, x->env);
    PVData pv = analyzeOne(x->expr, x->env);
    x->analyzing = false;
    if (!pv.ok())
        return NULL;
    x->analysis = new MultiPValue(PVData(pv.type, false));
    x->type = pv.type;
    return x->analysis;
}



//
// analyzeExternalVariable
//

PVData analyzeExternalVariable(ExternalVariablePtr x)
{
    if (!x->type2)
        x->type2 = evaluateType(x->type, x->env);
    return PVData(x->type2, false);
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
    for (size_t i = 0; i < x->args.size(); ++i) {
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

    MultiStaticPtr attrs = evaluateMultiStatic(x->attributes, x->env);

    for (size_t i = 0; i < attrs->size(); ++i) {
        ObjectPtr obj = attrs->values[i];

        switch (obj->objKind) {
        case PRIM_OP: {
            PrimOp *y = (PrimOp *)obj.ptr();
            switch (y->primOpCode) {
            case PRIM_AttributeStdCall : {
                if (callingConv != -1)
                    error(x, "cannot specify more than one calling convention in external attributes");
                callingConv = CC_STDCALL;
                break;
            }
            case PRIM_AttributeFastCall : {
                if (callingConv != -1)
                    error(x, "cannot specify more than one calling convention in external attributes");
                callingConv = CC_FASTCALL;
                break;
            }
            case PRIM_AttributeThisCall : {
                if (callingConv != -1)
                    error(x, "cannot specify more than one calling convention in external attributes");
                callingConv = CC_THISCALL;
                break;
            }
            case PRIM_AttributeCCall : {
                if (callingConv != -1)
                    error(x, "cannot specify more than one calling convention in external attributes");
                callingConv = CC_DEFAULT;
                break;
            }
            case PRIM_AttributeLLVMCall : {
                if (callingConv != -1)
                    error(x, "cannot specify more than one calling convention in external attributes");
                callingConv = CC_LLVM;
                break;
            }
            case PRIM_AttributeDLLImport : {
                if (x->attrDLLExport)
                    error(x, "dllimport specified after dllexport in external attributes");
                x->attrDLLImport = true;
                break;
            }
            case PRIM_AttributeDLLExport : {
                if (x->attrDLLImport)
                    error(x, "dllexport specified after dllimport in external attributes");
                x->attrDLLExport = true;
                break;
            }
            default : {
                string buf;
                llvm::raw_string_ostream os(buf);
                os << "invalid external function attribute: ";
                printStaticName(os, obj);
                error(x, os.str());
            }
            }
            break;
        }
        case IDENTIFIER : {
            Identifier *y = (Identifier *)obj.ptr();
            x->attrAsmLabel = y->str;
            break;
        }
        default: {
            string buf;
            llvm::raw_string_ostream os(buf);
            os << "invalid external function attribute: ";
            printStaticName(os, obj);
            error(x, os.str());
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

    MultiStaticPtr attrs = evaluateMultiStatic(var->attributes, var->env);

    for (size_t i = 0; i < attrs->size(); ++i) {
        ObjectPtr obj = attrs->values[i];

        if (obj->objKind != PRIM_OP) {
            string buf;
            llvm::raw_string_ostream os(buf);
            os << "invalid external variable attribute: ";
            printStaticName(os, obj);
            error(var, os.str());
        }

        PrimOp *y = (PrimOp *)obj.ptr();
        switch (y->primOpCode) {
        case PRIM_AttributeDLLImport : {
            if (var->attrDLLExport)
                error(var, "dllimport specified after dllexport in external attributes");
            var->attrDLLImport = true;
            break;
        }
        case PRIM_AttributeDLLExport : {
            if (var->attrDLLImport)
                error(var, "dllexport specified after dllimport in external attributes");
            var->attrDLLExport = true;
            break;
        }
        default : {
            string buf;
            llvm::raw_string_ostream os(buf);
            os << "invalid external variable attribute: ";
            printStaticName(os, obj);
            error(var, os.str());
        }
        }
    }
}

void verifyAttributes(ModulePtr mod)
{
    assert(!mod->attributesVerified);
    mod->attributesVerified = true;

    if (mod->declaration != NULL && mod->declaration->attributes != NULL) {
        MultiStaticPtr attrs = evaluateMultiStatic(mod->declaration->attributes, mod->env);

        for (size_t i = 0; i < attrs->size(); ++i) {
            ObjectPtr obj = attrs->values[i];

            switch (obj->objKind) {
            case TYPE: {
                if (obj->objKind == TYPE) {
                    Type *ty = (Type*)obj.ptr();
                    if (ty->typeKind == FLOAT_TYPE) {
                        mod->attrDefaultFloatType = (FloatType*)ty;
                    } else if (ty->typeKind == INTEGER_TYPE) {
                        mod->attrDefaultIntegerType = (IntegerType*)ty;
                    }
                }
                break;
            }
            case IDENTIFIER: {
                Identifier *y = (Identifier *)obj.ptr();
                mod->attrBuildFlags.push_back(y->str);
                break;
            }
            default:
                string buf;
                llvm::raw_string_ostream os(buf);
                os << "invalid module attribute: ";
                printStaticName(os, obj);
                error(mod->declaration, os.str());
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
        case PRIM_ExternalCodePointer :
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
    else if (x->objKind == RECORD_DECL) {
        return true;
    }
    else if (x->objKind == VARIANT_DECL) {
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
    PVData pv = analyzeOne(indexable, env);
    if (!pv.ok())
        return NULL;
    ObjectPtr obj = unwrapStaticType(pv.type);
    if (obj.ptr()) {
        if (isTypeConstructor(obj)) {
            MultiStaticPtr params = evaluateMultiStatic(args, env);
            PVData out = analyzeTypeConstructor(obj, params);
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
        if (obj->objKind != VALUE_HOLDER && obj->objKind != IDENTIFIER)
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
                string buf;
                llvm::raw_string_ostream ostr(buf);
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
            vector<uint8_t> returnIsRef(returnTypes.size(), false);
            for (size_t i = 0; i < returnTypes.size(); ++i)
                returnIsRef[i] = unwrapByRef(returnTypes[i]);
            return codePointerType(argTypes, returnIsRef, returnTypes);
        }

        case PRIM_ExternalCodePointer : {
            ensureArity(args, 4);

            CallingConv cc = staticToCallingConv(args, 0);
            bool hasVarArgs = staticToBool(args, 1);
            vector<TypePtr> argTypes;
            staticToTypeTuple(args, 2, argTypes);
            vector<TypePtr> returnTypes;
            staticToTypeTuple(args, 3, returnTypes);
            if (returnTypes.size() > 1)
                argumentError(1, "C code cannot return more than one value");
            TypePtr returnType;
            if (returnTypes.size() == 1)
                returnType = returnTypes[0];
            return cCodePointerType(cc, argTypes, hasVarArgs, returnType);
        }

        case PRIM_Array : {
            ensureArity(args, 2);
            TypePtr t = staticToType(args, 0);
            return arrayType(t, unsigned(staticToInt(args, 1)));
        }

        case PRIM_Vec : {
            ensureArity(args, 2);
            TypePtr t = staticToType(args, 0);
            return vecType(t, unsigned(staticToInt(args, 1)));
        }

        case PRIM_Tuple : {
            vector<TypePtr> types;
            for (size_t i = 0; i < args->size(); ++i)
                types.push_back(staticToType(args, i));
            return tupleType(types);
        }

        case PRIM_Union : {
            vector<TypePtr> types;
            for (size_t i = 0; i < args->size(); ++i)
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
    else if (constructor->objKind == RECORD_DECL) {
        RecordDeclPtr x = (RecordDecl *)constructor.ptr();
        if (x->varParam.ptr()) {
            if (args->size() < x->params.size())
                arityError2(x->params.size(), args->size());
        }
        else {
            ensureArity(args, x->params.size());
        }
        return recordType(x, args->values);
    }
    else if (constructor->objKind == VARIANT_DECL) {
        VariantDeclPtr x = (VariantDecl *)constructor.ptr();
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

PVData analyzeTypeConstructor(ObjectPtr obj, MultiStaticPtr args)
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
    for (size_t i = 0; i < x->params.size(); ++i) {
        addLocal(bodyEnv, x->params[i], params->values[i]);
    }
    if (x->varParam.ptr()) {
        MultiStaticPtr varParams = new MultiStatic();
        for (size_t i = x->params.size(); i < params->size(); ++i)
            varParams->add(params->values[i]);
        addLocal(bodyEnv, x->varParam, varParams.ptr());
    }
    evaluatePredicate(x->patternVars, x->predicate, bodyEnv);
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
        PVData const &pv = args->values[i];
        argsKey.push_back(pv.type);
        argsTempness.push_back(pv.isTemp ? TEMPNESS_RVALUE : TEMPNESS_LVALUE);
    }
}

MultiPValuePtr analyzeReturn(llvm::ArrayRef<uint8_t> returnIsRef,
                             llvm::ArrayRef<TypePtr> returnTypes)

{
    MultiPValuePtr x = new MultiPValue();
    for (size_t i = 0; i < returnTypes.size(); ++i) {
        PVData pv = PVData(returnTypes[i], !returnIsRef[i]);
        x->values.push_back(pv);
    }
    return x.ptr();
}


//
// analyzeIntrinsic
//

// this class is designed to pervert the intrinsic verification code
// in llvm/Intrinsics.gen, selected by #define GET_INTRINSIC_VERIFIER,
// into doing type checking and output type propagation for an intrinsic.
// GET_INTRINSIC_VERIFIER contains a function body that expects to have
// the following in scope:
//  - namespace llvm
//  - int-sized bit mask constants `ExtendedElementVectorType` and `TruncatedElementVectorType`
//  - an `llvm::Intrinsic::ID ID`
//  - an `llvm::FunctionType *IF`. This is unused by our code, so we provide
//    a dummy IF variable instead
//  - a function that can be called as
//    `VerifyIntrinsicPrototype(ID, IF, numReturns, numArgs, ...)`
//    where the varargs are either values from the llvm::MVT::SimpleValueType enum
//    signifying the expected return or argument type, or negative values
//    equal to `~index`, indicating that that argument must be the same type as
//    the `index`th vararg.
struct IntrinsicAnalyzer {
    llvm::Intrinsic::ID ID;
    llvm::ArrayRef<llvm::Type*> inputTypes;

    string outError;
    llvm::SmallVector<llvm::Type*, 4> ArgTys;

    IntrinsicAnalyzer(llvm::Intrinsic::ID ID,
                      llvm::ArrayRef<llvm::Type*> inputTypes)
    : ID(ID), inputTypes(inputTypes) {}

    static void skipReturnType(
        llvm::ArrayRef<llvm::Intrinsic::IITDescriptor> &Infos)
    {
        using namespace llvm;
        using namespace Intrinsic;
        IITDescriptor D = Infos.front();
        Infos = Infos.slice(1);

        switch (D.Kind) {
        case IITDescriptor::Void: return;
        case IITDescriptor::MMX: return;
        case IITDescriptor::Metadata: return;
        case IITDescriptor::Float: return;
        case IITDescriptor::Double: return;
        case IITDescriptor::Integer: return;
        case IITDescriptor::Vector:
            skipReturnType(Infos);
            return;
        case IITDescriptor::Pointer:
            skipReturnType(Infos);
            return;
        case IITDescriptor::Struct: {
            for (unsigned i = 0, e = D.Struct_NumElements; i != e; ++i)
                skipReturnType(Infos);
            return;
        }
        case IITDescriptor::Argument: return;
        case IITDescriptor::ExtendVecArgument: return;
        case IITDescriptor::TruncVecArgument: return;
        }
        llvm_unreachable("unhandled");
    }

    void VerifyIntrinsicType(
        llvm::Type *Ty,
        llvm::ArrayRef<llvm::Intrinsic::IITDescriptor> &Infos,
        llvm::raw_string_ostream &errors, size_t ai)
    {
        using namespace llvm;
        using namespace Intrinsic;

        // If we ran out of descriptors, there are too many arguments.
        if (Infos.empty()) {
            errors << "intrinsic has too many arguments";
            return;
        }
        IITDescriptor D = Infos.front();
        Infos = Infos.slice(1);

        switch (D.Kind) {
        case IITDescriptor::Void:
            if (!Ty->isVoidTy()) {
                errors << "intrinsic argument " << (ai+1)
                       << " must match LLVM void type, but got ";
                Ty->print(errors);
            }
            return;
        case IITDescriptor::MMX:
            if (!Ty->isX86_MMXTy()) {
                errors << "intrinsic argument " << (ai+1)
                       << " must match LLVM MMX type, but got ";
                Ty->print(errors);
            }
            return;
        case IITDescriptor::Metadata:
            if (!Ty->isMetadataTy()) {
                errors << "intrinsic argument " << (ai+1)
                       << " must match LLVM metadata type, but got ";
                Ty->print(errors);
            }
            return;
        case IITDescriptor::Float:
            if (!Ty->isFloatTy()) {
                errors << "intrinsic argument " << (ai+1)
                       << " must match LLVM float type, but got ";
                Ty->print(errors);
            }
            return;
        case IITDescriptor::Double:
            if (!Ty->isDoubleTy()) {
                errors << "intrinsic argument " << (ai+1)
                       << " must match LLVM double type, but got ";
                Ty->print(errors);
            }
            return;
        case IITDescriptor::Integer:
            if (!Ty->isIntegerTy(D.Integer_Width)) {
                errors << "intrinsic argument " << (ai+1)
                       << " must match LLVM i" << D.Integer_Width
                       << " type, but got ";
                Ty->print(errors);
            }
            return;
        case IITDescriptor::Vector: {
            llvm::VectorType *VT = dyn_cast<llvm::VectorType>(Ty);
            if (VT == 0) {
                errors << "intrinsic argument " << (ai+1)
                       << " must be of an LLVM vector type, but got ";
                Ty->print(errors);
            } else if (VT->getNumElements() != D.Vector_Width) {
                errors << "intrinsic argument " << (ai+1)
                       << " must be of an LLVM vector type with "
                       << D.Vector_Width
                       << " elements, but got ";
                Ty->print(errors);
            }
            VerifyIntrinsicType(VT->getElementType(), Infos, errors, ai);
            return;
        }
        case IITDescriptor::Pointer: {
            llvm::PointerType *PT = dyn_cast<llvm::PointerType>(Ty);
            if (PT == 0) {
                errors << "intrinsic argument " << (ai+1)
                       << " must be of an LLVM pointer type, but got ";
                Ty->print(errors);
            } else if (PT->getAddressSpace() != D.Pointer_AddressSpace) {
                errors << "intrinsic argument " << (ai+1)
                       << " must be of an LLVM pointer type with"
                       << D.Pointer_AddressSpace
                       << " address space, but got ";
                Ty->print(errors);
            }
            VerifyIntrinsicType(PT->getElementType(), Infos, errors, ai);
            return;
        }

        case IITDescriptor::Struct: {
            llvm::StructType *ST = dyn_cast<llvm::StructType>(Ty);
            if (ST == 0) {
                errors << "intrinsic argument " << (ai+1)
                       << " must be of an LLVM struct type, but got ";
                Ty->print(errors);
            } else if (ST->getNumElements() != D.Struct_NumElements) {
                errors << "intrinsic argument " << (ai+1)
                       << " must be of an LLVM struct type with "
                       << D.Struct_NumElements
                       << " elements, but got ";
                Ty->print(errors);
            }
            for (unsigned i = 0, e = D.Struct_NumElements; i != e; ++i)
                VerifyIntrinsicType(ST->getElementType(i), Infos, errors, ai);
            return;
        }

        case IITDescriptor::Argument:
            // Two cases here - If this is the second occurrence of an argument, verify
            // that the later instance matches the previous instance.
            if (D.getArgumentNumber() < ArgTys.size()) {
                if (Ty != ArgTys[D.getArgumentNumber()]) {
                    errors << "intrinsic argument " << (ai+1)
                           << " must match LLVM type of argument "
                           << D.getArgumentNumber()
                           << ", which is ";
                    ArgTys[D.getArgumentNumber()]->print(errors);
                    errors << ", but got ";
                    Ty->print(errors);
                }
                return;
            }

            // Otherwise, if this is the first instance of an argument, record it and
            // verify the "Any" kind.
            assert(D.getArgumentNumber() == ArgTys.size() && "Table consistency error");
            ArgTys.push_back(Ty);

            switch (D.getArgumentKind()) {
            case IITDescriptor::AK_AnyInteger:
                if (!Ty->isIntOrIntVectorTy()) {
                    errors << "intrinsic argument " << (ai+1)
                           << " must be of an LLVM integer type, but got ";
                    Ty->print(errors);
                }
                return;
            case IITDescriptor::AK_AnyFloat:
                if (!Ty->isFPOrFPVectorTy()) {
                    errors << "intrinsic argument " << (ai+1)
                           << " must be of an LLVM floating point type, but got ";
                    Ty->print(errors);
                }
                return;
            case IITDescriptor::AK_AnyVector:
                if (!isa<llvm::VectorType>(Ty)) {
                    errors << "intrinsic argument " << (ai+1)
                           << " must be of an LLVM vector type, but got ";
                    Ty->print(errors);
                }
                return;
            case IITDescriptor::AK_AnyPointer:
                if (!isa<llvm::PointerType>(Ty)) {
                    errors << "intrinsic argument " << (ai+1)
                           << " must be of an LLVM pointer type, but got ";
                    Ty->print(errors);
                }
                return;
            }
            llvm_unreachable("all argument kinds not covered");

        case IITDescriptor::ExtendVecArgument:
            // This may only be used when referring to a previous vector argument.
            if (D.getArgumentNumber() >= ArgTys.size() ||
                !isa<llvm::VectorType>(ArgTys[D.getArgumentNumber()]) ||
                llvm::VectorType::getExtendedElementVectorType(
                        cast<llvm::VectorType>(
                            ArgTys[D.getArgumentNumber()])) != Ty)
            {
                    errors << "intrinsic argument " << (ai+1)
                           << " must be of an LLVM vector type, but got ";
                    Ty->print(errors);
            }
            return;

        case IITDescriptor::TruncVecArgument:
            // This may only be used when referring to a previous vector argument.
            if (D.getArgumentNumber() >= ArgTys.size() ||
                !isa<llvm::VectorType>(ArgTys[D.getArgumentNumber()]) ||
                llvm::VectorType::getTruncatedElementVectorType(
                        cast<llvm::VectorType>(
                            ArgTys[D.getArgumentNumber()])) != Ty)
            {
                    errors << "intrinsic argument " << (ai+1)
                           << " must be of an LLVM vector type, but got ";
                    Ty->print(errors);
            }
            return;
        }
        llvm_unreachable("unhandled");
    }

    void run() {
        using namespace llvm;

        llvm::raw_string_ostream errors(outError);

        SmallVector<Intrinsic::IITDescriptor, 8> Table;
        getIntrinsicInfoTableEntries(ID, Table);
        ArrayRef<Intrinsic::IITDescriptor> TableRef = Table;

        skipReturnType(TableRef);

        for (unsigned i = 0, e = inputTypes.size(); i != e; ++i) {
            VerifyIntrinsicType(inputTypes[i], TableRef, errors, i);
        }
        if (!TableRef.empty()) {
            errors << "intrinsic has too few arguments";
        }

        errors.flush();
    }

    bool ok() const { return outError.empty(); }
};

static TypePtr intrinsicOutputType(llvm::Type *ty)
{
    if (llvm::IntegerType *intTy = llvm::dyn_cast<llvm::IntegerType>(ty)) {
        switch (intTy->getBitWidth()) {
        case 8:
            return uint8Type;
        case 16:
            return uint16Type;
        case 32:
            return uint32Type;
        case 64:
            return uint64Type;
        case 128:
            return uint128Type;
        default:
            return NULL;
        }
    } else if (ty == llvm::Type::getFloatTy(ty->getContext()))
        return float32Type;
    else if (ty == llvm::Type::getDoubleTy(ty->getContext()))
        return float64Type;
    else if (llvm::PointerType *pointerTy = llvm::dyn_cast<llvm::PointerType>(ty)) {
        TypePtr baseType = intrinsicOutputType(pointerTy->getPointerElementType());
        if (baseType != NULL)
            return pointerType(baseType);
        else
            return NULL;
    } else if (llvm::VectorType *vectorTy = llvm::dyn_cast<llvm::VectorType>(ty)) {
        TypePtr baseType = intrinsicOutputType(vectorTy->getElementType());
        if (baseType != NULL)
            return vecType(baseType, vectorTy->getNumElements());
        else
            return NULL;
    } else
        return NULL;
}

static MultiPValuePtr intrinsicOutputTypes(llvm::Function *instance)
{

    MultiPValuePtr ret = new MultiPValue();
    llvm::Type *ty = instance->getFunctionType()->getReturnType();

    if (ty->isVoidTy())
        return ret;

    TypePtr outType = intrinsicOutputType(ty);
    if (outType == NULL) {
        std::string errorBuf;
        llvm::raw_string_ostream errors(errorBuf);
        errors << "intrinsic returning LLVM type ";
        ty->print(errors);
        errors << " not yet supported";
        error(errors.str());
    }

    ret->add(PVData(outType, true));
    return ret;
}

static MultiPValuePtr analyzeIntrinsic(IntrinsicSymbol *intrin, MultiPValue *args)
{
    vector<TypePtr> argsKey;
    args->toArgsKey(&argsKey);

    map<vector<TypePtr>, IntrinsicInstance>::const_iterator instancePair
        = intrin->instances.find(argsKey);
    if (instancePair != intrin->instances.end()) {
        IntrinsicInstance const &instance = instancePair->second;
        if (!instance.error.empty()) {
            error(instance.error);
            return NULL;
        } else {
            assert(instance.function != NULL);
            return instance.outputTypes;
        }
    } else {
        vector<llvm::Type *> inputTypes;
        for (vector<TypePtr>::const_iterator i = argsKey.begin(), end = argsKey.end();
             i != end;
             ++i)
        {
            Type *type = i->ptr();
            if (type->typeKind == STATIC_TYPE) {
                StaticType *staticType = (StaticType*)type;
                if (staticType->obj->objKind != VALUE_HOLDER)
                    // XXX defer error for analyzeCallDefined
                    error("intrinsic static arguments must be boolean or integer statics");
                ValueHolder *vh = (ValueHolder*)staticType->obj.ptr();
                type = vh->type.ptr();
            }
            inputTypes.push_back(llvmType(type));
        }
        IntrinsicAnalyzer ia(intrin->id, inputTypes);
        ia.run();
        IntrinsicInstance &instance = intrin->instances[argsKey];
        if (ia.outError.empty()) {
            instance.function = llvm::Intrinsic::getDeclaration(llvmModule,
                                                                intrin->id,
                                                                ia.ArgTys);
            instance.outputTypes = intrinsicOutputTypes(instance.function);
            return instance.outputTypes;
        } else {
            instance.function = NULL;
            instance.outputTypes = NULL;
            instance.error.swap(ia.outError);
            error(instance.error); // XXX factor this out and use in analyzeCallDefined
            return NULL;
        }
    }
}

//
// analyzeCallExpr
//

MultiPValuePtr analyzeCallExpr(ExprPtr callable,
                               ExprListPtr args,
                               EnvPtr env)
{
    PVData pv = analyzeOne(callable, env);
    if (!pv.ok())
        return NULL;
    switch (pv.type->typeKind) {
    case CODE_POINTER_TYPE : {
        MultiPValuePtr mpv = analyzeMulti(args, env, 0);
        if (!mpv)
            return NULL;
        return analyzeCallPointer(pv, mpv);
    }
    default:
        break;
    }
    ObjectPtr obj = unwrapStaticType(pv.type);
    if (!obj) {
        ExprListPtr args2 = new ExprList(callable);
        args2->add(args);
        return analyzeCallExpr(operator_expr_call(), args2, env);
    }

    switch (obj->objKind) {

    case TYPE :
    case RECORD_DECL :
    case VARIANT_DECL :
    case PROCEDURE :
    case GLOBAL_ALIAS :
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
        InvokeEntry* entry =
            analyzeCallable(obj, argsKey, argsTempness);
        if (entry->callByName)
            return analyzeCallByName(entry, callable, args, env);
        if (!entry->analyzed)
            return NULL;
        return analyzeReturn(entry->returnIsRef, entry->returnTypes);
    }

    case INTRINSIC : {
        IntrinsicSymbol *intrin = (IntrinsicSymbol*)obj.ptr();
        MultiPValuePtr mpv = analyzeMulti(args, env, 0);
        if (!mpv)
            return NULL;
        return analyzeIntrinsic(intrin, mpv.ptr());
    }

    default :
        error("invalid call expression");
        return NULL;

    }
}



//
// analyzeDispatch
//

PVData analyzeDispatchIndex(PVData const &pv, unsigned tag)
{
    MultiPValuePtr args = new MultiPValue();
    args->add(pv);
    ValueHolderPtr vh = intToValueHolder((int)tag);
    args->add(staticPValue(vh.ptr()));

    MultiPValuePtr out =
        analyzeCallValue(staticPValue(operator_dispatchIndex()), args);
    ensureArity(out, 1);
    return out->values[0];
}

MultiPValuePtr analyzeDispatch(ObjectPtr obj,
                               MultiPValuePtr args,
                               llvm::ArrayRef<unsigned> dispatchIndices)
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
    PVData const &pvDispatch = args->values[index];
    unsigned memberCount = dispatchTagCount(pvDispatch.type);

    MultiPValuePtr result;
    vector<TypePtr> dispatchedTypes;
    for (unsigned i = 0; i < memberCount; ++i) {
        MultiPValuePtr args2 = new MultiPValue();
        args2->add(prefix);
        PVData pvDispatch2 = analyzeDispatchIndex(pvDispatch, i);
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
                    PVData const &pv = result->values[j];
                    PVData const &pv2 = result2->values[j];
                    if (pv.type != pv2.type) {
                        ok = false;
                        break;
                    }
                    if (pv.isTemp != pv2.isTemp) {
                        ok = false;
                        break;
                    }
                }
            }
            if (!ok) {
                string buf;
                llvm::raw_string_ostream ostr(buf);
                ostr << "mismatching result types with dispatch";
                ostr << "\n    expected ";
                unsigned j = 0;
                for (;j < result->size(); ++j) {
                    if (j != 0) ostr << ", ";
                    ostr << result->values[j].type;
                }
                ostr << "\n        from dispatching to ";
                for (j = 0; j < dispatchedTypes.size(); ++j) {
                    if (j != 0) ostr << ", ";
                    ostr << dispatchedTypes[j];
                }
                ostr << "\n     but got ";
                for (j = 0; j < result->size(); ++j) {
                    if (j != 0) ostr << ", ";
                    ostr << result2->values[j].type;
                }
                ostr << "\n        when dispatching to " << pvDispatch2.type;
                argumentError(index, ostr.str());
            }
        }
        dispatchedTypes.push_back(pvDispatch2.type);
    }
    assert(result.ptr());
    return result;
}



//
// analyzeCallValue
//

MultiPValuePtr analyzeCallValue(PVData const &callable,
                                MultiPValuePtr args)
{
    switch (callable.type->typeKind) {
    case CODE_POINTER_TYPE :
        return analyzeCallPointer(callable, args);
    default:
        break;
    }
    ObjectPtr obj = unwrapStaticType(callable.type);
    if (!obj) {
        MultiPValuePtr args2 = new MultiPValue(callable);
        args2->add(args);
        return analyzeCallValue(staticPValue(operator_call()), args2);
    }

    switch (obj->objKind) {

    case TYPE :
    case RECORD_DECL :
    case VARIANT_DECL :
    case PROCEDURE :
    case GLOBAL_ALIAS :
    case PRIM_OP : {
        if ((obj->objKind == PRIM_OP) && !isOverloadablePrimOp(obj)) {
            PrimOpPtr x = (PrimOp *)obj.ptr();
            return analyzePrimOp(x, args);
        }
        vector<TypePtr> argsKey;
        vector<ValueTempness> argsTempness;
        computeArgsKey(args, argsKey, argsTempness);
        CompileContextPusher pusher(obj, argsKey);
        InvokeEntry* entry =
            analyzeCallable(obj, argsKey, argsTempness);
        if (entry->callByName) {
            ExprListPtr objectExprs = new ExprList();
            for (PVData const *i = args->values.begin(), *end = args->values.end();
                 i < end;
                 ++i)
            {
                objectExprs->add(new ObjectExpr(new PValue(*i)));
            }
            ExprPtr callableObject = new ObjectExpr(new PValue(callable));
            callableObject->location = topLocation();
            return analyzeCallByName(entry, callableObject, objectExprs, new Env());
        } else if (!entry->analyzed)
            return NULL;
        else
            return analyzeReturn(entry->returnIsRef, entry->returnTypes);
    }

    case INTRINSIC : {
        IntrinsicSymbol *intrin = (IntrinsicSymbol*)obj.ptr();
        return analyzeIntrinsic(intrin, args.ptr());
    }

    default :
        error("invalid call expression");
        return NULL;

    }
}



//
// analyzeCallPointer
//

MultiPValuePtr analyzeCallPointer(PVData const &x,
                                  MultiPValuePtr /*args*/)
{
    switch (x.type->typeKind) {

    case CODE_POINTER_TYPE : {
        CodePointerType *y = (CodePointerType *)x.type.ptr();
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
                      llvm::ArrayRef<TypePtr> argsKey,
                      llvm::ArrayRef<ValueTempness> argsTempness)
{
    MatchFailureError failures;
    InvokeEntry* entry = lookupInvokeEntry(x, argsKey, argsTempness, failures);

    if (!entry || !entry->code->hasBody())
        return false;
    return true;
}

InvokeEntry* analyzeCallable(ObjectPtr x,
                               llvm::ArrayRef<TypePtr> argsKey,
                               llvm::ArrayRef<ValueTempness> argsTempness)
{
    MatchFailureError failures;
    InvokeEntry* entry = lookupInvokeEntry(x, argsKey, argsTempness, failures);

    if (!entry || !entry->code->hasBody()) {
        matchFailureError(failures);
    }

    if (entry->parent->shouldLog)
        matchFailureLog(failures);

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

MultiPValuePtr analyzeCallByName(InvokeEntry* entry,
                                 ExprPtr callable,
                                 ExprListPtr args,
                                 EnvPtr env)
{
    assert(entry->callByName);

    CodePtr code = entry->code;
    assert(code->body.ptr());

    if (code->hasReturnSpecs()) {
        vector<uint8_t> returnIsRef;
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
    unsigned i = 0, j = 0;
    for (; i < entry->varArgPosition; ++i) {
        ExprPtr expr = foreignExpr(env, args->exprs[i]);
        addLocal(bodyEnv, entry->fixedArgNames[i], expr.ptr());
    }
    if (entry->varArgName.ptr()) {
        ExprListPtr varArgs = new ExprList();
        for (; j < args->size() - entry->fixedArgNames.size(); ++j) {
            ExprPtr expr = foreignExpr(env, args->exprs[i+j]);
            varArgs->add(expr);
        }
        addLocal(bodyEnv, entry->varArgName, varArgs.ptr());
        for (; i < entry->fixedArgNames.size(); ++i) {
            ExprPtr expr = foreignExpr(env, args->exprs[i+j]);
            addLocal(bodyEnv, entry->fixedArgNames[i], expr.ptr());
        }
    }

    AnalysisContext ctx;
    StatementAnalysis sa = analyzeStatement(code->body, bodyEnv, &ctx);
    if ((sa == SA_RECURSIVE) && (!ctx.returnInitialized))
        return NULL;
    if (ctx.returnInitialized) {
        return analyzeReturn(ctx.returnIsRef, ctx.returnTypes);
    }
    else if ((sa == SA_TERMINATED) && ctx.hasRecursivePropagation) {
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

static void unifyInterfaceReturns(InvokeEntry* entry)
{
    if (entry->parent->callable->objKind == TYPE) {
        string buf;
        llvm::raw_string_ostream out(buf);
        if (entry->returnTypes.size() != 1) {
            out << "constructor overload for type " << entry->parent->callable
                << " must return a single value of that type";
            error(entry->code, out.str());
        }
        if (entry->returnIsRef[0]) {
            out << "constructor overload for type " << entry->parent->callable
                << " must return by value";
            error(entry->code, out.str());
        }
        if (entry->returnTypes[0].ptr() != entry->parent->callable.ptr()) {
            out << "constructor overload for type " << entry->parent->callable
                << " returns type " << entry->returnTypes[0];
            error(entry->code, out.str());
        }
        return;
    }

    if (entry->parent->interface == NULL)
        return;

    CodePtr interfaceCode = entry->parent->interface->code;
    if (!interfaceCode->returnSpecsDeclared)
        return;

    vector<uint8_t> interfaceReturnIsRef;
    vector<TypePtr> interfaceReturnTypes;

    evaluateReturnSpecs(interfaceCode->returnSpecs,
                        interfaceCode->varReturnSpec,
                        entry->interfaceEnv,
                        interfaceReturnIsRef, interfaceReturnTypes);
    if (entry->returnTypes.size() != interfaceReturnTypes.size()) {
        string buf;
        llvm::raw_string_ostream out(buf);
        out << "interface declares " << interfaceReturnTypes.size()
            << " arguments, but got " << entry->returnTypes.size() << "\n"
            << "    interface at ";
        printFileLineCol(out, entry->parent->interface->location);
        error(entry->code, out.str());
    }
    for (size_t i = 0; i < interfaceReturnTypes.size(); ++i) {
        if (interfaceReturnTypes[i] != entry->returnTypes[i]) {
            string buf;
            llvm::raw_string_ostream out(buf);
            out << "return value " << i+1 << ": "
                << "interface declares return type " << interfaceReturnTypes[i]
                << ", but overload returns type " << entry->returnTypes[i] << "\n"
                << "    interface at ";
            printFileLineCol(out, entry->parent->interface->location);
            error(entry->code, out.str());
        }
        if (interfaceReturnIsRef[i] && !entry->returnIsRef[i]) {
            string buf;
            llvm::raw_string_ostream out(buf);
            out << "return value " << i+1 << ": "
                << "interface declares return by reference, but overload returns by value\n"
                << "    interface at ";
            printFileLineCol(out, entry->parent->interface->location);
            error(entry->code, out.str());
        }
    }
}

void analyzeCodeBody(InvokeEntry* entry)
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

    size_t i = 0;
    for (; i < entry->varArgPosition; ++i) {
        bool flag = entry->forwardedRValueFlags[i];
        addLocal(bodyEnv, entry->fixedArgNames[i], new PValue(entry->fixedArgTypes[i], flag));
    }
    if (entry->varArgName.ptr()) {
        MultiPValuePtr varArgs = new MultiPValue();
        size_t j = 0;
        for (; j < entry->varArgTypes.size(); ++j) {
            bool flag = entry->forwardedRValueFlags[i+j];
            PVData parg(entry->varArgTypes[j], flag);
            varArgs->values.push_back(parg);
        }
        addLocal(bodyEnv, entry->varArgName, varArgs.ptr());
        for (; i < entry->fixedArgNames.size(); ++i) {
            bool flag = entry->forwardedRValueFlags[i+j];
            addLocal(bodyEnv, entry->fixedArgNames[i], new PValue(entry->fixedArgTypes[i], flag));
        }
    }

    AnalysisContext ctx;
    StatementAnalysis sa = analyzeStatement(code->body, bodyEnv, &ctx);
    if ((sa == SA_RECURSIVE) && (!ctx.returnInitialized))
        return;
    if (ctx.returnInitialized) {
        entry->returnIsRef = ctx.returnIsRef;
        entry->returnTypes = ctx.returnTypes;
    }
    else if ((sa == SA_TERMINATED) && ctx.hasRecursivePropagation) {
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

static StatementAnalysis analyzeBlockStatement(StatementPtr stmt, EnvPtr &env, AnalysisContext* ctx)
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
        llvm::ArrayRef<StatementPtr> evaled = desugarEvalStatement(eval, env);
        for (size_t i = 0; i < evaled.size(); ++i) {
            StatementAnalysis sa = analyzeBlockStatement(evaled[i], env, ctx);
            if (sa != SA_FALLTHROUGH)
                return sa;
        }
        return SA_FALLTHROUGH;
    } else
        return analyzeStatement(stmt, env, ctx);
}

static EnvPtr analyzeStatementExpressionStatements(llvm::ArrayRef<StatementPtr> stmts, EnvPtr env)
{
    EnvPtr env2 = env;
    for (StatementPtr const *i = stmts.begin(), *end = stmts.end();
         i != end;
         ++i)
    {
        switch ((*i)->stmtKind) {
        case BINDING:
            env2 = analyzeBinding((Binding*)i->ptr(), env2);
            if (env2 == NULL)
                return NULL;
            break;

        case ASSIGNMENT:
        case VARIADIC_ASSIGNMENT:
        case INIT_ASSIGNMENT:
        case EXPR_STATEMENT:
            break;

        default:
            assert(false);
            return NULL;
        }
    }
    return env2;
}

static StatementAnalysis analyzeStatement(StatementPtr stmt, EnvPtr env, AnalysisContext* ctx)
{
    LocationContext loc(stmt->location);

    switch (stmt->stmtKind) {

    case BLOCK : {
        Block *block = (Block *)stmt.ptr();
        for (size_t i = 0; i < block->statements.size(); ++i) {
            StatementAnalysis sa = analyzeBlockStatement(block->statements[i], env, ctx);
            if (sa != SA_FALLTHROUGH)
                return sa;
        }
        return SA_FALLTHROUGH;
    }

    case LABEL :
    case BINDING :
    case ASSIGNMENT :
    case INIT_ASSIGNMENT :
    case VARIADIC_ASSIGNMENT :
        return SA_FALLTHROUGH;

    case GOTO :
        return SA_TERMINATED;

    case RETURN : {
        Return *x = (Return *)stmt.ptr();
        unsigned wantCount = x->isReturnSpecs ? 1 : 0;
        MultiPValuePtr mpv = analyzeMulti(x->values, env, wantCount);

        if (!mpv) {
            ctx->hasRecursivePropagation = true;
            return SA_RECURSIVE;
        }
        if (ctx->returnInitialized) {
            ensureArity(mpv, ctx->returnTypes.size());
            for (unsigned i = 0; i < mpv->size(); ++i) {
                PVData const &pv = mpv->values[i];
                bool byRef = returnKindToByRef(x->returnKind, pv);
                if (ctx->returnTypes[i] != pv.type)
                    argumentTypeError(i, ctx->returnTypes[i], pv.type);
                if (byRef != ctx->returnIsRef[i])
                    argumentError(i, "mismatching by-ref and "
                                  "by-value returns");
                if (byRef && pv.isTemp)
                    argumentError(i, "cannot return a temporary by reference");
            }
        }
        else {
            ctx->returnIsRef.clear();
            ctx->returnTypes.clear();
            for (unsigned i = 0; i < mpv->size(); ++i) {
                PVData const &pv = mpv->values[i];
                bool byRef = returnKindToByRef(x->returnKind, pv);
                ctx->returnIsRef.push_back(byRef);
                if (byRef && pv.isTemp)
                    argumentError(i, "cannot return a temporary by reference");
                ctx->returnTypes.push_back(pv.type);
            }
            ctx->returnInitialized = true;
        }
        return SA_TERMINATED;
    }

    case IF : {
        If *x = (If *)stmt.ptr();

        EnvPtr env2 = analyzeStatementExpressionStatements(x->conditionStatements, env);
        if (env2 == NULL) {
            ctx->hasRecursivePropagation = true;
            return SA_RECURSIVE;
        }

        MultiPValuePtr cond = analyzeExpr(x->condition, env2);

        if (cond->values.size() != 1) {
            arityError(1, cond->values.size());
        }

        PVData& condPvData = cond->values[0];

        BoolKind condKind = typeBoolKind(condPvData.type);

        StatementAnalysis thenResult = SA_FALLTHROUGH;
        StatementAnalysis elseResult = SA_FALLTHROUGH;
        if (condKind == BOOL_EXPR || condKind == BOOL_STATIC_TRUE)
            thenResult = analyzeStatement(x->thenPart, env2, ctx);
        if ((condKind == BOOL_EXPR || condKind == BOOL_STATIC_FALSE) && x->elsePart.ptr())
            elseResult = analyzeStatement(x->elsePart, env2, ctx);
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
        llvm::ArrayRef<StatementPtr> evaled = desugarEvalStatement(eval, env);
        for (size_t i = 0; i < evaled.size(); ++i) {
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

        EnvPtr env2 = analyzeStatementExpressionStatements(x->conditionStatements, env);
        if (env2 == NULL) {
            ctx->hasRecursivePropagation = true;
            return SA_RECURSIVE;
        }

        analyzeStatement(x->body, env2, ctx);
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

        EnvPtr catchEnv = new Env(env, true);

        result2 = analyzeStatement(x->desugaredCatchBlock, catchEnv, ctx);
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
            addLocal(env2, x->variable, new PValue(mpv->values[i]));
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

    case STATIC_ASSERT_STATEMENT:
        return SA_FALLTHROUGH;

    default :
        assert(false);
        return SA_FALLTHROUGH;
    }
}

void initializeStaticForClones(StaticForPtr x, size_t count)
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

static EnvPtr analyzeBinding(BindingPtr x, EnvPtr env)
{
    LocationContext loc(x->location);

    switch (x->bindingKind) {
    case VAR :
    case REF :
    case FORWARD : {

        MultiPValuePtr mpv = analyzeMulti(x->values, env, x->args.size());
        if (!mpv)
            return NULL;

        if(x->hasVarArg){
            if (mpv->values.size() < x->args.size()-1)
                arityMismatchError(x->args.size()-1, mpv->values.size(), /*hasVarArg=*/ true);
        } else
            if (mpv->values.size() != x->args.size())
                arityMismatchError(x->args.size(), mpv->values.size(), /*hasVarArg=*/ false);

        vector<TypePtr> key;
        for (unsigned i = 0; i < mpv->size(); ++i) {
            PVData const &pv = mpv->values[i];
            key.push_back(pv.type);
        }

        llvm::ArrayRef<PatternVar> pvars = x->patternVars;
        EnvPtr patternEnv = new Env(env);
        vector<PatternCellPtr> cells;
        vector<MultiPatternCellPtr> multiCells;
        initializePatternEnv(patternEnv, pvars, cells, multiCells);

        llvm::ArrayRef<FormalArgPtr> formalArgs = x->args;
        size_t varArgSize = key.size()-formalArgs.size()+1;
        for (size_t i = 0, j = 0; i < formalArgs.size(); ++i) {
            FormalArgPtr y = formalArgs[i];
            if(y->varArg){
                if (y->type.ptr()) {
                    ExprPtr unpack = new Unpack(y->type.ptr());
                    unpack->location = y->type->location;
                    MultiStaticPtr types = new MultiStatic();
                    for (; j < varArgSize; ++j) {
                        types->add(key[i+j].ptr());
                    }
                    --j;
                    if (!unifyMulti(evaluateMultiPattern(new ExprList(unpack), patternEnv), types))
                        matchBindingError(new MatchMultiBindingError(unsigned(formalArgs.size()), types, y));
                } else {
                    j = varArgSize-1;
                }
            } else {
                if (y->type.ptr()) {
                    if (!unifyPatternObj(evaluateOnePattern(y->type, patternEnv), key[i+j].ptr()))
                        matchBindingError(new MatchBindingError(unsigned(i+j), key[i+j], y));
                }
            }
        }

        EnvPtr staticEnv = new Env(env);
        for (size_t i = 0; i < pvars.size(); ++i) {
            if (pvars[i].isMulti) {
                MultiStaticPtr ms = derefDeep(multiCells[i].ptr());
                if (!ms)
                    error(pvars[i].name, "unbound pattern variable");
                addLocal(staticEnv, pvars[i].name, ms.ptr());
                x->patternTypes.push_back(ms.ptr());
            }
            else {
                ObjectPtr v = derefDeep(cells[i].ptr());
                if (!v)
                    error(pvars[i].name, "unbound pattern variable");
                addLocal(staticEnv, pvars[i].name, v.ptr());
                x->patternTypes.push_back(v.ptr());
            }
        }

        evaluatePredicate(x->patternVars, x->predicate, staticEnv);

        EnvPtr env2 = new Env(staticEnv);
        for (size_t i = 0, j = 0; i < formalArgs.size(); ++i) {
            FormalArgPtr y = formalArgs[i];
            if(y->varArg){
                MultiPValuePtr varArgs = new MultiPValue();
                for (; j < varArgSize; ++j) {
                    PVData parg(key[i+j], false);
                    varArgs->values.push_back(parg);
                }
                --j;
                addLocal(env2, y->name, varArgs.ptr());
            } else {
                addLocal(env2, y->name, new PValue(key[i+j], false));
            }
        }
        return env2;
    }

    case ALIAS : {
        ensureArity(x->args, 1);
        ensureArity(x->values->exprs, 1);
        EnvPtr env2 = new Env(env);
        ExprPtr y = foreignExpr(env, x->values->exprs[0]);
        addLocal(env2, x->args[0]->name, y.ptr());
        return env2;
    }

    default :
        assert(false);
        return NULL;

    }
}

bool returnKindToByRef(ReturnKind returnKind, PVData const &pv)
{
    switch (returnKind) {
    case RETURN_VALUE : return false;
    case RETURN_REF : return true;
    case RETURN_FORWARD : return !pv.isTemp;
    default : assert(false); return false;
    }
}



BoolKind typeBoolKind(TypePtr type) {
    if (type == boolType) {
        return BOOL_EXPR;
    } else if (type->typeKind == STATIC_TYPE) {
        bool staticValue;
        StaticType * staticType = (StaticType *) type.ptr();
        if (staticType->obj->objKind != VALUE_HOLDER) {
            error("expecting value holder");
        }
        ValueHolder* valueHolder = (ValueHolder *) staticType->obj.ptr();
        if (valueHolder->type != boolType) {
            typeError(boolType, valueHolder->type);
        }
        staticValue = valueHolder->as<bool>();
        return staticValue ? BOOL_STATIC_TRUE : BOOL_STATIC_FALSE;
    } else {
        typeError("Bool or static Bool", type);
        return BOOL_STATIC_TRUE;
    }
}

ExprPtr implicitUnpackExpr(size_t wantCount, ExprListPtr exprs) {
    if (wantCount >= 1 && exprs->size() == 1 && exprs->exprs[0]->exprKind != UNPACK)
        return exprs->exprs[0];
    else
        return NULL;
}

}
