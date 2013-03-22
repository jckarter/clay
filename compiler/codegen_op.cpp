#include "types.hpp"
#include "error.hpp"
#include "loader.hpp"
#include "codegen.hpp"
#include "objects.hpp"
#include "evaluator.hpp"
#include "analyzer.hpp"
#include "operators.hpp"
#include "constructors.hpp"
#include "externals.hpp"
#include "env.hpp"

#include "codegen_op.hpp"


namespace clay {


static llvm::StringMap<llvm::Constant*> stringTableConstants;



//
// valueToStatic, valueToStaticSizeTOrInt
// valueToType, valueToNumericType, valueToIntegerType,
// valueToPointerLikeType, valueToTupleType, valueToRecordType,
// valueToVariantType, valueToEnumType, valueToIdentifier
//

static ObjectPtr valueToStatic(CValuePtr cv)
{
    if (cv->type->typeKind != STATIC_TYPE)
        return NULL;
    StaticType *st = (StaticType *)cv->type.ptr();
    return st->obj;
}

static ObjectPtr valueToStatic(MultiCValuePtr args, unsigned index)
{
    ObjectPtr obj = valueToStatic(args->values[index]);
    if (!obj)
        argumentError(index, "expecting a static value");
    return obj;
}

static size_t valueToStaticSizeTOrInt(MultiCValuePtr args, unsigned index)
{
    ObjectPtr obj = valueToStatic(args->values[index]);
    if (!obj || (obj->objKind != VALUE_HOLDER))
        argumentError(index, "expecting a static SizeT or Int value");
    ValueHolder *vh = (ValueHolder *)obj.ptr();
    if (vh->type == cSizeTType) {
        return valueHolderToSizeT(vh);
    }
    else if (vh->type == cIntType) {
        return size_t(vh->as<int>());
    }
    else {
        argumentError(index, "expecting a static SizeT or Int value");
        return 0;
    }
}

static TypePtr valueToType(MultiCValuePtr args, unsigned index)
{
    ObjectPtr obj = valueToStatic(args->values[index]);
    if (!obj || (obj->objKind != TYPE))
        argumentError(index, "expecting a type");
    return (Type *)obj.ptr();
}

static TypePtr valueToNumericType(MultiCValuePtr args, unsigned index)
{
    TypePtr t = valueToType(args, index);
    switch (t->typeKind) {
    case INTEGER_TYPE :
    case FLOAT_TYPE :
        return t;
    default :
        argumentTypeError(index, "numeric type", t);
        return NULL;
    }
}

static IntegerTypePtr valueToIntegerType(MultiCValuePtr args, unsigned index)
{
    TypePtr t = valueToType(args, index);
    if (t->typeKind != INTEGER_TYPE)
        argumentTypeError(index, "integer type", t);
    return (IntegerType *)t.ptr();
}

static TypePtr valueToPointerLikeType(MultiCValuePtr args, unsigned index)
{
    TypePtr t = valueToType(args, index);
    if (!isPointerOrCodePointerType(t))
        argumentTypeError(index, "pointer or code-pointer type", t);
    return t;
}

static TupleTypePtr valueToTupleType(MultiCValuePtr args, unsigned index)
{
    TypePtr t = valueToType(args, index);
    if (t->typeKind != TUPLE_TYPE)
        argumentTypeError(index, "tuple type", t);
    return (TupleType *)t.ptr();
}

static UnionTypePtr valueToUnionType(MultiCValuePtr args, unsigned index)
{
    TypePtr t = valueToType(args, index);
    if (t->typeKind != UNION_TYPE)
        argumentTypeError(index, "union type", t);
    return (UnionType *)t.ptr();
}

static RecordTypePtr valueToRecordType(MultiCValuePtr args, unsigned index)
{
    TypePtr t = valueToType(args, index);
    if (t->typeKind != RECORD_TYPE)
        argumentTypeError(index, "record type", t);
    return (RecordType *)t.ptr();
}

static VariantTypePtr valueToVariantType(MultiCValuePtr args, unsigned index)
{
    TypePtr t = valueToType(args, index);
    if (t->typeKind != VARIANT_TYPE)
        argumentTypeError(index, "variant type", t);
    return (VariantType *)t.ptr();
}

static EnumTypePtr valueToEnumType(MultiCValuePtr args, unsigned index)
{
    TypePtr t = valueToType(args, index);
    if (t->typeKind != ENUM_TYPE)
        argumentTypeError(index, "enum type", t);
    initializeEnumType((EnumType*)t.ptr());
    return (EnumType *)t.ptr();
}

static IdentifierPtr valueToIdentifier(MultiCValuePtr args, unsigned index)
{
    ObjectPtr obj = valueToStatic(args->values[index]);
    if (!obj || (obj->objKind != IDENTIFIER))
        argumentError(index, "expecting identifier value");
    return (Identifier *)obj.ptr();
}



//
// numericValue, integerValue, pointerValue,
// pointerLikeValue, cCodePointerValue,
// arrayValue, tupleValue, recordValue, variantValue, enumValue
//

static llvm::Value *numericValue(MultiCValuePtr args,
                                 unsigned index,
                                 TypePtr &type,
                                 CodegenContext* ctx)
{
    CValuePtr cv = args->values[index];
    if (type.ptr()) {
        if (cv->type != type)
            argumentTypeError(index, type, cv->type);
    }
    else {
        switch (cv->type->typeKind) {
        case INTEGER_TYPE :
        case FLOAT_TYPE :
            break;
        default :
            argumentTypeError(index, "numeric type", cv->type);
        }
        type = cv->type;
    }
    return ctx->builder->CreateLoad(cv->llValue);
}

static llvm::Value *floatValue(MultiCValuePtr args,
                               unsigned index,
                               FloatTypePtr &type,
                               CodegenContext* ctx)
{
    CValuePtr cv = args->values[index];
    if (type.ptr()) {
        if (cv->type.ptr() != type.ptr())
            argumentTypeError(index, type.ptr(), cv->type);
    }
    else {
        switch (cv->type->typeKind) {
        case FLOAT_TYPE :
            break;
        default :
            argumentTypeError(index, "float type", cv->type);
        }
        type = (FloatType*)cv->type.ptr();
    }
    return ctx->builder->CreateLoad(cv->llValue);
}

static llvm::Value *integerOrPointerLikeValue(MultiCValuePtr args,
                                              unsigned index,
                                              TypePtr &type,
                                              CodegenContext* ctx)
{
    CValuePtr cv = args->values[index];
    if (type.ptr()) {
        if (cv->type != type)
            argumentTypeError(index, type, cv->type);
    }
    else {
        switch (cv->type->typeKind) {
        case INTEGER_TYPE :
        case POINTER_TYPE :
        case CODE_POINTER_TYPE :
        case CCODE_POINTER_TYPE :
            break;
        default :
            argumentTypeError(index, "integer, pointer, or code pointer type", cv->type);
        }
        type = cv->type;
    }
    return ctx->builder->CreateLoad(cv->llValue);
}

static void checkIntegerValue(MultiCValuePtr args,
                              unsigned index,
                              IntegerTypePtr &type,
                              CodegenContext* ctx)
{
    CValuePtr cv = args->values[index];
    if (type.ptr()) {
        if (cv->type != type.ptr())
            argumentTypeError(index, type.ptr(), cv->type);
    }
    else {
        if (cv->type->typeKind != INTEGER_TYPE)
            argumentTypeError(index, "integer type", cv->type);
        type = (IntegerType *)cv->type.ptr();
    }
}

static llvm::Value *integerValue(MultiCValuePtr args,
                                 unsigned index,
                                 IntegerTypePtr &type,
                                 CodegenContext* ctx)
{
    checkIntegerValue(args, index, type, ctx);
    CValuePtr cv = args->values[index];
    return ctx->builder->CreateLoad(cv->llValue);
}

static llvm::Value *pointerValue(MultiCValuePtr args,
                                 unsigned index,
                                 PointerTypePtr &type,
                                 CodegenContext* ctx)
{
    CValuePtr cv = args->values[index];
    if (type.ptr()) {
        if (cv->type != (Type *)type.ptr())
            argumentTypeError(index, type.ptr(), cv->type);
    }
    else {
        if (cv->type->typeKind != POINTER_TYPE)
            argumentTypeError(index, "pointer type", cv->type);
        type = (PointerType *)cv->type.ptr();
        llvmType(type->pointeeType); // force the pointee type to be refined
    }
    return ctx->builder->CreateLoad(cv->llValue);
}

static llvm::Value *pointerLikeValue(MultiCValuePtr args,
                                     unsigned index,
                                     TypePtr &type,
                                     CodegenContext* ctx)
{
    CValuePtr cv = args->values[index];
    if (type.ptr()) {
        if (cv->type != type)
            argumentTypeError(index, type, cv->type);
    }
    else {
        if (!isPointerOrCodePointerType(cv->type))
            argumentTypeError(index,
                              "pointer or code-pointer type",
                              cv->type);
        type = cv->type;
    }
    return ctx->builder->CreateLoad(cv->llValue);
}

static llvm::Value* cCodePointerValue(MultiCValuePtr args,
                                      unsigned index,
                                      CCodePointerTypePtr &type)
{
    CValuePtr cv = args->values[index];
    if (type.ptr()) {
        if (cv->type != (Type *)type.ptr())
            argumentTypeError(index, type.ptr(), cv->type);
    }
    else {
        if (cv->type->typeKind != CCODE_POINTER_TYPE)
            argumentTypeError(index, "c code pointer type", cv->type);
        type = (CCodePointerType *)cv->type.ptr();
    }
    return cv->llValue;
}

static llvm::Value *arrayValue(MultiCValuePtr args,
                               unsigned index,
                               ArrayTypePtr &type)
{
    CValuePtr cv = args->values[index];
    if (type.ptr()) {
        if (cv->type != (Type *)type.ptr())
            argumentTypeError(index, type.ptr(), cv->type);
    }
    else {
        if (cv->type->typeKind != ARRAY_TYPE)
            argumentTypeError(index, "array type", cv->type);
        type = (ArrayType *)cv->type.ptr();
    }
    return cv->llValue;
}

static llvm::Value *tupleValue(MultiCValuePtr args,
                               unsigned index,
                               TupleTypePtr &type)
{
    CValuePtr cv = args->values[index];
    if (type.ptr()) {
        if (cv->type != (Type *)type.ptr())
            argumentTypeError(index, type.ptr(), cv->type);
    }
    else {
        if (cv->type->typeKind != TUPLE_TYPE)
            argumentTypeError(index, "tuple type", cv->type);
        type = (TupleType *)cv->type.ptr();
    }
    return cv->llValue;
}

static llvm::Value *recordValue(MultiCValuePtr args,
                                unsigned index,
                                RecordTypePtr &type)
{
    CValuePtr cv = args->values[index];
    if (type.ptr()) {
        if (cv->type != (Type *)type.ptr())
            argumentTypeError(index, type.ptr(), cv->type);
    }
    else {
        if (cv->type->typeKind != RECORD_TYPE)
            argumentTypeError(index, "record type", cv->type);
        type = (RecordType *)cv->type.ptr();
    }
    return cv->llValue;
}

static llvm::Value *enumValue(MultiCValuePtr args,
                              unsigned index,
                              EnumTypePtr &type,
                              CodegenContext* ctx)
{
    CValuePtr cv = args->values[index];
    if (type.ptr()) {
        if (cv->type != (Type *)type.ptr())
            argumentTypeError(index, type.ptr(), cv->type);
    }
    else {
        if (cv->type->typeKind != ENUM_TYPE)
            argumentTypeError(index, "enum type", cv->type);
        type = (EnumType *)cv->type.ptr();
    }
    return ctx->builder->CreateLoad(cv->llValue);
}

llvm::AtomicOrdering atomicOrderValue(MultiCValuePtr args, unsigned index)
{
    CValuePtr cv = args->values[index];
    ObjectPtr obj = unwrapStaticType(cv->type);
    if (obj != NULL && obj->objKind == PRIM_OP) {
        PrimOp *prim = (PrimOp*)obj.ptr();
        switch (prim->primOpCode) {
        case PRIM_OrderUnordered:
            return llvm::Unordered;
        case PRIM_OrderMonotonic:
            return llvm::Monotonic;
        case PRIM_OrderAcquire:
            return llvm::Acquire;
        case PRIM_OrderRelease:
            return llvm::Release;
        case PRIM_OrderAcqRel:
            return llvm::AcquireRelease;
        case PRIM_OrderSeqCst:
            return llvm::SequentiallyConsistent;
        }
    }
    argumentTypeError(index, "atomic ordering", cv->type);
    return llvm::Unordered;
}

llvm::AtomicRMWInst::BinOp atomicRMWOpValue(MultiCValuePtr args, unsigned index)
{
    CValuePtr cv = args->values[index];
    ObjectPtr obj = unwrapStaticType(cv->type);
    if (obj != NULL && obj->objKind == PRIM_OP) {
        PrimOp *prim = (PrimOp*)obj.ptr();
        switch (prim->primOpCode) {
        case PRIM_RMWXchg:
            return llvm::AtomicRMWInst::Xchg;
        case PRIM_RMWAdd:
            return llvm::AtomicRMWInst::Add;
        case PRIM_RMWSubtract:
            return llvm::AtomicRMWInst::Sub;
        case PRIM_RMWAnd:
            return llvm::AtomicRMWInst::And;
        case PRIM_RMWNAnd:
            return llvm::AtomicRMWInst::Nand;
        case PRIM_RMWOr:
            return llvm::AtomicRMWInst::Or;
        case PRIM_RMWXor:
            return llvm::AtomicRMWInst::Xor;
        case PRIM_RMWMin:
            return llvm::AtomicRMWInst::Min;
        case PRIM_RMWMax:
            return llvm::AtomicRMWInst::Max;
        case PRIM_RMWUMin:
            return llvm::AtomicRMWInst::UMin;
        case PRIM_RMWUMax:
            return llvm::AtomicRMWInst::UMax;
        }
    }
    argumentTypeError(index, "atomic rmw operation", cv->type);
    return llvm::AtomicRMWInst::Xchg;
}



//
// codegenPrimOp
//

static llvm::Constant *codegenStringTableConstant(llvm::StringRef s)
{
    llvm::StringMap<llvm::Constant*>::const_iterator oldConstant = stringTableConstants.find(s);
    if (oldConstant != stringTableConstants.end())
        return oldConstant->second;

    llvm::Constant *sizeInitializer =
        llvm::ConstantInt::get(llvmType(cSizeTType), s.size(), false);
    llvm::Constant *stringInitializer =
        llvm::ConstantDataArray::getString(llvm::getGlobalContext(), s, true);
    llvm::Constant *structEntries[] = {sizeInitializer, stringInitializer};
    llvm::Constant *initializer =
        llvm::ConstantStruct::getAnon(llvm::getGlobalContext(),
                structEntries,
                false);

    llvm::SmallString<128> buf;
    llvm::raw_svector_ostream symbolName(buf);
    symbolName << "\"" << s << "\" clay";

    llvm::GlobalVariable *gvar = new llvm::GlobalVariable(
        *llvmModule, initializer->getType(), true,
        llvm::GlobalVariable::PrivateLinkage,
        initializer, symbolName.str());
    llvm::Constant *idxs[] = {
        llvm::ConstantInt::get(llvmType(int32Type), 0),
        llvm::ConstantInt::get(llvmType(int32Type), 0),
    };

    llvm::Constant *theConstant = llvm::ConstantExpr::getGetElementPtr(gvar, idxs);
    stringTableConstants[s] = theConstant;
    return theConstant;
}



//
// codegenCWrapper
//

static void codegenCWrapper(InvokeEntry* entry, CallingConv cc)
{
    assert(!entry->llvmCWrappers[cc]);
    ExternalTargetPtr target = getExternalTarget();

    string callableName = getCodeName(entry);

    TypePtr returnType;
    if (entry->returnTypes.empty()) {
        returnType = NULL;
    }
    else {
        assert(entry->returnTypes.size() == 1);
        assert(!entry->returnIsRef[0]);
        returnType = entry->returnTypes[0];
    }

    ExternalFunction *extFunc = target->getExternalFunction(
        cc, returnType, entry->argsKey, entry->argsKey.size(), false);

    llvm::FunctionType *llFuncType = extFunc->getLLVMFunctionType();

    std::string ccName;
    switch (cc) {
    case CC_DEFAULT:
        ccName = "cdecl ";
        break;
    case CC_STDCALL:
        ccName = "stdcall ";
        break;
    case CC_FASTCALL:
        ccName = "fastcall ";
        break;
    case CC_THISCALL:
        ccName = "thiscall ";
        break;
    default:
        assert(false);
    }

    llvm::Function *llCWrapper =
        llvm::Function::Create(llFuncType,
                               llvm::Function::InternalLinkage,
                               ccName + callableName,
                               llvmModule);
    for (vector< pair<unsigned, llvm::Attributes> >::const_iterator
         attr = extFunc->attrs.begin();
         attr != extFunc->attrs.end();
         ++attr)
        llCWrapper->addAttribute(attr->first, attr->second);

    llCWrapper->setCallingConv(extFunc->llConv);

    entry->llvmCWrappers[cc] = llCWrapper;
    CodegenContext ctx(llCWrapper);

    llvm::BasicBlock *llInitBlock =
        llvm::BasicBlock::Create(llvm::getGlobalContext(),
                                 "init",
                                 llCWrapper);
    llvm::BasicBlock *llBlock =
        llvm::BasicBlock::Create(llvm::getGlobalContext(),
                                 "code",
                                 llCWrapper);
    ctx.initBuilder.reset(new llvm::IRBuilder<>(llInitBlock));
    ctx.builder.reset(new llvm::IRBuilder<>(llBlock));

    ctx.exceptionValue = ctx.initBuilder->CreateAlloca(exceptionReturnType(), NULL, "exception");

    vector<llvm::Value *> innerArgs;
    vector<CReturn> returns;
    llvm::Function::arg_iterator ai = llCWrapper->arg_begin();
    extFunc->allocReturnValue(extFunc->retInfo, ai, returns, &ctx);
    for (size_t i = 0; i < entry->argsKey.size(); ++i) {
        CValuePtr cv = extFunc->allocArgumentValue(
            extFunc->argInfos[i], "x", ai, &ctx);
        innerArgs.push_back(cv->llValue);
    }

    for (vector<CReturn>::const_iterator ret = returns.begin();
         ret != returns.end();
         ++ret)
        innerArgs.push_back(ret->value->llValue);

    // XXX check exception
    ctx.builder->CreateCall(entry->llvmFunc, llvm::makeArrayRef(innerArgs));

    extFunc->returnStatement(extFunc->retInfo, returns, &ctx);

    ctx.initBuilder->CreateBr(llBlock);
}


//
// codegenCallCCode, codegenCallCCodePointer
//

static void codegenCallCCode(CCodePointerTypePtr t,
                      llvm::Value *llCallable,
                      MultiCValuePtr args,
                      CodegenContext* ctx,
                      MultiCValuePtr out)
{
    ExternalTargetPtr target = getExternalTarget();

    if (!t->hasVarArgs)
        ensureArity(args, t->argTypes.size());
    else if (args->size() < t->argTypes.size())
        arityError2(t->argTypes.size(), args->size());

    vector<TypePtr> argTypes;
    for (size_t i = 0; i < args->size(); ++i) {
        CValuePtr cv = args->values[i];
        argTypes.push_back(cv->type);
    }

    ExternalFunction *extFunc = target->getExternalFunction(
        t->callingConv, t->returnType, argTypes,
        t->argTypes.size(), t->hasVarArgs);

    vector<llvm::Value*> llArgs;
    extFunc->loadStructRetArgument(extFunc->retInfo, llArgs, ctx, out);
    for (unsigned i = 0; i < t->argTypes.size(); ++i) {
        CValuePtr cv = args->values[i];
        if (cv->type != t->argTypes[i])
            argumentTypeError(i, t->argTypes[i], cv->type);
        extFunc->loadArgument(extFunc->argInfos[i], cv, llArgs, ctx, false);
    }
    if (t->hasVarArgs) {
        for (size_t i = t->argTypes.size(); i < args->size(); ++i) {
            CValuePtr cv = args->values[i];
            extFunc->loadArgument(extFunc->argInfos[i], cv, llArgs,
                                  ctx, true);
        }
    }
    llvm::Value *llCastCallable = t->callingConv == CC_LLVM
        ? llCallable
        : ctx->builder->CreateBitCast(llCallable, t->getCallType());
    llvm::CallInst *callInst =
        ctx->builder->CreateCall(llCastCallable, llvm::makeArrayRef(llArgs));
    llvm::CallingConv::ID callingConv = extFunc->llConv;
    callInst->setCallingConv(callingConv);
    for (vector< pair<unsigned, llvm::Attributes> >::iterator
         attr = extFunc->attrs.begin();
         attr != extFunc->attrs.end();
         ++attr)
        callInst->addAttribute(attr->first, attr->second);

    llvm::Value *llRet = callInst;
    extFunc->storeReturnValue(extFunc->retInfo, llRet, ctx, out);
}

static void codegenCallCCodePointer(CValuePtr x,
                             MultiCValuePtr args,
                             CodegenContext* ctx,
                             MultiCValuePtr out)
{
    llvm::Value *llCallable = ctx->builder->CreateLoad(x->llValue);
    assert(x->type->typeKind == CCODE_POINTER_TYPE);
    CCodePointerType *t = (CCodePointerType *)x->type.ptr();
    codegenCallCCode(t, llCallable, args, ctx, out);
}



void codegenPrimOp(PrimOpPtr x,
                   MultiCValuePtr args,
                   CodegenContext* ctx,
                   MultiCValuePtr out)
{
    switch (x->primOpCode) {

    case PRIM_TypeP : {
        ensureArity(args, 1);
        ObjectPtr obj = valueToStatic(args->values[0]);
        bool isType = (obj.ptr() && (obj->objKind == TYPE));
        ValueHolderPtr vh = boolToValueHolder(isType);
        codegenStaticObject(vh.ptr(), ctx, out);
        break;
    }

    case PRIM_TypeSize : {
        ensureArity(args, 1);
        TypePtr t = valueToType(args, 0);
        ValueHolderPtr vh = sizeTToValueHolder(typeSize(t));
        codegenStaticObject(vh.ptr(), ctx, out);
        break;
    }

    case PRIM_TypeAlignment : {
        ensureArity(args, 1);
        TypePtr t = valueToType(args, 0);
        ValueHolderPtr vh = sizeTToValueHolder(typeAlignment(t));
        codegenStaticObject(vh.ptr(), ctx, out);
        break;
    }

    case PRIM_OperatorP : {
        ensureArity(args, 1);
        ObjectPtr obj = valueToStatic(args->values[0]);
        bool isOperator = false;
        if (!!obj.ptr() && (obj->objKind==PROCEDURE || obj->objKind==GLOBAL_ALIAS)) {
            TopLevelItem *item = (TopLevelItem *)obj.ptr();
            isOperator = item->name->isOperator;
        }
        ValueHolderPtr vh = boolToValueHolder(isOperator);
        codegenStaticObject(vh.ptr(), ctx, out);
        break;
    }

    case PRIM_SymbolP : {
        ensureArity(args, 1);
        ObjectPtr obj = valueToStatic(args->values[0]);
        bool isSymbol = false;
        if (obj.ptr() != NULL) {
            switch (obj->objKind) {
            case TYPE :
            case RECORD_DECL :
            case VARIANT_DECL :
            case PROCEDURE :
            case INTRINSIC :
            case GLOBAL_ALIAS:
                isSymbol = true;
                break;
            default :
                break;
            }
        }
        ValueHolderPtr vh = boolToValueHolder(isSymbol);
        codegenStaticObject(vh.ptr(), ctx, out);
        break;
    }

    case PRIM_StaticCallDefinedP : {
        if (args->size() < 1)
            arityError2(1, args->size());
        ObjectPtr callable = valueToStatic(args->values[0]);
        if (!callable) {
            argumentError(0, "static callable expected");
        }
        switch (callable->objKind) {
        case TYPE :
        case RECORD_DECL :
        case VARIANT_DECL :
        case PROCEDURE :
        case INTRINSIC :
        case GLOBAL_ALIAS:
            break;
        default :
            argumentError(0, "invalid callable");
        }
        vector<TypePtr> argsKey;
        vector<ValueTempness> argsTempness;
        for (unsigned i = 1; i < args->size(); ++i) {
            TypePtr t = valueToType(args, i);
            argsKey.push_back(t);
            argsTempness.push_back(TEMPNESS_LVALUE);
        }
        CompileContextPusher pusher(callable, argsKey);
        bool isDefined = analyzeIsDefined(callable, argsKey, argsTempness);
        ValueHolderPtr vh = boolToValueHolder(isDefined);
        codegenStaticObject(vh.ptr(), ctx, out);
        break;
    }

    case PRIM_StaticCallOutputTypes :
        break;

    case PRIM_StaticMonoP : {
        ensureArity(args, 1);

        bool isMono;

        ObjectPtr callable = valueToStatic(args->values[0]);
        if (callable != NULL && callable->objKind == PROCEDURE) {
            Procedure *p = (Procedure*)callable.ptr();
            isMono = p->mono.monoState == Procedure_MonoOverload;
        } else
            isMono = false;

        ValueHolderPtr vh = boolToValueHolder(isMono);
        codegenStaticObject(vh.ptr(), ctx, out);
        break;
    }

    case PRIM_StaticMonoInputTypes :
        break;

    case PRIM_bitcopy : {
        ensureArity(args, 2);
        CValuePtr cv0 = args->values[0];
        CValuePtr cv1 = args->values[1];
        if (!isPrimitiveType(cv0->type))
            argumentTypeError(0, "primitive type", cv0->type);
        if (cv0->type != cv1->type)
            argumentTypeError(1, cv0->type, cv1->type);
        llvm::Value *v = ctx->builder->CreateLoad(cv1->llValue);
        ctx->builder->CreateStore(v, cv0->llValue);
        assert(out->size() == 0);
        break;
    }

    case PRIM_boolNot : {
        ensureArity(args, 1);
        CValuePtr cv = args->values[0];
        if (cv->type != boolType)
            argumentTypeError(0, boolType, cv->type);
        assert(out->size() == 1);
        llvm::Value *v = ctx->builder->CreateLoad(cv->llValue);
        llvm::Value *zero = llvm::ConstantInt::get(llvmType(boolType), 0);
        llvm::Value *flag = ctx->builder->CreateICmpEQ(v, zero);
        CValuePtr out0 = out->values[0];
        assert(out0->type == boolType);
        ctx->builder->CreateStore(flag, out0->llValue);
        break;
    }

    case PRIM_integerEqualsP :
    case PRIM_integerLesserP : {
        ensureArity(args, 2);
        TypePtr t;
        llvm::Value *v0 = integerOrPointerLikeValue(args, 0, t, ctx);
        llvm::Value *v1 = integerOrPointerLikeValue(args, 1, t, ctx);

        llvm::CmpInst::Predicate pred;

        switch (x->primOpCode) {
        case PRIM_integerEqualsP:
            pred = llvm::CmpInst::ICMP_EQ;
            break;
        case PRIM_integerLesserP: {
            bool isSigned;
            switch (t->typeKind) {
            case INTEGER_TYPE: {
                IntegerType *it = (IntegerType*)t.ptr();
                isSigned = it->isSigned;
                break;
            }
            default:
                isSigned = false;
                break;
            }
            pred = isSigned
                ? llvm::CmpInst::ICMP_SLT
                : llvm::CmpInst::ICMP_ULT;
            break;
        }
        default:
            assert(false);
        }

        llvm::Value *flag = ctx->builder->CreateICmp(pred, v0, v1);
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0->type == boolType);
        ctx->builder->CreateStore(flag, out0->llValue);
        break;
    }

    case PRIM_floatOrderedEqualsP :
    case PRIM_floatOrderedLesserP :
    case PRIM_floatOrderedLesserEqualsP :
    case PRIM_floatOrderedGreaterP :
    case PRIM_floatOrderedGreaterEqualsP :
    case PRIM_floatOrderedNotEqualsP :
    case PRIM_floatOrderedP :
    case PRIM_floatUnorderedEqualsP :
    case PRIM_floatUnorderedLesserP :
    case PRIM_floatUnorderedLesserEqualsP :
    case PRIM_floatUnorderedGreaterP :
    case PRIM_floatUnorderedGreaterEqualsP :
    case PRIM_floatUnorderedNotEqualsP :
    case PRIM_floatUnorderedP : {
        ensureArity(args, 2);
        FloatTypePtr t;
        llvm::Value *v0 = floatValue(args, 0, t, ctx);
        llvm::Value *v1 = floatValue(args, 1, t, ctx);

        llvm::CmpInst::Predicate pred;

        switch (x->primOpCode) {
        case PRIM_floatOrderedEqualsP :
            pred = llvm::CmpInst::FCMP_OEQ;
            break;
        case PRIM_floatOrderedLesserP :
            pred = llvm::CmpInst::FCMP_OLT;
            break;
        case PRIM_floatOrderedLesserEqualsP :
            pred = llvm::CmpInst::FCMP_OLE;
            break;
        case PRIM_floatOrderedGreaterP :
            pred = llvm::CmpInst::FCMP_OGT;
            break;
        case PRIM_floatOrderedGreaterEqualsP :
            pred = llvm::CmpInst::FCMP_OGE;
            break;
        case PRIM_floatOrderedNotEqualsP :
            pred = llvm::CmpInst::FCMP_ONE;
            break;
        case PRIM_floatOrderedP :
            pred = llvm::CmpInst::FCMP_ORD;
            break;
        case PRIM_floatUnorderedEqualsP :
            pred = llvm::CmpInst::FCMP_UEQ;
            break;
        case PRIM_floatUnorderedLesserP :
            pred = llvm::CmpInst::FCMP_ULT;
            break;
        case PRIM_floatUnorderedLesserEqualsP :
            pred = llvm::CmpInst::FCMP_ULE;
            break;
        case PRIM_floatUnorderedGreaterP :
            pred = llvm::CmpInst::FCMP_UGT;
            break;
        case PRIM_floatUnorderedGreaterEqualsP :
            pred = llvm::CmpInst::FCMP_UGE;
            break;
        case PRIM_floatUnorderedNotEqualsP :
            pred = llvm::CmpInst::FCMP_UNE;
            break;
        case PRIM_floatUnorderedP :
            pred = llvm::CmpInst::FCMP_UNO;
            break;
        default:
            assert(false);
        }

        llvm::Value *flag = ctx->builder->CreateFCmp(pred, v0, v1);
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0->type == boolType);
        ctx->builder->CreateStore(flag, out0->llValue);
        break;
    }

    case PRIM_numericAdd : {
        ensureArity(args, 2);
        TypePtr t;
        llvm::Value *v0 = numericValue(args, 0, t, ctx);
        llvm::Value *v1 = numericValue(args, 1, t, ctx);
        llvm::Value *result;
        switch (t->typeKind) {
        case INTEGER_TYPE :
            result = ctx->builder->CreateAdd(v0, v1);
            break;
        case FLOAT_TYPE :
            result = ctx->builder->CreateFAdd(v0, v1);
            break;
        default :
            assert(false);
        }
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0->type == t);
        ctx->builder->CreateStore(result, out0->llValue);
        break;
    }

    case PRIM_numericSubtract : {
        ensureArity(args, 2);
        TypePtr t;
        llvm::Value *v0 = numericValue(args, 0, t, ctx);
        llvm::Value *v1 = numericValue(args, 1, t, ctx);
        llvm::Value *result;
        switch (t->typeKind) {
        case INTEGER_TYPE :
            result = ctx->builder->CreateSub(v0, v1);
            break;
        case FLOAT_TYPE :
            result = ctx->builder->CreateFSub(v0, v1);
            break;
        default :
            assert(false);
        }
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0->type == t);
        ctx->builder->CreateStore(result, out0->llValue);
        break;
    }

    case PRIM_numericMultiply : {
        ensureArity(args, 2);
        TypePtr t;
        llvm::Value *v0 = numericValue(args, 0, t, ctx);
        llvm::Value *v1 = numericValue(args, 1, t, ctx);
        llvm::Value *result;
        switch (t->typeKind) {
        case INTEGER_TYPE :
            result = ctx->builder->CreateMul(v0, v1);
            break;
        case FLOAT_TYPE :
            result = ctx->builder->CreateFMul(v0, v1);
            break;
        default :
            assert(false);
        }
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0->type == t);
        ctx->builder->CreateStore(result, out0->llValue);
        break;
    }

    case PRIM_floatDivide : {
        ensureArity(args, 2);
        FloatTypePtr t;
        llvm::Value *v0 = floatValue(args, 0, t, ctx);
        llvm::Value *v1 = floatValue(args, 1, t, ctx);
        llvm::Value *result;
        result = ctx->builder->CreateFDiv(v0, v1);
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0->type == t.ptr());
        ctx->builder->CreateStore(result, out0->llValue);
        break;
    }

    case PRIM_numericNegate : {
        ensureArity(args, 1);
        TypePtr t;
        llvm::Value *v = numericValue(args, 0, t, ctx);
        llvm::Value *result;
        switch (t->typeKind) {
        case INTEGER_TYPE :
            result = ctx->builder->CreateNeg(v);
            break;
        case FLOAT_TYPE :
            result = ctx->builder->CreateFNeg(v);
            break;
        default :
            assert(false);
        }
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0->type == t);
        ctx->builder->CreateStore(result, out0->llValue);
        break;
    }

    case PRIM_integerQuotient : {
        ensureArity(args, 2);
        IntegerTypePtr t;
        llvm::Value *v0 = integerValue(args, 0, t, ctx);
        llvm::Value *v1 = integerValue(args, 1, t, ctx);
        llvm::Value *result;
        IntegerType *it = (IntegerType *)t.ptr();
        if (it->isSigned)
            result = ctx->builder->CreateSDiv(v0, v1);
        else
            result = ctx->builder->CreateUDiv(v0, v1);
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0->type == t.ptr());
        ctx->builder->CreateStore(result, out0->llValue);
        break;
    }

    case PRIM_integerRemainder : {
        ensureArity(args, 2);
        IntegerTypePtr t;
        llvm::Value *v0 = integerValue(args, 0, t, ctx);
        llvm::Value *v1 = integerValue(args, 1, t, ctx);
        llvm::Value *result;
        IntegerType *it = (IntegerType *)t.ptr();
        if (it->isSigned)
            result = ctx->builder->CreateSRem(v0, v1);
        else
            result = ctx->builder->CreateURem(v0, v1);
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0->type == t.ptr());
        ctx->builder->CreateStore(result, out0->llValue);
        break;
    }

    case PRIM_integerShiftLeft : {
        ensureArity(args, 2);
        IntegerTypePtr t;
        llvm::Value *v0 = integerValue(args, 0, t, ctx);
        llvm::Value *v1 = integerValue(args, 1, t, ctx);
        llvm::Value *result = ctx->builder->CreateShl(v0, v1);
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0->type == t.ptr());
        ctx->builder->CreateStore(result, out0->llValue);
        break;
    }

    case PRIM_integerShiftRight : {
        ensureArity(args, 2);
        IntegerTypePtr t;
        llvm::Value *v0 = integerValue(args, 0, t, ctx);
        llvm::Value *v1 = integerValue(args, 1, t, ctx);
        llvm::Value *result;
        if (t->isSigned)
            result = ctx->builder->CreateAShr(v0, v1);
        else
            result = ctx->builder->CreateLShr(v0, v1);
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0->type == t.ptr());
        ctx->builder->CreateStore(result, out0->llValue);
        break;
    }

    case PRIM_integerBitwiseAnd : {
        ensureArity(args, 2);
        IntegerTypePtr t;
        llvm::Value *v0 = integerValue(args, 0, t, ctx);
        llvm::Value *v1 = integerValue(args, 1, t, ctx);
        llvm::Value *result = ctx->builder->CreateAnd(v0, v1);
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0->type == t.ptr());
        ctx->builder->CreateStore(result, out0->llValue);
        break;
    }

    case PRIM_integerBitwiseOr : {
        ensureArity(args, 2);
        IntegerTypePtr t;
        llvm::Value *v0 = integerValue(args, 0, t, ctx);
        llvm::Value *v1 = integerValue(args, 1, t, ctx);
        llvm::Value *result = ctx->builder->CreateOr(v0, v1);
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0->type == t.ptr());
        ctx->builder->CreateStore(result, out0->llValue);
        break;
    }

    case PRIM_integerBitwiseXor : {
        ensureArity(args, 2);
        IntegerTypePtr t;
        llvm::Value *v0 = integerValue(args, 0, t, ctx);
        llvm::Value *v1 = integerValue(args, 1, t, ctx);
        llvm::Value *result = ctx->builder->CreateXor(v0, v1);
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0->type == t.ptr());
        ctx->builder->CreateStore(result, out0->llValue);
        break;
    }

    case PRIM_integerBitwiseNot : {
        ensureArity(args, 1);
        IntegerTypePtr t;
        llvm::Value *v = integerValue(args, 0, t, ctx);
        llvm::Value *result = ctx->builder->CreateNot(v);
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0->type == t.ptr());
        ctx->builder->CreateStore(result, out0->llValue);
        break;
    }

    case PRIM_numericConvert : {
        ensureArity(args, 2);
        TypePtr dest = valueToNumericType(args, 0);
        TypePtr src;
        llvm::Value *v = numericValue(args, 1, src, ctx);
        llvm::Value *result;
        if (src == dest) {
            result = v;
        }
        else {
            switch (dest->typeKind) {
            case INTEGER_TYPE : {
                IntegerType *dest2 = (IntegerType *)dest.ptr();
                if (src->typeKind == INTEGER_TYPE) {
                    IntegerType *src2 = (IntegerType *)src.ptr();
                    if (dest2->bits < src2->bits)
                        result = ctx->builder->CreateTrunc(v, llvmType(dest));
                    else if (src2->isSigned)
                        result = ctx->builder->CreateSExt(v, llvmType(dest));
                    else
                        result = ctx->builder->CreateZExt(v, llvmType(dest));
                }
                else if (src->typeKind == FLOAT_TYPE) {
                    if (dest2->isSigned)
                        result = ctx->builder->CreateFPToSI(v, llvmType(dest));
                    else
                        result = ctx->builder->CreateFPToUI(v, llvmType(dest));
                }
                else {
                    assert(false);
                    result = NULL;
                }
                break;
            }
            case FLOAT_TYPE : {
                FloatType *dest2 = (FloatType *)dest.ptr();
                if (src->typeKind == INTEGER_TYPE) {
                    IntegerType *src2 = (IntegerType *)src.ptr();
                    if (src2->isSigned)
                        result = ctx->builder->CreateSIToFP(v, llvmType(dest));
                    else
                        result = ctx->builder->CreateUIToFP(v, llvmType(dest));
                }
                else if (src->typeKind == FLOAT_TYPE) {
                    FloatType *src2 = (FloatType *)src.ptr();
                    if (dest2->bits < src2->bits)
                        result = ctx->builder->CreateFPTrunc(v, llvmType(dest));
                    else
                        result = ctx->builder->CreateFPExt(v, llvmType(dest));
                }
                else {
                    assert(false);
                    result = NULL;
                }
                break;
            }

            default :
                assert(false);
                result = NULL;
                break;
            }
        }
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0->type == dest);
        ctx->builder->CreateStore(result, out0->llValue);
        break;
    }

    case PRIM_integerAddChecked : {
        ensureArity(args, 2);
        IntegerTypePtr t;
        checkIntegerValue(args, 0, t, ctx);
        checkIntegerValue(args, 1, t, ctx);
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0->type.ptr() == t.ptr());
        codegenCallValue(staticCValue(operator_doIntegerAddChecked(), ctx),
                         args,
                         ctx,
                         out);
        break;
    }

    case PRIM_integerSubtractChecked : {
        ensureArity(args, 2);
        IntegerTypePtr t;
        checkIntegerValue(args, 0, t, ctx);
        checkIntegerValue(args, 1, t, ctx);
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0->type.ptr() == t.ptr());
        codegenCallValue(staticCValue(operator_doIntegerSubtractChecked(), ctx),
                         args,
                         ctx,
                         out);
        break;
    }

    case PRIM_integerMultiplyChecked : {
        ensureArity(args, 2);
        IntegerTypePtr t;
        checkIntegerValue(args, 0, t, ctx);
        checkIntegerValue(args, 1, t, ctx);
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0->type.ptr() == t.ptr());
        codegenCallValue(staticCValue(operator_doIntegerMultiplyChecked(), ctx),
                         args,
                         ctx,
                         out);
        break;
    }

    case PRIM_integerQuotientChecked : {
        ensureArity(args, 2);
        IntegerTypePtr t;
        checkIntegerValue(args, 0, t, ctx);
        checkIntegerValue(args, 1, t, ctx);
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0->type.ptr() == t.ptr());
        codegenCallValue(staticCValue(operator_doIntegerQuotientChecked(), ctx),
                         args,
                         ctx,
                         out);
        break;
    }

    case PRIM_integerRemainderChecked : {
        ensureArity(args, 2);
        IntegerTypePtr t;
        checkIntegerValue(args, 0, t, ctx);
        checkIntegerValue(args, 1, t, ctx);
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0->type.ptr() == t.ptr());
        codegenCallValue(staticCValue(operator_doIntegerRemainderChecked(), ctx),
                         args,
                         ctx,
                         out);
        break;
    }

    case PRIM_integerShiftLeftChecked : {
        ensureArity(args, 2);
        IntegerTypePtr t;
        checkIntegerValue(args, 0, t, ctx);
        checkIntegerValue(args, 1, t, ctx);
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0->type.ptr() == t.ptr());
        codegenCallValue(staticCValue(operator_doIntegerShiftLeftChecked(), ctx),
                         args,
                         ctx,
                         out);
        break;
    }

    case PRIM_integerNegateChecked : {
        ensureArity(args, 1);
        IntegerTypePtr t;
        checkIntegerValue(args, 0, t, ctx);
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0->type.ptr() == t.ptr());
        codegenCallValue(staticCValue(operator_doIntegerNegateChecked(), ctx),
                         args,
                         ctx,
                         out);
        break;
    }

    case PRIM_integerConvertChecked : {
        ensureArity(args, 2);
        IntegerTypePtr dest = valueToIntegerType(args, 0);
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0->type == dest.ptr());
        codegenCallValue(staticCValue(operator_doIntegerConvertChecked(), ctx),
                         args,
                         ctx,
                         out);
        break;
    }

    case PRIM_Pointer :
        error("no Pointer primitive overload found");

    case PRIM_addressOf : {
        ensureArity(args, 1);
        CValuePtr cv = args->values[0];
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0->type == pointerType(cv->type));
        ctx->builder->CreateStore(cv->llValue, out0->llValue);
        break;
    }

    case PRIM_pointerDereference : {
        ensureArity(args, 1);
        PointerTypePtr t;
        llvm::Value *v = pointerValue(args, 0, t, ctx);
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0->type == t.ptr());
        ctx->builder->CreateStore(v, out0->llValue);
        break;
    }

    case PRIM_pointerOffset : {
        ensureArity(args, 2);
        PointerTypePtr t;
        llvm::Value *v0 = pointerValue(args, 0, t, ctx);
        IntegerTypePtr offsetT;
        llvm::Value *v1 = integerValue(args, 1, offsetT, ctx);
        if (!offsetT->isSigned && (size_t)offsetT->bits < typeSize(cSizeTType)*8)
            v1 = ctx->builder->CreateZExt(v1, llvmType(cSizeTType));
        vector<llvm::Value *> indices;
        indices.push_back(v1);
        llvm::Value *result = ctx->builder->CreateGEP(v0,
                                                    llvm::makeArrayRef(indices));
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0->type == t.ptr());
        ctx->builder->CreateStore(result, out0->llValue);
        break;
    }

    case PRIM_pointerToInt : {
        ensureArity(args, 2);
        IntegerTypePtr dest = valueToIntegerType(args, 0);
        llvm::Type *llDest = llvmType(dest.ptr());
        TypePtr pt;
        llvm::Value *v = pointerLikeValue(args, 1, pt, ctx);
        llvm::Value *result = ctx->builder->CreatePtrToInt(v, llDest);
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0->type == dest.ptr());
        ctx->builder->CreateStore(result, out0->llValue);
        break;
    }

    case PRIM_intToPointer : {
        ensureArity(args, 2);
        TypePtr dest = valueToPointerLikeType(args, 0);
        IntegerTypePtr t;
        llvm::Value *v = integerValue(args, 1, t, ctx);
        if (t->isSigned && (typeSize(t.ptr()) < typeSize(cPtrDiffTType)))
            v = ctx->builder->CreateSExt(v, llvmType(cPtrDiffTType));
        llvm::Value *result = ctx->builder->CreateIntToPtr(v, llvmType(dest));
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0->type == dest);
        ctx->builder->CreateStore(result, out0->llValue);
        break;
    }

    case PRIM_nullPointer : {
        ensureArity(args, 1);
        TypePtr dest = valueToPointerLikeType(args, 0);
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0->type == dest);
        llvm::PointerType *llType = llvm::dyn_cast<llvm::PointerType>(llvmType(dest));
        assert(llType != NULL);
        llvm::Value *result = llvm::ConstantPointerNull::get(llType);
        ctx->builder->CreateStore(result, out0->llValue);
        break;
    }

    case PRIM_ByRef :
        error("no ByRef primitive overload found");

    case PRIM_RecordWithProperties :
        error("no RecordWithProperties primitive overload found");

    case PRIM_CodePointer :
        error("no CodePointer primitive overload found");

    case PRIM_makeCodePointer : {
        if (args->size() < 1)
            arityError2(1, args->size());
        ObjectPtr callable = valueToStatic(args, 0);
        switch (callable->objKind) {
        case TYPE :
        case RECORD_DECL :
        case VARIANT_DECL :
        case PROCEDURE :
        case GLOBAL_ALIAS :
            break;
        case PRIM_OP :
            if (!isOverloadablePrimOp(callable))
                argumentError(0, "pointer to primitive cannot be taken");
            break;
        case INTRINSIC :
            argumentError(0, "pointer to LLVM intrinsic cannot be taken");
            break;
        default :
            argumentError(0, "invalid callable");
        }
        vector<TypePtr> argsKey;
        vector<ValueTempness> argsTempness;
        for (unsigned i = 1; i < args->size(); ++i) {
            TypePtr t = valueToType(args, i);
            argsKey.push_back(t);
            argsTempness.push_back(TEMPNESS_LVALUE);
        }

        CompileContextPusher pusher(callable, argsKey);

        InvokeEntry* entry =
            safeAnalyzeCallable(callable, argsKey, argsTempness);
        if (entry->callByName)
            argumentError(0, "cannot create pointer to call-by-name code");
        assert(entry->analyzed);
        if (!entry->llvmFunc)
            codegenCodeBody(entry);
        assert(entry->llvmFunc);
        TypePtr cpType = codePointerType(argsKey,
                                         entry->returnIsRef,
                                         entry->returnTypes);
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0->type == cpType);
        ctx->builder->CreateStore(entry->llvmFunc, out0->llValue);
        break;
    }

    case PRIM_ExternalCodePointer :
        error("no ExternalCodePointer primitive overload found");

    case PRIM_makeExternalCodePointer : {
        if (args->size() < 3)
            arityError2(3, args->size());
        ObjectPtr callable = valueToStatic(args, 0);
        switch (callable->objKind) {
        case TYPE :
        case RECORD_DECL :
        case VARIANT_DECL :
        case PROCEDURE :
        case GLOBAL_ALIAS :
            break;
        case PRIM_OP :
            if (!isOverloadablePrimOp(callable))
                argumentError(0, "pointer to primitive cannot be taken");
            break;
        case INTRINSIC :
            argumentError(0, "pointer to intrinsic cannot be taken");
            break;
        default :
            argumentError(0, "invalid callable");
        }

        ObjectPtr ccObj = valueToStatic(args, 1);
        ObjectPtr isVarArgObj = valueToStatic(args, 2);
        CallingConv cc;
        bool isVarArg;
        if (!staticToCallingConv(ccObj, cc))
            argumentError(1, "expecting a calling convention attribute");
        TypePtr type;
        if (!staticToBool(isVarArgObj, isVarArg, type))
            argumentTypeError(2, "static boolean", type);

        if (isVarArg)
            argumentError(2, "implementing variadic external functions is not yet supported");

        vector<TypePtr> argsKey;
        vector<ValueTempness> argsTempness;
        for (unsigned i = 3; i < args->size(); ++i) {
            TypePtr t = valueToType(args, i);
            argsKey.push_back(t);
            argsTempness.push_back(TEMPNESS_LVALUE);
        }

        CompileContextPusher pusher(callable, argsKey);

        InvokeEntry* entry =
            safeAnalyzeCallable(callable, argsKey, argsTempness);
        if (entry->callByName)
            argumentError(0, "cannot create pointer to call-by-name code");
        assert(entry->analyzed);
        if (!entry->llvmFunc)
            codegenCodeBody(entry);
        assert(entry->llvmFunc);
        if (!entry->llvmCWrappers[cc])
            codegenCWrapper(entry, cc);
        assert(entry->llvmCWrappers[cc]);
        TypePtr returnType;
        if (entry->returnTypes.size() == 0) {
            returnType = NULL;
        }
        else if (entry->returnTypes.size() == 1) {
            if (entry->returnIsRef[0])
                argumentError(0, "cannot create C compatible pointer "
                              " to return-by-reference code");
            returnType = entry->returnTypes[0];
        }
        else {
            argumentError(0, "cannot create C compatible pointer "
                          "to multi-return code");
        }
        TypePtr ccpType = cCodePointerType(cc,
                                           argsKey,
                                           isVarArg,
                                           returnType);
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0->type == ccpType);

        llvm::Value *opaqueValue = ctx->builder->CreateBitCast(
            entry->llvmCWrappers[cc], llvmType(out0->type));
        ctx->builder->CreateStore(opaqueValue, out0->llValue);
        break;
    }

    case PRIM_callExternalCodePointer : {
        if (args->size() < 1)
            arityError2(1, args->size());
        CCodePointerTypePtr cpt;
        cCodePointerValue(args, 0, cpt);
        MultiCValuePtr args2 = new MultiCValue();
        for (size_t i = 1; i < args->size(); ++i)
            args2->add(args->values[i]);
        codegenCallCCodePointer(args->values[0], args2, ctx, out);
        break;
    }

    case PRIM_bitcast : {
        ensureArity(args, 2);
        TypePtr dest = valueToType(args, 0);
        CValuePtr src = args->values[1];
        if (typeSize(dest) > typeSize(src->type))
            error("destination type for bitcast is larger than source type");
        if (typeAlignment(dest) > typeAlignment(src->type))
            error("destination type for bitcast has stricter alignment than source type");
        llvm::Value *arg1 = args->values[1]->llValue;
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0->type == pointerType(dest));
        llvm::Value *result = ctx->builder->CreateBitCast(arg1, llvmPointerType(dest));
        ctx->builder->CreateStore(result, out0->llValue);
        break;
    }

    case PRIM_Array :
        error("no Array primitive overload found");

    case PRIM_arrayRef : {
        ensureArity(args, 2);
        ArrayTypePtr at;
        llvm::Value *av = arrayValue(args, 0, at);
        IntegerTypePtr indexType;
        llvm::Value *iv = integerValue(args, 1, indexType, ctx);
        if (!indexType->isSigned && (size_t)indexType->bits < typeSize(cSizeTType)*8)
            iv = ctx->builder->CreateZExt(iv, llvmType(cSizeTType));
        vector<llvm::Value *> indices;
        indices.push_back(llvm::ConstantInt::get(llvmIntType(32), 0));
        indices.push_back(iv);
        llvm::Value *ptr =
            ctx->builder->CreateGEP(av, llvm::makeArrayRef(indices));
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0->type == pointerType(at->elementType));
        ctx->builder->CreateStore(ptr, out0->llValue);
        break;
    }

    case PRIM_arrayElements : {
        ensureArity(args, 1);
        ArrayTypePtr at;
        llvm::Value *varray = arrayValue(args, 0, at);
        assert(out->size() == (size_t)at->size);
        for (unsigned i = 0; i < at->size; ++i) {
            CValuePtr outi = out->values[i];
            assert(outi->type == pointerType(at->elementType));
            llvm::Value *ptr = ctx->builder->CreateConstGEP2_32(varray, 0, i);
            ctx->builder->CreateStore(ptr, outi->llValue);
        }
        break;
    }

    case PRIM_Vec :
        error("no Vec primitive overload found");

    case PRIM_Tuple :
        error("no Tuple primitive overload found");

    case PRIM_TupleElementCount : {
        ensureArity(args, 1);
        TupleTypePtr t = valueToTupleType(args, 0);
        ValueHolderPtr vh = sizeTToValueHolder(t->elementTypes.size());
        codegenStaticObject(vh.ptr(), ctx, out);
        break;
    }

    case PRIM_tupleRef : {
        ensureArity(args, 2);
        TupleTypePtr tt;
        llvm::Value *vtuple = tupleValue(args, 0, tt);
        size_t i = valueToStaticSizeTOrInt(args, 1);
        if (i >= tt->elementTypes.size())
            argumentIndexRangeError(1, "tuple element index",
                                    i, tt->elementTypes.size());
        llvm::Value *ptr = ctx->builder->CreateConstGEP2_32(vtuple, 0, unsigned(i));
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0->type == pointerType(tt->elementTypes[i]));
        ctx->builder->CreateStore(ptr, out0->llValue);
        break;
    }

    case PRIM_tupleElements : {
        ensureArity(args, 1);
        TupleTypePtr tt;
        llvm::Value *vtuple = tupleValue(args, 0, tt);
        assert(out->size() == tt->elementTypes.size());
        for (unsigned i = 0; i < tt->elementTypes.size(); ++i) {
            CValuePtr outi = out->values[i];
            assert(outi->type == pointerType(tt->elementTypes[i]));
            llvm::Value *ptr = ctx->builder->CreateConstGEP2_32(vtuple, 0, i);
            ctx->builder->CreateStore(ptr, outi->llValue);
        }
        break;
    }

    case PRIM_Union :
        error("no Union primitive overload found");

    case PRIM_UnionMemberCount : {
        ensureArity(args, 1);
        UnionTypePtr t = valueToUnionType(args, 0);
        ValueHolderPtr vh = sizeTToValueHolder(t->memberTypes.size());
        codegenStaticObject(vh.ptr(), ctx, out);
        break;
    }

    case PRIM_RecordP : {
        ensureArity(args, 1);
        bool isRecordType = false;
        ObjectPtr obj = valueToStatic(args->values[0]);
        if (obj.ptr() && (obj->objKind == TYPE)) {
            Type *t = (Type *)obj.ptr();
            if (t->typeKind == RECORD_TYPE)
                isRecordType = true;
        }
        ValueHolderPtr vh = boolToValueHolder(isRecordType);
        codegenStaticObject(vh.ptr(), ctx, out);
        break;
    }

    case PRIM_RecordFieldCount : {
        ensureArity(args, 1);
        RecordTypePtr rt = valueToRecordType(args, 0);
        ValueHolderPtr vh = sizeTToValueHolder(rt->fieldCount());
        codegenStaticObject(vh.ptr(), ctx, out);
        break;
    }

    case PRIM_RecordFieldName : {
        ensureArity(args, 2);
        RecordTypePtr rt = valueToRecordType(args, 0);
        size_t i = valueToStaticSizeTOrInt(args, 1);
        llvm::ArrayRef<IdentifierPtr> fieldNames = recordFieldNames(rt);
        if (i >= fieldNames.size())
            argumentIndexRangeError(1, "record field index",
                                    i, fieldNames.size());
        codegenStaticObject(fieldNames[i].ptr(), ctx, out);
        break;
    }

    case PRIM_RecordWithFieldP : {
        ensureArity(args, 2);
        bool result = false;
        ObjectPtr obj = valueToStatic(args->values[0]);
        IdentifierPtr fname = valueToIdentifier(args, 1);
        if (obj.ptr() && (obj->objKind == TYPE)) {
            Type *t = (Type *)obj.ptr();
            if (t->typeKind == RECORD_TYPE) {
                RecordType *rt = (RecordType *)t;
                const llvm::StringMap<size_t> &fieldIndexMap =
                    recordFieldIndexMap(rt);
                result = (fieldIndexMap.find(fname->str)
                          != fieldIndexMap.end());
            }
        }
        ValueHolderPtr vh = boolToValueHolder(result);
        codegenStaticObject(vh.ptr(), ctx, out);
        break;
    }

    case PRIM_recordFieldRef : {
        ensureArity(args, 2);
        RecordTypePtr rt;
        llvm::Value *vrec = recordValue(args, 0, rt);
        size_t i = valueToStaticSizeTOrInt(args, 1);
        llvm::ArrayRef<TypePtr> fieldTypes = recordFieldTypes(rt);
        if (i >= rt->fieldCount())
            argumentIndexRangeError(1, "record field index",
                                    i, rt->fieldCount());

        if (rt->hasVarField && i >= rt->varFieldPosition) {
            if (i == rt->varFieldPosition) {
                assert(out->size() == rt->varFieldSize());
                for (unsigned j = 0; j < rt->varFieldSize(); ++j) {
                    CValuePtr outi = out->values[j];
                    assert(outi->type == pointerType(fieldTypes[i+j]));
                    llvm::Value *ptr = ctx->builder->CreateConstGEP2_32(vrec, 0, unsigned(i)+j);
                    ctx->builder->CreateStore(ptr, outi->llValue);
                }
            } else {
                unsigned k = unsigned(i+rt->varFieldSize()-1);
                llvm::Value *ptr = ctx->builder->CreateConstGEP2_32(vrec, 0, k);
                assert(out->size() == 1);
                CValuePtr out0 = out->values[0];
                assert(out0->type == pointerType(fieldTypes[k]));
                ctx->builder->CreateStore(ptr, out0->llValue);
            }
        } else {
            llvm::Value *ptr = ctx->builder->CreateConstGEP2_32(vrec, 0, unsigned(i));
            assert(out->size() == 1);
            CValuePtr out0 = out->values[0];
            assert(out0->type == pointerType(fieldTypes[i]));
            ctx->builder->CreateStore(ptr, out0->llValue);
        }

        break;
    }

    case PRIM_recordFieldRefByName : {
        ensureArity(args, 2);
        RecordTypePtr rt;
        llvm::Value *vrec = recordValue(args, 0, rt);
        IdentifierPtr fname = valueToIdentifier(args, 1);
        const llvm::StringMap<size_t> &fieldIndexMap = recordFieldIndexMap(rt);
        llvm::StringMap<size_t>::const_iterator fi =
            fieldIndexMap.find(fname->str);
        if (fi == fieldIndexMap.end()) {
            string buf;
            llvm::raw_string_ostream sout(buf);
            sout << "field not found: " << fname->str;
            argumentError(1, sout.str());
        }
        unsigned i = unsigned(fi->second);
        llvm::ArrayRef<TypePtr> fieldTypes = recordFieldTypes(rt);
        if (rt->hasVarField && i >= rt->varFieldPosition) {
            if (i == rt->varFieldPosition) {
                assert(out->size() == rt->varFieldSize());
                for (unsigned j = 0; j < rt->varFieldSize(); ++j) {
                    CValuePtr outi = out->values[j];
                    assert(outi->type == pointerType(fieldTypes[i+j]));
                    llvm::Value *ptr = ctx->builder->CreateConstGEP2_32(vrec, 0, i+j);
                    ctx->builder->CreateStore(ptr, outi->llValue);
                }
            } else {
                unsigned k = i+unsigned(rt->varFieldSize())-1;
                llvm::Value *ptr = ctx->builder->CreateConstGEP2_32(vrec, 0, k);
                assert(out->size() == 1);
                CValuePtr out0 = out->values[0];
                assert(out0->type == pointerType(fieldTypes[k]));
                ctx->builder->CreateStore(ptr, out0->llValue);
            }
        } else {
            llvm::Value *ptr = ctx->builder->CreateConstGEP2_32(vrec, 0, i);
            assert(out->size() == 1);
            CValuePtr out0 = out->values[0];
            assert(out0->type == pointerType(fieldTypes[i]));
            ctx->builder->CreateStore(ptr, out0->llValue);
        }
        break;
    }

    case PRIM_recordFields : {
        ensureArity(args, 1);
        RecordTypePtr rt;
        llvm::Value *vrec = recordValue(args, 0, rt);
        llvm::ArrayRef<TypePtr> fieldTypes = recordFieldTypes(rt);
        assert(out->size() == fieldTypes.size());
        for (unsigned i = 0; i < fieldTypes.size(); ++i) {
            CValuePtr outi = out->values[i];
            assert(outi->type == pointerType(fieldTypes[i]));
            llvm::Value *ptr = ctx->builder->CreateConstGEP2_32(vrec, 0, i);
            ctx->builder->CreateStore(ptr, outi->llValue);
        }
        break;
    }

    case PRIM_recordVariadicField : {
        ensureArity(args, 1);
        RecordTypePtr rt;
        llvm::Value *vrec = recordValue(args, 0, rt);
        llvm::ArrayRef<TypePtr> fieldTypes = recordFieldTypes(rt);
        if (!rt->hasVarField)
            argumentError(0, "expecting a record with a variadic field");
        assert(out->size() == rt->varFieldSize());
        for (unsigned i = 0; i < rt->varFieldSize(); ++i) {
            unsigned k = rt->varFieldPosition + i;
            CValuePtr outi = out->values[i];
            assert(outi->type == pointerType(fieldTypes[k]));
            llvm::Value *ptr = ctx->builder->CreateConstGEP2_32(vrec, 0, k);
            ctx->builder->CreateStore(ptr, outi->llValue);
        }
        break;
    }

    case PRIM_VariantP : {
        ensureArity(args, 1);
        bool isVariantType = false;
        ObjectPtr obj = valueToStatic(args->values[0]);
        if (obj.ptr() && (obj->objKind == TYPE)) {
            Type *t = (Type *)obj.ptr();
            if (t->typeKind == VARIANT_TYPE)
                isVariantType = true;
        }
        ValueHolderPtr vh = boolToValueHolder(isVariantType);
        codegenStaticObject(vh.ptr(), ctx, out);
        break;
    }

    case PRIM_VariantMemberIndex : {
        ensureArity(args, 2);
        VariantTypePtr vt = valueToVariantType(args, 0);
        TypePtr mt = valueToType(args, 1);
        llvm::ArrayRef<TypePtr> memberTypes = variantMemberTypes(vt);
        size_t index = (size_t)-1;
        for (size_t i = 0; i < memberTypes.size(); ++i) {
            if (memberTypes[i] == mt) {
                index = i;
                break;
            }
        }
        ValueHolderPtr vh = sizeTToValueHolder(index);
        codegenStaticObject(vh.ptr(), ctx, out);
        break;
    }

    case PRIM_VariantMemberCount : {
        ensureArity(args, 1);
        VariantTypePtr vt = valueToVariantType(args, 0);
        llvm::ArrayRef<TypePtr> memberTypes = variantMemberTypes(vt);
        size_t size = memberTypes.size();
        ValueHolderPtr vh = sizeTToValueHolder(size);
        codegenStaticObject(vh.ptr(), ctx, out);
        break;
    }

    case PRIM_VariantMembers :
        break;

    case PRIM_BaseType :
        break;

    case PRIM_Static :
        error("no Static primitive overload found");

    case PRIM_StaticName :
    case PRIM_MainModule :
    case PRIM_StaticModule :
    case PRIM_ModuleName :
    case PRIM_ModuleMemberNames :
        break;

    case PRIM_staticIntegers :
        break;

    case PRIM_integers : {
        ensureArity(args, 1);
        ObjectPtr obj = valueToStatic(args, 0);
        if (obj->objKind != VALUE_HOLDER)
            argumentError(0, "expecting a static SizeT or Int value");
        ValueHolder *vh = (ValueHolder *)obj.ptr();
        if (vh->type == cIntType) {
            int count = vh->as<int>();
            if (count < 0)
                argumentError(0, "negative values are not allowed");
            assert(out->size() == (size_t)count);
            for (int i = 0; i < count; ++i) {
                ValueHolderPtr vhi = intToValueHolder(i);
                CValuePtr outi = out->values[(size_t)i];
                assert(outi->type == cIntType);
                llvm::Constant *value = llvm::ConstantInt::get(llvmIntType(32), (size_t)i);
                ctx->builder->CreateStore(value, outi->llValue);
            }
        }
        else if (vh->type == cSizeTType) {
            size_t count = vh->as<size_t>();
            assert(out->size() == count);
            for (size_t i = 0; i < count; ++i) {
                ValueHolderPtr vhi = sizeTToValueHolder(i);
                CValuePtr outi = out->values[i];
                assert(outi->type == cSizeTType);
                llvm::Type *llSizeTType = llvmIntType(unsigned(typeSize(cSizeTType)*8));
                llvm::Constant *value = llvm::ConstantInt::get(llSizeTType, i);
                ctx->builder->CreateStore(value, outi->llValue);
            }
        }
        else {
            argumentError(0, "expecting a static SizeT or Int value");
        }
        break;
    }

    case PRIM_staticFieldRef : {
        ensureArity(args, 2);
        ObjectPtr moduleObj = valueToStatic(args, 0);
        if (moduleObj->objKind != MODULE)
            argumentError(0, "expecting a module");
        Module *module = (Module *)moduleObj.ptr();
        IdentifierPtr ident = valueToIdentifier(args, 1);
        ObjectPtr obj = safeLookupPublic(module, ident);
        codegenStaticObject(obj, ctx, out);
        break;
    }

    case PRIM_EnumP : {
        ensureArity(args, 1);
        bool isEnumType = false;
        ObjectPtr obj = valueToStatic(args->values[0]);
        if (obj.ptr() && (obj->objKind == TYPE)) {
            Type *t = (Type *)obj.ptr();
            if (t->typeKind == ENUM_TYPE)
                isEnumType = true;
        }
        ValueHolderPtr vh = boolToValueHolder(isEnumType);
        codegenStaticObject(vh.ptr(), ctx, out);
        break;
    }

    case PRIM_EnumMemberCount : {
        ensureArity(args, 1);
        EnumTypePtr et = valueToEnumType(args, 0);
        size_t n = et->enumeration->members.size();
        ValueHolderPtr vh = sizeTToValueHolder(n);
        codegenStaticObject(vh.ptr(), ctx, out);
        break;
    }

    case PRIM_EnumMemberName :
        break;

    case PRIM_enumToInt : {
        ensureArity(args, 1);
        EnumTypePtr et;
        llvm::Value *v = enumValue(args, 0, et, ctx);
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0->type == cIntType);
        ctx->builder->CreateStore(v, out0->llValue);
        break;
    }

    case PRIM_intToEnum : {
        ensureArity(args, 2);
        EnumTypePtr et = valueToEnumType(args, 0);
        IntegerTypePtr it = (IntegerType *)cIntType.ptr();
        llvm::Value *v = integerValue(args, 1, it, ctx);
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0->type == et.ptr());
        ctx->builder->CreateStore(v, out0->llValue);
        break;
    }

    case PRIM_StringLiteralP : {
        ensureArity(args, 1);
        bool result = false;
        CValuePtr cv0 = args->values[0];
        if (cv0->type->typeKind == STATIC_TYPE) {
            StaticType *st = (StaticType *)cv0->type.ptr();
            result = (st->obj->objKind == IDENTIFIER);
        }
        ValueHolderPtr vh = boolToValueHolder(result);
        codegenStaticObject(vh.ptr(), ctx, out);
        break;
    }

    case PRIM_stringLiteralByteIndex : {
        ensureArity(args, 2);
        IdentifierPtr ident = valueToIdentifier(args, 0);
        unsigned n = unsigned(valueToStaticSizeTOrInt(args, 1));
        if (n >= ident->str.size())
            argumentError(1, "string literal index out of bounds");

        assert(out->size() == 1);
        CValuePtr outi = out->values[0];
        assert(outi->type == cIntType);
        llvm::Constant *value = llvm::ConstantInt::get(llvmIntType(32), size_t(ident->str[n]));
        ctx->builder->CreateStore(value, outi->llValue);
        break;
    }

    case PRIM_stringLiteralBytes : {
        ensureArity(args, 1);
        IdentifierPtr ident = valueToIdentifier(args, 0);
        assert(out->size() == ident->str.size());
        for (unsigned i = 0; i < ident->str.size(); ++i) {
            CValuePtr outi = out->values[i];
            assert(outi->type == cIntType);
            llvm::Constant *value = llvm::ConstantInt::get(llvmIntType(32), size_t(ident->str[i]));
            ctx->builder->CreateStore(value, outi->llValue);
        }
        break;
    }

    case PRIM_stringLiteralByteSize : {
        ensureArity(args, 1);
        IdentifierPtr ident = valueToIdentifier(args, 0);
        ValueHolderPtr vh = sizeTToValueHolder(ident->str.size());
        codegenStaticObject(vh.ptr(), ctx, out);
        break;
    }

    case PRIM_stringLiteralByteSlice :
    case PRIM_stringLiteralConcat :
    case PRIM_stringLiteralFromBytes :
        break;

    case PRIM_stringTableConstant : {
        ensureArity(args, 1);
        IdentifierPtr ident = valueToIdentifier(args, 0);
        llvm::Value *value = codegenStringTableConstant(ident->str);

        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0->type == pointerType(cSizeTType));
        ctx->builder->CreateStore(value, out0->llValue);
        break;
    }

    case PRIM_FlagP : {
        ensureArity(args, 1);
        IdentifierPtr ident = valueToIdentifier(args, 0);
        llvm::StringMap<string>::const_iterator flag = globalFlags.find(ident->str);
        ValueHolderPtr vh = boolToValueHolder(flag != globalFlags.end());
        codegenStaticObject(vh.ptr(), ctx, out);
        break;
    }

    case PRIM_Flag :
        break;

    case PRIM_atomicFence : {
        ensureArity(args, 1);
        llvm::AtomicOrdering order = atomicOrderValue(args, 0);

        ctx->builder->CreateFence(order);
        assert(out->size() == 0);
        break;
    }

    case PRIM_atomicRMW : {
        ensureArity(args, 4);
        llvm::AtomicOrdering order = atomicOrderValue(args, 0);
        llvm::AtomicRMWInst::BinOp op = atomicRMWOpValue(args, 1);

        PointerTypePtr ptrT;
        llvm::Value *ptr = pointerValue(args, 2, ptrT, ctx);
        TypePtr argT = args->values[3]->type;
        llvm::Value *arg = ctx->builder->CreateLoad(args->values[3]->llValue);

        if (ptrT->pointeeType != argT)
            argumentTypeError(3, ptrT->pointeeType, argT);

        llvm::Value *result = ctx->builder->CreateAtomicRMW(op, ptr, arg, order);

        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0->type == argT);
        ctx->builder->CreateStore(result, out0->llValue);
        break;
    }

    case PRIM_atomicLoad : {
        ensureArity(args, 2);
        llvm::AtomicOrdering order = atomicOrderValue(args, 0);

        PointerTypePtr ptrT;
        llvm::Value *ptr = pointerValue(args, 1, ptrT, ctx);

        llvm::Value *result = ctx->builder->Insert(
            new llvm::LoadInst(ptr, "", false, unsigned(typeAlignment(ptrT->pointeeType)), order));

        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0->type == ptrT->pointeeType);
        ctx->builder->CreateStore(result, out0->llValue);
        break;
    }

    case PRIM_atomicStore : {
        ensureArity(args, 3);
        llvm::AtomicOrdering order = atomicOrderValue(args, 0);

        PointerTypePtr ptrT;
        llvm::Value *ptr = pointerValue(args, 1, ptrT, ctx);
        TypePtr argT = args->values[2]->type;
        llvm::Value *arg = ctx->builder->CreateLoad(args->values[2]->llValue);

        if (ptrT->pointeeType != argT)
            argumentTypeError(2, ptrT->pointeeType, argT);

        ctx->builder->Insert(new llvm::StoreInst(
            arg, ptr, false, unsigned(typeAlignment(argT)), order));

        assert(out->size() == 0);
        break;
    }

    case PRIM_atomicCompareExchange : {
        ensureArity(args, 4);
        llvm::AtomicOrdering order = atomicOrderValue(args, 0);

        PointerTypePtr ptrT;
        llvm::Value *ptr = pointerValue(args, 1, ptrT, ctx);
        TypePtr oldvT = args->values[2]->type;
        llvm::Value *oldv = ctx->builder->CreateLoad(args->values[2]->llValue);
        TypePtr newvT = args->values[3]->type;
        llvm::Value *newv = ctx->builder->CreateLoad(args->values[3]->llValue);

        if (ptrT->pointeeType != oldvT)
            argumentTypeError(2, ptrT->pointeeType, oldvT);
        if (ptrT->pointeeType != newvT)
            argumentTypeError(3, ptrT->pointeeType, newvT);

        llvm::Value *result = ctx->builder->CreateAtomicCmpXchg(ptr, oldv, newv, order);

        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0->type == oldvT);
        ctx->builder->CreateStore(result, out0->llValue);
        break;
    }

    case PRIM_activeException : {
        ensureArity(args, 0);
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0->type == pointerType(uint8Type));

        assert(ctx->exceptionValue != NULL);

        llvm::Value *expv = ctx->builder->CreateLoad(ctx->exceptionValue);
        ctx->builder->CreateStore(expv, out0->llValue);
        break;
    }

    case PRIM_memcpy :
    case PRIM_memmove : {
        ensureArity(args, 3);
        PointerTypePtr topt;
        PointerTypePtr frompt;
        llvm::Value *toptr = pointerValue(args, 0, topt, ctx);
        llvm::Value *fromptr = pointerValue(args, 1, frompt, ctx);
        IntegerTypePtr it;
        llvm::Value *count = integerValue(args, 2, it, ctx);

        size_t pointerSize = typeSize(topt.ptr());
        llvm::Type *sizeType = llvmIntType(unsigned(8*pointerSize));

        if (typeSize(it.ptr()) > pointerSize)
            argumentError(2, "integer type for memcpy must be pointer-sized or smaller");
        if (typeSize(it.ptr()) < pointerSize)
            count = ctx->builder->CreateZExt(count, sizeType);

        unsigned alignment = unsigned(std::min(
            typeAlignment(topt->pointeeType), typeAlignment(frompt->pointeeType)));

        if (x->primOpCode == PRIM_memcpy)
            ctx->builder->CreateMemCpy(toptr, fromptr, count, alignment);
        else
            ctx->builder->CreateMemMove(toptr, fromptr, count, alignment);

        break;
    }

    case PRIM_countValues : {
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0->type == cIntType);
        llvm::Constant *value = llvm::ConstantInt::get(llvmIntType(32), args->size());
        ctx->builder->CreateStore(value, out0->llValue);
        break;
    }

    case PRIM_nthValue : {
        if (args->size() < 1)
            arityError2(1, args->size());
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];

        size_t i = valueToStaticSizeTOrInt(args, 0);
        if (i+1 >= args->size())
            argumentError(0, "nthValue argument out of bounds");

        codegenValueForward(out0, args->values[i+1], ctx);
        break;
    }

    case PRIM_withoutNthValue : {
        if (args->size() < 1)
            arityError2(1, args->size());
        assert(out->size() == args->size() - 2);

        size_t i = valueToStaticSizeTOrInt(args, 0);
        if (i+1 >= args->size())
            argumentError(0, "withoutNthValue argument out of bounds");

        for (size_t argi = 1, outi = 0; argi < args->size(); ++argi) {
            if (argi == i+1)
                continue;
            assert(outi < out->size());
            codegenValueForward(out->values[outi], args->values[argi], ctx);
            ++outi;
        }
        break;
    }

    case PRIM_takeValues : {
        if (args->size() < 1)
            arityError2(1, args->size());

        size_t i = valueToStaticSizeTOrInt(args, 0);
        if (i+1 >= args->size())
            i = args->size() - 1;

        assert(out->size() == i);
        for (size_t argi = 1, outi = 0; argi < i+1; ++argi, ++outi) {
            assert(outi < out->size());
            codegenValueForward(out->values[outi], args->values[argi], ctx);
        }
        break;
    }

    case PRIM_dropValues : {
        if (args->size() < 1)
            arityError2(1, args->size());

        size_t i = valueToStaticSizeTOrInt(args, 0);
        if (i+1 >= args->size())
            i = args->size() - 1;

        assert(out->size() == args->size() - i - 1);
        for (size_t argi = i+1, outi = 0; argi < args->size(); ++argi, ++outi) {
            assert(outi < out->size());
            codegenValueForward(out->values[outi], args->values[argi], ctx);
        }
        break;
    }

    case PRIM_LambdaRecordP : {
        ensureArity(args, 1);

        bool isLambda;

        ObjectPtr callable = valueToStatic(args->values[0]);
        if (callable != NULL && callable->objKind == TYPE) {
            Type *t = (Type*)callable.ptr();
            if (t->typeKind == RECORD_TYPE) {
                RecordType *r = (RecordType*)t;
                isLambda = r->record->lambda != NULL;
            } else
                isLambda = false;
        } else {
            isLambda = false;
        }

        ValueHolderPtr vh = boolToValueHolder(isLambda);
        codegenStaticObject(vh.ptr(), ctx, out);
        break;
    }

    case PRIM_LambdaSymbolP : {
        ensureArity(args, 1);

        bool isLambda;

        ObjectPtr callable = valueToStatic(args->values[0]);
        if (callable != NULL && callable->objKind == PROCEDURE) {
            Procedure *p = (Procedure*)callable.ptr();
            isLambda = p->lambda != NULL;
        } else
            isLambda = false;

        ValueHolderPtr vh = boolToValueHolder(isLambda);
        codegenStaticObject(vh.ptr(), ctx, out);
        break;
    }

    case PRIM_LambdaMonoP : {
        ensureArity(args, 1);

        bool isMono;

        ObjectPtr callable = valueToStatic(args->values[0]);
        if (callable != NULL && callable->objKind == TYPE) {
            Type *t = (Type*)callable.ptr();
            if (t->typeKind == RECORD_TYPE) {
                RecordType *r = (RecordType*)t;
                isMono = r->record->lambda->mono.monoState == Procedure_MonoOverload;
            } else
                isMono = false;
        } else {
            isMono = false;
        }

        ValueHolderPtr vh = boolToValueHolder(isMono);
        codegenStaticObject(vh.ptr(), ctx, out);
        break;
    }

    case PRIM_LambdaMonoInputTypes :
        break;

    case PRIM_GetOverload :
        break;

    case PRIM_usuallyEquals : {
        ensureArity(args, 2);
        if (!isPrimitiveType(args->values[0]->type))
            argumentTypeError(0, "primitive type", args->values[0]->type);
        llvm::Value *expectee = ctx->builder->CreateLoad(args->values[0]->llValue);
        ObjectPtr expectedValue = unwrapStaticType(args->values[1]->type);
        if (expectedValue == NULL
            || expectedValue->objKind != VALUE_HOLDER
            || ((ValueHolder*)expectedValue.ptr())->type != args->values[0]->type)
            error("second argument to usuallyEquals must be a static value of the same type as the first argument");
        llvm::Constant *expected =
            valueHolderToLLVMConstant((ValueHolder*)expectedValue.ptr(), ctx);

        assert(expectee->getType() == expected->getType());

        llvm::Value *argValues[2] = {expectee, expected};

        llvm::Function *expectFunction = llvm::Intrinsic::getDeclaration(
            llvmModule,
            llvm::Intrinsic::expect,
            expectee->getType());

        llvm::Value *hinted = ctx->builder->CreateCall(expectFunction, argValues);
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0->type == args->values[0]->type);
        ctx->builder->CreateStore(hinted, out0->llValue);
        break;
    }

    default :
        assert(false);
        break;

    }
}





}
