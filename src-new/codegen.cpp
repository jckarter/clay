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
    CValuePtr returnVal;
    JumpTarget returnTarget;
    map<string, JumpTarget> labels;
    vector<JumpTarget> breaks;
    vector<JumpTarget> continues;
    CodeContext(InvokeEntryPtr entry,
                CValuePtr returnVal,
                const JumpTarget &returnTarget)
        : entry(entry), returnVal(returnVal),
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
static void destroyAndPopStack(int marker);

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

void codegenCollectLabels(const vector<StatementPtr> &statements,
                          unsigned startIndex,
                          CodeContext &ctx);
EnvPtr codegenBinding(BindingPtr x, EnvPtr env);

void codegenRootIntoValue(ExprPtr expr,
                          EnvPtr env,
                          PValuePtr pv,
                          CValuePtr cv);
CValuePtr codegenRootValue(ExprPtr expr, EnvPtr env, CValuePtr out);

void codegenIntoValue(ExprPtr expr, EnvPtr env, PValuePtr pv, CValuePtr cv);
CValuePtr codegenAsRef(ExprPtr expr, EnvPtr env, PValuePtr pv);
CValuePtr codegenValue(ExprPtr expr, EnvPtr env, CValuePtr out);
CValuePtr codegenMaybeVoid(ExprPtr expr, EnvPtr env, CValuePtr out);

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

static void destroyAndPopStack(int marker)
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

    CValuePtr returnVal;
    if (entry->analysis->objKind != VOID_TYPE) {
        PValue *x = (PValue *)entry->analysis.ptr();
        if (x->isTemp) {
            returnVal = new CValue(x->type, &(*ai));
        }
        else {
            const llvm::Type *llt = llvmType(pointerType(x->type));
            returnVal = new CValue(x->type, llvmInitBuilder->CreateAlloca(llt));
        }
    }

    JumpTarget returnTarget(returnBlock, markStack());
    CodeContext ctx(entry, returnVal, returnTarget);

    bool terminated = codegenStatement(entry->code->body, env, ctx);
    if (!terminated) {
        destroyStack(returnTarget.stackMarker);
        llvmBuilder->CreateBr(returnBlock);
    }
    popStack(returnTarget.stackMarker);

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
            llvm::Value *v = llvmBuilder->CreateLoad(returnVal->llValue);
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



//
// codegenStatement
//

bool codegenStatement(StatementPtr stmt, EnvPtr env, CodeContext &ctx)
{
    LocationContext loc(stmt->location);

    switch (stmt->objKind) {

    case BLOCK : {
        Block *x = (Block *)stmt.ptr();
        int blockMarker = markStack();
        codegenCollectLabels(x->statements, 0, ctx);
        bool terminated = false;
        for (unsigned i = 0; i < x->statements.size(); ++i) {
            StatementPtr y = x->statements[i];
            if (y->objKind == LABEL) {
                Label *z = (Label *)y.ptr();
                map<string, JumpTarget>::iterator li =
                    ctx.labels.find(z->name->str);
                assert(li != ctx.labels.end());
                const JumpTarget &jt = li->second;
                if (!terminated)
                    llvmBuilder->CreateBr(jt.block);
                llvmBuilder->SetInsertPoint(jt.block);
            }
            else if (terminated) {
                error(y, "unreachable code");
            }
            else if (y->objKind == BINDING) {
                env = codegenBinding((Binding *)y.ptr(), env);
                codegenCollectLabels(x->statements, i+1, ctx);
            }
            else {
                terminated = codegenStatement(y, env, ctx);
            }
        }
        if (!terminated)
            destroyStack(blockMarker);
        popStack(blockMarker);
        return terminated;
    }

    case LABEL :
    case BINDING :
        error("invalid statement");

    case ASSIGNMENT : {
        Assignment *x = (Assignment *)stmt.ptr();
        PValuePtr pvLeft = analyzeValue(x->left, env);
        PValuePtr pvRight = analyzeValue(x->right, env);
        if (pvLeft->isTemp)
            error(x->left, "cannot assign to a temporary");
        int marker = markStack();
        CValuePtr cvLeft = codegenValue(x->left, env, NULL);
        CValuePtr cvRight = codegenAsRef(x->right, env, pvRight);
        codegenValueAssign(cvLeft, cvRight);
        destroyAndPopStack(marker);
        return false;
    }

    case GOTO : {
        Goto *x = (Goto *)stmt.ptr();
        map<string, JumpTarget>::iterator li =
            ctx.labels.find(x->labelName->str);
        if (li == ctx.labels.end())
            error("goto label not found");
        const JumpTarget &jt = li->second;
        destroyStack(jt.stackMarker);
        llvmBuilder->CreateBr(jt.block);
        return true;
    }

    case RETURN : {
        Return *x = (Return *)stmt.ptr();
        ObjectPtr returnAnalysis = ctx.entry->analysis;
        if (returnAnalysis->objKind == VOID_TYPE) {
            if (x->expr.ptr())
                error("void return expected");
        }
        else {
            PValuePtr returnPV = (PValue *)returnAnalysis.ptr();
            if (!x->expr)
                error("non-void return expected");
            if (!returnPV->isTemp)
                error("return by reference expected");
            PValuePtr pv = analyzeValue(x->expr, env);
            if (pv->type != returnPV->type)
                error("type mismatch in return");
            codegenRootIntoValue(x->expr, env, pv, ctx.returnVal);
        }
        const JumpTarget &jt = ctx.returnTarget;
        destroyStack(jt.stackMarker);
        llvmBuilder->CreateBr(jt.block);
        return true;
    }

    case RETURN_REF : {
        ReturnRef *x = (ReturnRef *)stmt.ptr();
        if (ctx.entry->analysis->objKind == VOID_TYPE)
            error("void return expected");
        PValuePtr returnPV = (PValue *)ctx.entry->analysis.ptr();
        if (returnPV->isTemp)
            error("return by value expected");
        PValuePtr pv = analyzeValue(x->expr, env);
        if (pv->type != returnPV->type)
            error("type mismatch in return");
        if (pv->isTemp)
            error("cannot return a temporary by reference");
        CValuePtr ref = codegenRootValue(x->expr, env, NULL);
        llvmBuilder->CreateStore(ref->llValue, ctx.returnVal->llValue);
        const JumpTarget &jt = ctx.returnTarget;
        destroyStack(jt.stackMarker);
        llvmBuilder->CreateBr(jt.block);
        return true;
    }

    case IF : {
        If *x = (If *)stmt.ptr();
        PValuePtr pv = analyzeValue(x->condition, env);
        int marker = markStack();
        CValuePtr cv = codegenAsRef(x->condition, env, pv);
        llvm::Value *cond = codegenToBoolFlag(cv);
        destroyAndPopStack(marker);

        llvm::BasicBlock *trueBlock = newBasicBlock("ifTrue");
        llvm::BasicBlock *falseBlock = newBasicBlock("ifFalse");
        llvm::BasicBlock *mergeBlock = NULL;

        llvmBuilder->CreateCondBr(cond, trueBlock, falseBlock);

        bool terminated1 = false;
        bool terminated2 = false;

        llvmBuilder->SetInsertPoint(trueBlock);
        terminated1 = codegenStatement(x->thenPart, env, ctx);
        if (!terminated1) {
            if (!mergeBlock)
                mergeBlock = newBasicBlock("ifMerge");
            llvmBuilder->CreateBr(mergeBlock);
        }

        llvmBuilder->SetInsertPoint(falseBlock);
        if (x->elsePart.ptr())
            terminated2 = codegenStatement(x->elsePart, env, ctx);
        if (!terminated2) {
            if (!mergeBlock)
                mergeBlock = newBasicBlock("ifMerge");
            llvmBuilder->CreateBr(mergeBlock);
        }

        if (!terminated1 || !terminated2)
            llvmBuilder->SetInsertPoint(mergeBlock);

        return terminated1 && terminated2;
    }

    case EXPR_STATEMENT : {
        ExprStatement *x = (ExprStatement *)stmt.ptr();
        ObjectPtr a = analyze(x->expr, env);
        int marker = markStack();
        if (a->objKind == VOID_TYPE) {
            codegenMaybeVoid(x->expr, env, NULL);
        }
        else if (a->objKind == PVALUE) {
            PValuePtr pv = (PValue *)a.ptr();
            codegenAsRef(x->expr, env, pv);
        }
        else {
            error("invalid expression statement");
        }
        destroyAndPopStack(marker);
        return false;
    }

    case WHILE : {
        While *x = (While *)stmt.ptr();

        llvm::BasicBlock *whileBegin = newBasicBlock("whileBegin");
        llvm::BasicBlock *whileBody = newBasicBlock("whileBody");
        llvm::BasicBlock *whileEnd = newBasicBlock("whileEnd");

        llvmBuilder->CreateBr(whileBegin);
        llvmBuilder->SetInsertPoint(whileBegin);

        PValuePtr pv = analyzeValue(x->condition, env);
        int marker = markStack();
        CValuePtr cv = codegenAsRef(x->condition, env, pv);
        llvm::Value *cond = codegenToBoolFlag(cv);
        destroyAndPopStack(marker);

        llvmBuilder->CreateCondBr(cond, whileBody, whileEnd);

        ctx.breaks.push_back(JumpTarget(whileEnd, markStack()));
        ctx.continues.push_back(JumpTarget(whileBegin, markStack()));
        llvmBuilder->SetInsertPoint(whileBody);
        bool terminated = codegenStatement(x->body, env, ctx);
        if (!terminated)
            llvmBuilder->CreateBr(whileBegin);
        ctx.breaks.pop_back();
        ctx.continues.pop_back();

        llvmBuilder->SetInsertPoint(whileEnd);
        return false;
    }

    case BREAK : {
        if (ctx.breaks.empty())
            error("invalid break statement");
        const JumpTarget &jt = ctx.breaks.back();
        destroyStack(jt.stackMarker);
        llvmBuilder->CreateBr(jt.block);
        return true;
    }

    case CONTINUE : {
        if (ctx.continues.empty())
            error("invalid continue statement");
        const JumpTarget &jt = ctx.breaks.back();
        destroyStack(jt.stackMarker);
        llvmBuilder->CreateBr(jt.block);
        return true;
    }

    case FOR : {
        For *x = (For *)stmt.ptr();
        if (!x->desugared)
            x->desugared = desugarForStatement(x);
        return codegenStatement(x->desugared, env, ctx);
    }

    default :
        assert(false);
        return false;

    }
}

void codegenCollectLabels(const vector<StatementPtr> &statements,
                          unsigned startIndex,
                          CodeContext &ctx)
{
    for (unsigned i = startIndex; i < statements.size(); ++i) {
        StatementPtr x = statements[i];
        switch (x->objKind) {

        case LABEL : {
            Label *y = (Label *)x.ptr();
            map<string, JumpTarget>::iterator li =
                ctx.labels.find(y->name->str);
            if (li != ctx.labels.end())
                error(x, "label redefined");
            llvm::BasicBlock *bb = newBasicBlock(y->name->str.c_str());
            ctx.labels[y->name->str] = JumpTarget(bb, markStack());
            break;
        }

        case BINDING :
            return;

        }
    }
}

EnvPtr codegenBinding(BindingPtr x, EnvPtr env)
{
    switch (x->bindingKind) {

    case VAR : {
        PValuePtr pv = analyzeValue(x->expr, env);
        CValuePtr cv = codegenAllocValue(pv->type);
        codegenRootIntoValue(x->expr, env, pv, cv);
        EnvPtr env2 = new Env(env);
        addLocal(env2, x->name, cv.ptr());
        return env2;
    }

    case REF : {
        PValuePtr pv = analyzeValue(x->expr, env);
        CValuePtr cv;
        if (pv->isTemp) {
            cv = codegenAllocValue(pv->type);
            codegenRootValue(x->expr, env, cv);
        }
        else {
            cv = codegenRootValue(x->expr, env, NULL);
        }
        EnvPtr env2 = new Env(env);
        addLocal(env2, x->name, cv.ptr());
        return env2;
    }

    case STATIC : {
        ObjectPtr v = evaluateStatic(x->expr, env);
        EnvPtr env2 = new Env(env);
        addLocal(env2, x->name, v.ptr());
        return env2;
    }

    default :
        assert(false);
        return NULL;

    }
}
