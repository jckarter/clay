#include "clay.hpp"
#include "common.hpp"

llvm::Module *llvmModule;
llvm::ExecutionEngine *llvmEngine;
const llvm::TargetData *llvmTargetData;

void initLLVM() {
    llvm::InitializeNativeTarget();
    llvmModule = new llvm::Module("clay", llvm::getGlobalContext());
    llvm::EngineBuilder eb(llvmModule);
    llvmEngine = eb.create();
    assert(llvmEngine);
    llvmTargetData = llvmEngine->getTargetData();
    llvmModule->setDataLayout(llvmTargetData->getStringRepresentation());
}

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

TypePtr compilerObjectType;

TypePtr voidType;

static vector<vector<ArrayTypePtr> > arrayTypes;
static vector<vector<TupleTypePtr> > tupleTypes;
static vector<vector<PointerTypePtr> > pointerTypes;
static vector<vector<RecordTypePtr> > recordTypes;

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

    compilerObjectType = new CompilerObjectType();

    voidType = new VoidType();

    int N = 1024;
    arrayTypes.resize(N);
    tupleTypes.resize(N);
    pointerTypes.resize(N);
    recordTypes.resize(N);
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

TypePtr arrayType(TypePtr elementType, int size) {
    int h = pointerToInt(elementType.ptr()) + size;
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
        h += pointerToInt(ei->ptr());
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

TypePtr pointerType(TypePtr pointeeType) {
    int h = pointerToInt(pointeeType.ptr());
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

static bool valueVectorEquals(const vector<ValuePtr> &a,
                              const vector<ValuePtr> &b) {
    if (a.size() != b.size()) return false;
    vector<ValuePtr>::const_iterator ai, aend, bi;
    for (ai = a.begin(), aend = a.end(), bi = b.begin();
         ai != aend; ++ai, ++bi) {
        if (!valueEquals(*ai, *bi))
            return false;
    }
    return true;
}

TypePtr recordType(RecordPtr record, const vector<ValuePtr> &params) {
    int h = pointerToInt(record.ptr());
    vector<ValuePtr>::const_iterator vi, vend;
    for (vi = params.begin(), vend = params.end(); vi != vend; ++vi)
        h += valueHash(*vi);
    h &= recordTypes.size() - 1;
    vector<RecordTypePtr>::iterator i, end;
    for (i = recordTypes[h].begin(), end = recordTypes[h].end();
         i != end; ++i) {
        RecordType *t = i->ptr();
        if ((t->record == record) && valueVectorEquals(t->params, params))
            return t;
    }
    RecordTypePtr t = new RecordType(record);
    for (vi = params.begin(), vend = params.end(); vi != vend; ++vi)
        t->params.push_back(cloneValue(*vi));
    recordTypes[h].push_back(t);
    return t.ptr();
}



//
// recordFieldTypes
//

static void initializeRecordFields(RecordTypePtr t) {
    t->fieldsInitialized = true;
    RecordPtr r = t->record;
    assert(t->params.size() == r->patternVars.size());
    EnvPtr env = new Env(r->env);
    for (unsigned i = 0; i < t->params.size(); ++i)
        addLocal(env, r->patternVars[i], t->params[i].ptr());
    for (unsigned i = 0; i < r->formalArgs.size(); ++i) {
        FormalArg *x = r->formalArgs[i].ptr();
        switch (x->objKind) {
        case VALUE_ARG : {
            ValueArg *y = (ValueArg *)x;
            t->fieldIndexMap[y->name->str] = t->fieldTypes.size();
            TypePtr ftype = evaluateNonVoidType(y->type, env);
            t->fieldTypes.push_back(ftype);
            break;
        }
        case STATIC_ARG :
            break;
        default :
            assert(false);
        }
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
    case BOOL_TYPE :
        return llvm::Type::getInt8Ty(llvm::getGlobalContext());
    case INTEGER_TYPE : {
        IntegerType *x = (IntegerType *)t.ptr();
        return llvm::IntegerType::get(llvm::getGlobalContext(), x->bits);
    }
    case FLOAT_TYPE : {
        FloatType *x = (FloatType *)t.ptr();
        switch (x->bits) {
        case 32 :
            return llvm::Type::getFloatTy(llvm::getGlobalContext());
        case 64 :
            return llvm::Type::getDoubleTy(llvm::getGlobalContext());
        default :
            assert(false);
            return NULL;
        }
    }
    case ARRAY_TYPE : {
        ArrayType *x = (ArrayType *)t.ptr();
        return llvm::ArrayType::get(llvmType(x->elementType), x->size);
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
    case POINTER_TYPE : {
        PointerType *x = (PointerType *)t.ptr();
        return llvm::PointerType::getUnqual(llvmType(x->pointeeType));
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
    case COMPILER_OBJECT_TYPE :
        return llvm::IntegerType::get(llvm::getGlobalContext(), 32);
    case VOID_TYPE :
        return llvm::Type::getVoidTy(llvm::getGlobalContext());
    default :
        assert(false);
        return NULL;
    }
}

int typeSize(TypePtr t) {
    if (t->typeSize < 0)
        t->typeSize = llvmTargetData->getTypeAllocSize(llvmType(t));
    return t->typeSize;
}



//
// typePrint
//

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
    case POINTER_TYPE : {
        PointerType *x = (PointerType *)t.ptr();
        out << "Pointer[" << x->pointeeType << "]";
        break;
    }
    case RECORD_TYPE : {
        RecordType *x = (RecordType *)t.ptr();
        out << x->record->name->str;
        if (!x->params.empty())
            out << x->params;
        break;
    }
    case COMPILER_OBJECT_TYPE :
        out << "CompilerObject";
        break;
    case VOID_TYPE :
        out << "Void";
        break;
    default :
        assert(false);
    }
}
