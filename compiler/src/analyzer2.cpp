#include "clay.hpp"

namespace two {

MultiPValuePtr analyzeMulti(const vector<ExprPtr> &exprs, EnvPtr env);
PValuePtr analyzeOne(ExprPtr expr, EnvPtr env);
MultiPValuePtr analyzeExpr(ExprPtr expr, EnvPtr env);
MultiPValuePtr analyzeStaticObject(ObjectPtr x);
PValuePtr analyzeGlobalVariable(GlobalVariablePtr x);
PValuePtr analyzeExternalVariable(ExternalVariablePtr x);
void analyzeExternalProcedure(ExternalProcedurePtr x);
MultiPValuePtr analyzeIndexingExpr(ExprPtr indexable,
                                   const vector<ExprPtr> &args,
                                   EnvPtr env);
PValuePtr analyzeTypeConstructor(ObjectPtr obj, MultiStaticPtr args);
MultiPValuePtr analyzeFieldRefExpr(ExprPtr base,
                                   IdentifierPtr name,
                                   EnvPtr env);
MultiPValuePtr analyzeCallExpr(ExprPtr callable,
                               const vector<ExprPtr> &args,
                               EnvPtr env);

MultiPValuePtr analyzeCallValue(PValuePtr callable,
                                MultiPValuePtr args);

}


//
// utility procs
//

TypePtr objectType(ObjectPtr x)
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

ObjectPtr lowerValueHolder(ValueHolderPtr vh)
{
    ObjectPtr x = unwrapStaticType(vh->type);
    if (!x)
        return vh.ptr();
    return x;
}

bool unwrapStaticToType(ObjectPtr x, TypePtr &out)
{
    if (x->objKind != TYPE)
        return false;
    out = (Type *)x.ptr();
    return true;
}

TypePtr unwrapStaticToType(MultiStaticPtr x, unsigned index)
{
    TypePtr out;
    if (!unwrapStaticToType(x->values[index], out))
        argumentError(0, "expecting a type");
    return out;
}

bool unwrapStaticToTypeTuple(ObjectPtr x, vector<TypePtr> &out)
{
    TypePtr t;
    if (unwrapStaticToType(x, t)) {
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

void unwrapStaticToTypeTuple(MultiStaticPtr x, unsigned index,
                             vector<TypePtr> &out)
{
    if (!unwrapStaticToTypeTuple(x->values[index], out))
        argumentError(index, "expecting zero-or-more types");
}

bool unwrapStaticToInt(ObjectPtr x, int &out)
{
    if (x->objKind != VALUE_HOLDER)
        return false;
    ValueHolderPtr vh = (ValueHolder *)x.ptr();
    if (vh->type != cIntType)
        return false;
    out = *((int *)vh->buf);
    return true;
}

int unwrapStaticToInt(MultiStaticPtr x, unsigned index)
{
    int out = -1;
    if (!unwrapStaticToInt(x->values[index], out))
        argumentError(index, "expecting Int value");
    return out;
}



//
// staticPValue, kernelPValue
//

namespace two {

static PValuePtr staticPValue(ObjectPtr x)
{
    TypePtr t = staticType(x);
    return new PValue(t, true);
}

static PValuePtr kernelPValue(const string &name)
{
    return staticPValue(kernelName(name));
}



//
// analyzeMulti
//

MultiPValuePtr analyzeMulti(const vector<ExprPtr> &exprs, EnvPtr env)
{
    MultiPValuePtr out = new MultiPValue();
    for (unsigned i = 0; i < exprs.size(); ++i) {
        ExprPtr x = exprs[i];
        if (x->exprKind == VAR_ARGS_REF) {
            MultiPValuePtr y = analyzeExpr(x, env);
            if (!y)
                return NULL;
            out->add(y);
        }
        else {
            PValuePtr y = two::analyzeOne(x, env);
            if (!y)
                return NULL;
            out->values.push_back(y);
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
    ensureArity(x, 1);
    return x->values[0];
}



//
// analyzeExpr
//

MultiPValuePtr analyzeExpr(ExprPtr expr, EnvPtr env)
{
    switch (expr->exprKind) {

    case BOOL_LITERAL : {
        return new MultiPValue(new PValue(boolType, true));
    }

    case INT_LITERAL : {
        IntLiteral *x = (IntLiteral *)expr.ptr();
        ValueHolderPtr v = parseIntLiteral(x);
        return new MultiPValue(new PValue(v->type, true));
    }
        
    case FLOAT_LITERAL : {
        FloatLiteral *x = (FloatLiteral *)expr.ptr();
        ValueHolderPtr v = parseFloatLiteral(x);
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
        PValuePtr pv = new PValue(arrayType(int8Type, x->value.size()), true);
        MultiPValuePtr args = new MultiPValue(pv);
        return analyzeCallValue(kernelPValue("stringRef"), args);
    }

    case NAME_REF : {
        NameRef *x = (NameRef *)expr.ptr();
        ObjectPtr y = lookupEnv(env, x->name);
        if (y->objKind == EXPRESSION)
            return analyzeExpr((Expr *)y.ptr(), env);
        return two::analyzeStaticObject(y);
    }

    case TUPLE : {
        Tuple *x = (Tuple *)expr.ptr();
        if ((x->args.size() == 1) &&
            (x->args[0]->exprKind != VAR_ARGS_REF))
        {
            return analyzeExpr(x->args[0], env);
        }
        return analyzeCallExpr(primNameRef("tuple"), x->args, env);
    }

    case ARRAY : {
        Array *x = (Array *)expr.ptr();
        return analyzeCallExpr(primNameRef("array"), x->args, env);
    }

    case INDEXING : {
        Indexing *x = (Indexing *)expr.ptr();
        return analyzeIndexingExpr(x->expr, x->args, env);
    }

    case CALL : {
        Call *x = (Call *)expr.ptr();
        return analyzeCallExpr(x->expr, x->args, env);
    }

    case FIELD_REF : {
        FieldRef *x = (FieldRef *)expr.ptr();
        return analyzeFieldRefExpr(x->expr, x->name, env);
    }

    case TUPLE_REF : {
        TupleRef *x = (TupleRef *)expr.ptr();
        PValuePtr pv = two::analyzeOne(x->expr, env);
        if (!pv)
            return NULL;
        MultiPValuePtr args = new MultiPValue(pv);
        ValueHolderPtr vh = sizeTToValueHolder(x->index);
        args->values.push_back(staticPValue(vh.ptr()));
        return analyzeCallValue(kernelPValue("tupleRef"), args);
    }

    case UNARY_OP : {
        UnaryOp *x = (UnaryOp *)expr.ptr();
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

    case AND : {
        And *x = (And *)expr.ptr();
        PValuePtr a = two::analyzeOne(x->expr1, env);
        if (!a)
            return NULL;
        if (a->isTemp)
            return new MultiPValue(new PValue(a->type, true));
        PValuePtr b = two::analyzeOne(x->expr2, env);
        if (!b)
            return NULL;
        if (a->type != b->type)
            error("type mismatch in 'and' expression");
        PValuePtr pv = new PValue(a->type, a->isTemp || b->isTemp);
        return new MultiPValue(pv);
    }

    case OR : {
        Or *x = (Or *)expr.ptr();
        PValuePtr a = two::analyzeOne(x->expr1, env);
        if (!a)
            return NULL;
        if (a->isTemp)
            return new MultiPValue(new PValue(a->type, true));
        PValuePtr b = two::analyzeOne(x->expr2, env);
        if (!b)
            return NULL;
        if (a->type != b->type)
            error("type mismatch in 'and' expression");
        PValuePtr pv = new PValue(a->type, a->isTemp || b->isTemp);
        return new MultiPValue(pv);
    }

    case LAMBDA : {
        Lambda *x = (Lambda *)expr.ptr();
        if (!x->initialized)
            initializeLambda(x, env);
        return analyzeExpr(x->converted, env);
    }

    case VAR_ARGS_REF : {
        ObjectPtr y = lookupEnv(env, new Identifier("%varArgs"));
        VarArgsInfo *vaInfo = (VarArgsInfo *)y.ptr();
        if (!vaInfo->hasVarArgs)
            error("varargs unavailable");
        return analyzeMulti(vaInfo->varArgs, env);
    }

    case NEW : {
        New *x = (New *)expr.ptr();
        if (!x->desugared)
            x->desugared = desugarNew(x);
        return analyzeExpr(x->desugared, env);
    }

    case STATIC_EXPR : {
        StaticExpr *x = (StaticExpr *)expr.ptr();
        if (!x->desugared)
            x->desugared = desugarStaticExpr(x);
        return analyzeExpr(x->desugared, env);
    }

    case FOREIGN_EXPR : {
        ForeignExpr *x = (ForeignExpr *)expr.ptr();
        return analyzeExpr(x->expr, x->getEnv());
    }

    case OBJECT_EXPR : {
        error("ObjectExpr deprecated");
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
        PValuePtr pv = new PValue(y->type, true);
        return new MultiPValue(pv);
    }

    case GLOBAL_VARIABLE : {
        GlobalVariable *y = (GlobalVariable *)x.ptr();
        PValuePtr pv = analyzeGlobalVariable(y);
        return new MultiPValue(pv);
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

    case STATIC_GLOBAL : {
        StaticGlobal *y = (StaticGlobal *)x.ptr();
        return analyzeExpr(y->expr, y->env);
    }

    case EVALUE : {
        EValue *y = (EValue *)x.ptr();
        PValuePtr pv = new PValue(y->type, false);
        return new MultiPValue(pv);
    }

    case MULTI_EVALUE : {
        MultiEValue *y = (MultiEValue *)x.ptr();
        MultiPValuePtr z = new MultiPValue();
        for (unsigned i = 0; i < y->values.size(); ++i)
            z->values.push_back(new PValue(y->values[i]->type, false));
        return z;
    }

    case CVALUE : {
        CValue *y = (CValue *)x.ptr();
        PValuePtr pv = new PValue(y->type, false);
        return new MultiPValue(pv);
    }

    case MULTI_CVALUE : {
        MultiCValue *y = (MultiCValue *)x.ptr();
        MultiPValuePtr z = new MultiPValue();
        for (unsigned i = 0; i < y->values.size(); ++i)
            z->values.push_back(new PValue(y->values[i]->type, false));
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

    case TYPE :
    case PRIM_OP :
    case PROCEDURE :
    case RECORD :
    case MODULE_HOLDER :
    case IDENTIFIER : {
        TypePtr t = staticType(x);
        PValuePtr pv = new PValue(t, true);
        return new MultiPValue(pv);
    }

    case PATTERN :
        error("pattern cannot be used as value");
        return NULL;

    default :
        error("invalid static object");
        return NULL;

    }
}



//
// analyzeGlobalVariable
//

PValuePtr analyzeGlobalVariable(GlobalVariablePtr x)
{
    if (!x->type) {
        if (x->analyzing)
            return NULL;
        x->analyzing = true;
        PValuePtr pv = two::analyzeOne(x->expr, x->env);
        x->analyzing = false;
        if (!pv)
            return NULL;
        x->type = pv->type;
    }
    return new PValue(x->type, false);
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
    CallingConv callingConv = CC_DEFAULT;
    if (x->attrStdCall)
        callingConv = CC_STDCALL;
    else if (x->attrFastCall)
        callingConv = CC_FASTCALL;
    x->ptrType = cCodePointerType(callingConv, argTypes,
                                  x->hasVarArgs, x->returnType2);
    x->analyzed = true;
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
        case PRIM_RefCodePointer :
        case PRIM_CCodePointer :
        case PRIM_StdCallCodePointer :
        case PRIM_FastCallCodePointer :
        case PRIM_Array :
        case PRIM_Tuple :
        case PRIM_Static :
            return true;
        default :
            return false;
        }
    }
    else if (x->objKind == RECORD) {
        return true;
    }
    else {
        return false;
    }
}

MultiPValuePtr analyzeIndexingExpr(ExprPtr indexable,
                                   const vector<ExprPtr> &args,
                                   EnvPtr env)
{
    PValuePtr pv = two::analyzeOne(indexable, env);
    if (!pv)
        return NULL;
    ObjectPtr obj = unwrapStaticType(pv->type);
    if (obj.ptr()) {
        if (isTypeConstructor(obj)) {
            MultiStaticPtr params = evaluateMultiStatic(args, env);
            PValuePtr out = analyzeTypeConstructor(obj, params);
            return new MultiPValue(out);
        }
        if (obj->objKind != VALUE_HOLDER)
            error("invalid indexing operation");
    }
    MultiPValuePtr mpv = analyzeMulti(args, env);
    if (!mpv)
        return NULL;
    MultiPValuePtr args2 = new MultiPValue(pv);
    args2->add(mpv);
    return analyzeCallValue(kernelPValue("index"), args2);
}



//
// analyzeTypeConstructor
//

PValuePtr analyzeTypeConstructor(ObjectPtr obj, MultiStaticPtr args)
{
    if (obj->objKind == PRIM_OP) {
        PrimOpPtr x = (PrimOp *)obj.ptr();

        switch (x->objKind) {

        case PRIM_Pointer : {
            ensureArity(args, 1);
            TypePtr pointeeType = unwrapStaticToType(args, 0);
            return staticPValue(pointerType(pointeeType).ptr());
        }

        case PRIM_CodePointer :
        case PRIM_RefCodePointer : {
            ensureArity(args, 2);
            vector<TypePtr> argTypes;
            unwrapStaticToTypeTuple(args, 0, argTypes);
            vector<TypePtr> returnTypes;
            unwrapStaticToTypeTuple(args, 1, returnTypes);
            bool byRef = (x->objKind == PRIM_RefCodePointer);
            vector<bool> returnIsRef(returnTypes.size(), byRef);
            TypePtr t = codePointerType(argTypes, returnIsRef, returnTypes);
            return staticPValue(t.ptr());
        }

        case PRIM_CCodePointer :
        case PRIM_StdCallCodePointer :
        case PRIM_FastCallCodePointer : {
            ensureArity(args, 2);
            vector<TypePtr> argTypes;
            unwrapStaticToTypeTuple(args, 0, argTypes);
            vector<TypePtr> returnTypes;
            unwrapStaticToTypeTuple(args, 1, returnTypes);
            if (returnTypes.size() > 1)
                argumentError(1, "C code cannot return more than one value");
            TypePtr returnType;
            if (returnTypes.size() == 1)
                returnType = returnTypes[0];
            CallingConv cc;
            switch (x->objKind) {
            case PRIM_CCodePointer : cc = CC_DEFAULT; break;
            case PRIM_StdCallCodePointer : cc = CC_STDCALL; break;
            case PRIM_FastCallCodePointer : cc = CC_FASTCALL; break;
            default : assert(false);
            }
            TypePtr t = cCodePointerType(cc, argTypes, false, returnType);
            return staticPValue(t.ptr());
        }

        case PRIM_Array : {
            ensureArity(args, 2);
            TypePtr t = unwrapStaticToType(args, 0);
            int size = unwrapStaticToInt(args, 1);
            TypePtr at = arrayType(t, size);
            return staticPValue(at.ptr());
        }

        case PRIM_Tuple : {
            vector<TypePtr> types;
            for (unsigned i = 0; i < args->size(); ++i)
                types.push_back(unwrapStaticToType(args, i));
            TypePtr t = tupleType(types);
            return staticPValue(t.ptr());
        }

        case PRIM_Static : {
            ensureArity(args, 1);
            TypePtr t = staticType(args->values[0]);
            return staticPValue(t.ptr());
        }

        default :
            assert(false);
            return NULL;
        }
    }
    else if (obj->objKind == RECORD) {
        RecordPtr x = (Record *)obj.ptr();
        if (args->size() != x->patternVars.size())
            arityError(x->patternVars.size(), args->size());
        return staticPValue(recordType(x, args->values).ptr());
    }
    else {
        assert(false);
        return NULL;
    }
}



//
// analyzeFieldRefExpr
//

MultiPValuePtr analyzeFieldRefExpr(ExprPtr base,
                                   IdentifierPtr name,
                                   EnvPtr env)
{
    PValuePtr pv = two::analyzeOne(base, env);
    ObjectPtr obj = unwrapStaticType(pv->type);
    if (obj.ptr()) {
        if (obj->objKind == MODULE_HOLDER) {
            ModuleHolderPtr y = (ModuleHolder *)obj.ptr();
            ObjectPtr z = lookupModuleMember(y, name);
            return two::analyzeStaticObject(z);
        }
        if (obj->objKind != VALUE_HOLDER)
            error("invalid field access");
    }
    MultiPValuePtr args = new MultiPValue(pv);
    args->values.push_back(staticPValue(name.ptr()));
    return analyzeCallValue(kernelPValue("fieldRef"), args);
}



//
// analyzeCallExpr
//

MultiPValuePtr analyzeCallExpr(ExprPtr callable,
                               const vector<ExprPtr> &args,
                               EnvPtr env)
{
    return NULL;
}



//
// analyzeCallValue
//

MultiPValuePtr analyzeCallValue(PValuePtr callable,
                                MultiPValuePtr args)
{
    return NULL;
}

}
