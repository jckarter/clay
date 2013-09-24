#include "clay.hpp"
#include "externals.hpp"
#include "objects.hpp"
#include "error.hpp"



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
        NewType *nt = (NewType *)t.ptr();
        return promoteCVarArg(conv, newtypeReprType(nt), llv, ctx);
    }
    default :
        return llv;
    }
}

llvm::FunctionType* ExternalFunction::getLLVMFunctionType() const {
    llvm::Type *retType;
    vector<llvm::Type*> argTypes;

    if (retInfo.type == NULL) {
        retType = llvmVoidType();
    } else if (retInfo.ext.isIndirect()) {
        retType = llvmVoidType();
        argTypes.push_back(llvmPointerType(retInfo.type));
    } else {
        retType = retInfo.ext.type ?
            retInfo.ext.type : llvmType(retInfo.type);
    }

    for (vector<ArgInfo>::const_iterator
        it = argInfos.begin(), ie = argInfos.end();
        it != ie; ++it)
    {
        llvm::Type *type;
        if (it->ext.isIndirect()) {
            type = llvmPointerType(it->type);
        } else {
            type = it->ext.type ? it->ext.type : llvmType(it->type);
        }
        argTypes.push_back(type);
    }

    return llvm::FunctionType::get(retType, argTypes, isVarArg);
}

void ExternalFunction::allocReturnValue(ArgInfo info,
                                        llvm::Function::arg_iterator &ai,
                                        vector<CReturn> &returns,
                                        CodegenContext* ctx)
{
    if (info.type == NULL)
        return;
    else if (info.ext.isIndirect()) {
        llvm::Argument *structReturnArg = &(*ai);
        assert(structReturnArg->getType() == llvmPointerType(info.type));
        structReturnArg->setName("returned");
        ++ai;
        CValuePtr cret = new CValue(info.type, structReturnArg);
        returns.push_back(CReturn(false, info.type, cret));
    } else {
        llvm::Value *llRetVal =
            ctx->initBuilder->CreateAlloca(llvmType(info.type));
        CValuePtr cret = new CValue(info.type, llRetVal);
        returns.push_back(CReturn(false, info.type, cret));
    }
}

CValuePtr ExternalFunction::allocArgumentValue(ArgInfo info,
                                               llvm::StringRef name,
                                               llvm::Function::arg_iterator &ai,
                                               CodegenContext* ctx)
{
    if (info.ext.isIndirect()) {
        llvm::Argument *llArg = &(*ai);
        assert(llArg->getType() == llvmPointerType(info.type));
        llArg->setName(name);
        CValuePtr cvalue = new CValue(info.type, llArg);
        ++ai;
        return cvalue;
    } else {
        llvm::Type *bitcastType = info.ext.type;
        if (bitcastType != NULL) {
            llvm::Argument *llArg = &(*ai);
            assert(llArg->getType() == bitcastType);
            llArg->setName(name);
            llvm::Value *llArgAlloc =
                ctx->initBuilder->CreateAlloca(bitcastType);
            ctx->builder->CreateStore(llArg, llArgAlloc);
            llvm::Value *llArgVar =
                ctx->initBuilder->CreateBitCast(
                    llArgAlloc, llvmPointerType(info.type));
            CValuePtr cvalue = new CValue(info.type, llArgVar);
            ++ai;
            return cvalue;
        } else {
            llvm::Argument *llArg = &(*ai);
            llArg->setName(name);
            llvm::Value *llArgVar =
                ctx->initBuilder->CreateAlloca(llvmType(info.type));
            llArgVar->setName(name);
            ctx->builder->CreateStore(llArg, llArgVar);
            CValuePtr cvalue = new CValue(info.type, llArgVar);
            ++ai;
            return cvalue;
        }
    }
}

void ExternalFunction::returnStatement(ArgInfo info,
                                       vector<CReturn> &returns,
                                       CodegenContext* ctx)
{
    if (info.type == NULL || info.ext.isIndirect()) {
        ctx->builder->CreateRetVoid();
    } else {
        llvm::Type *bitcastType = info.ext.type;
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

void ExternalFunction::loadStructRetArgument(ArgInfo info,
                                             vector<llvm::Value *> &llArgs,
                                             CodegenContext* ctx,
                                             MultiCValuePtr out)
{
    if (info.type != NULL && info.ext.isIndirect()) {
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0->type == info.type);
        llArgs.push_back(out0->llValue);
    }
}

void ExternalFunction::loadArgument(ArgInfo info,
                                    CValuePtr cv,
                                    vector<llvm::Value *> &llArgs,
                                    CodegenContext* ctx,
                                    bool varArg)
{
    if (info.ext.isIndirect()) {
        llArgs.push_back(cv->llValue);
    } else {
        llvm::Type *bitcastType = info.ext.type;
        if (bitcastType != NULL) {
            llvm::Value *llArg = ctx->builder->CreateBitCast(cv->llValue,
                llvm::PointerType::getUnqual(bitcastType));
            llvm::Value *llv = ctx->builder->CreateLoad(llArg);
            llArgs.push_back(llv);
        } else {
            llvm::Value *llv = ctx->builder->CreateLoad(cv->llValue);
            if (varArg) {
                llv = promoteCVarArg(conv, info.type, llv, ctx);
            }
            llArgs.push_back(llv);
        }
    }
}

void ExternalFunction::storeReturnValue(ArgInfo info,
                                        llvm::Value *callReturnValue,
                                        CodegenContext* ctx,
                                        MultiCValuePtr out)
{
    if (info.type == NULL || info.ext.isIndirect()) {
        return;
    } else {
        llvm::Type *bitcastType = info.ext.type;
        assert(out->size() == 1);
        if (bitcastType != NULL) {
            CValuePtr out0 = out->values[0];
            assert(out0->type == info.type);
            llvm::Value *outRet = ctx->builder->CreateBitCast(out0->llValue,
                llvm::PointerType::getUnqual(bitcastType));
            ctx->builder->CreateStore(callReturnValue, outRet);
        } else {
            CValuePtr out0 = out->values[0];
            assert(out0->type == info.type);
            ctx->builder->CreateStore(callReturnValue, out0->llValue);
        }
    }
}

//
// ExternalTarget
//

ExternalFunction* ExternalTarget::getExternalFunction(
    CallingConv conv, TypePtr ret, vector<TypePtr> &args,
    size_t numReqArg, bool isVarArg)
{
    llvm::FoldingSetNodeID ID;
    ExternalFunction::Profile(ID, conv, ret, args, numReqArg, isVarArg);

    void *insertPos = 0;
    ExternalFunction *func = extFuncs.FindNodeOrInsertPos(ID, insertPos);
    if (func)
        return func;

    func = new ExternalFunction(conv, ret, args, numReqArg, isVarArg);
    extFuncs.InsertNode(func, insertPos);
    computeInfo(func);

    return func;
}


//
// Use the LLVM calling convention as-is with no argument mangling.
//

struct LLVMExternalTarget : public ExternalTarget {
    llvm::Triple target;

    explicit LLVMExternalTarget(llvm::Triple target)
        : ExternalTarget(), target(target) {}

    virtual void computeInfo(ExternalFunction *f);

    ExtArgInfo pushReturnType(
        CallingConv conv, TypePtr type,
        llvm::AttributeSet &llAttrs,
        unsigned &attrPos);

    ExtArgInfo pushArgumentType(
        CallingConv conv, TypePtr type,
        llvm::AttributeSet &llAttrs,
        unsigned &argPos, bool varArg);

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

void LLVMExternalTarget::computeInfo(ExternalFunction *f) {
    unsigned attrPos = 1;

    f->llConv = callingConvention(f->conv);
    f->retInfo.ext = pushReturnType(
        f->conv, f->retInfo.type, f->attrs, attrPos);

    for (size_t i = 0, e = f->argInfos.size(); i < e; ++i) {
        f->argInfos[i].ext = pushArgumentType(
            f->conv, f->argInfos[i].type, f->attrs,
            attrPos, i >= f->numReqArg);
    }
}

ExtArgInfo LLVMExternalTarget::pushReturnType(
    CallingConv conv, TypePtr type,
    llvm::AttributeSet &llAttrs,
    unsigned &attrPos)
{
    if (type == NULL)
        return ExtArgInfo::getDirect();
    else if (typeReturnsBySretPointer(conv, type)) {
        llAttrs = llAttrs.addAttribute(
            llvm::getGlobalContext(),
            attrPos++,
            llvm::Attribute::StructRet);
        return ExtArgInfo::getIndirect();
    } else {
        llvm::Type *bitcastType = typeReturnsAsBitcastType(conv, type);
        if (bitcastType != NULL)
            return ExtArgInfo::getDirect(bitcastType);
        else
            return ExtArgInfo::getDirect();
    }
}

ExtArgInfo LLVMExternalTarget::pushArgumentType(
    CallingConv conv, TypePtr type,
    llvm::AttributeSet &llAttrs,
    unsigned &attrPos, bool varArg)
{
    if (typePassesByByvalPointer(conv, type, varArg)) {
        llAttrs = llAttrs.addAttribute(
            llvm::getGlobalContext(),
            attrPos++,
            llvm::Attribute::ByVal);
        return ExtArgInfo::getIndirect();
    } else {
        llvm::Type *bitcastType = typePassesAsBitcastType(conv, type, varArg);
        if (bitcastType != NULL)
            return ExtArgInfo::getDirect(bitcastType);
        else
            return ExtArgInfo::getDirect();
    }
}


//
// x86-32 (and Windows x64)
//

struct X86_32_ExternalTarget : public LLVMExternalTarget {
    // Linux, Solaris, and NetBSD pass all aggregates on the stack
    // (except long long and complex float32)
    bool alwaysPassStructsOnStack:1;

    // Windows x64 has only one calling convention
    bool alwaysUseCCallingConv:1;

    // MacOS X passes XMM vectors in registers; other platforms treat them
    // as aggregates
    bool passVecAsNonaggregate:1;

    // MacOS X returns aggregates containing a single float or double
    // on the x87 stack
    bool returnSingleFloatAggregateAsNonaggregate:1;

    explicit X86_32_ExternalTarget(llvm::Triple target)
        : LLVMExternalTarget(target)
    {
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
                return int(typeSize(type));

        case COMPLEX_TYPE:
            return int(typeSize(type));

        case VARIANT_TYPE:
        case UNION_TYPE:
        case RECORD_TYPE:
        case TUPLE_TYPE:
            if (alwaysPassStructsOnStack)
                return PASS_ON_STACK;
            else
                return int(typeSize(type));

        case NEW_TYPE: {
            NewType *nt = (NewType*)type.ptr();
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
            return llvmIntType(unsigned(8*size));
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
            NewType *nt = (NewType*)type.ptr();
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
            llvm::ArrayRef<TypePtr> fieldTypes = recordFieldTypes(r);
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
    map<TypePtr, vector<WordClass> > typeClassifications;
    map<TypePtr, llvm::Type*> llvmWordTypes;
    // Mac OS X does its own thing with X87UP chunks not preceded by X87
    bool passesOrphanX87UPAsSSE:1;

    explicit X86_64_ExternalTarget(llvm::Triple target)
        : LLVMExternalTarget(target)
    {
        passesOrphanX87UPAsSSE = target.getOS() == llvm::Triple::Darwin;
    }

    llvm::ArrayRef<WordClass> getTypeClassification(TypePtr type);
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
            NewType *nt = (NewType*)type.ptr();
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

    static bool areYmmWordClasses(llvm::ArrayRef<WordClass> wordClasses) {
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

        llvm::ArrayRef<WordClass> wordClasses = getTypeClassification(type);
        assert(!wordClasses.empty());

        return wordClasses[0] == MEMORY;
    }

    virtual bool typePassesByByvalPointer(CallingConv conv, TypePtr type, bool varArg)
    {
        if (conv == CC_LLVM) return false;

        llvm::ArrayRef<WordClass> wordClasses = getTypeClassification(type);
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

static void _classifyStructType(llvm::ArrayRef<TypePtr> fields,
                                vector<WordClass>::iterator begin,
                                size_t offset);

static void _classifyType(TypePtr type, vector<WordClass>::iterator begin, size_t offset)
{
    size_t misalign = offset % typeAlignment(type);
    if (misalign != 0) {
        for (size_t i = offset/8; i < (offset+typeSize(type)+7)/8; ++i)
            unifyWordClass(begin + (long int)i, MEMORY);
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
        unifyWordClass(begin + (long int)offset/8, INTEGER);
        break;
    }
    case FLOAT_TYPE: {
        FloatType *x = (FloatType *)type.ptr();
        switch (x->bits) {
        case 32:
            if (offset % 8 == 4)
                unifyWordClass(begin + (long int)offset/8, SSE_FLOAT_VECTOR);
            else
                unifyWordClass(begin + (long int)offset/8, SSE_FLOAT_SCALAR);
            break;
        case 64:
            unifyWordClass(begin + (long int)offset/8, SSE_DOUBLE_SCALAR);
            break;
        case 80:
            unifyWordClass(begin + (long int)offset/8, X87);
            unifyWordClass(begin + (long int)offset/8 + 1, X87UP);
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
            _classifyType(imagType(complexType->bits), begin, offset + (size_t)complexType->bits/8);
            break;
        case 80:
            unifyWordClass(begin + (long int)offset/8, COMPLEX_X87);
            unifyWordClass(begin + (long int)offset/8 + 1, COMPLEX_X87);
            unifyWordClass(begin + (long int)offset/8 + 2, COMPLEX_X87);
            unifyWordClass(begin + (long int)offset/8 + 3, COMPLEX_X87);
            break;
        default:
            assert(false);
            break;
        }
        break;
    }
    case ARRAY_TYPE: {
        ArrayType *arrayType = (ArrayType *)type.ptr();
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
                unifyWordClass(begin + (long int)offset/8, SSE_FLOAT_VECTOR);
                break;
            case 64:
                unifyWordClass(begin + (long int)offset/8, SSE_DOUBLE_VECTOR);
                break;
            case 80:
                error("Float80 vec types not supported");
                break;
            }
        }
        default:
            unifyWordClass(begin + (long int)offset/8, SSE_INT_VECTOR);
            break;
        }
        for (size_t i = 1; i < (typeSize(type)+7)/8; ++i)
            unifyWordClass(begin + (long int)(offset/8 + i), SSEUP);
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
        _classifyStructType(tupleType->elementTypes,
                            begin,
                            offset);
        break;
    }
    case RECORD_TYPE: {
        RecordType *recordType = (RecordType *)type.ptr();
        llvm::ArrayRef<TypePtr> fieldTypes = recordFieldTypes(recordType);
        _classifyStructType(fieldTypes,
                            begin,
                            offset);
        break;
    }
    case NEW_TYPE: {
        NewType *nt = (NewType *)type.ptr();
        _classifyType(newtypeReprType(nt), begin, offset);
    }
    }
}

static void _classifyStructType(llvm::ArrayRef<TypePtr> fields,
                                vector<WordClass>::iterator begin,
                                size_t offset)
{
    if (fields.empty())
        _classifyType(intType(8), begin, offset);
    else {
        size_t fieldOffset = offset;
        for (TypePtr const *i = fields.begin(), *end = fields.end();
             i != end;
             ++i)
        {
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

llvm::ArrayRef<WordClass> X86_64_ExternalTarget::getTypeClassification(TypePtr type)
{
    map< TypePtr, vector<WordClass> >::iterator found = typeClassifications.find(type);
    if (found == typeClassifications.end())
        return typeClassifications.insert(make_pair(type, classifyType(type))).first->second;
    else
        return found->second;
}

llvm::Type *X86_64_ExternalTarget::llvmWordType(TypePtr type)
{
    llvm::ArrayRef<WordClass> wordClasses = getTypeClassification(type);
    assert(!wordClasses.empty());

    llvm::StructType *llType = llvm::StructType::create(llvm::getGlobalContext(), "x86-64 " + typeName(type));
    vector<llvm::Type*> llWordTypes;
    WordClass const *i = wordClasses.begin();
    size_t size = typeSize(type);
    while (i != wordClasses.end()) {
        assert(size > 0);
        switch (*i) {
        // docs don't cover this case. is it possible?
        // e.g. struct { __m128 a; __m256 b; };
        case NO_CLASS:
            assert(false);
            break;
        case INTEGER: {
            unsigned wordSize = size >= 8 ? 64 : unsigned(size*8);
            llWordTypes.push_back(llvmIntType(wordSize));
            ++i;
            break;
        }
        case SSE_INT_VECTOR: {
            unsigned vectorRun = 0;
            do { ++vectorRun; ++i; } while (i != wordClasses.end() && *i == SSEUP);
            // 8-byte int vectors are allocated to MMX registers, so always generate
            // a <float x n> vector for 64-bit SSE words.
            if (vectorRun == 1)
                llWordTypes.push_back(llvm::VectorType::get(llvmFloatType(64), vectorRun));
            else
                llWordTypes.push_back(llvm::VectorType::get(llvmIntType(64), vectorRun));
            break;
        }
        case SSE_FLOAT_VECTOR: {
            unsigned vectorRun = 0;
            do { ++vectorRun; ++i; } while (i != wordClasses.end() && *i == SSEUP);
            llWordTypes.push_back(llvm::VectorType::get(llvmFloatType(32), vectorRun*2));
            break;
        }
        case SSE_DOUBLE_VECTOR: {
            unsigned vectorRun = 0;
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
        assert(size >= 8 || i == wordClasses.end());
        size -= 8;
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
// Unix MIPS
//

struct Mips32_ExternalTarget : public ExternalTarget {
    size_t stackAlign;
    size_t minABIAlign;

    explicit Mips32_ExternalTarget() {
        minABIAlign = 4;
        stackAlign = 8;
    }

    void coerceToIntArgs(size_t tySize, vector<llvm::Type*> &argList) const;
    llvm::Type* getPaddingType(size_t align, size_t offset) const;
    llvm::Type* getLLVMWordType(TypePtr type,
                                llvm::Type *padding,
                                bool coercion) const;
    ExtArgInfo classifyArgumentType(TypePtr type, size_t &offset);
    ExtArgInfo classifyReturnType(TypePtr type);

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
            return true;
        case VEC_TYPE:
        case COMPLEX_TYPE:
        case VARIANT_TYPE:
        case UNION_TYPE:
        case RECORD_TYPE:
        case TUPLE_TYPE:
            return false;
        case NEW_TYPE: {
            NewType *nt = (NewType*)type.ptr();
            return canPassThrough(newtypeReprType(nt));
        }
        default:
            assert(false);
            return false;
        }
    }

    virtual void computeInfo(ExternalFunction *f) {
        f->llConv = llvm::CallingConv::C;
        f->retInfo.ext = classifyReturnType(f->retInfo.type);

        unsigned attrPos = 1;
        bool sret = f->retInfo.ext.isIndirect();
        if (sret)
            f->attrs = f->attrs.addAttribute(llvm::getGlobalContext(),
                                            attrPos++,
                                            llvm::Attribute::StructRet);

        size_t offset = sret ? minABIAlign : 0;
        for (vector<ExternalFunction::ArgInfo>::iterator
            it = f->argInfos.begin(), ie = f->argInfos.end();
            it != ie; ++it)
        {
            it->ext = classifyArgumentType(it->type, offset);
            if (it->ext.isIndirect())
                f->attrs = f->attrs.addAttribute(llvm::getGlobalContext(),
                                                attrPos++,
                                                llvm::Attribute::ByVal);
        }
    }
};

void Mips32_ExternalTarget::coerceToIntArgs(size_t tySize,
                                            vector<llvm::Type*> &argList)
                                            const {
    llvm::Type *intType = llvmIntType(minABIAlign * 8);

    for (unsigned n = tySize / (minABIAlign * 8); n > 0; --n) {
        argList.push_back(intType);
    }

    unsigned r = tySize % (minABIAlign * 8);

    if (r > 0) {
        argList.push_back(llvmIntType(r));
    }
}

llvm::Type* Mips32_ExternalTarget::getPaddingType(size_t align,
                                                  size_t offset) const {
    if (((align - 1) & offset) > 0) {
        return llvmIntType(minABIAlign * 8);
    }

    return NULL;
}

llvm::Type* Mips32_ExternalTarget::getLLVMWordType(TypePtr type,
                                                   llvm::Type *padding,
                                                   bool coercion) const {
    size_t tySize = typeSize(type) * 8;
    vector<llvm::Type*> argList;

    if (padding) {
        argList.push_back(padding);
    }
    if (coercion) {
        coerceToIntArgs(tySize, argList);
    } else {
        argList.push_back(llvmType(type));
    }

    llvm::StructType *llType = llvm::StructType::get(
        llvm::getGlobalContext(), argList);

    return llType;
}

ExtArgInfo Mips32_ExternalTarget::classifyArgumentType(
    TypePtr type, size_t &offset)
{
    size_t origOffset = offset;
    size_t tySize = typeSize(type) * 8;
    size_t align = typeAlignment(type);

    align = std::min(std::max(align, minABIAlign), stackAlign);
    offset = alignedUpTo(offset, align);
    offset += alignedUpTo(tySize, align * 8) / 8;

    llvm::Type *padding = getPaddingType(align, origOffset);
    llvm::Type *llType = NULL;

    if (!canPassThrough(type)) {
        llType = getLLVMWordType(type, padding, true);
    } else if (padding) {
        llType = getLLVMWordType(type, padding, false);
    }

    return ExtArgInfo::getDirect(llType);
}

ExtArgInfo Mips32_ExternalTarget::classifyReturnType(TypePtr type) {
    if (type == NULL) return ExtArgInfo::getDirect();

    size_t tySize = typeSize(type) * 8;

    if (!canPassThrough(type)) {
        if (tySize <= 128) {
            if (type->typeKind == COMPLEX_TYPE) {
                return ExtArgInfo::getDirect();
            }

            if (type->typeKind == VEC_TYPE) {
                VecType *vecType = (VecType *)type.ptr();
                if (vecType->elementType->typeKind != FLOAT_TYPE) {
                    return ExtArgInfo::getDirect(
                        getLLVMWordType(type, NULL, true));
                }
            }
        }

        return ExtArgInfo::getIndirect();
    }

    return ExtArgInfo::getDirect();;
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
    } else if (target.getArch() == llvm::Triple::mips) {
        externalTarget = new Mips32_ExternalTarget();
    } else
        externalTarget = new LLVMExternalTarget(target);
}

ExternalTargetPtr getExternalTarget()
{
    return externalTarget;
}

}
