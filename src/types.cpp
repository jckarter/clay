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

VoidTypePtr voidType;
VoidValuePtr voidValue;

static vector<vector<PointerTypePtr> > pointerTypes;
static vector<vector<CodePointerTypePtr> > codePointerTypes;
static vector<vector<CCodePointerTypePtr> > cCodePointerTypes;
static vector<vector<ArrayTypePtr> > arrayTypes;
static vector<vector<TupleTypePtr> > tupleTypes;
static vector<vector<RecordTypePtr> > recordTypes;
static vector<vector<StaticObjectTypePtr> > staticObjectTypes;

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
    voidType = new VoidType();
    voidValue = new VoidValue();

    int N = 1024;
    pointerTypes.resize(N);
    codePointerTypes.resize(N);
    cCodePointerTypes.resize(N);
    arrayTypes.resize(N);
    tupleTypes.resize(N);
    recordTypes.resize(N);
    staticObjectTypes.resize(N);
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

TypePtr codePointerType(const vector<TypePtr> &argTypes, TypePtr returnType,
                        bool returnIsTemp) {
    int h = 0;
    for (unsigned i = 0; i < argTypes.size(); ++i) {
        h += pointerHash(argTypes[i].ptr());
    }
    if (returnType.ptr()) {
        h += pointerHash(returnType.ptr());
        h += returnIsTemp ? 1 : 0;
    }
    h &= codePointerTypes.size() - 1;
    vector<CodePointerTypePtr> &bucket = codePointerTypes[h];
    for (unsigned i = 0; i < bucket.size(); ++i) {
        CodePointerType *t = bucket[i].ptr();
        if ((t->argTypes == argTypes) && (t->returnType == returnType)) {
            if (!returnType || (t->returnIsTemp == returnIsTemp))
                return t;
        }
    }
    CodePointerTypePtr t = new CodePointerType(argTypes, returnType,
                                               returnIsTemp);
    bucket.push_back(t);
    return t.ptr();
}

TypePtr cCodePointerType(const vector<TypePtr> &argTypes, TypePtr returnType) {
    int h = 0;
    for (unsigned i = 0; i < argTypes.size(); ++i) {
        h += pointerHash(argTypes[i].ptr());
    }
    if (returnType.ptr())
        h += pointerHash(returnType.ptr());
    h &= cCodePointerTypes.size() - 1;
    vector<CCodePointerTypePtr> &bucket = cCodePointerTypes[h];
    for (unsigned i = 0; i < bucket.size(); ++i) {
        CCodePointerType *t = bucket[i].ptr();
        if ((t->argTypes == argTypes) && (t->returnType == returnType))
            return t;
    }
    CCodePointerTypePtr t = new CCodePointerType(argTypes, returnType);
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

TypePtr staticObjectType(ObjectPtr obj)
{
    int h = objectHash(obj);
    h &= staticObjectTypes.size() - 1;
    vector<StaticObjectTypePtr> &bucket = staticObjectTypes[h];
    for (unsigned i = 0; i < bucket.size(); ++i) {
        if (objectEquals(obj, bucket[i]->obj))
            return bucket[i].ptr();
    }
    StaticObjectTypePtr t = new StaticObjectType(obj);
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
// recordFieldTypes
//

static void initializeRecordFields(RecordTypePtr t) {
    assert(!t->fieldsInitialized);
    t->fieldsInitialized = true;
    RecordPtr r = t->record;
    assert(t->params.size() == r->patternVars.size());
    EnvPtr env = new Env(r->env);
    for (unsigned i = 0; i < t->params.size(); ++i)
        addLocal(env, r->patternVars[i], t->params[i].ptr());
    for (unsigned i = 0; i < r->fields.size(); ++i) {
        RecordField *x = r->fields[i].ptr();
        t->fieldIndexMap[x->name->str] = i;
        TypePtr ftype = evaluateType(x->type, env);
        t->fieldTypes.push_back(ftype);
    }
}

const vector<TypePtr> &recordFieldTypes(RecordTypePtr t) {
    if (!t->fieldsInitialized)
        initializeRecordFields(t);
    return t->fieldTypes;
}

const map<string, int> &recordFieldIndexMap(RecordTypePtr t) {
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
        const llvm::Type *llReturnType;
        if (!x->returnType) {
            llReturnType = llvmVoidType();
        }
        else if (x->returnIsTemp) {
            llArgTypes.push_back(llvmPointerType(x->returnType));
            llReturnType = llvmVoidType();
        }
        else {
            llReturnType = llvmPointerType(x->returnType);
        }
        llvm::FunctionType *llFuncType =
            llvm::FunctionType::get(llReturnType, llArgTypes, false);
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
            llvm::FunctionType::get(llReturnType, llArgTypes, false);
        return llvm::PointerType::getUnqual(llFuncType);
    }
    case ARRAY_TYPE : {
        ArrayType *x = (ArrayType *)t.ptr();
        return llvmArrayType(x->elementType, x->size);
    }
    case TUPLE_TYPE : {
        TupleType *x = (TupleType *)t.ptr();
        vector<const llvm::Type *> llTypes;
        vector<TypePtr>::iterator i, end;
        for (i = x->elementTypes.begin(), end = x->elementTypes.end();
             i != end; ++i)
            llTypes.push_back(llvmType(*i));
        return llvm::StructType::get(llvm::getGlobalContext(), llTypes);
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
        llvm::StructType *st =
            llvm::StructType::get(llvm::getGlobalContext(), llTypes);
        opaque->refineAbstractTypeTo(st);
        return x->llTypeHolder->get();
    }
    case STATIC_OBJECT_TYPE : {
        vector<const llvm::Type *> llTypes;
        return llvm::StructType::get(llvm::getGlobalContext(), llTypes);
    }
    case ENUM_TYPE : {
        return llvmIntType(32);
    }
    default :
        assert(false);
        return NULL;
    }
}



//
// typeSize, typePrint
//

int typeSize(TypePtr t) {
    if (t->typeSize < 0)
        t->typeSize = llvmTargetData->getTypeAllocSize(llvmType(t));
    return t->typeSize;
}

void typePrint(TypePtr t, ostream &out) {
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
        vector<ObjectPtr> v;
        for (unsigned i = 0; i < x->argTypes.size(); ++i)
            v.push_back(x->argTypes[i].ptr());
        v.push_back(x->returnType.ptr());
        if (!x->returnIsTemp)
            out << "Ref";
        out << "CodePointer" << v;
        break;
    }
    case CCODE_POINTER_TYPE : {
        CCodePointerType *x = (CCodePointerType *)t.ptr();
        vector<ObjectPtr> v;
        for (unsigned i = 0; i < x->argTypes.size(); ++i)
            v.push_back(x->argTypes[i].ptr());
        v.push_back(x->returnType.ptr());
        out << "CCodePointer" << v;
        break;
    }
    case ARRAY_TYPE : {
        ArrayType *x = (ArrayType *)t.ptr();
        out << "Array[" << x->elementType << ", " << x->size << "]";
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
        if (!x->params.empty())
            out << x->params;
        break;
    }
    case STATIC_OBJECT_TYPE : {
        StaticObjectType *x = (StaticObjectType *)t.ptr();
        out << "StaticObject[" << x->obj << "]";
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
