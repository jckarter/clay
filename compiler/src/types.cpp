#include "clay.hpp"

namespace clay {

TypePtr boolType;
TypePtr int8Type;
TypePtr int16Type;
TypePtr int32Type;
TypePtr int64Type;
TypePtr int128Type;
TypePtr uint8Type;
TypePtr uint16Type;
TypePtr uint32Type;
TypePtr uint64Type;
TypePtr uint128Type;
TypePtr float32Type;
TypePtr float64Type;
TypePtr float80Type;
TypePtr imag32Type;
TypePtr imag64Type;
TypePtr imag80Type;
TypePtr complex32Type;
TypePtr complex64Type;
TypePtr complex80Type;

TypePtr cIntType;
TypePtr cSizeTType;
TypePtr cPtrDiffTType;

static vector<vector<PointerTypePtr> > pointerTypes;
static vector<vector<CodePointerTypePtr> > codePointerTypes;
static vector<vector<CCodePointerTypePtr> > cCodePointerTypes;
static vector<vector<ArrayTypePtr> > arrayTypes;
static vector<vector<VecTypePtr> > vecTypes;
static vector<vector<TupleTypePtr> > tupleTypes;
static vector<vector<UnionTypePtr> > unionTypes;
static vector<vector<RecordTypePtr> > recordTypes;
static vector<vector<VariantTypePtr> > variantTypes;
static vector<vector<StaticTypePtr> > staticTypes;

//
// type size and alignment for debug generation
//

static size_t llTypeSize(llvm::Type *llt) {
    return (size_t)llvmTargetData->getTypeAllocSize(llt);
}

static size_t llTypeAlignment(llvm::Type *llt) {
    return (size_t)llvmTargetData->getABITypeAlignment(llt);
}

static size_t debugTypeSize(llvm::Type *llt) {
    return llTypeSize(llt)*8;
}

static size_t debugTypeAlignment(llvm::Type *llt) {
    return llTypeAlignment(llt)*8;
}

void initTypes() {
    boolType = new BoolType();
    int8Type = new IntegerType(8, true);
    int16Type = new IntegerType(16, true);
    int32Type = new IntegerType(32, true);
    int64Type = new IntegerType(64, true);
    int128Type = new IntegerType(128, true);
    uint8Type = new IntegerType(8, false);
    uint16Type = new IntegerType(16, false);
    uint32Type = new IntegerType(32, false);
    uint64Type = new IntegerType(64, false);
    uint128Type = new IntegerType(128, false);
    float32Type = new FloatType(32, false);
    float64Type = new FloatType(64, false);
    float80Type = new FloatType(80, false);
    imag32Type = new FloatType(32, true);
    imag64Type = new FloatType(64, true);
    imag80Type = new FloatType(80, true);
    complex32Type = new ComplexType(32);
    complex64Type = new ComplexType(64);
    complex80Type = new ComplexType(80);

    cIntType = int32Type;
    switch (llvmTargetData->getPointerSizeInBits()) {
    case 32 :
        cSizeTType = uint32Type;
        cPtrDiffTType = int32Type;
        break;
    case 64 :
        cSizeTType = uint64Type;
        cPtrDiffTType = int64Type;
        break;
    default :
        assert(false);
    }

    int N = 1024;
    pointerTypes.resize(N);
    codePointerTypes.resize(N);
    cCodePointerTypes.resize(N);
    arrayTypes.resize(N);
    vecTypes.resize(N);
    tupleTypes.resize(N);
    unionTypes.resize(N);
    recordTypes.resize(N);
    variantTypes.resize(N);
    staticTypes.resize(N);
}

TypePtr integerType(int bits, bool isSigned) {
    if (isSigned)
        return intType(bits);
    else
        return uintType(bits);
}

TypePtr intType(int bits) {
    switch (bits) {
    case 8 : return int8Type;
    case 16 : return int16Type;
    case 32 : return int32Type;
    case 64 : return int64Type;
    case 128 : return int128Type;
    default :
        assert(false);
        return NULL;
    }
}

TypePtr uintType(int bits) {
    switch (bits) {
    case 8 : return uint8Type;
    case 16 : return uint16Type;
    case 32 : return uint32Type;
    case 64 : return uint64Type;
    case 128 : return uint128Type;
    default :
        assert(false);
        return NULL;
    }
}

TypePtr floatType(int bits) {
    switch (bits) {
    case 32 : return float32Type;
    case 64 : return float64Type;
    case 80 : return float80Type;
    default :
        assert(false);
        return NULL;
    }
}

TypePtr imagType(int bits) {
    switch (bits) {
    case 32 : return imag32Type;
    case 64 : return imag64Type;
    case 80 : return imag80Type;
    default :
        assert(false);
        return NULL;
    }
}

TypePtr complexType(int bits) {
    switch (bits) {
    case 32 : return complex32Type;
    case 64 : return complex64Type;
    case 80 : return complex80Type;
    default :
        assert(false);
        return NULL;
    }
}

static int pointerHash(void *p) {
    return int(size_t(p));
}

TypePtr pointerType(TypePtr pointeeType) {
    int h = pointerHash(pointeeType.ptr());
    h &= pointerTypes.size() - 1;
    vector<PointerTypePtr>::iterator i, end;
    for (i = pointerTypes[h].begin(), end = pointerTypes[h].end();
         i != end; ++i) {
        PointerType *t = i->ptr();
        if (t->pointeeType == pointeeType)
            return t;
    }
    PointerTypePtr t = new PointerType(pointeeType);
    pointerTypes[h].push_back(t);
    return t.ptr();
}

TypePtr codePointerType(const vector<TypePtr> &argTypes,
                        const vector<bool> &returnIsRef,
                        const vector<TypePtr> &returnTypes) {
    assert(returnIsRef.size() == returnTypes.size());
    int h = 0;
    for (unsigned i = 0; i < argTypes.size(); ++i) {
        h += pointerHash(argTypes[i].ptr());
    }
    for (unsigned i = 0; i < returnTypes.size(); ++i) {
        int factor = returnIsRef[i] ? 23 : 11;
        h += factor*pointerHash(returnTypes[i].ptr());
    }
    h &= codePointerTypes.size() - 1;
    vector<CodePointerTypePtr> &bucket = codePointerTypes[h];
    for (unsigned i = 0; i < bucket.size(); ++i) {
        CodePointerType *t = bucket[i].ptr();
        if ((t->argTypes == argTypes) &&
            (t->returnIsRef == returnIsRef) &&
            (t->returnTypes == returnTypes))
        {
            return t;
        }
    }
    CodePointerTypePtr t =
        new CodePointerType(argTypes, returnIsRef, returnTypes);
    bucket.push_back(t);
    return t.ptr();
}

TypePtr cCodePointerType(CallingConv callingConv,
                         const vector<TypePtr> &argTypes,
                         bool hasVarArgs,
                         TypePtr returnType) {
    int h = int(callingConv)*100;
    for (unsigned i = 0; i < argTypes.size(); ++i) {
        h += pointerHash(argTypes[i].ptr());
    }
    h += (hasVarArgs ? 1 : 0);
    if (returnType.ptr())
        h += pointerHash(returnType.ptr());
    h &= cCodePointerTypes.size() - 1;
    vector<CCodePointerTypePtr> &bucket = cCodePointerTypes[h];
    for (unsigned i = 0; i < bucket.size(); ++i) {
        CCodePointerType *t = bucket[i].ptr();
        if ((t->callingConv == callingConv) &&
            (t->argTypes == argTypes) &&
            (t->hasVarArgs == hasVarArgs) &&
            (t->returnType == returnType))
        {
            return t;
        }
    }
    CCodePointerTypePtr t = new CCodePointerType(callingConv,
                                                 argTypes,
                                                 hasVarArgs,
                                                 returnType);
    bucket.push_back(t);
    return t.ptr();
}

TypePtr arrayType(TypePtr elementType, int size) {
    int h = pointerHash(elementType.ptr()) + size;
    h &= arrayTypes.size() - 1;
    vector<ArrayTypePtr>::iterator i, end;
    for (i = arrayTypes[h].begin(), end = arrayTypes[h].end();
         i != end; ++i) {
        ArrayType *t = i->ptr();
        if ((t->elementType == elementType) && (t->size == size))
            return t;
    }
    ArrayTypePtr t = new ArrayType(elementType, size);
    arrayTypes[h].push_back(t);
    return t.ptr();
}

TypePtr vecType(TypePtr elementType, int size) {
    if (elementType->typeKind != INTEGER_TYPE && elementType->typeKind != FLOAT_TYPE)
        error("Vec element type must be an integer or float type");
    int h = pointerHash(elementType.ptr()) + size;
    h &= vecTypes.size() - 1;
    vector<VecTypePtr>::iterator i, end;
    for (i = vecTypes[h].begin(), end = vecTypes[h].end();
         i != end; ++i) {
        VecType *t = i->ptr();
        if ((t->elementType == elementType) && (t->size == size))
            return t;
    }
    VecTypePtr t = new VecType(elementType, size);
    vecTypes[h].push_back(t);
    return t.ptr();
}

TypePtr tupleType(const vector<TypePtr> &elementTypes) {
    int h = 0;
    vector<TypePtr>::const_iterator ei, eend;
    for (ei = elementTypes.begin(), eend = elementTypes.end();
         ei != eend; ++ei) {
        h += pointerHash(ei->ptr());
    }
    h &= tupleTypes.size() - 1;
    vector<TupleTypePtr>::iterator i, end;
    for (i = tupleTypes[h].begin(), end = tupleTypes[h].end();
         i != end; ++i) {
        TupleType *t = i->ptr();
        if (t->elementTypes == elementTypes)
            return t;
    }
    TupleTypePtr t = new TupleType(elementTypes);
    tupleTypes[h].push_back(t);
    return t.ptr();
}

TypePtr unionType(const vector<TypePtr> &memberTypes) {
    int h = 0;
    vector<TypePtr>::const_iterator mi, mend;
    for (mi = memberTypes.begin(), mend = memberTypes.end();
         mi != mend; ++mi) {
        h += pointerHash(mi->ptr());
    }
    h &= memberTypes.size() - 1;
    vector<UnionTypePtr>::iterator i, end;
    for (i = unionTypes[h].begin(), end = unionTypes[h].end();
         i != end; ++i) {
        UnionType *t = i->ptr();
        if (t->memberTypes == memberTypes)
            return t;
    }
    UnionTypePtr t = new UnionType(memberTypes);
    unionTypes[h].push_back(t);
    return t.ptr();
}

TypePtr recordType(RecordPtr record, const vector<ObjectPtr> &params) {
    int h = pointerHash(record.ptr());
    vector<ObjectPtr>::const_iterator pi, pend;
    for (pi = params.begin(), pend = params.end(); pi != pend; ++pi)
        h += objectHash(*pi);
    h &= recordTypes.size() - 1;
    vector<RecordTypePtr>::iterator i, end;
    for (i = recordTypes[h].begin(), end = recordTypes[h].end();
         i != end; ++i) {
        RecordType *t = i->ptr();
        if ((t->record == record) && objectVectorEquals(t->params, params))
            return t;
    }
    RecordTypePtr t = new RecordType(record);
    for (pi = params.begin(), pend = params.end(); pi != pend; ++pi)
        t->params.push_back(*pi);
    recordTypes[h].push_back(t);
    initializeRecordFields(t);
    return t.ptr();
}

TypePtr variantType(VariantPtr variant, const vector<ObjectPtr> &params) {
    int h = pointerHash(variant.ptr());
    for (unsigned i = 0; i < params.size(); ++i)
        h += objectHash(params[i]);
    h &= variantTypes.size() - 1;
    vector<VariantTypePtr> &bucket = variantTypes[h];
    for (unsigned i = 0; i < bucket.size(); ++i) {
        VariantType *t = bucket[i].ptr();
        if ((t->variant == variant) && objectVectorEquals(t->params, params))
            return t;
    }
    VariantTypePtr t = new VariantType(variant);
    for (unsigned i = 0; i < params.size(); ++i)
        t->params.push_back(params[i]);
    bucket.push_back(t);
    return t.ptr();
}

TypePtr staticType(ObjectPtr obj)
{
    int h = objectHash(obj);
    h &= staticTypes.size() - 1;
    vector<StaticTypePtr> &bucket = staticTypes[h];
    for (unsigned i = 0; i < bucket.size(); ++i) {
        if (objectEquals(obj, bucket[i]->obj))
            return bucket[i].ptr();
    }
    StaticTypePtr t = new StaticType(obj);
    bucket.push_back(t);
    return t.ptr();
}

void initializeEnumType(EnumTypePtr t) 
{
    if (t->initialized)
        return;

    CompileContextPusher pusher(t.ptr());

    EnvPtr env = new Env(t->enumeration->env);
    evaluatePredicate(t->enumeration->patternVars, t->enumeration->predicate, env);
    t->initialized = true;
}

TypePtr enumType(EnumerationPtr enumeration)
{
    if (!enumeration->type)
        enumeration->type = new EnumType(enumeration);
    return enumeration->type;
}

void initializeNewType(NewTypeTypePtr t) 
{
    if (t->newtype->initialized)
        return;
    CompileContextPusher pusher(t.ptr());
    t->newtype->baseType = evaluateType(t->newtype->expr, t->newtype->env);
    t->newtype->initialized = true;
}

TypePtr newType(NewTypePtr newtype)
{
    if (!newtype->type)
        newtype->type = new NewTypeType(newtype);
    return newtype->type;
}

TypePtr newtypeReprType(NewTypeTypePtr t)
{
    if (!t->newtype->initialized)
        initializeNewType(t);
    return t->newtype->baseType;
}



//
// isPrimitiveType, isPrimitiveAggregateType, isPointerOrCodePointerType
//

bool isPrimitiveType(TypePtr t)
{
    switch (t->typeKind) {
    case BOOL_TYPE :
    case INTEGER_TYPE :
    case FLOAT_TYPE :
    case COMPLEX_TYPE :
    case POINTER_TYPE :
    case CODE_POINTER_TYPE :
    case CCODE_POINTER_TYPE :
    case STATIC_TYPE :
    case ENUM_TYPE :
    case NEW_TYPE :
        return true;
    default :
        return false;
    }
}

bool isPrimitiveAggregateType(TypePtr t)
{
    switch (t->typeKind) {
    case BOOL_TYPE :
    case INTEGER_TYPE :
    case FLOAT_TYPE :
    case COMPLEX_TYPE :
    case POINTER_TYPE :
    case CODE_POINTER_TYPE :
    case CCODE_POINTER_TYPE :
    case STATIC_TYPE :
    case ENUM_TYPE :
    case NEW_TYPE :
        return true;
    case TUPLE_TYPE : {
        TupleType *tt = (TupleType *)t.ptr();
        for (unsigned i = 0; i < tt->elementTypes.size(); ++i) {
            if (!isPrimitiveAggregateType(tt->elementTypes[i]))
                return false;
        }
        return true;
    }
    case ARRAY_TYPE : {
        ArrayType *at = (ArrayType *)t.ptr();
        return isPrimitiveAggregateType(at->elementType);
    }
    default :
        return false;
    }
}

bool isPrimitiveAggregateTooLarge(TypePtr t)
{
    switch (t->typeKind) {
    case BOOL_TYPE :
    case INTEGER_TYPE :
    case FLOAT_TYPE :
    case COMPLEX_TYPE :
    case POINTER_TYPE :
    case CODE_POINTER_TYPE :
    case CCODE_POINTER_TYPE :
    case STATIC_TYPE :
    case ENUM_TYPE :
    case NEW_TYPE :
        return false;
    case TUPLE_TYPE : {
        TupleType *tt = (TupleType *)t.ptr();
        for (unsigned i = 0; i < tt->elementTypes.size(); ++i) {
            if (isPrimitiveAggregateTooLarge(tt->elementTypes[i]))
                return true;
        }
        return false;
    }
    case ARRAY_TYPE : {
        ArrayType *at = (ArrayType *)t.ptr();
        if (at->size > 8)
            return true;
        if (isPrimitiveAggregateTooLarge(at->elementType))
            return true;
        return false;
    }
    default :
        return false;
    }
}

bool isPointerOrCodePointerType(TypePtr t)
{
    switch (t->typeKind) {
    case POINTER_TYPE :
    case CODE_POINTER_TYPE :
    case CCODE_POINTER_TYPE :
        return true;
    default :
        return false;
    }
}

bool isStaticOrTupleOfStatics(TypePtr t) {
    switch (t->typeKind) {
    case STATIC_TYPE :
        return true;
    case TUPLE_TYPE : {
        TupleType *tt = (TupleType *)t.ptr();
        for (unsigned i = 0; i < tt->elementTypes.size(); ++i) {
            if (!isStaticOrTupleOfStatics(tt->elementTypes[i]))
                return false;
        }
        return true;
    }
    default :
        return false;
    }
}



//
// recordFieldTypes
//

static bool unpackField(TypePtr x, IdentifierPtr &name, TypePtr &type) {
    if (x->typeKind != TUPLE_TYPE)
        return false;
    TupleType *tt = (TupleType *)x.ptr();
    if (tt->elementTypes.size() != 2)
        return false;
    if (tt->elementTypes[0]->typeKind != STATIC_TYPE)
        return false;
    StaticType *st0 = (StaticType *)tt->elementTypes[0].ptr();
    if (st0->obj->objKind != IDENTIFIER)
        return false;
    name = (Identifier *)st0->obj.ptr();
    if (tt->elementTypes[1]->typeKind != STATIC_TYPE)
        return false;
    StaticType *st1 = (StaticType *)tt->elementTypes[1].ptr();
    if (st1->obj->objKind != TYPE)
        return false;
    type = (Type *)st1->obj.ptr();
    return true;
}

static void setProperty(TypePtr type,
                        ProcedurePtr proc,
                        const vector<ExprPtr> &exprs) {
    CodePtr code = new Code();

    TypePtr typeType = staticType(type.ptr());
    ExprPtr argType = new ObjectExpr(typeType.ptr());
    FormalArgPtr arg = new FormalArg(Identifier::get("x"), argType);
    code->formalArgs.push_back(arg.ptr());

    ExprListPtr returnExprs = new ExprList();
    for (unsigned i = 0; i < exprs.size(); ++i)
        returnExprs->add(exprs[i]);
    code->body = new Return(RETURN_VALUE, returnExprs);

    ExprPtr target = new ObjectExpr(proc.ptr());
    OverloadPtr overload = new Overload(target, code, false, false, IGNORE);
    overload->env = new Env();
    proc->overloads.insert(proc->overloads.begin(), overload);
}

static ExprPtr convertStaticObjectToExpr(TypePtr t) {
    if (t->typeKind == STATIC_TYPE) {
        StaticType *st = (StaticType *)t.ptr();
        return new ObjectExpr(st->obj);
    }
    else if (t->typeKind == TUPLE_TYPE) {
        TupleType *tt = (TupleType *)t.ptr();
        ExprListPtr exprs = new ExprList();
        for (unsigned i = 0; i < tt->elementTypes.size(); ++i)
            exprs->add(convertStaticObjectToExpr(tt->elementTypes[i]));
        return new Tuple(exprs);
    }
    else {
        error("non-static value found in object property");
        return NULL;
    }
}

static void setProperty(TypePtr type, TypePtr propType) {
    if (propType->typeKind != TUPLE_TYPE)
        error("each property should be a tuple [procedure, ... static values]");
    TupleTypePtr tt = (TupleType *)propType.ptr();
    if (tt->elementTypes.size() < 1)
        error("each property should be a tuple [procedure, ... static values]");
    TypePtr t0 = tt->elementTypes[0];
    if (t0->typeKind != STATIC_TYPE)
        error("each property should be a tuple [procedure, ... static values]");
    StaticTypePtr st0 = (StaticType *)t0.ptr();
    if (st0->obj->objKind != PROCEDURE)
        error("each property should be a tuple [procedure, ... static values]");
    ProcedurePtr proc = (Procedure *)st0->obj.ptr();
    vector<ExprPtr> exprs;
    for (unsigned i = 1; i < tt->elementTypes.size(); ++i) {
        TypePtr ti = tt->elementTypes[i];
        exprs.push_back(convertStaticObjectToExpr(ti));
    }
    setProperty(type, proc, exprs);
}

static void setProperties(TypePtr type, const vector<TypePtr> &props) {
    for (unsigned i = 0; i < props.size(); ++i)
        setProperty(type, props[i]);
}

void initializeRecordFields(RecordTypePtr t) {
    CompileContextPusher pusher(t.ptr());

    assert(!t->fieldsInitialized);
    t->fieldsInitialized = true;
    RecordPtr r = t->record;
    if (r->varParam.ptr())
        assert(t->params.size() >= r->params.size());
    else
        assert(t->params.size() == r->params.size());
    EnvPtr env = new Env(r->env);
    for (size_t i = 0; i < r->params.size(); ++i)
        addLocal(env, r->params[i], t->params[i].ptr());
    if (r->varParam.ptr()) {
        MultiStaticPtr rest = new MultiStatic();
        for (size_t i = r->params.size(); i < t->params.size(); ++i)
            rest->add(t->params[i]);
        addLocal(env, r->varParam, rest.ptr());
    }

    evaluatePredicate(r->patternVars, r->predicate, env);

    RecordBodyPtr body = r->body;
    if (body->isComputed) {
        LocationContext loc(body->location);
        AnalysisCachingDisabler disabler;
        MultiPValuePtr mpv = analyzeMulti(body->computed, env, 0);
        vector<TypePtr> fieldInfoTypes;
        if ((mpv->size() == 1) &&
            (mpv->values[0].type->typeKind == RECORD_TYPE) &&
            (((RecordType *)mpv->values[0].type.ptr())->record.ptr() ==
             primitive_RecordWithProperties().ptr()))
        {
            const vector<ObjectPtr> &params =
                ((RecordType *)mpv->values[0].type.ptr())->params;
            assert(params.size() == 2);
            if (params[0]->objKind != TYPE)
                argumentError(0, "expecting a tuple of properties");
            TypePtr param0 = (Type *)params[0].ptr();
            if (param0->typeKind != TUPLE_TYPE)
                argumentError(0, "expecting a tuple of properties");
            TupleTypePtr props = (TupleType *)param0.ptr();
            if (params[1]->objKind != TYPE)
                argumentError(1, "expecting a tuple of fields");
            TypePtr param1 = (Type *)params[1].ptr();
            if (param1->typeKind != TUPLE_TYPE)
                argumentError(1, "expecting a tuple of fields");
            TupleTypePtr fields = (TupleType *)param1.ptr();
            fieldInfoTypes = fields->elementTypes;
            setProperties(t.ptr(), props->elementTypes);
        }
        else {
            for (unsigned i = 0; i < mpv->size(); ++i)
                fieldInfoTypes.push_back(mpv->values[i].type);
        }
        for (unsigned i = 0; i < fieldInfoTypes.size(); ++i) {
            TypePtr x = fieldInfoTypes[i];
            IdentifierPtr name;
            TypePtr type;
            if (!unpackField(x, name, type))
                argumentError(i, "each field should be a "
                              "tuple of (name,type)");
            t->fieldIndexMap[name->str] = i;
            t->fieldTypes.push_back(type);
            t->fieldNames.push_back(name);
        }
    }
    else {
        for (unsigned i = 0; i < body->fields.size(); ++i) {
            RecordField *x = body->fields[i].ptr();
            t->fieldIndexMap[x->name->str] = i;
            TypePtr ftype = evaluateType(x->type, env);
            t->fieldTypes.push_back(ftype);
            t->fieldNames.push_back(x->name);
        }
    }
}

const vector<IdentifierPtr> &recordFieldNames(RecordTypePtr t) {
    if (!t->fieldsInitialized)
        initializeRecordFields(t);
    return t->fieldNames;
}

const vector<TypePtr> &recordFieldTypes(RecordTypePtr t) {
    if (!t->fieldsInitialized)
        initializeRecordFields(t);
    return t->fieldTypes;
}

const llvm::StringMap<size_t> &recordFieldIndexMap(RecordTypePtr t) {
    if (!t->fieldsInitialized)
        initializeRecordFields(t);
    return t->fieldIndexMap;
}



//
// variantMemberTypes, variantReprType, dispatchTagCount
//

static TypePtr getVariantReprType(VariantTypePtr t) {
    ExprPtr variantReprType = operator_expr_variantReprType();
    ExprPtr reprExpr = new Call(variantReprType, new ExprList(new ObjectExpr(t.ptr())));

    return evaluateType(reprExpr, new Env());
}

static void initializeVariantType(VariantTypePtr t) {
    assert(!t->initialized);

    CompileContextPusher pusher(t.ptr());

    EnvPtr variantEnv = new Env(t->variant->env);
    {
        const vector<IdentifierPtr> &params = t->variant->params;
        IdentifierPtr varParam = t->variant->varParam;
        assert(params.size() <= t->params.size());
        for (size_t j = 0; j < params.size(); ++j)
            addLocal(variantEnv, params[j], t->params[j]);
        if (varParam.ptr()) {
            MultiStaticPtr ms = new MultiStatic();
            for (size_t j = params.size(); j < t->params.size(); ++j)
                ms->add(t->params[j]);
            addLocal(variantEnv, varParam, ms.ptr());
        }
        else {
            assert(params.size() == t->params.size());
        }
    }

    evaluatePredicate(t->variant->patternVars,
        t->variant->predicate,
        variantEnv);

    ExprListPtr defaultInstances = t->variant->defaultInstances;
    evaluateMultiType(defaultInstances, variantEnv, t->memberTypes);

    const vector<InstancePtr> &instances = t->variant->instances;
    for (size_t i = 0; i < instances.size(); ++i) {
        InstancePtr x = instances[i];
        vector<PatternCellPtr> cells;
        vector<MultiPatternCellPtr> multiCells;
        const vector<PatternVar> &pvars = x->patternVars;
        EnvPtr patternEnv = new Env(x->env);
        for (size_t j = 0; j < pvars.size(); ++j) {
            if (pvars[j].isMulti) {
                MultiPatternCellPtr multiCell = new MultiPatternCell(NULL);
                multiCells.push_back(multiCell);
                cells.push_back(NULL);
                addLocal(patternEnv, pvars[j].name, multiCell.ptr());
            }
            else {
                PatternCellPtr cell = new PatternCell(NULL);
                cells.push_back(cell);
                multiCells.push_back(NULL);
                addLocal(patternEnv, pvars[j].name, cell.ptr());
            }
        }
        PatternPtr pattern = evaluateOnePattern(x->target, patternEnv);
        if (!unifyPatternObj(pattern, t.ptr()))
            continue;
        EnvPtr staticEnv = new Env(x->env);
        for (unsigned j = 0; j < pvars.size(); ++j) {
            if (pvars[j].isMulti) {
                MultiStaticPtr ms = derefDeep(multiCells[j].ptr());
                if (!ms)
                    error(pvars[j].name, "unbound pattern variable");
                addLocal(staticEnv, pvars[j].name, ms.ptr());
            }
            else {
                ObjectPtr v = derefDeep(cells[j].ptr());
                if (!v)
                    error(pvars[j].name, "unbound pattern variable");
                addLocal(staticEnv, pvars[j].name, v.ptr());
            }
        }
        if (x->predicate.ptr())
            if (!evaluateBool(x->predicate, staticEnv))
                continue;
        evaluateMultiType(x->members, staticEnv, t->memberTypes);
    }

    if (t->memberTypes.empty())
        error(t->variant, "variant type must have at least one instance");

    t->initialized = true;
    t->reprType = getVariantReprType(t);
}

const vector<TypePtr> &variantMemberTypes(VariantTypePtr t) {
    if (!t->initialized)
        initializeVariantType(t);
    return t->memberTypes;
}

TypePtr variantReprType(VariantTypePtr t) {
    if (!t->initialized)
        initializeVariantType(t);
    return t->reprType;
}

int dispatchTagCount(TypePtr t) {
    ExprPtr dtc = operator_expr_DispatchTagCount();
    ExprPtr dtcExpr = new Call(dtc, new ExprList(new ObjectExpr(t.ptr())));

    EValuePtr ev = evalOneAsRef(dtcExpr, new Env());
    if (ev->type != cIntType)
        error("DispatchTagCount must return an Int32");
    int tag = ev->as<int>();
    if (tag <= 0)
        error("DispatchTagCount must return a value greater than zero");
    return tag;
}


//
// tupleTypeLayout, recordTypeLayout
//

const llvm::StructLayout *tupleTypeLayout(TupleType *t) {
    if (t->layout == NULL) {
        llvm::StructType *st =
            llvm::cast<llvm::StructType>(llvmType(t));
        t->layout = llvmTargetData->getStructLayout(st);
    }
    return t->layout;
}

const llvm::StructLayout *complexTypeLayout(ComplexType *t) {
    if (t->layout == NULL) {
        llvm::StructType *st =
            llvm::cast<llvm::StructType>(llvmType(t));
        t->layout = llvmTargetData->getStructLayout(st);
    }
    return t->layout;
}

const llvm::StructLayout *recordTypeLayout(RecordType *t) {
    if (t->layout == NULL) {
        llvm::StructType *st =
            llvm::cast<llvm::StructType>(llvmType(t));
        t->layout = llvmTargetData->getStructLayout(st);
    }
    return t->layout;
}



//
// verifyRecursionCorrectness
//

static void verifyRecursionCorrectness(TypePtr t,
                                       set<TypePtr> &visited);

static void verifyRecursionCorrectness(TypePtr t) {
    set<TypePtr> visited;
    verifyRecursionCorrectness(t, visited);
}

static void verifyRecursionCorrectness(TypePtr t,
                                       set<TypePtr> &visited) {
    pair<set<TypePtr>::iterator, bool>
        result = visited.insert(t);
    if (!result.second) {
        string buf;
        llvm::raw_string_ostream sout(buf);
        sout << "invalid recursion in type: " << t;
        error(sout.str());
    }

    switch (t->typeKind) {
    case ARRAY_TYPE : {
        ArrayType *at = (ArrayType *)t.ptr();
        verifyRecursionCorrectness(at->elementType, visited);
        break;
    }
    case VEC_TYPE : {
        VecType *vt = (VecType *)t.ptr();
        verifyRecursionCorrectness(vt->elementType, visited);
        break;
    }
    case TUPLE_TYPE : {
        TupleType *tt = (TupleType *)t.ptr();
        for (unsigned i = 0; i < tt->elementTypes.size(); ++i)
            verifyRecursionCorrectness(tt->elementTypes[i], visited);
        break;
    }
    case UNION_TYPE : {
        UnionType *ut = (UnionType *)t.ptr();
        for (unsigned i = 0; i < ut->memberTypes.size(); ++i)
            verifyRecursionCorrectness(ut->memberTypes[i], visited);
        break;
    }
    case RECORD_TYPE : {
        RecordType *rt = (RecordType *)t.ptr();
        const vector<TypePtr> &fieldTypes = recordFieldTypes(rt);
        for (unsigned i = 0; i < fieldTypes.size(); ++i)
            verifyRecursionCorrectness(fieldTypes[i], visited);
        break;
    }
    case VARIANT_TYPE : {
        VariantType *vt = (VariantType *)t.ptr();
        const vector<TypePtr> &memberTypes = variantMemberTypes(vt);
        for (unsigned i = 0; i < memberTypes.size(); ++i)
            verifyRecursionCorrectness(memberTypes[i], visited);
        break;
    }
    case NEW_TYPE : {
        NewTypeType *nt = (NewTypeType *)t.ptr();
        verifyRecursionCorrectness(nt->newtype->baseType, visited);
        break;
    }
    default :
        break;
    }

    visited.erase(result.first);
}



//
// llvmIntType, llvmFloatType, llvmPointerType, llvmArrayType, llvmVoidType
//

llvm::Type *llvmIntType(int bits) {
    return llvm::IntegerType::get(llvm::getGlobalContext(), bits);
}

llvm::Type *llvmFloatType(int bits) {
    switch (bits) {
    case 32 :
        return llvm::Type::getFloatTy(llvm::getGlobalContext());
    case 64 :
        return llvm::Type::getDoubleTy(llvm::getGlobalContext());
    case 80 :
        return llvm::Type::getX86_FP80Ty(llvm::getGlobalContext());
    default :
        assert(false);
        return NULL;
    }
}


llvm::PointerType *llvmPointerType(llvm::Type *llType) {
    return llvm::PointerType::getUnqual(llType);
}

static void declareLLVMType(TypePtr t);
static void defineLLVMType(TypePtr t);
static void makeLLVMType(TypePtr t);

llvm::PointerType *llvmPointerType(TypePtr t) {
    if (!t->llType)
        declareLLVMType(t);
    return llvmPointerType(t->llType);
}

llvm::Type *llvmArrayType(llvm::Type *llType, int size) {
    return llvm::ArrayType::get(llType, size);
}

llvm::Type *llvmArrayType(TypePtr type, int size) {
    return llvmArrayType(llvmType(type), size);
}

llvm::Type *llvmVoidType() {
    return llvm::Type::getVoidTy(llvm::getGlobalContext());
}

llvm::Type *CCodePointerType::getCallType() {
    if (this->callType == NULL) {
        ExternalTargetPtr target = getExternalTarget();

        vector<llvm::Type *> llArgTypes;
        vector< pair<unsigned, llvm::Attributes> > llAttributes;
        llvm::Type *llRetType =
            target->pushReturnType(this->callingConv, this->returnType, llArgTypes, llAttributes);
        for (unsigned i = 0; i < this->argTypes.size(); ++i)
            target->pushArgumentType(this->callingConv, this->argTypes[i], llArgTypes, llAttributes);
        llvm::FunctionType *llFuncType =
            llvm::FunctionType::get(llRetType, llArgTypes, this->hasVarArgs);
        this->callType = llvm::PointerType::getUnqual(llFuncType);
    }
    return this->callType;
}


//
// llvmType
//

static void makeLLVMType(TypePtr t) {
    if (t->llType == NULL) {
        declareLLVMType(t);
        verifyRecursionCorrectness(t);
    }
    if (!t->defined) {
        defineLLVMType(t);
    }
}

llvm::Type *llvmType(TypePtr t) {
    makeLLVMType(t);

    return t->llType;
}

llvm::StructType *llvmStaticType() {
    static llvm::StructType *theType = NULL;
    if (theType == NULL) {
        llvm::SmallVector<llvm::Type *, 2> llTypes;
        llTypes.push_back(llvmIntType(8));
        theType = llvm::StructType::get(llvm::getGlobalContext(), llTypes);
    }
    return theType;
}

llvm::DIType llvmTypeDebugInfo(TypePtr t) {
    if (t->llType == NULL)
        declareLLVMType(t);

    return t->getDebugInfo();
}

llvm::DIType llvmVoidTypeDebugInfo() {
    return llvm::DIType(NULL);
}

static void declareLLVMType(TypePtr t) {
    assert(t->llType == NULL);

    switch (t->typeKind) {
    case BOOL_TYPE : {
        t->llType = llvmIntType(1);
        if (llvmDIBuilder != NULL)
            t->debugInfo = (llvm::MDNode*)llvmDIBuilder->createBasicType(
                typeName(t),
                debugTypeSize(t->llType),
                debugTypeAlignment(t->llType),
                llvm::dwarf::DW_ATE_boolean);
        break;
    }
    case INTEGER_TYPE : {
        IntegerType *x = (IntegerType *)t.ptr();
        t->llType = llvmIntType(x->bits);
        if (llvmDIBuilder != NULL)
            t->debugInfo = (llvm::MDNode*)llvmDIBuilder->createBasicType(
                typeName(t),
                debugTypeSize(t->llType),
                debugTypeAlignment(t->llType),
                x->isSigned ? llvm::dwarf::DW_ATE_signed : llvm::dwarf::DW_ATE_unsigned);
        break;
    }
    case FLOAT_TYPE : {
        FloatType *x = (FloatType *)t.ptr();
        t->llType = llvmFloatType(x->bits);
        if (llvmDIBuilder != NULL)
            t->debugInfo = (llvm::MDNode*)llvmDIBuilder->createBasicType(
                typeName(t),
                debugTypeSize(t->llType),
                debugTypeAlignment(t->llType),
                x->isImaginary ? llvm::dwarf::DW_ATE_imaginary_float : llvm::dwarf::DW_ATE_float);
        break;
    }
    case COMPLEX_TYPE : {
        ComplexType *x = (ComplexType *)t.ptr();
        vector<llvm::Type *> llTypes;
        TypePtr realT = floatType(x->bits), imagT = imagType(x->bits);
        llTypes.push_back(llvmType(realT));
        llTypes.push_back(llvmType(imagT));
        t->llType = llvm::StructType::create(llvm::getGlobalContext(), llTypes, typeName(t));
        if (llvmDIBuilder != NULL) {
            t->debugInfo = (llvm::MDNode*)llvmDIBuilder->createBasicType(
                typeName(t),
                debugTypeSize(t->llType),
                debugTypeAlignment(t->llType),
                llvm::dwarf::DW_ATE_complex_float);
        }
        break;
    }
    case POINTER_TYPE : {
        PointerType *x = (PointerType *)t.ptr();
        t->llType = llvmPointerType(x->pointeeType);
        if (llvmDIBuilder != NULL)
            t->debugInfo = (llvm::MDNode*)llvmDIBuilder->createPointerType(
                llvmTypeDebugInfo(x->pointeeType),
                debugTypeSize(t->llType),
                debugTypeAlignment(t->llType),
                typeName(t));
        break;
    }
    case CODE_POINTER_TYPE : {
        CodePointerType *x = (CodePointerType *)t.ptr();
        vector<llvm::Type *> llArgTypes;
        for (unsigned i = 0; i < x->argTypes.size(); ++i)
            llArgTypes.push_back(llvmPointerType(x->argTypes[i]));
        for (unsigned i = 0; i < x->returnTypes.size(); ++i) {
            TypePtr t = x->returnTypes[i];
            if (x->returnIsRef[i])
                llArgTypes.push_back(llvmPointerType(pointerType(t)));
            else
                llArgTypes.push_back(llvmPointerType(t));
        }
        llvm::FunctionType *llFuncType =
            llvm::FunctionType::get(exceptionReturnType(), llArgTypes, false);
        t->llType = llvm::PointerType::getUnqual(llFuncType);
        if (llvmDIBuilder != NULL) {
            vector<llvm::Value*> debugParamTypes;
            debugParamTypes.push_back(llvmVoidTypeDebugInfo());
            for (unsigned i = 0; i < x->argTypes.size(); ++i) {
                llvm::DIType argType = llvmTypeDebugInfo(x->argTypes[i]);
                llvm::DIType argRefType
                    = llvmDIBuilder->createReferenceType(argType);
                debugParamTypes.push_back(argRefType);
            }
            for (unsigned i = 0; i < x->returnTypes.size(); ++i) {
                llvm::DIType returnType = llvmTypeDebugInfo(x->returnTypes[i]);
                llvm::DIType returnRefType = x->returnIsRef[i]
                    ? llvmDIBuilder->createReferenceType(
                        llvmDIBuilder->createReferenceType(returnType))
                    : llvmDIBuilder->createReferenceType(returnType);

                debugParamTypes.push_back(returnRefType);
            }

            llvm::DIArray debugParamArray = llvmDIBuilder->getOrCreateArray(
                llvm::makeArrayRef(debugParamTypes));

            llvm::DIType pointeeType = llvmDIBuilder->createSubroutineType(
                llvm::DIFile(NULL),
                debugParamArray);

            t->debugInfo = (llvm::MDNode*)llvmDIBuilder->createPointerType(
                pointeeType,
                debugTypeSize(t->llType),
                debugTypeAlignment(t->llType),
                typeName(t));
        }
        break;
    }
    case CCODE_POINTER_TYPE : {
        CCodePointerType *x = (CCodePointerType *)t.ptr();

        if (x->callingConv == CC_LLVM) {
            // LLVM intrinsic function pointers can't be bitcast. Generate the LLVM type
            // directly.
            llvm::Type *returnType = llvmType(x->returnType);
            vector<llvm::Type*> argTypes;
            for (size_t i = 0; i < x->argTypes.size(); ++i)
                argTypes.push_back(llvmType(x->argTypes[i]));
            llvm::FunctionType *funcType =
                llvm::FunctionType::get(returnType, argTypes, x->hasVarArgs);
            t->llType = llvm::PointerType::getUnqual(funcType);
        } else {
            // Deriving the C ABI type may require type recursion, so cast to void()*
            // for now. We can bitcast to the proper type when we know it.
            llvm::FunctionType *llOpaqueFuncType =
                llvm::FunctionType::get(llvmVoidType(), vector<llvm::Type*>(), false);
            t->llType = llvm::PointerType::getUnqual(llOpaqueFuncType);
        }

        if (llvmDIBuilder != NULL) {
            llvm::SmallVector<llvm::Value*,16> debugParamTypes;
            debugParamTypes.push_back(x->returnType == NULL
                ? llvmVoidTypeDebugInfo()
                : llvmTypeDebugInfo(x->returnType));

            for (size_t i = 0; i < x->argTypes.size(); ++i) {
                debugParamTypes.push_back(llvmTypeDebugInfo(x->argTypes[i]));
            }

            llvm::DIArray debugParamArray = llvmDIBuilder->getOrCreateArray(debugParamTypes);

            llvm::DIType pointeeType = llvmDIBuilder->createSubroutineType(
                llvm::DIFile(NULL),
                debugParamArray);

            t->debugInfo = (llvm::MDNode*)llvmDIBuilder->createPointerType(
                pointeeType,
                debugTypeSize(t->llType),
                debugTypeAlignment(t->llType),
                typeName(t));
        }

        break;
    }
    case ARRAY_TYPE : {
        ArrayType *x = (ArrayType *)t.ptr();
        t->llType = llvmArrayType(x->elementType, x->size);
        if (llvmDIBuilder != NULL) {
            llvm::Value* elementRange = llvmDIBuilder->getOrCreateSubrange(
                0,
                x->size - 1);
            llvm::DIArray elementRangeArray = llvmDIBuilder->getOrCreateArray(
                llvm::makeArrayRef(&elementRange, &elementRange + 1));
            t->debugInfo = (llvm::MDNode*)llvmDIBuilder->createArrayType(
                debugTypeSize(t->llType),
                debugTypeAlignment(t->llType),
                llvmTypeDebugInfo(x->elementType),
                elementRangeArray);
        }
        break;
    }
    case VEC_TYPE : {
        VecType *x = (VecType *)t.ptr();
        t->llType = llvm::VectorType::get(llvmType(x->elementType), x->size);
        if (llvmDIBuilder != NULL) {
            llvm::Value* elementRange = llvmDIBuilder->getOrCreateSubrange(
                0,
                x->size - 1);
            llvm::DIArray elementRangeArray = llvmDIBuilder->getOrCreateArray(
                llvm::makeArrayRef(&elementRange, &elementRange + 1));
            t->debugInfo = (llvm::MDNode*)llvmDIBuilder->createVectorType(
                x->size,
                debugTypeAlignment(llvmType(x->elementType)),
                llvmTypeDebugInfo(x->elementType),
                elementRangeArray);
        }
        break;
    }
    case TUPLE_TYPE : {
        t->llType = llvm::StructType::create(llvm::getGlobalContext(), typeName(t));
        if (llvmDIBuilder != NULL)
            t->debugInfo = (llvm::MDNode*)llvmDIBuilder->createTemporaryType();
        break;
    }
    case UNION_TYPE : {
        t->llType = llvm::StructType::create(llvm::getGlobalContext(), typeName(t));
        if (llvmDIBuilder != NULL)
            t->debugInfo = (llvm::MDNode*)llvmDIBuilder->createTemporaryType();
        break;
    }
    case RECORD_TYPE : {
        t->llType = llvm::StructType::create(llvm::getGlobalContext(), typeName(t));
        if (llvmDIBuilder != NULL)
            t->debugInfo = (llvm::MDNode*)llvmDIBuilder->createTemporaryType();
        break;
    }
    case VARIANT_TYPE : {
        VariantType *x = (VariantType *)t.ptr();
        TypePtr reprType = variantReprType(x);
        if (!reprType->llType)
            declareLLVMType(reprType);
        t->llType = reprType->llType;
        if (llvmDIBuilder != NULL)
            t->debugInfo = (llvm::MDNode*)llvmDIBuilder->createTemporaryType();
        break;
    }
    case STATIC_TYPE : {
        t->llType = llvmStaticType();
        if (llvmDIBuilder != NULL)
            t->debugInfo = (llvm::MDNode*)llvmDIBuilder->createBasicType(
                typeName(t),
                debugTypeSize(t->llType),
                debugTypeAlignment(t->llType),
                llvm::dwarf::DW_ATE_signed);
        break;
    }
    case ENUM_TYPE : {
        t->llType = llvmType(cIntType);
        if (llvmDIBuilder != NULL) {
            EnumType *en = (EnumType*)t.ptr();
            llvm::SmallVector<llvm::Value*,16> enumerators;
            for (vector<EnumMemberPtr>::const_iterator i = en->enumeration->members.begin(),
                    end = en->enumeration->members.end();
                 i != end;
                 ++i)
            {
                enumerators.push_back(
                    llvmDIBuilder->createEnumerator((*i)->name->str, (*i)->index));
            }
            llvm::DIArray enumArray = llvmDIBuilder->getOrCreateArray(enumerators);

            int line, column;
            llvm::DIFile file = getDebugLineCol(en->enumeration->location, line, column);

            t->debugInfo = (llvm::MDNode*)llvmDIBuilder->createEnumerationType(
                lookupModuleDebugInfo(en->enumeration->env), // scope
                typeName(t),
                file,
                line,
                debugTypeSize(t->llType),
                debugTypeAlignment(t->llType),
                enumArray);
        }
        break;
    }
    case NEW_TYPE : {
        NewTypeType *x = (NewTypeType *)t.ptr();
        TypePtr reprType = newtypeReprType(x);
        if (!reprType->llType)
            declareLLVMType(reprType);
        t->llType = reprType->llType;
        if (llvmDIBuilder != NULL)
            t->debugInfo = (llvm::MDNode*)llvmDIBuilder->createTemporaryType();
        break;
    }
    default :
        assert(false);
    }
}

static void defineLLVMType(TypePtr t) {
    assert(t->llType != NULL && !t->defined);

    switch (t->typeKind) {
    case BOOL_TYPE :
    case INTEGER_TYPE :
    case FLOAT_TYPE :
    case COMPLEX_TYPE :
    case POINTER_TYPE :
    case CODE_POINTER_TYPE :
    case CCODE_POINTER_TYPE :
    case ARRAY_TYPE :
    case VEC_TYPE :
    case STATIC_TYPE :
    case ENUM_TYPE :
        break;

    case TUPLE_TYPE : {
        TupleType *x = (TupleType *)t.ptr();

        llvm::StructType *theType = llvm::cast<llvm::StructType>(t->llType);

        vector<llvm::Type *> llTypes;
        vector<TypePtr>::iterator i, end;
        for (i = x->elementTypes.begin(), end = x->elementTypes.end();
             i != end; ++i)
            llTypes.push_back(llvmType(*i));
        if (x->elementTypes.empty())
            llTypes.push_back(llvmIntType(8));

        theType->setBody(llTypes);

        if (llvmDIBuilder != NULL) {
            llvm::TrackingVH<llvm::MDNode> placeholderNode = (llvm::MDNode*)x->getDebugInfo();
            llvm::DIType placeholder(placeholderNode);

            vector<llvm::Value*> members;
            size_t debugOffset = 0;
            vector<TypePtr>::iterator i, end;
            for (i = x->elementTypes.begin(), end = x->elementTypes.end();
                 i != end;
                 ++i)
            {
                llvm::Type *memberLLT = llvmType(*i);
                size_t debugAlign = debugTypeAlignment(memberLLT);
                size_t debugSize = debugTypeSize(memberLLT);
                debugOffset = alignedUpTo(debugOffset, debugAlign);
                llvm::SmallString<128> buf;
                llvm::raw_svector_ostream name(buf);
                name << "element" << i - x->elementTypes.begin();
                members.push_back(llvmDIBuilder->createMemberType(
                    placeholder,
                    name.str(),
                    llvm::DIFile(NULL), // file
                    0, // lineNo
                    debugSize, // size
                    debugAlign, // align
                    debugOffset, // offset
                    0, // flags
                    llvmTypeDebugInfo(*i) // type
                    ));
                debugOffset += debugSize;
            }

            llvm::DIArray memberArray = llvmDIBuilder->getOrCreateArray(
                llvm::makeArrayRef(members));

            t->debugInfo = (llvm::MDNode*)llvmDIBuilder->createStructType(
                primitivesModule()->getDebugInfo(), // scope
                typeName(t),
                globalMainModule->source->getDebugInfo(), // file
                0, // lineNo
                debugTypeSize(t->llType),
                debugTypeAlignment(t->llType),
                0, // flags
                memberArray);
            llvm::DIType(placeholderNode).replaceAllUsesWith(t->getDebugInfo());
        }
        break;
    }
    case UNION_TYPE : {
        UnionType *x = (UnionType *)t.ptr();

        llvm::StructType *theType = llvm::cast<llvm::StructType>(t->llType);

        llvm::Type *maxAlignType = NULL;
        size_t maxAlign = 0;
        size_t maxAlignSize = 0;
        size_t maxSize = 0;
        for (unsigned i = 0; i < x->memberTypes.size(); ++i) {
            llvm::Type *llt = llvmType(x->memberTypes[i]);
            size_t align = (size_t)llvmTargetData->getABITypeAlignment(llt);
            size_t size = (size_t)llvmTargetData->getTypeAllocSize(llt);
            if (align > maxAlign) {
                maxAlign = align;
                maxAlignType = llt;
                maxAlignSize = size;
            }
            if (size > maxSize)
                maxSize = size;
        }
        if (!maxAlignType) {
            maxAlignType = llvmIntType(8);
            maxAlign = 1;
        }
        vector<llvm::Type *> llTypes;
        llTypes.push_back(maxAlignType);
        if (maxSize > maxAlignSize) {
            llvm::Type *padding =
                llvm::ArrayType::get(llvmIntType(8), maxSize-maxAlignSize);
            llTypes.push_back(padding);
        }

        theType->setBody(llTypes);

        if (llvmDIBuilder != NULL) {
            llvm::TrackingVH<llvm::MDNode> placeholderNode = (llvm::MDNode*)x->getDebugInfo();
            llvm::DIType placeholder(placeholderNode);

            vector<llvm::Value*> members;
            vector<TypePtr>::iterator i, end;
            for (i = x->memberTypes.begin(), end = x->memberTypes.end();
                 i != end;
                 ++i)
            {
                llvm::Type *memberLLT = llvmType(*i);
                size_t debugAlign = debugTypeAlignment(memberLLT);
                size_t debugSize = debugTypeSize(memberLLT);
                llvm::SmallString<128> buf;
                llvm::raw_svector_ostream name(buf);
                name << "element" << i - x->memberTypes.begin();
                members.push_back(llvmDIBuilder->createMemberType(
                    placeholder,
                    name.str(),
                    llvm::DIFile(NULL), // file
                    0, // lineNo
                    debugSize, // size
                    debugAlign, // align
                    0, // offset
                    0, // flags
                    llvmTypeDebugInfo(*i) // type
                    ));
            }

            llvm::DIArray memberArray = llvmDIBuilder->getOrCreateArray(
                llvm::makeArrayRef(members));

            t->debugInfo = (llvm::MDNode*)llvmDIBuilder->createUnionType(
                primitivesModule()->getDebugInfo(), // scope
                typeName(t),
                globalMainModule->source->getDebugInfo(), // file
                0, // lineNo
                debugTypeSize(t->llType),
                debugTypeAlignment(t->llType),
                0, // flags
                memberArray);
            llvm::DIType(placeholderNode).replaceAllUsesWith(t->getDebugInfo());
        }
        break;
    }
    case RECORD_TYPE : {
        RecordType *x = (RecordType *)t.ptr();

        llvm::StructType *theType = llvm::cast<llvm::StructType>(t->llType);

        const vector<IdentifierPtr> &fieldNames = recordFieldNames(x);
        const vector<TypePtr> &fieldTypes = recordFieldTypes(x);
        vector<llvm::Type *> llTypes;
        vector<TypePtr>::const_iterator i, end;
        for (i = fieldTypes.begin(), end = fieldTypes.end(); i != end; ++i)
            llTypes.push_back(llvmType(*i));
        if (fieldTypes.empty())
            llTypes.push_back(llvmIntType(8));

        theType->setBody(llTypes);

        if (llvmDIBuilder != NULL) {
            llvm::TrackingVH<llvm::MDNode> placeholderNode = (llvm::MDNode*)x->getDebugInfo();
            llvm::DIType placeholder(placeholderNode);

            vector<llvm::Value*> members;
            size_t debugOffset = 0;
            for (size_t i = 0; i < fieldNames.size(); ++i) {
                llvm::Type *memberLLT = llvmType(fieldTypes[i]);
                size_t debugAlign = debugTypeAlignment(memberLLT);
                size_t debugSize = debugTypeSize(memberLLT);
                debugOffset = alignedUpTo(debugOffset, debugAlign);

                Location fieldLocation = fieldNames[i]->location;
                if (!fieldLocation.ok())
                    fieldLocation = x->record->location;
                int fieldLine, fieldColumn;
                llvm::DIFile fieldFile = getDebugLineCol(fieldLocation, fieldLine, fieldColumn);
                members.push_back(llvmDIBuilder->createMemberType(
                    placeholder,
                    fieldNames[i]->str,
                    fieldFile, // file
                    fieldLine, // lineNo
                    debugSize, // size
                    debugAlign, // align
                    debugOffset, // offset
                    0, // flags
                    llvmTypeDebugInfo(fieldTypes[i]) // type
                    ));
                debugOffset += debugSize;
            }

            llvm::DIArray memberArray = llvmDIBuilder->getOrCreateArray(
                llvm::makeArrayRef(members));

            int line, column;
            llvm::DIFile file = getDebugLineCol(x->record->location, line, column);

            t->debugInfo = (llvm::MDNode*)llvmDIBuilder->createStructType(
                lookupModuleDebugInfo(x->record->env), // scope
                typeName(t),
                file, // file
                line, // lineNo
                debugTypeSize(t->llType),
                debugTypeAlignment(t->llType),
                0, // flags
                memberArray);
            llvm::DIType(placeholderNode).replaceAllUsesWith(t->getDebugInfo());
        }
        break;
    }
    case VARIANT_TYPE : {
        VariantType *x = (VariantType *)t.ptr();
        TypePtr reprType = variantReprType(x);
        if (!reprType->llType)
            declareLLVMType(reprType);
        if (!reprType->defined)
            defineLLVMType(reprType);

        if (llvmDIBuilder != NULL) {
            llvm::TrackingVH<llvm::MDNode> placeholderNode = (llvm::MDNode*)x->getDebugInfo();
            
            llvm::DIType(placeholderNode).replaceAllUsesWith(llvmTypeDebugInfo(reprType));
        }
        break;
    }
    case NEW_TYPE : {
        NewTypeType *x = (NewTypeType *)t.ptr();
        TypePtr reprType = newtypeReprType(x);
        if (!reprType->llType)
            declareLLVMType(reprType);
        if (!reprType->defined)
            defineLLVMType(reprType);

        if (llvmDIBuilder != NULL) {
            llvm::TrackingVH<llvm::MDNode> placeholderNode = (llvm::MDNode*)x->getDebugInfo();
            
            llvm::DIType(placeholderNode).replaceAllUsesWith(llvmTypeDebugInfo(reprType));
        }
        break;
    }
    default :
        assert(false);
    }
    t->defined = true;
}



//
// typeSize
//

static void initTypeInfo(TypePtr t) {
    if (!t->typeInfoInitialized) {
        t->typeInfoInitialized = true;
        llvm::Type *llt = llvmType(t);
        t->typeSize = llTypeSize(llt);
        t->typeAlignment = llTypeAlignment(llt);
    }
}

size_t typeSize(TypePtr t) {
    initTypeInfo(t.ptr());
    return t->typeSize;
}

size_t typeAlignment(TypePtr t) {
    initTypeInfo(t.ptr());
    return t->typeAlignment;
}
string typeName(TypePtr type)
{
    llvm::SmallString<128> buf;
    llvm::raw_svector_ostream os(buf);
    typePrint(os, type);
    return os.str();
}

}
