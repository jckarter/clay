#include "clay.hpp"


//
// declarations
//

struct JumpTarget {
    llvm::BasicBlock *block;
    int stackMarker;
    JumpTarget() : block(NULL), stackMarker(-1) {}
    JumpTarget(llvm::BasicBlock *block, int stackMarker)
        : block(block), stackMarker(stackMarker) {}
};

struct CodeContext {
    InvokeEntryPtr entry;
    llvm::Value *returnOutPtr;
    JumpTarget returnTarget;
    map<string, JumpTarget> labels;
    vector<JumpTarget> breaks;
    vector<JumpTarget> continues;
    CodeContext(InvokeEntryPtr entry,
                llvm::Value *returnOutPtr,
                const JumpTarget &returnTarget)
        : entry(entry), returnOutPtr(returnOutPtr),
          returnTarget(returnTarget) {}
};

void codegenValueInit(CValuePtr dest);
void codegenValueDestroy(CValuePtr dest);
void codegenValueCopy(CValuePtr dest, CValuePtr src);
void codegenValueAssign(CValuePtr dest, CValuePtr src);

llvm::Value *codegenToBoolFlag(CValuePtr a);

vector<CValuePtr> stackCValues;

static int markStack();
static void destroyStack(int marker);
static void popStack(int marker);
static void sweepStack(int marker);

CValuePtr codegenAllocValue(TypePtr t);

InvokeEntryPtr codegenProcedure(ProcedurePtr x,
                                const vector<ObjectPtr> &argsKey,
                                const vector<LocationPtr> &argLocations);
InvokeEntryPtr codegenOverloadable(OverloadablePtr x,
                                   const vector<ObjectPtr> &argsKey,
                                   const vector<LocationPtr> &argLocations);
InvokeEntryPtr codegenCodeBody(ObjectPtr callable,
                               const vector<ObjectPtr> &argsKey,
                               const string &callableName);

bool codegenStatement(StatementPtr stmt, EnvPtr env, CodeContext &ctx);

void codegenInvokeVoid(ObjectPtr callable,
                       const vector<ExprPtr> &args,
                       EnvPtr env);


//
// codegen value ops
//

void codegenValueInit(CValuePtr dest)
{
    vector<ExprPtr> args;
    args.push_back(new CValueExpr(dest));
    codegenInvokeVoid(kernelName("init"), args, new Env());
}

void codegenValueDestroy(CValuePtr dest)
{
    vector<ExprPtr> args;
    args.push_back(new CValueExpr(dest));
    codegenInvokeVoid(kernelName("destroy"), args, new Env());
}

void codegenValueCopy(CValuePtr dest, CValuePtr src)
{
    vector<ExprPtr> args;
    args.push_back(new CValueExpr(dest));
    args.push_back(new CValueExpr(src));
    codegenInvokeVoid(kernelName("copy"), args, new Env());
}

void codegenValueAssign(CValuePtr dest, CValuePtr src)
{
    vector<ExprPtr> args;
    args.push_back(new CValueExpr(dest));
    args.push_back(new CValueExpr(src));
    codegenInvokeVoid(kernelName("assign"), args, new Env());
}

llvm::Value *codegenToBoolFlag(CValuePtr a)
{
    if (a->type != boolType)
        error("expecting bool type");
    llvm::Value *b1 = llvmBuilder->CreateLoad(a->llValue);
    llvm::Value *zero = llvm::ConstantInt::get(llvmType(boolType), 0);
    llvm::Value *flag1 = llvmBuilder->CreateICmpNE(b1, zero);
    return flag1;
}



//
// codegen temps
//

static int markStack()
{
    return stackCValues.size();
}

static void destroyStack(int marker)
{
    int i = (int)stackCValues.size();
    assert(marker <= i);
    while (marker < i) {
        --i;
        codegenValueDestroy(stackCValues[i]);
    }
}

static void popStack(int marker)
{
    assert(marker <= (int)stackCValues.size());
    while (marker < (int)stackCValues.size())
        stackCValues.pop_back();
}

static void sweepStack(int marker)
{
    assert(marker <= (int)stackCValues.size());
    while (marker < (int)stackCValues.size()) {
        codegenValueDestroy(stackCValues.back());
        stackCValues.pop_back();
    }
}

CValuePtr codegenAllocValue(TypePtr t)
{
    const llvm::Type *llt = llvmType(t);
    llvm::Value *llval = llvmInitBuilder->CreateAlloca(llt);
    CValuePtr cv = new CValue(t, llval);
    stackCValues.push_back(cv);
    return cv;
}



//
// codegenProcedure, codegenOverloadable, codegenCodeBody
//

InvokeEntryPtr codegenProcedure(ProcedurePtr x,
                                const vector<ObjectPtr> &argsKey,
                                const vector<LocationPtr> &argLocations)
{
    analyzeInvokeProcedure2(x, argsKey, argLocations);
    return codegenCodeBody(x.ptr(), argsKey, x->name->str);
}

InvokeEntryPtr codegenOverloadable(OverloadablePtr x,
                                   const vector<ObjectPtr> &argsKey,
                                   const vector<LocationPtr> &argLocations)
{
    analyzeInvokeOverloadable2(x, argsKey, argLocations);
    return codegenCodeBody(x.ptr(), argsKey, x->name->str);
}

InvokeEntryPtr codegenCodeBody(ObjectPtr callable,
                               const vector<ObjectPtr> &argsKey,
                               const string &callableName)
{
    InvokeEntryPtr entry = lookupInvoke(callable, argsKey);
    assert(entry->analyzed);
    if (entry->llvmFunc)
        return entry;

    vector<IdentifierPtr> argNames;
    vector<TypePtr> argTypes;
    vector<const llvm::Type *> llArgTypes;

    const vector<FormalArgPtr> &formalArgs = entry->code->formalArgs;
    assert(argsKey.size() == formalArgs.size());

    for (unsigned i = 0; i < formalArgs.size(); ++i) {
        if (formalArgs[i]->objKind != VALUE_ARG)
            continue;
        ValueArg *x = (ValueArg *)formalArgs[i].ptr();
        argNames.push_back(x->name);
        assert(argsKey[i]->objKind == TYPE);
        Type *y = (Type *)argsKey[i].ptr();
        argTypes.push_back(y);
        llArgTypes.push_back(llvmType(pointerType(y)));
    }

    const llvm::Type *llReturnType;
    if (entry->analysis->objKind == VOID_TYPE) {
        llReturnType = llvmReturnType(voidType.ptr());
    }
    else {
        PValue *x = (PValue *)entry->analysis.ptr();
        if (x->isTemp) {
            llArgTypes.push_back(llvmType(pointerType(x->type)));
            llReturnType = llvmReturnType(voidType.ptr());
        }
        else {
            llReturnType = llvmType(pointerType(x->type));
        }
    }

    llvm::FunctionType *llFuncType =
        llvm::FunctionType::get(llReturnType, llArgTypes, false);

    llvm::Function *llFunc =
        llvm::Function::Create(llFuncType,
                               llvm::Function::InternalLinkage,
                               "clay_" + callableName,
                               llvmModule);

    entry->llvmFunc = llFunc;

    llvm::Function *savedLLVMFunction = llvmFunction;
    llvm::IRBuilder<> *savedLLVMInitBuilder = llvmInitBuilder;
    llvm::IRBuilder<> *savedLLVMBuilder = llvmBuilder;

    llvmFunction = llFunc;

    llvm::BasicBlock *initBlock = newBasicBlock("init");
    llvm::BasicBlock *codeBlock = newBasicBlock("code");
    llvm::BasicBlock *returnBlock = newBasicBlock("return");

    llvmInitBuilder = new llvm::IRBuilder<>(initBlock);
    llvmBuilder = new llvm::IRBuilder<>(codeBlock);

    EnvPtr env = new Env(entry->env);

    llvm::Function::arg_iterator ai = llFunc->arg_begin();
    for (unsigned i = 0; i < argNames.size(); ++i, ++ai) {
        llvm::Argument *llArgValue = &(*ai);
        llArgValue->setName(argNames[i]->str);
        CValuePtr cvalue = new CValue(argTypes[i], llArgValue);
        addLocal(env, argNames[i], cvalue.ptr());
    }

    llvm::Value *outPtr = NULL;
    if (entry->analysis->objKind != VOID_TYPE) {
        PValue *x = (PValue *)entry->analysis.ptr();
        if (x->isTemp) {
            outPtr = &(*ai);
        }
        else {
            const llvm::Type *llt = llvmType(pointerType(x->type));
            outPtr = llvmInitBuilder->CreateAlloca(llt);
        }
    }

    JumpTarget returnTarget(returnBlock, markStack());
    CodeContext ctx(entry, outPtr, returnTarget);

    bool terminated = codegenStatement(entry->code->body, env, ctx);
    if (!terminated)
        llvmBuilder->CreateBr(returnBlock);

    llvmInitBuilder->CreateBr(codeBlock);

    llvmBuilder->SetInsertPoint(returnBlock);

    if (entry->analysis->objKind == VOID_TYPE) {
        llvmBuilder->CreateRetVoid();
    }
    else {
        PValue *x = (PValue *)entry->analysis.ptr();
        if (x->isTemp) {
            llvmBuilder->CreateRetVoid();
        }
        else {
            llvm::Value *v = llvmBuilder->CreateLoad(outPtr);
            llvmBuilder->CreateRet(v);
        }
    }

    delete llvmInitBuilder;
    delete llvmBuilder;

    llvmInitBuilder = savedLLVMInitBuilder;
    llvmBuilder = savedLLVMBuilder;
    llvmFunction = savedLLVMFunction;

    return entry;
}
