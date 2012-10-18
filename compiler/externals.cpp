#include "clay.hpp"
#include "externals.hpp"



#pragma clang diagnostic ignored "-Wcovered-switch-default"


namespace clay {

static llvm::Value *promoteCVarArg(CallingConv conv,
                                   TypePtr t,
                                   llvm::Value *llv,
                                   CodegenContext* ctx)
{
    if (conv == CC_LLVM)
        return llv;

    switch (t->typeKind) {
    case INTEGER_TYPE : {
        IntegerType *it = (IntegerType *)t.ptr();
        if (it->bits < 32) {
            if (it->isSigned)
                return ctx->builder->CreateSExt(llv, llvmType(int32Type));
            else
                return ctx->builder->CreateZExt(llv, llvmType(uint32Type));
        }
        return llv;
    }
    case FLOAT_TYPE : {
        FloatType *ft = (FloatType *)t.ptr();
        if (ft->bits == 32) {
            if(ft->isImaginary)
                return ctx->builder->CreateFPExt(llv, llvmType(imag64Type));
            else
                return ctx->builder->CreateFPExt(llv, llvmType(float64Type));
        }
        return llv;
    }
    case NEW_TYPE : {
        NewTypeType *nt = (NewTypeType *)t.ptr();
        return promoteCVarArg(conv, newtypeReprType(nt), llv, ctx);
    }
    default :
        return llv;
    }
}

llvm::Type *ExternalTarget::pushReturnType(CallingConv conv,
                                           TypePtr type,
                                           vector<llvm::Type *> &llArgTypes,
                                           vector< pair<unsigned, llvm::Attributes> > &llAttributes)
{
    if (type == NULL)
        return llvmVoidType();
    else if (typeReturnsBySretPointer(conv, type)) {
        llArgTypes.push_back(llvmPointerType(type));
        llAttributes.push_back(make_pair(llArgTypes.size(), llvm::Attribute::StructRet));
        return llvmVoidType();
    } else {
        llvm::Type *bitcastType = typeReturnsAsBitcastType(conv, type);
        if (bitcastType != NULL)
            return bitcastType;
        else
            return llvmType(type);
    }
}

void ExternalTarget::pushArgumentType(CallingConv conv,
                                      TypePtr type,
                                      vector<llvm::Type *> &llArgTypes,
                                      vector< pair<unsigned, llvm::Attributes> > &llAttributes)
{
    if (typePassesByByvalPointer(conv, type, false)) {
        llArgTypes.push_back(llvmPointerType(type));
        llAttributes.push_back(make_pair(llArgTypes.size(), llvm::Attribute::ByVal));
    } else {
        llvm::Type *bitcastType = typePassesAsBitcastType(conv, type, false);
        if (bitcastType != NULL)
            llArgTypes.push_back(bitcastType);
        else
            llArgTypes.push_back(llvmType(type));
    }
}

void ExternalTarget::allocReturnValue(CallingConv conv,
                                      TypePtr type,
                                      llvm::Function::arg_iterator &ai,
                                      vector<CReturn> &returns,
                                      CodegenContext* ctx)
{
    if (type == NULL)
        return;
    else if (typeReturnsBySretPointer(conv, type)) {
        llvm::Argument *structReturnArg = &(*ai);
        assert(structReturnArg->getType() == llvmPointerType(type));
        structReturnArg->setName("returned");
        ++ai;
        CValuePtr cret = new CValue(type, structReturnArg);
        returns.push_back(CReturn(false, type, cret));
    } else {
        llvm::Value *llRetVal =
            ctx->initBuilder->CreateAlloca(llvmType(type));
        CValuePtr cret = new CValue(type, llRetVal);
        returns.push_back(CReturn(false, type, cret));
    }
}

CValuePtr ExternalTarget::allocArgumentValue(CallingConv conv,
                                             TypePtr type,
                                             llvm::StringRef name,
                                             llvm::Function::arg_iterator &ai,
                                             CodegenContext* ctx)
{
    if (typePassesByByvalPointer(conv, type, false)) {
        llvm::Argument *llArg = &(*ai);
        assert(llArg->getType() == llvmPointerType(type));
        llArg->setName(name);
        CValuePtr cvalue = new CValue(type, llArg);
        ++ai;
        return cvalue;
    } else {
        llvm::Type *bitcastType = typePassesAsBitcastType(conv, type, false);
        if (bitcastType != NULL) {
            llvm::Argument *llArg = &(*ai);
            assert(llArg->getType() == bitcastType);
            llArg->setName(name);
            llvm::Value *llArgAlloc =
                ctx->initBuilder->CreateAlloca(bitcastType);
            ctx->builder->CreateStore(llArg, llArgAlloc);
            llvm::Value *llArgVar =
                ctx->initBuilder->CreateBitCast(llArgAlloc, llvmPointerType(type));
            CValuePtr cvalue = new CValue(type, llArgVar);
            ++ai;
            return cvalue;
        } else {
            llvm::Argument *llArg = &(*ai);
            llArg->setName(name);
            llvm::Value *llArgVar =
                ctx->initBuilder->CreateAlloca(llvmType(type));
            llArgVar->setName(name);
            ctx->builder->CreateStore(llArg, llArgVar);
            CValuePtr cvalue = new CValue(type, llArgVar);
            ++ai;
            return cvalue;
        }
    }
}

void ExternalTarget::returnStatement(CallingConv conv,
                                     TypePtr type,
                                     vector<CReturn> &returns,
                                     CodegenContext* ctx)
{
    if (type == NULL || typeReturnsBySretPointer(conv, type)) {
        ctx->builder->CreateRetVoid();
    } else {
        llvm::Type *bitcastType = typeReturnsAsBitcastType(conv, type);
        if (bitcastType != NULL) {
            CValuePtr retVal = returns[0].value;
            llvm::Value *bitcast = ctx->builder->CreateBitCast(retVal->llValue,
                llvm::PointerType::getUnqual(bitcastType));
            llvm::Value *v = ctx->builder->CreateLoad(bitcast);
            ctx->builder->CreateRet(v);
        } else {
            CValuePtr retVal = returns[0].value;
            llvm::Value *v = ctx->builder->CreateLoad(retVal->llValue);
            ctx->builder->CreateRet(v);
        }
    }
}

void ExternalTarget::loadStructRetArgument(CallingConv conv,
                                           TypePtr type,
                                           vector<llvm::Value *> &llArgs,
                                           vector< pair<unsigned, llvm::Attributes> > &llAttributes,
                                           CodegenContext* ctx,
                                           MultiCValuePtr out)
{
    if (type != NULL && typeReturnsBySretPointer(conv, type)) {
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0->type == type);
        llArgs.push_back(out0->llValue);
        llAttributes.push_back(make_pair(llArgs.size(), llvm::Attribute::StructRet));
    }
}

void ExternalTarget::loadArgument(CallingConv conv,
                                  CValuePtr cv,
                                  vector<llvm::Value *> &llArgs,
                                  vector< pair<unsigned, llvm::Attributes> > &llAttributes,
                                  CodegenContext* ctx)
{
    if (typePassesByByvalPointer(conv, cv->type, false)) {
        llArgs.push_back(cv->llValue);
        llAttributes.push_back(make_pair(llArgs.size(), llvm::Attribute::ByVal));
    } else {
        llvm::Type *bitcastType = typePassesAsBitcastType(conv, cv->type, false);
        if (bitcastType != NULL) {
            llvm::Value *llArg = ctx->builder->CreateBitCast(cv->llValue,
                llvm::PointerType::getUnqual(bitcastType));
            llvm::Value *llv = ctx->builder->CreateLoad(llArg);
            llArgs.push_back(llv);
        } else {
            llvm::Value *llv = ctx->builder->CreateLoad(cv->llValue);
            llArgs.push_back(llv);
        }
    }
}

void ExternalTarget::loadVarArgument(CallingConv conv,
                                     CValuePtr cv,
                                     vector<llvm::Value *> &llArgs,
                                     vector< pair<unsigned, llvm::Attributes> > &llAttributes,
                                     CodegenContext* ctx)
{
    if (typePassesByByvalPointer(conv, cv->type, true)) {
        llArgs.push_back(cv->llValue);
        llAttributes.push_back(make_pair(llArgs.size(), llvm::Attribute::ByVal));
    } else {
        llvm::Type *bitcastType = typePassesAsBitcastType(conv, cv->type, true);
        if (bitcastType != NULL) {
            llvm::Value *llArg = ctx->builder->CreateBitCast(cv->llValue,
                llvm::PointerType::getUnqual(bitcastType));
            llvm::Value *llv = ctx->builder->CreateLoad(llArg);
            llArgs.push_back(llv);
        } else {
            llvm::Value *llv = ctx->builder->CreateLoad(cv->llValue);
            llvm::Value *llv2 = promoteCVarArg(conv, cv->type, llv, ctx);

            llArgs.push_back(llv2);
        }
    }
}

void ExternalTarget::storeReturnValue(CallingConv conv,
                                      llvm::Value *callReturnValue,
                                      TypePtr returnType,
                                      CodegenContext* ctx,
                                      MultiCValuePtr out)
{
    if (returnType == NULL || typeReturnsBySretPointer(conv, returnType)) {
        return;
    } else {
        llvm::Type *bitcastType = typeReturnsAsBitcastType(conv, returnType);
        assert(out->size() == 1);
        if (bitcastType != NULL) {
            CValuePtr out0 = out->values[0];
            assert(out0->type == returnType);
            llvm::Value *outRet = ctx->builder->CreateBitCast(out0->llValue,
                llvm::PointerType::getUnqual(bitcastType));
            ctx->builder->CreateStore(callReturnValue, outRet);
        } else {
            CValuePtr out0 = out->values[0];
            assert(out0->type == returnType);
            ctx->builder->CreateStore(callReturnValue, out0->llValue);
        }
    }
}


//
// Use the LLVM calling convention as-is with no argument mangling.
//

struct LLVMExternalTarget : public ExternalTarget {
    llvm::Triple target;

    explicit LLVMExternalTarget(llvm::Triple target)
        : ExternalTarget(), target(target) {}

    virtual llvm::CallingConv::ID callingConvention(CallingConv conv) {
        return llvm::CallingConv::C;
    }

    virtual llvm::Type *typeReturnsAsBitcastType(CallingConv conv, TypePtr type) {
        return NULL;
    }
    virtual llvm::Type *typePassesAsBitcastType(CallingConv conv, TypePtr type, bool varArg) {
        return NULL;
    }
    virtual bool typeReturnsBySretPointer(CallingConv conv, TypePtr type) {
        return false;
    }
    virtual bool typePassesByByvalPointer(CallingConv conv, TypePtr type, bool varArg) {
        return false;
    }
};


//
// x86-32 (and Windows x64)
//

struct X86_32_ExternalTarget : public ExternalTarget {
    // Linux, Solaris, and NetBSD pass all aggregates on the stack
    // (except long long and complex float32)
    bool alwaysPassStructsOnStack;

    // Windows x64 has only one calling convention
    bool alwaysUseCCallingConv;

    // MacOS X passes XMM vectors in registers; other platforms treat them
    // as aggregates
    bool passVecAsNonaggregate;

    // MacOS X returns aggregates containing a single float or double
    // on the x87 stack
    bool returnSingleFloatAggregateAsNonaggregate;

    explicit X86_32_ExternalTarget(llvm::Triple target) {
        alwaysUseCCallingConv = target.getArch() == llvm::Triple::x86_64;
        alwaysPassStructsOnStack = target.getOS() == llvm::Triple::NetBSD
            || target.getOS() == llvm::Triple::Linux
            || target.getOS() == llvm::Triple::Solaris;
        passVecAsNonaggregate = target.getOS() == llvm::Triple::Darwin;
        returnSingleFloatAggregateAsNonaggregate
            = target.getOS() != llvm::Triple::Win32;
    }

    virtual llvm::CallingConv::ID callingConvention(CallingConv conv) {
        if (alwaysUseCCallingConv)
            return llvm::CallingConv::C;

        switch (conv) {
        case CC_DEFAULT :
        case CC_LLVM :
            return llvm::CallingConv::C;
        case CC_STDCALL :
            return llvm::CallingConv::X86_StdCall;
        case CC_FASTCALL :
            return llvm::CallingConv::X86_FastCall;
        case CC_THISCALL :
            return llvm::CallingConv::X86_ThisCall;
        default :
            assert(false);
            return llvm::CallingConv::C;
        }
    }

    static const int PASS_ON_STACK = -1;
    static const int PASS_THROUGH = -2;

    int aggregateTypeSize(TypePtr type) {
        switch (type->typeKind) {
        case BOOL_TYPE:
        case INTEGER_TYPE:
        case FLOAT_TYPE:
        case POINTER_TYPE:
        case CODE_POINTER_TYPE:
        case CCODE_POINTER_TYPE:
        case STATIC_TYPE:
        case ENUM_TYPE:
            return PASS_THROUGH;

        case VEC_TYPE:
            if (passVecAsNonaggregate)
                return PASS_THROUGH;
            else
                return typeSize(type);

        case COMPLEX_TYPE:
            return typeSize(type);

        case VARIANT_TYPE:
        case UNION_TYPE:
        case RECORD_TYPE:
        case TUPLE_TYPE:
            if (alwaysPassStructsOnStack)
                return PASS_ON_STACK;
            else
                return typeSize(type);
        
        case NEW_TYPE: {
            NewTypeType *nt = (NewTypeType*)type.ptr();
            return aggregateTypeSize(newtypeReprType(nt));
        }

        default:
            assert(false);
            return 0;
        }
    }

    llvm::Type *intTypeForAggregateSize(int size) {
        switch (size) {
        case 1:
        case 2:
        case 4:
        case 8:
            return llvmIntType(8*size);
        default:
            return NULL;
        }
    }

    bool isSingleFloatMemberType(TypePtr type) {
        if (type->typeKind == FLOAT_TYPE) {
            FloatType *ft = (FloatType *)type.ptr();
            return ft->bits <= 64;
        } else
            return false;
    }

    llvm::Type *singleFloatAggregateType(TypePtr type) {
        switch (type->typeKind) {
        case BOOL_TYPE:
        case INTEGER_TYPE:
        case FLOAT_TYPE:
        case POINTER_TYPE:
        case CODE_POINTER_TYPE:
        case CCODE_POINTER_TYPE:
        case STATIC_TYPE:
        case ENUM_TYPE:
        case VEC_TYPE:
        case COMPLEX_TYPE:
        case VARIANT_TYPE:
            return NULL;
        
        case NEW_TYPE: {
            NewTypeType *nt = (NewTypeType*)type.ptr();
            return singleFloatAggregateType(newtypeReprType(nt));
        }

        case UNION_TYPE: {
            UnionType *u = (UnionType*)type.ptr();
            if (u->memberTypes.size() == 1 && isSingleFloatMemberType(u->memberTypes[0]))
                return llvmType(u->memberTypes[0]);
            return NULL;
        }

        case RECORD_TYPE: {
            RecordType *r = (RecordType*)type.ptr();
            const vector<TypePtr> &fieldTypes = recordFieldTypes(r);
            if (fieldTypes.size() == 1 && isSingleFloatMemberType(fieldTypes[0]))
                return llvmType(fieldTypes[0]);
            return NULL;
        }

        case TUPLE_TYPE: {
            TupleType *t = (TupleType*)type.ptr();
            if (t->elementTypes.size() == 1 && isSingleFloatMemberType(t->elementTypes[0]))
                return llvmType(t->elementTypes[0]);
            return NULL;
        }

        default:
            assert(false);
            return NULL;
        }
    }

    virtual llvm::Type *typeReturnsAsBitcastType(CallingConv conv, TypePtr type) {
        if (conv == CC_LLVM) return NULL;
        if (returnSingleFloatAggregateAsNonaggregate) {
            llvm::Type *singleType = singleFloatAggregateType(type);
            if (singleType != NULL)
                return singleType;
        }
        return intTypeForAggregateSize(aggregateTypeSize(type));
    }
    virtual llvm::Type *typePassesAsBitcastType(CallingConv conv, TypePtr type, bool varArg) {
        if (conv == CC_LLVM) return NULL;
        return intTypeForAggregateSize(aggregateTypeSize(type));
    }
    virtual bool typeReturnsBySretPointer(CallingConv conv, TypePtr type) {
        if (conv == CC_LLVM) return false;
        if (returnSingleFloatAggregateAsNonaggregate) {
            llvm::Type *singleType = singleFloatAggregateType(type);
            if (singleType != NULL)
                return false;
        }

        int size = aggregateTypeSize(type);
        return size != PASS_THROUGH && intTypeForAggregateSize(size) == NULL;
    }
    virtual bool typePassesByByvalPointer(CallingConv conv, TypePtr type, bool varArg) {
        if (conv == CC_LLVM) return false;
        int size = aggregateTypeSize(type);
        return size != PASS_THROUGH && intTypeForAggregateSize(size) == NULL;
    }
};


//
// Unix x86-64
//

enum WordClass {
    NO_CLASS,
    INTEGER,
    // Vector and scalar SSE values are treated equivalently by the classification
    // algorithm, but LLVM breaks stuff if we don't emit the right vec type
    SSE_FLOAT_SCALAR,
    SSE_DOUBLE_SCALAR,
    SSE_FLOAT_VECTOR,
    SSE_DOUBLE_VECTOR,
    SSE_INT_VECTOR,
    SSEUP,
    X87,
    X87UP,
    COMPLEX_X87,
    MEMORY
};

struct X86_64_ExternalTarget : public LLVMExternalTarget {
    // Mac OS X does its own thing with X87UP chunks not preceded by X87
    bool passesOrphanX87UPAsSSE;

    explicit X86_64_ExternalTarget(llvm::Triple target)
        : LLVMExternalTarget(target)
    {
        passesOrphanX87UPAsSSE = target.getOS() == llvm::Triple::Darwin;
    }

    map<TypePtr, vector<WordClass> > typeClassifications;
    map<TypePtr, llvm::Type*> llvmWordTypes;
    vector<WordClass> const &getTypeClassification(TypePtr type);
    llvm::Type* getLLVMWordType(TypePtr type);

    llvm::Type *llvmWordType(TypePtr type);
    vector<WordClass> classifyType(TypePtr type);

    static bool canPassThrough(TypePtr type) {
        switch (type->typeKind) {
        case BOOL_TYPE:
        case INTEGER_TYPE:
        case FLOAT_TYPE:
        case POINTER_TYPE:
        case CODE_POINTER_TYPE:
        case CCODE_POINTER_TYPE:
        case STATIC_TYPE:
        case ENUM_TYPE:
        case VEC_TYPE:
            return true;
        case COMPLEX_TYPE:
        case VARIANT_TYPE:
        case UNION_TYPE:
        case RECORD_TYPE:
        case TUPLE_TYPE:
            return false;
        case NEW_TYPE: {
            NewTypeType *nt = (NewTypeType*)type.ptr();
            return canPassThrough(newtypeReprType(nt));
        }
        default:
            assert(false);
            return false;
        }
    }

    static bool isSSEClass(WordClass c) {
        return c == SSE_FLOAT_SCALAR
            || c == SSE_DOUBLE_SCALAR
            || c == SSE_FLOAT_VECTOR
            || c == SSE_DOUBLE_VECTOR
            || c == SSE_INT_VECTOR;
    }

    static bool areYmmWordClasses(vector<WordClass> const &wordClasses) {
        return (wordClasses.size() > 2
            && isSSEClass(wordClasses[0]) && wordClasses[1] == SSEUP && wordClasses[2] == SSEUP)
        || (wordClasses.size() > 3
            && isSSEClass(wordClasses[1]) && wordClasses[2] == SSEUP && wordClasses[3] == SSEUP);
    }

    void fixupClassification(TypePtr type, vector<WordClass> &wordClasses);

    virtual llvm::CallingConv::ID callingConvention(CallingConv conv) {
        return llvm::CallingConv::C;
    }

    virtual llvm::Type *typeReturnsAsBitcastType(CallingConv conv, TypePtr type)
    {
        if (conv == CC_LLVM) return NULL;
        if (typeReturnsBySretPointer(conv, type)) return NULL;
        if (canPassThrough(type)) return NULL;

        return getLLVMWordType(type);
    }

    virtual llvm::Type *typePassesAsBitcastType(CallingConv conv, TypePtr type, bool varArg)
    {
        if (conv == CC_LLVM) return NULL;
        if (typePassesByByvalPointer(conv, type, varArg)) return NULL;
        if (canPassThrough(type)) return NULL;

        return getLLVMWordType(type);
    }

    virtual bool typeReturnsBySretPointer(CallingConv conv, TypePtr type)
    {
        if (conv == CC_LLVM) return false;

        vector<WordClass> const &wordClasses = getTypeClassification(type);
        assert(!wordClasses.empty());

        return wordClasses[0] == MEMORY;
    }

    virtual bool typePassesByByvalPointer(CallingConv conv, TypePtr type, bool varArg)
    {
        if (conv == CC_LLVM) return false;

        vector<WordClass> const &wordClasses = getTypeClassification(type);
        assert(!wordClasses.empty());

        if (varArg && areYmmWordClasses(wordClasses))
            return true;

        // According to the spec, X87-class arguments should also be passed by byval
        // here; however, as of LLVM 3.1, LLVM appears not to pass an x87_fp80 correctly
        // unless it is passed by value instead of by byval pointer.
        return wordClasses[0] == MEMORY
            || wordClasses[0] == COMPLEX_X87;
    }

};

static void unifyWordClass(vector<WordClass>::iterator wordi, WordClass newClass)
{
    if (*wordi == newClass)
        return;
    else if (*wordi == NO_CLASS)
        *wordi = newClass;
    else if (newClass == NO_CLASS)
        return;
    else if (*wordi == MEMORY || newClass == MEMORY)
        *wordi = MEMORY;
    else if (*wordi == INTEGER || newClass == INTEGER)
        *wordi = INTEGER;
    else if (*wordi == X87 || *wordi == X87UP || *wordi == COMPLEX_X87
             || newClass == X87 || newClass == X87UP || newClass == COMPLEX_X87)
        *wordi = MEMORY;
    else {
        assert(X86_64_ExternalTarget::isSSEClass(newClass));
        *wordi = newClass;
    }
}

static void _classifyStructType(vector<TypePtr>::const_iterator fieldBegin,
                                vector<TypePtr>::const_iterator fieldEnd,
                                vector<WordClass>::iterator begin,
                                size_t offset);

static void _classifyType(TypePtr type, vector<WordClass>::iterator begin, size_t offset)
{
    size_t misalign = offset % typeAlignment(type);
    if (misalign != 0) {
        for (size_t i = offset/8; i < (offset+typeSize(type)+7)/8; ++i)
            unifyWordClass(begin + i, MEMORY);
        return;
    }

    switch (type->typeKind) {
    case BOOL_TYPE:
    case STATIC_TYPE:
    case ENUM_TYPE:
    case INTEGER_TYPE:
    case POINTER_TYPE:
    case CODE_POINTER_TYPE:
    case CCODE_POINTER_TYPE: {
        unifyWordClass(begin + offset/8, INTEGER);
        break;
    }
    case FLOAT_TYPE: {
        FloatType *x = (FloatType *)type.ptr();
        switch (x->bits) {
        case 32:
            if (offset % 8 == 4)
                unifyWordClass(begin + offset/8, SSE_FLOAT_VECTOR);
            else
                unifyWordClass(begin + offset/8, SSE_FLOAT_SCALAR);
            break;
        case 64:
            unifyWordClass(begin + offset/8, SSE_DOUBLE_SCALAR);
            break;
        case 80:
            unifyWordClass(begin + offset/8, X87);
            unifyWordClass(begin + offset/8 + 1, X87UP);
            break;
        default:
            assert(false);
            break;
        }
        break;
    }
    case COMPLEX_TYPE: {
        ComplexType *complexType = (ComplexType *)type.ptr();
        switch (complexType->bits) {
        case 32:
        case 64:
            _classifyType(floatType(complexType->bits), begin, offset);
            _classifyType(imagType(complexType->bits), begin, offset + complexType->bits/8);
            break;
        case 80:
            unifyWordClass(begin + offset/8, COMPLEX_X87);
            unifyWordClass(begin + offset/8 + 1, COMPLEX_X87);
            unifyWordClass(begin + offset/8 + 2, COMPLEX_X87);
            unifyWordClass(begin + offset/8 + 3, COMPLEX_X87);
            break;
        default:
            assert(false);
            break;
        }
        break;
    }
    case ARRAY_TYPE: {
        ArrayType *arrayType = (ArrayType *)type.ptr();
        assert(arrayType->size >= 0);
        for (size_t i = 0; i < (size_t)arrayType->size; ++i)
            _classifyType(arrayType->elementType, begin,
                          offset + i * typeSize(arrayType->elementType));
        break;
    }
    case VEC_TYPE: {
        VecType *vecType = (VecType *)type.ptr();
        switch (vecType->elementType->typeKind) {
        case FLOAT_TYPE: {
            FloatType *floatElementType = (FloatType*)vecType->elementType.ptr();
            switch (floatElementType->bits) {
            case 32:
                unifyWordClass(begin + offset/8, SSE_FLOAT_VECTOR);
                break;
            case 64:
                unifyWordClass(begin + offset/8, SSE_DOUBLE_VECTOR);
                break;
            case 80:
                error("Float80 vec types not supported");
                break;
            }
        }
        default:
            unifyWordClass(begin + offset/8, SSE_INT_VECTOR);
            break;
        }
        for (size_t i = 1; i < (typeSize(type)+7)/8; ++i)
            unifyWordClass(begin + offset/8 + i, SSEUP);
        break;
    }
    case VARIANT_TYPE: {
        VariantType *x = (VariantType *)type.ptr();
        _classifyType(variantReprType(x), begin, offset);
        break;
    }
    case UNION_TYPE: {
        UnionType *unionType = (UnionType *)type.ptr();
        for (size_t i = 0; i < unionType->memberTypes.size(); ++i)
            _classifyType(unionType->memberTypes[i], begin, offset);
        break;
    }
    case TUPLE_TYPE: {
        TupleType *tupleType = (TupleType *)type.ptr();
        _classifyStructType(tupleType->elementTypes.begin(),
                            tupleType->elementTypes.end(),
                            begin,
                            offset);
        break;
    }
    case RECORD_TYPE: {
        RecordType *recordType = (RecordType *)type.ptr();
        const vector<TypePtr> &fieldTypes = recordFieldTypes(recordType);
        _classifyStructType(fieldTypes.begin(),
                            fieldTypes.end(),
                            begin,
                            offset);
        break;
    }
    case NEW_TYPE: {
        NewTypeType *nt = (NewTypeType *)type.ptr();
        _classifyType(newtypeReprType(nt), begin, offset);
    }
    }
}

static void _classifyStructType(vector<TypePtr>::const_iterator fieldBegin,
                                vector<TypePtr>::const_iterator fieldEnd,
                                vector<WordClass>::iterator begin,
                                size_t offset)
{
    if (fieldBegin == fieldEnd)
        _classifyType(intType(8), begin, offset);
    else {
        size_t fieldOffset = offset;
        for (vector<TypePtr>::const_iterator i = fieldBegin; i != fieldEnd; ++i) {
            fieldOffset = alignedUpTo(fieldOffset, *i);
            _classifyType(*i, begin, fieldOffset);
            fieldOffset += typeSize(*i);
        }
    }
}

static void _allMemory(vector<WordClass> &wordClasses)
{
    fill(wordClasses.begin(), wordClasses.end(), MEMORY);
}

void X86_64_ExternalTarget::fixupClassification(TypePtr type, vector<WordClass> &wordClasses)
{
    vector<WordClass>::iterator i = wordClasses.begin(),
                                end = wordClasses.end();
    if (wordClasses.size() > 2 &&
        (type->typeKind == ARRAY_TYPE || type->typeKind == RECORD_TYPE
            || type->typeKind == VARIANT_TYPE || type->typeKind == UNION_TYPE
            || type->typeKind == TUPLE_TYPE || type->typeKind == RECORD_TYPE))
    {
        assert(i != end);
        if (X86_64_ExternalTarget::isSSEClass(*i)) {
            ++i;
            for (; i != end; ++i) {
                if (*i != SSEUP) {
                    _allMemory(wordClasses);
                    return;
                }
            }
        } else {
            _allMemory(wordClasses);
            return;
        }
    } else while (i != end) {
        if (*i == MEMORY) {
            _allMemory(wordClasses);
            return;
        }
        if (*i == X87UP) {
            if (passesOrphanX87UPAsSSE)
                *i = SSE_DOUBLE_SCALAR;
            else
                _allMemory(wordClasses);
            return;
        }
        if (*i == SSEUP) {
            *i = SSE_DOUBLE_VECTOR;
        } else if (X86_64_ExternalTarget::isSSEClass(*i)) {
            do { ++i; } while ((i != end) && (*i == SSEUP));
        } else if (*i == X87) {
            do { ++i; } while ((i != end) && (*i == X87UP));
        } else
            ++i;
    }
}

vector<WordClass> X86_64_ExternalTarget::classifyType(TypePtr type)
{
    size_t wordSize = (typeSize(type) + 7)/8;
    assert(wordSize > 0);
    vector<WordClass> wordClasses(wordSize, NO_CLASS);

    if (wordSize > 4) {
        _allMemory(wordClasses);
        return wordClasses;
    }
    _classifyType(type, wordClasses.begin(), 0);
    fixupClassification(type, wordClasses);

    return wordClasses;
}

vector<WordClass> const &X86_64_ExternalTarget::getTypeClassification(TypePtr type)
{
    map< TypePtr, vector<WordClass> >::iterator found = typeClassifications.find(type);
    if (found == typeClassifications.end())
        return typeClassifications.insert(make_pair(type, classifyType(type))).first->second;
    else
        return found->second;
}

llvm::Type *X86_64_ExternalTarget::llvmWordType(TypePtr type)
{
    vector<WordClass> const &wordClasses = getTypeClassification(type);
    assert(!wordClasses.empty());

    llvm::StructType *llType = llvm::StructType::create(llvm::getGlobalContext(), "x86-64 " + typeName(type));
    vector<llvm::Type*> llWordTypes;
    vector<WordClass>::const_iterator i = wordClasses.begin();
    while (i != wordClasses.end()) {
        switch (*i) {
        // docs don't cover this case. is it possible?
        // e.g. struct { __m128 a; __m256 b; };
        case NO_CLASS:
            assert(false);
            break;
        case INTEGER:
            llWordTypes.push_back(llvmIntType(64));
            ++i;
            break;
        case SSE_INT_VECTOR: {
            int vectorRun = 0;
            do { ++vectorRun; ++i; } while (i != wordClasses.end() && *i == SSEUP);
            // 8-byte int vectors are allocated to MMX registers
            if (vectorRun == 1)
                llWordTypes.push_back(llvm::VectorType::get(llvmFloatType(64), vectorRun));
            else
                llWordTypes.push_back(llvm::VectorType::get(llvmIntType(64), vectorRun));
            break;
        }
        case SSE_FLOAT_VECTOR: {
            int vectorRun = 0;
            do { ++vectorRun; ++i; } while (i != wordClasses.end() && *i == SSEUP);
            llWordTypes.push_back(llvm::VectorType::get(llvmFloatType(32), vectorRun*2));
            break;
        }
        case SSE_DOUBLE_VECTOR: {
            int vectorRun = 0;
            do { ++vectorRun; ++i; } while (i != wordClasses.end() && *i == SSEUP);
            llWordTypes.push_back(llvm::VectorType::get(llvmFloatType(64), vectorRun));
            break;
        }
        case SSE_FLOAT_SCALAR:
            llWordTypes.push_back(llvmFloatType(32));
            ++i;
            break;
        case SSE_DOUBLE_SCALAR:
            llWordTypes.push_back(llvmFloatType(64));
            ++i;
            break;
        case SSEUP:
            assert(false);
            break;
        case X87:
            assert(wordClasses.end() - i >= 2 && i[1] == X87UP);
            llWordTypes.push_back(llvmFloatType(80));
            i += 2;
            break;
        case X87UP:
            assert(false);
            break;
        case COMPLEX_X87:
            assert(wordClasses.end() - i >= 4
                && i[1] == COMPLEX_X87
                && i[2] == COMPLEX_X87
                && i[3] == COMPLEX_X87);
            llWordTypes.push_back(llvmFloatType(80));
            llWordTypes.push_back(llvmFloatType(80));
            i += 4;
            break;
        case MEMORY:
            assert(false);
            break;
        default:
            assert(false);
            break;
        }
    }
    llType->setBody(llWordTypes);
    return llType;
}

llvm::Type *X86_64_ExternalTarget::getLLVMWordType(TypePtr type)
{
    map<TypePtr, llvm::Type*>::iterator found = llvmWordTypes.find(type);
    if (found == llvmWordTypes.end()) {
        return llvmWordTypes.insert(make_pair(type, llvmWordType(type))).first->second;
    } else {
        return found->second;
    }
}



//
// getExternalTarget
//

static ExternalTargetPtr externalTarget;

void initExternalTarget(string targetString)
{
    llvm::Triple target(targetString);
    if (target.getArch() == llvm::Triple::x86_64
        && target.getOS() != llvm::Triple::Win32
        && target.getOS() != llvm::Triple::MinGW32
        && target.getOS() != llvm::Triple::Cygwin)
    {
        externalTarget = new X86_64_ExternalTarget(target);
    } else if (target.getArch() == llvm::Triple::x86
        || target.getArch() == llvm::Triple::x86_64)
    {
        externalTarget = new X86_32_ExternalTarget(target);
    } else
        externalTarget = new LLVMExternalTarget(target);
}

ExternalTargetPtr getExternalTarget()
{
    return externalTarget;
}

}
