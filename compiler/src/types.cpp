#include "clay.hpp"

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
static vector<vector<PackTypePtr> > packTypes;
static vector<vector<TupleTypePtr> > tupleTypes;
static vector<vector<RecordTypePtr> > recordTypes;
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
    switch (sizeof(void*)) {
    case 4 :
        cSizeTType = uint32Type;
        cPtrDiffTType = int32Type;
        break;
    case 8 :
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
    packTypes.resize(N);
    tupleTypes.resize(N);
    recordTypes.resize(N);
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
    unsigned long long v = (unsigned long long)p;
    return (int)v;
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

TypePtr packType(TypePtr elementType, int size) {
    int h = pointerHash(elementType.ptr()) + size;
    h &= packTypes.size() - 1;
    vector<PackTypePtr>::iterator i, end;
    for (i = packTypes[h].begin(), end = packTypes[h].end();
         i != end; ++i) {
        PackType *t = i->ptr();
        if ((t->elementType == elementType) && (t->size == size))
            return t;
    }
    PackTypePtr t = new PackType(elementType, size);
    packTypes[h].push_back(t);
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

static void initializeRecordFields(RecordTypePtr t) {
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
        for (unsigned i = 0; i < mpv->size(); ++i) {
            TypePtr x = mpv->values[i]->type;
            IdentifierPtr name;
            TypePtr type;
            if (!unpackField(mpv->values[i]->type, name, type))
                argumentError(i, "each value should be a "
                              "tuple of (name,type)");
            t->fieldIndexMap[name->str] = i;
            t->fieldTypes.push_back(type);
        }
    }
    else {
        for (unsigned i = 0; i < body->fields.size(); ++i) {
            RecordField *x = body->fields[i].ptr();
            t->fieldIndexMap[x->name->str] = i;
            TypePtr ftype = evaluateType(x->type, env);
            t->fieldTypes.push_back(ftype);
        }
    }
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
    if (t->llTypeHolder != NULL)
        return t->llTypeHolder->get();
    const llvm::Type *llt = makeLLVMType(t);
    if (t->llTypeHolder == NULL)
        t->llTypeHolder = new llvm::PATypeHolder(llt);
    return llt;
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
    case PACK_TYPE : {
        PackType *x = (PackType *)t.ptr();
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
        const llvm::Type *llType =
            llvm::StructType::get(llvm::getGlobalContext(), llTypes);
        ostringstream out;
        out << t;
        llvmModule->addTypeName(out.str(), llType);
        return llType;
    }
    case RECORD_TYPE : {
        RecordType *x = (RecordType *)t.ptr();
        llvm::OpaqueType *opaque =
            llvm::OpaqueType::get(llvm::getGlobalContext());
        x->llTypeHolder = new llvm::PATypeHolder(opaque);
        const vector<TypePtr> &fieldTypes = recordFieldTypes(x);
        vector<const llvm::Type *> llTypes;
        vector<TypePtr>::const_iterator i, end;
        for (i = fieldTypes.begin(), end = fieldTypes.end(); i != end; ++i)
            llTypes.push_back(llvmType(*i));
        if (fieldTypes.empty())
            llTypes.push_back(llvmIntType(8));
        llvm::StructType *st =
            llvm::StructType::get(llvm::getGlobalContext(), llTypes);
        opaque->refineAbstractTypeTo(st);
        const llvm::Type *llType = x->llTypeHolder->get();
        ostringstream out;
        out << t;
        llvmModule->addTypeName(out.str(), llType);
        return llType;
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

size_t typeSize(TypePtr t) {
    if (!t->typeSizeInitialized) {
        t->typeSizeInitialized = true;
        t->typeSize = llvmTargetData->getTypeAllocSize(llvmType(t));
    }
    return t->typeSize;
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
        out << "(";
        for (unsigned i = 0; i < x->argTypes.size(); ++i) {
            if (i != 0)
                out << ", ";
            out << x->argTypes[i];
        }
        out << ")";
        out << ", (";
        for (unsigned i = 0; i < x->returnTypes.size(); ++i) {
            if (i != 0)
                out << ", ";
            if (x->returnIsRef[i])
                out << "ref ";
            out << x->argTypes[i];
        }
        out << ")";
        out << "]";
        break;
    }
    case CCODE_POINTER_TYPE : {
        CCodePointerType *x = (CCodePointerType *)t.ptr();
        out << "CCodePointer[";
        out << "(";
        for (unsigned i = 0; i < x->argTypes.size(); ++i) {
            if (i != 0)
                out << ", ";
            out << x->argTypes[i];
        }
        out << ")";
        out << ", (";
        if (x->returnType.ptr())
            out << x->returnType;
        out << ")";
        out << "]";
        break;
    }
    case ARRAY_TYPE : {
        ArrayType *x = (ArrayType *)t.ptr();
        out << "Array[" << x->elementType << ", " << x->size << "]";
        break;
    }
    case PACK_TYPE : {
        PackType *x = (PackType *)t.ptr();
        out << "Pack[" << x->elementType << ", " << x->size << "]";
        break;
    }
    case TUPLE_TYPE : {
        TupleType *x = (TupleType *)t.ptr();
        out << "Tuple" << x->elementTypes;
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
