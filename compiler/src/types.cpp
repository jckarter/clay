#include "clay.hpp"
#include "libclaynames.hpp"

TypePtr boolType;
TypePtr int8Type;
TypePtr int16Type;
TypePtr int32Type;
TypePtr int64Type;
TypePtr uint8Type;
TypePtr uint16Type;
TypePtr uint32Type;
TypePtr uint64Type;
TypePtr float32Type;
TypePtr float64Type;

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

void initTypes() {
    boolType = new BoolType();
    int8Type = new IntegerType(8, true);
    int16Type = new IntegerType(16, true);
    int32Type = new IntegerType(32, true);
    int64Type = new IntegerType(64, true);
    uint8Type = new IntegerType(8, false);
    uint16Type = new IntegerType(16, false);
    uint32Type = new IntegerType(32, false);
    uint64Type = new IntegerType(64, false);
    float32Type = new FloatType(32);
    float64Type = new FloatType(64);

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
    default :
        assert(false);
        return NULL;
    }
}

TypePtr floatType(int bits) {
    switch (bits) {
    case 32 : return float32Type;
    case 64 : return float64Type;
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

TypePtr enumType(EnumerationPtr enumeration)
{
    if (!enumeration->type)
        enumeration->type = new EnumType(enumeration);
    return enumeration->type;
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
    case POINTER_TYPE :
    case CODE_POINTER_TYPE :
    case CCODE_POINTER_TYPE :
    case STATIC_TYPE :
    case ENUM_TYPE :
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
    case POINTER_TYPE :
    case CODE_POINTER_TYPE :
    case CCODE_POINTER_TYPE :
    case STATIC_TYPE :
    case ENUM_TYPE :
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
    FormalArgPtr arg = new FormalArg(new Identifier("x"), argType);
    code->formalArgs.push_back(arg.ptr());

    ExprListPtr returnExprs = new ExprList();
    for (unsigned i = 0; i < exprs.size(); ++i)
        returnExprs->add(exprs[i]);
    code->body = new Return(RETURN_VALUE, returnExprs);

    ExprPtr target = new ObjectExpr(proc.ptr());
    OverloadPtr overload = new Overload(target, code, false, false);
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
        error("each property should be a tuple (procedure, ... static values)");
    TupleTypePtr tt = (TupleType *)propType.ptr();
    if (tt->elementTypes.size() < 1)
        error("each property should be a tuple (procedure, ... static values)");
    TypePtr t0 = tt->elementTypes[0];
    if (t0->typeKind != STATIC_TYPE)
        error("each property should be a tuple (procedure, ... static values)");
    StaticTypePtr st0 = (StaticType *)t0.ptr();
    if (st0->obj->objKind != PROCEDURE)
        error("each property should be a tuple (procedure, ... static values)");
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
    assert(!t->fieldsInitialized);
    t->fieldsInitialized = true;
    RecordPtr r = t->record;
    if (r->varParam.ptr())
        assert(t->params.size() >= r->params.size());
    else
        assert(t->params.size() == r->params.size());
    EnvPtr env = new Env(r->env);
    for (unsigned i = 0; i < r->params.size(); ++i)
        addLocal(env, r->params[i], t->params[i].ptr());
    if (r->varParam.ptr()) {
        MultiStaticPtr rest = new MultiStatic();
        for (unsigned i = r->params.size(); i < t->params.size(); ++i)
            rest->add(t->params[i]);
        addLocal(env, r->varParam, rest.ptr());
    }
    RecordBodyPtr body = r->body;
    if (body->isComputed) {
        LocationContext loc(body->location);
        MultiPValuePtr mpv = analyzeMulti(body->computed, env);
        vector<TypePtr> fieldInfoTypes;
        if ((mpv->size() == 1) &&
            (mpv->values[0]->type->typeKind == RECORD_TYPE) &&
            (((RecordType *)mpv->values[0]->type.ptr())->record.ptr() ==
             prelude_RecordWithProperties().ptr()))
        {
            const vector<ObjectPtr> &params =
                ((RecordType *)mpv->values[0]->type.ptr())->params;
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
                fieldInfoTypes.push_back(mpv->values[i]->type);
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

const map<string, size_t> &recordFieldIndexMap(RecordTypePtr t) {
    if (!t->fieldsInitialized)
        initializeRecordFields(t);
    return t->fieldIndexMap;
}



//
// variantMemberTypes, variantReprType
//

static RecordPtr getVariantReprRecord() {
    static RecordPtr rec;
    if (!rec) {
        ObjectPtr obj = prelude_VariantRepr();
        if (obj->objKind != RECORD)
            error("lib-clay error: VariantRepr is not a record");
        rec = (Record *)obj.ptr();
    }
    return rec;
}

static void initializeVariantType(VariantTypePtr t) {
    assert(!t->initialized);

    EnvPtr variantEnv = new Env(t->variant->env);
    {
        const vector<IdentifierPtr> &params = t->variant->params;
        IdentifierPtr varParam = t->variant->varParam;
        assert(params.size() <= t->params.size());
        for (unsigned j = 0; j < params.size(); ++j)
            addLocal(variantEnv, params[j], t->params[j]);
        if (varParam.ptr()) {
            MultiStaticPtr ms = new MultiStatic();
            for (unsigned j = params.size(); j < t->params.size(); ++j)
                ms->add(t->params[j]);
            addLocal(variantEnv, varParam, ms.ptr());
        }
        else {
            assert(params.size() == t->params.size());
        }
    }
    ExprListPtr defaultInstances = t->variant->defaultInstances;
    for (unsigned i = 0; i < defaultInstances->size(); ++i) {
        ExprPtr x = defaultInstances->exprs[i];
        TypePtr memberType = evaluateType(x, variantEnv);
        t->memberTypes.push_back(memberType);
    }

    const vector<InstancePtr> &instances = t->variant->instances;
    for (unsigned i = 0; i < instances.size(); ++i) {
        InstancePtr x = instances[i];
        vector<PatternCellPtr> cells;
        vector<MultiPatternCellPtr> multiCells;
        const vector<PatternVar> &pvars = x->patternVars;
        EnvPtr patternEnv = new Env(x->env);
        for (unsigned j = 0; j < pvars.size(); ++j) {
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
        TypePtr memberType = evaluateType(x->member, staticEnv);
        t->memberTypes.push_back(memberType);
    }

    RecordPtr reprRecord = getVariantReprRecord();
    vector<ObjectPtr> params;
    for (unsigned i = 0; i < t->memberTypes.size(); ++i)
        params.push_back(t->memberTypes[i].ptr());
    t->reprType = recordType(reprRecord, params);

    t->initialized = true;
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



//
// tupleTypeLayout, recordTypeLayout
//

const llvm::StructLayout *tupleTypeLayout(TupleType *t) {
    if (t->layout == NULL) {
        const llvm::StructType *st =
            llvm::cast<llvm::StructType>(llvmType(t));
        t->layout = llvmTargetData->getStructLayout(st);
    }
    return t->layout;
}

const llvm::StructLayout *recordTypeLayout(RecordType *t) {
    if (t->layout == NULL) {
        const llvm::StructType *st =
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
        ostringstream sout;
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
    default :
        break;
    }

    visited.erase(result.first);
}



//
// llvmIntType, llvmFloatType, llvmPointerType, llvmArrayType, llvmVoidType
//

const llvm::Type *llvmIntType(int bits) {
    return llvm::IntegerType::get(llvm::getGlobalContext(), bits);
}

const llvm::Type *llvmFloatType(int bits) {
    switch (bits) {
    case 32 :
        return llvm::Type::getFloatTy(llvm::getGlobalContext());
    case 64 :
        return llvm::Type::getDoubleTy(llvm::getGlobalContext());
    default :
        assert(false);
        return NULL;
    }
}

const llvm::Type *llvmPointerType(const llvm::Type *llType) {
    return llvm::PointerType::getUnqual(llType);
}

const llvm::Type *llvmPointerType(TypePtr t) {
    return llvmPointerType(llvmType(t));
}

const llvm::Type *llvmArrayType(const llvm::Type *llType, int size) {
    return llvm::ArrayType::get(llType, size);
}

const llvm::Type *llvmArrayType(TypePtr type, int size) {
    return llvmArrayType(llvmType(type), size);
}

const llvm::Type *llvmVoidType() {
    return llvm::Type::getVoidTy(llvm::getGlobalContext());
}



//
// llvmType
//

static const llvm::Type *makeLLVMType(TypePtr t);

const llvm::Type *llvmType(TypePtr t) {
    if (t->llTypeHolder == NULL) {
        verifyRecursionCorrectness(t);
        llvm::OpaqueType *opaque =
            llvm::OpaqueType::get(llvm::getGlobalContext());
        t->llTypeHolder = new llvm::PATypeHolder(opaque);
        const llvm::Type *llt = makeLLVMType(t);
        opaque->refineAbstractTypeTo(llt);
        t->refined = true;
    }
    return t->llTypeHolder->get();
}

// return opaque llvm pointers for all clay pointer types
// otherwise behaves like llvmType
static const llvm::Type *makeLLVMApproximateType(TypePtr t) {
    if (t->refined)
        return t->llTypeHolder->get();

    switch (t->typeKind) {

    case BOOL_TYPE :
    case INTEGER_TYPE :
    case FLOAT_TYPE :
    case UNION_TYPE :
    case STATIC_TYPE :
    case ENUM_TYPE :
        return makeLLVMType(t);

    case POINTER_TYPE :
    case CODE_POINTER_TYPE :
    case CCODE_POINTER_TYPE :
        return llvmPointerType(llvmIntType(8));

    case ARRAY_TYPE : {
        ArrayType *x = (ArrayType *)t.ptr();
        const llvm::Type *et = makeLLVMApproximateType(x->elementType);
        return llvmArrayType(et, x->size);
    }
    case VEC_TYPE : {
        VecType *x = (VecType *)t.ptr();
        const llvm::Type *et = makeLLVMApproximateType(x->elementType);
        return llvm::VectorType::get(et, x->size);
    }
    case TUPLE_TYPE : {
        TupleType *x = (TupleType *)t.ptr();
        vector<const llvm::Type *> llTypes;
        vector<TypePtr>::iterator i, end;
        for (i = x->elementTypes.begin(), end = x->elementTypes.end();
             i != end; ++i)
            llTypes.push_back(makeLLVMApproximateType(*i));
        if (x->elementTypes.empty())
            llTypes.push_back(llvmIntType(8));
        return llvm::StructType::get(llvm::getGlobalContext(), llTypes);
    }
    case RECORD_TYPE : {
        RecordType *x = (RecordType *)t.ptr();
        const vector<TypePtr> &fieldTypes = recordFieldTypes(x);
        vector<const llvm::Type *> llTypes;
        vector<TypePtr>::const_iterator i, end;
        for (i = fieldTypes.begin(), end = fieldTypes.end(); i != end; ++i)
            llTypes.push_back(makeLLVMApproximateType(*i));
        if (fieldTypes.empty())
            llTypes.push_back(llvmIntType(8));
        return llvm::StructType::get(llvm::getGlobalContext(), llTypes);
    }
    case VARIANT_TYPE : {
        VariantType *x = (VariantType *)t.ptr();
        return makeLLVMApproximateType(variantReprType(x));
    }

    default :
        assert(false);
        return NULL;
    }
}

static const llvm::Type *makeLLVMType(TypePtr t) {
    switch (t->typeKind) {
    case BOOL_TYPE : return llvmIntType(8);
    case INTEGER_TYPE : {
        IntegerType *x = (IntegerType *)t.ptr();
        return llvmIntType(x->bits);
    }
    case FLOAT_TYPE : {
        FloatType *x = (FloatType *)t.ptr();
        return llvmFloatType(x->bits);
    }
    case POINTER_TYPE : {
        PointerType *x = (PointerType *)t.ptr();
        return llvmPointerType(x->pointeeType);
    }
    case CODE_POINTER_TYPE : {
        CodePointerType *x = (CodePointerType *)t.ptr();
        vector<const llvm::Type *> llArgTypes;
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
            llvm::FunctionType::get(llvmIntType(32), llArgTypes, false);
        return llvm::PointerType::getUnqual(llFuncType);
    }
    case CCODE_POINTER_TYPE : {
        CCodePointerType *x = (CCodePointerType *)t.ptr();
        vector<const llvm::Type *> llArgTypes;
        for (unsigned i = 0; i < x->argTypes.size(); ++i)
            llArgTypes.push_back(llvmType(x->argTypes[i]));
        const llvm::Type *llReturnType =
            x->returnType.ptr() ? llvmType(x->returnType) : llvmVoidType();
        llvm::FunctionType *llFuncType =
            llvm::FunctionType::get(llReturnType, llArgTypes, x->hasVarArgs);
        return llvm::PointerType::getUnqual(llFuncType);
    }
    case ARRAY_TYPE : {
        ArrayType *x = (ArrayType *)t.ptr();
        return llvmArrayType(x->elementType, x->size);
    }
    case VEC_TYPE : {
        VecType *x = (VecType *)t.ptr();
        return llvm::VectorType::get(llvmType(x->elementType), x->size);
    }
    case TUPLE_TYPE : {
        TupleType *x = (TupleType *)t.ptr();
        vector<const llvm::Type *> llTypes;
        vector<TypePtr>::iterator i, end;
        for (i = x->elementTypes.begin(), end = x->elementTypes.end();
             i != end; ++i)
            llTypes.push_back(llvmType(*i));
        if (x->elementTypes.empty())
            llTypes.push_back(llvmIntType(8));
        return llvm::StructType::get(llvm::getGlobalContext(), llTypes);
    }
    case UNION_TYPE : {
        UnionType *x = (UnionType *)t.ptr();
        const llvm::Type *maxAlignType = NULL;
        size_t maxAlign = 0;
        size_t maxAlignSize = 0;
        size_t maxSize = 0;
        for (unsigned i = 0; i < x->memberTypes.size(); ++i) {
            const llvm::Type *llt =
                makeLLVMApproximateType(x->memberTypes[i]);
            size_t align = llvmTargetData->getABITypeAlignment(llt);
            size_t size = llvmTargetData->getTypeAllocSize(llt);
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
        vector<const llvm::Type *> llTypes;
        llTypes.push_back(maxAlignType);
        if (maxSize > maxAlignSize) {
            const llvm::Type *padding =
                llvm::ArrayType::get(llvmIntType(8), maxSize-maxAlignSize);
            llTypes.push_back(padding);
        }
        return llvm::StructType::get(llvm::getGlobalContext(), llTypes);
    }
    case RECORD_TYPE : {
        RecordType *x = (RecordType *)t.ptr();
        const vector<TypePtr> &fieldTypes = recordFieldTypes(x);
        vector<const llvm::Type *> llTypes;
        vector<TypePtr>::const_iterator i, end;
        for (i = fieldTypes.begin(), end = fieldTypes.end(); i != end; ++i)
            llTypes.push_back(llvmType(*i));
        if (fieldTypes.empty())
            llTypes.push_back(llvmIntType(8));
        return llvm::StructType::get(llvm::getGlobalContext(), llTypes);
    }
    case VARIANT_TYPE : {
        VariantType *x = (VariantType *)t.ptr();
        return llvmType(variantReprType(x));
    }
    case STATIC_TYPE : {
        vector<const llvm::Type *> llTypes;
        llTypes.push_back(llvmIntType(8));
        return llvm::StructType::get(llvm::getGlobalContext(), llTypes);
    }
    case ENUM_TYPE : {
        return llvmType(cIntType);
    }
    default :
        assert(false);
        return NULL;
    }
}



//
// typeSize, typePrint
//

static void initTypeInfo(Type *t) {
    if (!t->typeInfoInitialized) {
        t->typeInfoInitialized = true;
        const llvm::Type *llt = llvmType(t);
        t->typeSize = llvmTargetData->getTypeAllocSize(llt);
        t->typeAlignment = llvmTargetData->getABITypeAlignment(llt);
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

void typePrint(ostream &out, TypePtr t) {
    switch (t->typeKind) {
    case BOOL_TYPE :
        out << "Bool";
        break;
    case INTEGER_TYPE : {
        IntegerType *x = (IntegerType *)t.ptr();
        if (!x->isSigned)
            out << "U";
        out << "Int" << x->bits;
        break;
    }
    case FLOAT_TYPE : {
        FloatType *x = (FloatType *)t.ptr();
        out << "Float" << x->bits;
        break;
    }
    case POINTER_TYPE : {
        PointerType *x = (PointerType *)t.ptr();
        out << "Pointer[" << x->pointeeType << "]";
        break;
    }
    case CODE_POINTER_TYPE : {
        CodePointerType *x = (CodePointerType *)t.ptr();
        out << "CodePointer[";
        if (x->argTypes.size() == 1) {
            out << x->argTypes[0];
        }
        else {
            out << "(";
            for (unsigned i = 0; i < x->argTypes.size(); ++i) {
                if (i != 0)
                    out << ", ";
                out << x->argTypes[i];
            }
            out << ")";
        }
        out << ", ";
        if (x->returnTypes.size() == 1) {
            if (x->returnIsRef[0])
                out << "ByRef[" << x->returnTypes[0] << "]";
            else
                out << x->returnTypes[0];
        }
        else {
            out << "(";
            for (unsigned i = 0; i < x->returnTypes.size(); ++i) {
                if (i != 0)
                    out << ", ";
                if (x->returnIsRef[i])
                    out << "ByRef[" << x->returnTypes[i] << "]";
                else
                    out << x->returnTypes[i];
            }
            out << ")";
        }
        out << "]";
        break;
    }
    case CCODE_POINTER_TYPE : {
        CCodePointerType *x = (CCodePointerType *)t.ptr();
        switch (x->callingConv) {
        case CC_DEFAULT :
            if (x->hasVarArgs)
                out << "VarArgsCCodePointer";
            else
                out << "CCodePointer";
            break;
        case CC_STDCALL :
            out << "StdCallCodePointer";
            break;
        case CC_FASTCALL :
            out << "FastCallCodePointer";
            break;
        default :
            assert(false);
        }
        out << "[";
        if (x->argTypes.size() == 1) {
            out << x->argTypes[0];
        }
        else {
            out << "(";
            for (unsigned i = 0; i < x->argTypes.size(); ++i) {
                if (i != 0)
                    out << ", ";
                out << x->argTypes[i];
            }
            out << ")";
        }
        out << ", ";
        if (x->returnType.ptr())
            out << x->returnType;
        else
            out << "()";
        out << "]";
        break;
    }
    case ARRAY_TYPE : {
        ArrayType *x = (ArrayType *)t.ptr();
        out << "Array[" << x->elementType << ", " << x->size << "]";
        break;
    }
    case VEC_TYPE : {
        VecType *x = (VecType *)t.ptr();
        out << "Vec[" << x->elementType << ", " << x->size << "]";
        break;
    }
    case TUPLE_TYPE : {
        TupleType *x = (TupleType *)t.ptr();
        out << "Tuple" << x->elementTypes;
        break;
    }
    case UNION_TYPE : {
        UnionType *x = (UnionType *)t.ptr();
        out << "Union" << x->memberTypes;
        break;
    }
    case RECORD_TYPE : {
        RecordType *x = (RecordType *)t.ptr();
        out << x->record->name->str;
        if (!x->params.empty()) {
            out << "[";
            printNameList(out, x->params);
            out << "]";
        }
        break;
    }
    case VARIANT_TYPE : {
        VariantType *x = (VariantType *)t.ptr();
        out << x->variant->name->str;
        if (!x->params.empty()) {
            out << "[";
            printNameList(out, x->params);
            out << "]";
        }
        break;
    }
    case STATIC_TYPE : {
        StaticType *x = (StaticType *)t.ptr();
        out << "Static[";
        printName(out, x->obj);
        out << "]";
        break;
    }
    case ENUM_TYPE : {
        EnumType *x = (EnumType *)t.ptr();
        out << x->enumeration->name->str;
        break;
    }
    default :
        assert(false);
    }
}
