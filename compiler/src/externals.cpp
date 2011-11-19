#include "clay.hpp"

static llvm::Value *promoteCVarArg(TypePtr t,
                                   llvm::Value *llv,
                                   CodegenContextPtr ctx)
{
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
        if (ft->bits == 32)
            return ctx->builder->CreateFPExt(llv, llvmType(float64Type));
        return llv;
    }
    default :
        return llv;
    }
}


//
// Use the LLVM calling convention as-is with no argument mangling.
// Seems to work on most platforms but not x86-64
//

struct VanillaExternalTarget : public ExternalTarget {
    llvm::Triple target;

    explicit VanillaExternalTarget(llvm::Triple target)
        : ExternalTarget(), target(target) {}

    virtual llvm::CallingConv::ID callingConvention(CallingConv conv) {
        if (target.getArch() != llvm::Triple::x86)
            return llvm::CallingConv::C;
        else switch (conv) {
        case CC_DEFAULT :
        case CC_LLVM :
            return llvm::CallingConv::C;
        case CC_STDCALL :
            return llvm::CallingConv::X86_StdCall;
        case CC_FASTCALL :
            return llvm::CallingConv::X86_FastCall;
        default :
            assert(false);
            return llvm::CallingConv::C;
        }
    }

    virtual llvm::Type *pushReturnType(TypePtr type,
                                       vector<llvm::Type *> &llArgTypes,
                                       vector< pair<unsigned, llvm::Attributes> > &llAttributes);
    virtual void pushArgumentType(TypePtr type,
                                  vector<llvm::Type *> &llArgTypes,
                                  vector< pair<unsigned, llvm::Attributes> > &llAttributes);

    virtual void allocReturnValue(TypePtr type,
                                  llvm::Function::arg_iterator &ai,
                                  vector<CReturn> &returns,
                                  CodegenContextPtr ctx);
    virtual CValuePtr allocArgumentValue(TypePtr type,
                                         string const &name,
                                         llvm::Function::arg_iterator &ai,
                                         CodegenContextPtr ctx);
    virtual void returnStatement(TypePtr type,
                                 vector<CReturn> &returns,
                                 CodegenContextPtr ctx);

    virtual void loadStructRetArgument(TypePtr type,
                                       vector<llvm::Value *> &llArgs,
                                       vector< pair<unsigned, llvm::Attributes> > &llAttributes,
                                       CodegenContextPtr ctx,
                                       MultiCValuePtr out);
    virtual void loadArgument(CValuePtr cv,
                              vector<llvm::Value *> &llArgs,
                              vector< pair<unsigned, llvm::Attributes> > &llAttributes,
                              CodegenContextPtr ctx);
    virtual void loadVarArgument(CValuePtr cv,
                                 vector<llvm::Value *> &llArgs,
                                 vector< pair<unsigned, llvm::Attributes> > &llAttributes,
                                 CodegenContextPtr ctx);
    virtual void storeReturnValue(llvm::Value *callReturnValue,
                                  TypePtr returnType,
                                  CodegenContextPtr ctx,
                                  MultiCValuePtr out);
};

llvm::Type *VanillaExternalTarget::pushReturnType(TypePtr type,
                                                  vector<llvm::Type *> &llArgTypes,
                                                  vector< pair<unsigned, llvm::Attributes> > &llAttributes)
{
    return type != NULL ? llvmType(type) : llvmVoidType();
}

void VanillaExternalTarget::pushArgumentType(TypePtr type,
                                             vector<llvm::Type *> &llArgTypes,
                                             vector< pair<unsigned, llvm::Attributes> > &llAttributes)
{
    llArgTypes.push_back(llvmType(type));
}

void VanillaExternalTarget::allocReturnValue(TypePtr type,
                                             llvm::Function::arg_iterator &ai,
                                             vector<CReturn> &returns,
                                             CodegenContextPtr ctx)
{
    if (type != NULL) {
        llvm::Value *llRetVal =
            ctx->initBuilder->CreateAlloca(llvmType(type));
        CValuePtr cret = new CValue(type, llRetVal);
        returns.push_back(CReturn(false, type, cret));
    }
}

CValuePtr VanillaExternalTarget::allocArgumentValue(TypePtr type,
                                                    string const &name,
                                                    llvm::Function::arg_iterator &ai,
                                                    CodegenContextPtr ctx)
{
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

void VanillaExternalTarget::returnStatement(TypePtr type,
                                            vector<CReturn> &returns,
                                            CodegenContextPtr ctx)
{
    if (type != NULL) {
        CValuePtr retVal = returns[0].value;
        llvm::Value *v = ctx->builder->CreateLoad(retVal->llValue);
        ctx->builder->CreateRet(v);
    }
    else {
        ctx->builder->CreateRetVoid();
    }
}

void VanillaExternalTarget::loadStructRetArgument(TypePtr type,
                                                  vector<llvm::Value *> &llArgs,
                                                  vector< pair<unsigned, llvm::Attributes> > &llAttributes,
                                                  CodegenContextPtr ctx,
                                                  MultiCValuePtr out)
{
}

void VanillaExternalTarget::loadArgument(CValuePtr cv,
                                         vector<llvm::Value *> &llArgs,
                                         vector< pair<unsigned, llvm::Attributes> > &llAttributes,
                                         CodegenContextPtr ctx)
{
    llvm::Value *llv = ctx->builder->CreateLoad(cv->llValue);
    llArgs.push_back(llv);
}

void VanillaExternalTarget::loadVarArgument(CValuePtr cv,
                                            vector<llvm::Value *> &llArgs,
                                            vector< pair<unsigned, llvm::Attributes> > &llAttributes,
                                            CodegenContextPtr ctx)
{
    llvm::Value *llv = ctx->builder->CreateLoad(cv->llValue);
    llvm::Value *llv2 = promoteCVarArg(cv->type, llv, ctx);
    llArgs.push_back(llv2);
}

void VanillaExternalTarget::storeReturnValue(llvm::Value *callReturnValue,
                                             TypePtr returnType,
                                             CodegenContextPtr ctx,
                                             MultiCValuePtr out)
{
    if (returnType != NULL) {
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0->type == returnType);
        ctx->builder->CreateStore(callReturnValue, out0->llValue);
    }
    else {
        assert(structReturnValue == NULL);
        assert(out->size() == 0);
    }
}


//
// Break down unboxable argument types per the x86-64 ABI
//

enum WordClass {
    NO_CLASS,
    INTEGER,
    // Vector and scalar SSE values are treated equivalently by the classification
    // algorithm, but LLVM's optimizer likes it better if we emit the proper type
    SSE_VECTOR,
    SSE_SCALAR,
    SSEUP,
    X87,
    X87UP,
    COMPLEX_X87,
    MEMORY
};

struct X86_64_ExternalTarget : public VanillaExternalTarget {
    explicit X86_64_ExternalTarget(llvm::Triple target)
        : VanillaExternalTarget(target) {}

    virtual llvm::Type *pushReturnType(TypePtr type,
                                       vector<llvm::Type *> &llArgTypes,
                                       vector< pair<unsigned, llvm::Attributes> > &llAttributes);
    virtual void pushArgumentType(TypePtr type,
                                  vector<llvm::Type *> &llArgTypes,
                                  vector< pair<unsigned, llvm::Attributes> > &llAttributes);

    virtual void allocReturnValue(TypePtr type,
                                  llvm::Function::arg_iterator &ai,
                                  vector<CReturn> &returns,
                                  CodegenContextPtr ctx);
    virtual CValuePtr allocArgumentValue(TypePtr type,
                                         string const &name,
                                         llvm::Function::arg_iterator &ai,
                                         CodegenContextPtr ctx);
    virtual void returnStatement(TypePtr type,
                                 vector<CReturn> &returns,
                                 CodegenContextPtr ctx);

    virtual void loadStructRetArgument(TypePtr type,
                                       vector<llvm::Value *> &llArgs,
                                       vector< pair<unsigned, llvm::Attributes> > &llAttributes,
                                       CodegenContextPtr ctx,
                                       MultiCValuePtr out);
    virtual void loadArgument(CValuePtr cv,
                              vector<llvm::Value *> &llArgs,
                              vector< pair<unsigned, llvm::Attributes> > &llAttributes,
                              CodegenContextPtr ctx);
    virtual void loadVarArgument(CValuePtr cv,
                                 vector<llvm::Value *> &llArgs,
                                 vector< pair<unsigned, llvm::Attributes> > &llAttributes,
                                 CodegenContextPtr ctx);
    virtual void storeReturnValue(llvm::Value *callReturnValue,
                                  TypePtr returnType,
                                  CodegenContextPtr ctx,
                                  MultiCValuePtr out);

    map<TypePtr, vector<WordClass> > typeClassifications;
    map<TypePtr, llvm::Type*> llvmWordTypes;
    vector<WordClass> const &getTypeClassification(TypePtr type);
    llvm::Type* getLLVMWordType(TypePtr type);

    vector<WordClass> classifyType(TypePtr type);

    bool canPassBySimpleValue(TypePtr type)
    {
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
        default:
            assert(false);
            return false;
        }
    }

    bool canReturnByClassifiedWords(TypePtr type)
    {
        // assumes canReturnBySimpleValue is false
        vector<WordClass> const &wordClasses = getTypeClassification(type);
        assert(!wordClasses.empty());
        return wordClasses[0] != MEMORY;
    }

    bool canPassByClassifiedWords(TypePtr type)
    {
        // assumes canReturnBySimpleValue is false
        vector<WordClass> const &wordClasses = getTypeClassification(type);
        assert(!wordClasses.empty());
        return wordClasses[0] != MEMORY
            && wordClasses[0] != X87
            && wordClasses[0] != COMPLEX_X87;
    }

    bool canPassVarArgByClassifiedWords(TypePtr type)
    {
        // assumes canReturnBySimpleValue is false
        vector<WordClass> const &wordClasses = getTypeClassification(type);
        assert(!wordClasses.empty());
        if (wordClasses[0] == MEMORY
            || wordClasses[0] == X87
            || wordClasses[0] == COMPLEX_X87)
            return false;
        // __m256 varargs are passed on the stack
        if ((wordClasses.size() > 2
            && wordClasses[0] == SSE_VECTOR && wordClasses[1] == SSEUP && wordClasses[2] == SSEUP)
            || (wordClasses.size() > 3
            && wordClasses[1] == SSE_VECTOR && wordClasses[2] == SSEUP && wordClasses[3] == SSEUP))
            return false;
        return true;
    }

    llvm::Type *llvmWordType(TypePtr type)
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
            case SSE_VECTOR: {
                int vectorRun = 0;
                do { ++vectorRun; ++i; } while (i != wordClasses.end() && *i == SSEUP);
                llWordTypes.push_back(llvm::VectorType::get(llvmIntType(8), vectorRun*8));
                break;
            }
            case SSE_SCALAR:
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
        assert(newClass == SSE_SCALAR || newClass == SSE_VECTOR);
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
            unifyWordClass(begin + offset/8, SSE_VECTOR);
            break;
        case 64:
            unifyWordClass(begin + offset/8, SSE_SCALAR);
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
            _classifyType(floatType(complexType->bits), begin, offset + complexType->bits/8);
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
        for (size_t i = 0; i < arrayType->size; ++i)
            _classifyType(arrayType->elementType, begin,
                          offset + i * typeSize(arrayType->elementType));
        break;
    }
    case VEC_TYPE: {
        unifyWordClass(begin + offset/8, SSE_VECTOR);
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
            size_t align = typeAlignment(*i);
            fieldOffset = (fieldOffset+align-1)/align*align;
            _classifyType(*i, begin, fieldOffset);
            size_t size = typeSize(*i);
            fieldOffset += size;
        }
    }
}

static void _allMemory(vector<WordClass> &wordClasses)
{
    fill(wordClasses.begin(), wordClasses.end(), MEMORY);
}

static void _fixupClassification(vector<WordClass> &wordClasses)
{
    vector<WordClass>::iterator i = wordClasses.begin(),
                                end = wordClasses.end();
    if (wordClasses.size() > 2) {
        if (*i == SSE_VECTOR || *i == SSE_SCALAR) {
            ++i;
            for (; i != end; ++i) {
                if (*i != SSEUP) {
                    _allMemory(wordClasses);
                    return;
                }
            }
        // this goes against the documentation but it seems like it has to
        // be this way for complex long double returns to work as advertised
        } else if (*i == COMPLEX_X87) {
            ++i;
            for (; i != end; ++i) {
                if (*i != COMPLEX_X87) {
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
            _allMemory(wordClasses);
            return;
        }
        if (*i == SSEUP) {
            *i = SSE_VECTOR;
        } else if (*i == SSE_VECTOR || *i == SSE_SCALAR) {
            do { ++i; } while (*i == SSEUP);
        } else if (*i == X87) {
            do { ++i; } while (*i == X87UP);
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
    _fixupClassification(wordClasses);

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

llvm::Type *X86_64_ExternalTarget::getLLVMWordType(TypePtr type)
{
    map<TypePtr, llvm::Type*>::iterator found = llvmWordTypes.find(type);
    if (found == llvmWordTypes.end()) {
        return llvmWordTypes.insert(make_pair(type, llvmWordType(type))).first->second;
    } else {
        return found->second;
    }
}

llvm::Type *X86_64_ExternalTarget::pushReturnType(TypePtr type,
                                                  vector<llvm::Type *> &llArgTypes,
                                                  vector< pair<unsigned, llvm::Attributes> > &llAttributes)
{
    if (type == NULL || canPassBySimpleValue(type))
        return VanillaExternalTarget::pushReturnType(type, llArgTypes, llAttributes);
    else if (canReturnByClassifiedWords(type))
        return getLLVMWordType(type);
    else { // return by StructRet
        llArgTypes.push_back(llvmPointerType(type));
        llAttributes.push_back(make_pair(llArgTypes.size(), llvm::Attribute::StructRet));
        return llvmVoidType();
    }
}

void X86_64_ExternalTarget::pushArgumentType(TypePtr type,
                                             vector<llvm::Type *> &llArgTypes,
                                             vector< pair<unsigned, llvm::Attributes> > &llAttributes)
{
    if (canPassBySimpleValue(type))
        VanillaExternalTarget::pushArgumentType(type, llArgTypes, llAttributes);
    else if (canPassByClassifiedWords(type))
        llArgTypes.push_back(getLLVMWordType(type));
    else { // pass ByVal
        llArgTypes.push_back(llvmPointerType(type));
        llAttributes.push_back(make_pair(llArgTypes.size(), llvm::Attribute::ByVal));
    }
}

void X86_64_ExternalTarget::allocReturnValue(TypePtr type,
                                             llvm::Function::arg_iterator &ai,
                                             vector<CReturn> &returns,
                                             CodegenContextPtr ctx)
{
    if (type == NULL || canPassBySimpleValue(type) || canReturnByClassifiedWords(type))
        VanillaExternalTarget::allocReturnValue(type, ai, returns, ctx);
    else { // return by StructRet
        llvm::Argument *structReturnArg = &(*ai);
        assert(structReturnArg->getType() == llvmPointerType(type));
        structReturnArg->setName("returned");
        ++ai;
        CValuePtr cret = new CValue(type, structReturnArg);
        returns.push_back(CReturn(false, type, cret));
    }
}

CValuePtr X86_64_ExternalTarget::allocArgumentValue(TypePtr type,
                                                    string const &name,
                                                    llvm::Function::arg_iterator &ai,
                                                    CodegenContextPtr ctx)
{
    if (canPassBySimpleValue(type))
        return VanillaExternalTarget::allocArgumentValue(type, name, ai, ctx);
    else if (canPassByClassifiedWords(type)) {
        llvm::Argument *llArg = &(*ai);
        assert(llArg->getType() == getLLVMWordType(type));
        llArg->setName(name);
        llvm::Value *llArgWords =
            ctx->initBuilder->CreateAlloca(getLLVMWordType(type));
        ctx->builder->CreateStore(llArg, llArgWords);
        llvm::Value *llArgVar =
            ctx->initBuilder->CreateBitCast(llArgWords, llvmPointerType(type));
        CValuePtr cvalue = new CValue(type, llArgVar);
        ++ai;
        return cvalue;
    } else { // pass ByVal
        llvm::Argument *llArg = &(*ai);
        assert(llArg->getType() == llvmPointerType(type));
        llArg->setName(name);
        CValuePtr cvalue = new CValue(type, llArg);
        ++ai;
        return cvalue;
    }
}

void X86_64_ExternalTarget::returnStatement(TypePtr type,
                                            vector<CReturn> &returns,
                                            CodegenContextPtr ctx)
{
    if (type == NULL || canPassBySimpleValue(type))
        VanillaExternalTarget::returnStatement(type, returns, ctx);
    else if (canReturnByClassifiedWords(type)) {
        CValuePtr retVal = returns[0].value;
        llvm::Value *bitcast = ctx->builder->CreateBitCast(retVal->llValue,
            llvm::PointerType::getUnqual(getLLVMWordType(type)));
        llvm::Value *v = ctx->builder->CreateLoad(bitcast);
        ctx->builder->CreateRet(v);
    } else { // return by StructRet
        ctx->builder->CreateRetVoid();
    }
}

void X86_64_ExternalTarget::loadStructRetArgument(TypePtr type,
                                                  vector<llvm::Value *> &llArgs,
                                                  vector< pair<unsigned, llvm::Attributes> > &llAttributes,
                                                  CodegenContextPtr ctx,
                                                  MultiCValuePtr out)
{
    if (type != NULL && !canPassBySimpleValue(type) && !canReturnByClassifiedWords(type)) {
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0->type == returnType);
        llArgs.push_back(out0->llValue);
        llAttributes.push_back(make_pair(llArgs.size(), llvm::Attribute::StructRet));
    } else
        VanillaExternalTarget::loadStructRetArgument(type, llArgs, llAttributes, ctx, out);
}

void X86_64_ExternalTarget::loadArgument(CValuePtr cv,
                                         vector<llvm::Value *> &llArgs,
                                         vector< pair<unsigned, llvm::Attributes> > &llAttributes,
                                         CodegenContextPtr ctx)
{
    if (canPassBySimpleValue(cv->type))
        VanillaExternalTarget::loadArgument(cv, llArgs, llAttributes, ctx);
    else if (canPassByClassifiedWords(cv->type)) {
        llvm::Value *llWords = ctx->builder->CreateBitCast(cv->llValue,
            llvm::PointerType::getUnqual(getLLVMWordType(cv->type)));
        llvm::Value *llv = ctx->builder->CreateLoad(llWords);
        llArgs.push_back(llv);
    } else { // pass ByVal
        llArgs.push_back(cv->llValue);
        llAttributes.push_back(make_pair(llArgs.size(), llvm::Attribute::ByVal));
    }
}

void X86_64_ExternalTarget::loadVarArgument(CValuePtr cv,
                                            vector<llvm::Value *> &llArgs,
                                            vector< pair<unsigned, llvm::Attributes> > &llAttributes,
                                            CodegenContextPtr ctx)
{
    if (canPassBySimpleValue(cv->type))
        VanillaExternalTarget::loadVarArgument(cv, llArgs, llAttributes, ctx);
    else if (canPassVarArgByClassifiedWords(cv->type)) {
        llvm::Value *llWords = ctx->builder->CreateBitCast(cv->llValue,
            llvm::PointerType::getUnqual(getLLVMWordType(cv->type)));
        llvm::Value *llv = ctx->builder->CreateLoad(llWords);
        llArgs.push_back(llv);
    } else { // pass ByVal
        llArgs.push_back(cv->llValue);
        llAttributes.push_back(make_pair(llArgs.size(), llvm::Attribute::ByVal));
    }
}

void X86_64_ExternalTarget::storeReturnValue(llvm::Value *callReturnValue,
                                             TypePtr returnType,
                                             CodegenContextPtr ctx,
                                             MultiCValuePtr out)
{
    if (returnType == NULL || canPassBySimpleValue(returnType)) {
        VanillaExternalTarget::storeReturnValue(callReturnValue, returnType, ctx, out);
    } else if (canReturnByClassifiedWords(returnType)) {
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0->type == returnType);
        llvm::Value *outWords = ctx->builder->CreateBitCast(out0->llValue,
            llvm::PointerType::getUnqual(getLLVMWordType(returnType)));
        ctx->builder->CreateStore(callReturnValue, outWords);
    }
    else { // return by StructRet
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
        && target.getOS() != llvm::Triple::Cygwin) {
        externalTarget = new X86_64_ExternalTarget(target);
    } else
        externalTarget = new VanillaExternalTarget(target);
}

ExternalTargetPtr getExternalTarget()
{
    return externalTarget;
}
