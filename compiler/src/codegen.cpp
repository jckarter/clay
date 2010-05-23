#include "clay.hpp"


vector<CValuePtr> stackCValues;

static bool exceptionsEnabled = true;
static bool llvmExceptionsInited = false;

void setExceptionsEnabled(bool enabled)
{
    exceptionsEnabled = enabled;
}

static void initExceptions() 
{
    getDeclaration(llvmModule, llvm::Intrinsic::eh_selector);
    getDeclaration(llvmModule, llvm::Intrinsic::eh_exception);
    
    llvm::FunctionType *llFunc1Type =
        llvm::FunctionType::get(llvmIntType(32), true);
    
    llvm::Function::Create(llFunc1Type,
                           llvm::Function::ExternalLinkage,
                           "__gxx_personality_v0",
                           llvmModule);
                            
    vector<const llvm::Type *> argTypes;
    argTypes.push_back(llvmBuilder->getInt8Ty()->getPointerTo());

    llvm::FunctionType *llFunc2Type = 
        llvm::FunctionType::get(llvmVoidType(), argTypes, false);
    llvm::Function::Create(llFunc2Type,
                           llvm::Function::ExternalLinkage,
                           "_Unwind_Resume_or_Rethrow",
                           llvmModule);
    
    llvmExceptionsInited = true;    
}

static llvm::BasicBlock *createLandingPad(CodegenContextPtr ctx) 
{
    llvm::BasicBlock *lpad = newBasicBlock("lpad");
    llvmBuilder->SetInsertPoint(lpad);
    llvm::Function *ehException = llvmModule->getFunction("llvm.eh.exception");
    llvm::Function *ehSelector = llvmModule->getFunction("llvm.eh.selector");
    llvm::Function *personality = 
        llvmModule->getFunction("__gxx_personality_v0");
    llvm::Value *ehPtr = llvmBuilder->CreateCall(ehException);
    llvmBuilder->CreateStore(ehPtr, ctx->exception);
    llvm::Value* funcPtr = llvmBuilder->CreateBitCast(personality,
                                    llvmBuilder->getInt8Ty()->getPointerTo());
    std::vector<llvm::Value*> llArgs;
    llArgs.push_back(ehPtr);
    llArgs.push_back(funcPtr);
    llArgs.push_back(
        llvm::Constant::getNullValue(
            llvm::PointerType::getUnqual(
                llvm::IntegerType::getInt8Ty(llvm::getGlobalContext()))));
    llvmBuilder->CreateCall(ehSelector, llArgs.begin(), llArgs.end());
    return lpad;
}

static llvm::BasicBlock *createUnwindBlock(CodegenContextPtr ctx) {
    llvm::BasicBlock *unwindBlock = newBasicBlock("Unwind");
    llvm::IRBuilder<> llBuilder(unwindBlock);
    llvm::Function *unwindResume = 
        llvmModule->getFunction("_Unwind_Resume_or_Rethrow");
    llvm::Value *arg = llBuilder.CreateLoad(ctx->exception);
    llBuilder.CreateCall(unwindResume, arg);
    llBuilder.CreateUnreachable();
    return unwindBlock;
}

static llvm::Value *createCall(llvm::Value *llCallable, 
                               vector<llvm::Value *>::iterator argBegin,
                               vector<llvm::Value *>::iterator argEnd,
                               EnvPtr env,
                               CodegenContextPtr ctx) 
{  
    // We don't have a context
    if (!exceptionsEnabled || !ctx.ptr())
        return llvmBuilder->CreateCall(llCallable, argBegin, argEnd);

    llvm::BasicBlock *landingPad;

    int startMarker = ctx->returnTarget.stackMarker;
    int endMarker = cgMarkStack();

    // If we are inside a try block then adjust marker
    if (ctx->catchBlock)
        startMarker = ctx->tryBlockStackMarker;

    if (endMarker <= startMarker && !ctx->catchBlock)
        return llvmBuilder->CreateCall(llCallable, argBegin, argEnd);

    llvm::BasicBlock *savedInsertPoint = llvmBuilder->GetInsertBlock();
   
    if (!llvmExceptionsInited) 
       initExceptions(); 
    if (!ctx->exception)
        ctx->exception = llvmInitBuilder->CreateAlloca(
                llvmBuilder->getInt8Ty()->getPointerTo());
    
    for (int i = startMarker; i < endMarker; ++i) {
        CValuePtr val = stackCValues[i];
        if (val->destructor) 
            continue;
        
        val->destructor = newBasicBlock("destructor");
        llvmBuilder->SetInsertPoint(val->destructor);
        codegenValueDestroy(val, env, ctx);
        
        if (ctx->catchBlock && ctx->tryBlockStackMarker == i)
            llvmBuilder->CreateBr(ctx->catchBlock);
        else if (i == ctx->returnTarget.stackMarker) {
            if (!ctx->unwindBlock)
                ctx->unwindBlock = createUnwindBlock(ctx);
            llvmBuilder->CreateBr(ctx->unwindBlock);
        }
        else {
            assert(stackCValues[i-1]->destructor);
            llvmBuilder->CreateBr(stackCValues[i-1]->destructor);
        }
    }

    // No live vars, but we were inside a try block
    if (endMarker <= startMarker) {
        landingPad = createLandingPad(ctx);
        llvmBuilder->CreateBr(ctx->catchBlock);
    }
    else {
        if (!stackCValues[endMarker-1]->landingPad) {
            stackCValues[endMarker-1]->landingPad = createLandingPad(ctx);
            llvmBuilder->CreateBr(stackCValues[endMarker-1]->destructor);
        }
        landingPad = stackCValues[endMarker-1]->landingPad;
    }

    llvmBuilder->SetInsertPoint(savedInsertPoint);
    
    llvm::BasicBlock *normalBlock = newBasicBlock("normal");
    llvm::Value *result = llvmBuilder->CreateInvoke(llCallable, normalBlock, 
            landingPad, argBegin, argEnd);
    llvmBuilder->SetInsertPoint(normalBlock);
    return result;
}



//
// codegen value ops
//

void codegenValueInit(CValuePtr dest, EnvPtr env, CodegenContextPtr ctx)
{
    codegenInvoke(dest->type.ptr(), vector<ExprPtr>(), env, ctx,
                  new MultiCValue(dest));
}

void codegenValueDestroy(CValuePtr dest, EnvPtr env, CodegenContextPtr ctx)
{
    vector<ExprPtr> args;
    args.push_back(new ObjectExpr(dest.ptr()));
    codegenInvokeVoid(kernelName("destroy"), args, env, ctx);
}

void codegenValueCopy(CValuePtr dest, CValuePtr src,
                      EnvPtr env, CodegenContextPtr ctx)
{
    vector<ExprPtr> args;
    args.push_back(new ObjectExpr(src.ptr()));
    codegenInvoke(dest->type.ptr(), args, env, ctx, new MultiCValue(dest));
}

void codegenValueAssign(CValuePtr dest, CValuePtr src,
                        EnvPtr env, CodegenContextPtr ctx)
{
    vector<ExprPtr> args;
    args.push_back(new ObjectExpr(dest.ptr()));
    args.push_back(new ObjectExpr(src.ptr()));
    codegenInvokeVoid(kernelName("assign"), args, env, ctx);
}

llvm::Value *codegenToBoolFlag(CValuePtr a, EnvPtr env, CodegenContextPtr ctx)
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

int cgMarkStack()
{
    return stackCValues.size();
}

void cgDestroyStack(int marker, EnvPtr env, CodegenContextPtr ctx)
{
    int i = (int)stackCValues.size();
    assert(marker <= i);
    while (marker < i) {
        --i;
        codegenValueDestroy(stackCValues[i], env, ctx);
    }
}

void cgPopStack(int marker)
{
    assert(marker <= (int)stackCValues.size());
    while (marker < (int)stackCValues.size())
        stackCValues.pop_back();
}

void cgDestroyAndPopStack(int marker, EnvPtr env, CodegenContextPtr ctx)
{
    assert(marker <= (int)stackCValues.size());
    while (marker < (int)stackCValues.size()) {
        codegenValueDestroy(stackCValues.back(), env, ctx);
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
// codegenCodeBody
//

void codegenCodeBody(InvokeEntryPtr entry, const string &callableName)
{
    assert(entry->analyzed);
    if (entry->llvmFunc)
        return;

    vector<const llvm::Type *> llArgTypes;
    for (unsigned i = 0; i < entry->fixedArgTypes.size(); ++i)
        llArgTypes.push_back(llvmPointerType(entry->fixedArgTypes[i]));

    if (entry->hasVarArgs) {
        for (unsigned i = 0; i < entry->varArgTypes.size(); ++i)
            llArgTypes.push_back(llvmPointerType(entry->varArgTypes[i]));
    }
    for (unsigned i = 0; i < entry->returnTypes.size(); ++i) {
        TypePtr rt = entry->returnTypes[i];
        if (entry->returnIsRef[i])
            llArgTypes.push_back(llvmPointerType(pointerType(rt)));
        else
            llArgTypes.push_back(llvmPointerType(rt));
    }

    llvm::FunctionType *llFuncType =
        llvm::FunctionType::get(llvmVoidType(), llArgTypes, false);

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
    for (unsigned i = 0; i < entry->fixedArgTypes.size(); ++i, ++ai) {
        llvm::Argument *llArgValue = &(*ai);
        llArgValue->setName(entry->fixedArgNames[i]->str);
        CValuePtr cvalue = new CValue(entry->fixedArgTypes[i], llArgValue);
        addLocal(env, entry->fixedArgNames[i], cvalue.ptr());
    }

    VarArgsInfoPtr vaInfo = new VarArgsInfo(entry->hasVarArgs);
    if (entry->hasVarArgs) {
        for (unsigned i = 0; i < entry->varArgTypes.size(); ++i, ++ai) {
            llvm::Argument *llArgValue = &(*ai);
            ostringstream sout;
            sout << "varg" << i;
            llArgValue->setName(sout.str());
            CValuePtr cvalue = new CValue(entry->varArgTypes[i], llArgValue);
            vaInfo->varArgs.push_back(new ObjectExpr(cvalue.ptr()));
        }
    }
    addLocal(env, new Identifier("%varArgs"), vaInfo.ptr());

    const vector<ReturnSpecPtr> &returnSpecs = entry->code->returnSpecs;
    if (!returnSpecs.empty()) {
        assert(returnSpecs.size() == entry->returnTypes.size());
    }

    vector<CReturn> returns;
    for (unsigned i = 0; i < entry->returnTypes.size(); ++i, ++ai) {
        llvm::Argument *llArgValue = &(*ai);
        TypePtr rt = entry->returnTypes[i];
        ReturnSpecPtr rspec;
        if (!returnSpecs.empty())
            rspec = returnSpecs[i];
        if (entry->returnIsRef[i]) {
            CValuePtr cv = new CValue(pointerType(rt), llArgValue);
            returns.push_back(CReturn(true, rt, cv));
            if (rspec.ptr()) {
                assert(!rspec->name);
            }
        }
        else {
            CValuePtr cv = new CValue(rt, llArgValue);
            returns.push_back(CReturn(false, rt, cv));
            if (rspec.ptr() && rspec->name.ptr()) {
                addLocal(env, rspec->name, cv.ptr());
                llArgValue->setName(rspec->name->str);
            }
        }
    }

    JumpTarget returnTarget(returnBlock, cgMarkStack());
    CodegenContextPtr ctx = new CodegenContext(returns, returnTarget);

    bool terminated = codegenStatement(entry->code->body, env, ctx);
    if (!terminated) {
        cgDestroyStack(returnTarget.stackMarker, env, ctx);
        llvmBuilder->CreateBr(returnBlock);
    }
    cgPopStack(returnTarget.stackMarker);

    llvmInitBuilder->CreateBr(codeBlock);

    llvmBuilder->SetInsertPoint(returnBlock);
    llvmBuilder->CreateRetVoid();

    delete llvmInitBuilder;
    delete llvmBuilder;

    llvmInitBuilder = savedLLVMInitBuilder;
    llvmBuilder = savedLLVMBuilder;
    llvmFunction = savedLLVMFunction;
}



//
// codegenStatement
//

bool codegenStatement(StatementPtr stmt, EnvPtr env, CodegenContextPtr ctx)
{
    LocationContext loc(stmt->location);

    switch (stmt->stmtKind) {

    case BLOCK : {
        Block *x = (Block *)stmt.ptr();
        int blockMarker = cgMarkStack();
        codegenCollectLabels(x->statements, 0, ctx);
        bool terminated = false;
        for (unsigned i = 0; i < x->statements.size(); ++i) {
            StatementPtr y = x->statements[i];
            if (y->stmtKind == LABEL) {
                Label *z = (Label *)y.ptr();
                map<string, JumpTarget>::iterator li =
                    ctx->labels.find(z->name->str);
                assert(li != ctx->labels.end());
                const JumpTarget &jt = li->second;
                if (!terminated)
                    llvmBuilder->CreateBr(jt.block);
                llvmBuilder->SetInsertPoint(jt.block);
            }
            else if (terminated) {
                error(y, "unreachable code");
            }
            else if (y->stmtKind == BINDING) {
                env = codegenBinding((Binding *)y.ptr(), env, ctx);
                codegenCollectLabels(x->statements, i+1, ctx);
            }
            else {
                terminated = codegenStatement(y, env, ctx);
            }
        }
        if (!terminated)
            cgDestroyStack(blockMarker, env, ctx);
        cgPopStack(blockMarker);
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
        int marker = cgMarkStack();
        CValuePtr cvLeft = codegenOneAsRef(x->left, env, ctx);
        CValuePtr cvRight = codegenOneAsRef(x->right, env, ctx);
        codegenValueAssign(cvLeft, cvRight, env, ctx);
        cgDestroyAndPopStack(marker, env, ctx);
        return false;
    }

    case INIT_ASSIGNMENT : {
        InitAssignment *x = (InitAssignment *)stmt.ptr();
        PValuePtr pvLeft = analyzeValue(x->left, env);
        PValuePtr pvRight = analyzeValue(x->right, env);
        if (pvLeft->type != pvRight->type)
            error("type mismatch");
        if (pvLeft->isTemp)
            error(x->left, "cannot assign to a temporary");
        int marker = cgMarkStack();
        CValuePtr cvLeft = codegenOneAsRef(x->left, env, ctx);
        codegenIntoOne(x->right, env, ctx, cvLeft);
        cgDestroyAndPopStack(marker, env, ctx);
        return false;
    }

    case UPDATE_ASSIGNMENT : {
        UpdateAssignment *x = (UpdateAssignment *)stmt.ptr();
        PValuePtr pvLeft = analyzeValue(x->left, env);
        if (pvLeft->isTemp)
            error(x->left, "cannot assign to a temporary");
        CallPtr call = new Call(kernelNameRef(updateOperatorName(x->op)));
        call->args.push_back(x->left);
        call->args.push_back(x->right);
        return codegenStatement(new ExprStatement(call.ptr()), env, ctx);
    }

    case GOTO : {
        Goto *x = (Goto *)stmt.ptr();
        map<string, JumpTarget>::iterator li =
            ctx->labels.find(x->labelName->str);
        if (li == ctx->labels.end())
            error("goto label not found");
        const JumpTarget &jt = li->second;
        cgDestroyStack(jt.stackMarker, env, ctx);
        llvmBuilder->CreateBr(jt.block);
        return true;
    }

    case RETURN : {
        Return *x = (Return *)stmt.ptr();
        if (x->exprs.size() != ctx->returns.size())
            arityError(ctx->returns.size(), x->exprs.size());

        for (unsigned i = 0; i < x->exprs.size(); ++i) {
            CReturn &y = ctx->returns[i];
            if (y.byRef) {
                if (!x->isRef[i])
                    error(x->exprs[i], "return by reference expected");
                CValuePtr cret = codegenOneAsRef(x->exprs[i], env, ctx);
                if (cret->type != y.type)
                    error(x->exprs[i], "type mismatch");
                llvmBuilder->CreateStore(cret->llValue, y.value->llValue);
            }
            else {
                if (x->isRef[i])
                    error(x->exprs[i], "return by value expected");
                PValuePtr pret = analyzeValue(x->exprs[i], env);
                if (pret->type != y.type)
                    error(x->exprs[i], "type mismatch");
                codegenIntoOne(x->exprs[i], env, ctx, y.value);
            }
        }
        const JumpTarget &jt = ctx->returnTarget;
        cgDestroyStack(jt.stackMarker, env, ctx);
        llvmBuilder->CreateBr(jt.block);
        return true;
    }

    case IF : {
        If *x = (If *)stmt.ptr();
        int marker = cgMarkStack();
        CValuePtr cv = codegenOneAsRef(x->condition, env, ctx);
        llvm::Value *cond = codegenToBoolFlag(cv, env, ctx);
        cgDestroyAndPopStack(marker, env, ctx);

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
        int marker = cgMarkStack();
        MultiPValuePtr mpv = analyzeMultiValue(x->expr, env);
        MultiCValuePtr mcv = new MultiCValue();
        for (unsigned i = 0; i < mpv->size(); ++i) {
            PValuePtr pv = mpv->values[i];
            if (pv->isTemp) {
                CValuePtr cv = codegenAllocValue(pv->type);
                mcv->values.push_back(cv);
            }
            else {
                mcv->values.push_back(NULL);
            }
        }
        codegenExpr(x->expr, env, ctx, mcv);
        cgDestroyAndPopStack(marker, env, ctx);
        return false;
    }

    case WHILE : {
        While *x = (While *)stmt.ptr();

        llvm::BasicBlock *whileBegin = newBasicBlock("whileBegin");
        llvm::BasicBlock *whileBody = newBasicBlock("whileBody");
        llvm::BasicBlock *whileEnd = newBasicBlock("whileEnd");

        llvmBuilder->CreateBr(whileBegin);
        llvmBuilder->SetInsertPoint(whileBegin);

        int marker = cgMarkStack();
        CValuePtr cv = codegenOneAsRef(x->condition, env, ctx);
        llvm::Value *cond = codegenToBoolFlag(cv, env, ctx);
        cgDestroyAndPopStack(marker, env, ctx);

        llvmBuilder->CreateCondBr(cond, whileBody, whileEnd);

        ctx->breaks.push_back(JumpTarget(whileEnd, cgMarkStack()));
        ctx->continues.push_back(JumpTarget(whileBegin, cgMarkStack()));
        llvmBuilder->SetInsertPoint(whileBody);
        bool terminated = codegenStatement(x->body, env, ctx);
        if (!terminated)
            llvmBuilder->CreateBr(whileBegin);
        ctx->breaks.pop_back();
        ctx->continues.pop_back();

        llvmBuilder->SetInsertPoint(whileEnd);
        return false;
    }

    case BREAK : {
        if (ctx->breaks.empty())
            error("invalid break statement");
        const JumpTarget &jt = ctx->breaks.back();
        cgDestroyStack(jt.stackMarker, env, ctx);
        llvmBuilder->CreateBr(jt.block);
        return true;
    }

    case CONTINUE : {
        if (ctx->continues.empty())
            error("invalid continue statement");
        const JumpTarget &jt = ctx->continues.back();
        cgDestroyStack(jt.stackMarker, env, ctx);
        llvmBuilder->CreateBr(jt.block);
        return true;
    }

    case FOR : {
        For *x = (For *)stmt.ptr();
        if (!x->desugared)
            x->desugared = desugarForStatement(x);
        return codegenStatement(x->desugared, env, ctx);
    }

    case FOREIGN_STATEMENT : {
        ForeignStatement *x = (ForeignStatement *)stmt.ptr();
        return codegenStatement(x->statement, x->getEnv(), ctx);
    }

    case TRY : {
        Try *x = (Try *)stmt.ptr();

        llvm::BasicBlock *catchBegin = newBasicBlock("catchBegin");
        llvm::BasicBlock *savedCatchBegin = ctx->catchBlock;
        ctx->catchBlock = catchBegin;
        ctx->tryBlockStackMarker = cgMarkStack();
        
        codegenStatement(x->tryBlock, env, ctx);
        ctx->catchBlock = savedCatchBegin;
        
        llvm::BasicBlock *catchEnd = newBasicBlock("catchEnd");
        llvmBuilder->CreateBr(catchEnd);
        
        llvmBuilder->SetInsertPoint(catchBegin);
        codegenStatement(x->catchBlock, env, ctx);
        llvmBuilder->CreateBr(catchEnd);

        llvmBuilder->SetInsertPoint(catchEnd);
        
        return false;
    }


    default :
        assert(false);
        return false;

    }
}

void codegenCollectLabels(const vector<StatementPtr> &statements,
                          unsigned startIndex,
                          CodegenContextPtr ctx)
{
    for (unsigned i = startIndex; i < statements.size(); ++i) {
        StatementPtr x = statements[i];
        switch (x->stmtKind) {
        case LABEL : {
            Label *y = (Label *)x.ptr();
            map<string, JumpTarget>::iterator li =
                ctx->labels.find(y->name->str);
            if (li != ctx->labels.end())
                error(x, "label redefined");
            llvm::BasicBlock *bb = newBasicBlock(y->name->str.c_str());
            ctx->labels[y->name->str] = JumpTarget(bb, cgMarkStack());
            break;
        }
        case BINDING :
            return;
        default :
            break;
        }
    }
}

EnvPtr codegenBinding(BindingPtr x, EnvPtr env, CodegenContextPtr ctx)
{
    LocationContext loc(x->location);

    switch (x->bindingKind) {

    case VAR : {
        MultiPValuePtr mpv = analyzeMultiValue(x->expr, env);
        if (mpv->size() != x->names.size())
            arityError(x->expr, x->names.size(), mpv->size());
        MultiCValuePtr mcv = new MultiCValue();
        for (unsigned i = 0; i < x->names.size(); ++i) {
            CValuePtr cv = codegenAllocValue(mpv->values[i]->type);
            mcv->values.push_back(cv);
        }
        int marker = cgMarkStack();
        codegenIntoValues(x->expr, env, ctx, mcv);
        cgDestroyAndPopStack(marker, env, ctx);
        EnvPtr env2 = new Env(env);
        for (unsigned i = 0; i < x->names.size(); ++i)
            addLocal(env2, x->names[i], mcv->values[i].ptr());
        return env2;
    }

    case REF : {
        MultiPValuePtr mpv = analyzeMultiValue(x->expr, env);
        if (mpv->size() != x->names.size())
            arityError(x->expr, x->names.size(), mpv->size());
        MultiCValuePtr mcv = new MultiCValue();
        for (unsigned i = 0; i < x->names.size(); ++i) {
            PValuePtr pv = mpv->values[i];
            if (pv->isTemp) {
                CValuePtr cv = codegenAllocValue(pv->type);
                mcv->values.push_back(cv);
            }
            else {
                mcv->values.push_back(NULL);
            }
        }
        int marker = cgMarkStack();
        codegenExpr(x->expr, env, ctx, mcv);
        cgDestroyAndPopStack(marker, env, ctx);
        EnvPtr env2 = new Env(env);
        for (unsigned i = 0; i < x->names.size(); ++i) {
            CValuePtr cv = mcv->values[i];
            assert(cv.ptr());
            addLocal(env2, x->names[i], cv.ptr());
        }
        return env2;
    }

    case STATIC : {
        if (x->names.size() != 1)
            error("static multiple values are not supported");
        ObjectPtr right = evaluateStatic(x->expr, env);
        EnvPtr env2 = new Env(env);
        addLocal(env2, x->names[0], right.ptr());
        return env2;
    }

    default : {
        assert(false);
        return NULL;
    }

    }
}



//
// codegen expressions
//

void codegenIntoValues(ExprPtr expr, EnvPtr env, CodegenContextPtr ctx,
                       MultiCValuePtr out)
{
    MultiPValuePtr mpv = analyzeMultiValue(expr, env);
    if (mpv->size() != out->size())
        arityError(expr, out->size(), mpv->size());
    MultiCValuePtr out2 = new MultiCValue();
    for (unsigned i = 0; i < mpv->size(); ++i) {
        if (mpv->values[i]->isTemp)
            out2->values.push_back(out->values[i]);
        else
            out2->values.push_back(NULL);
    }
    codegenExpr(expr, env, ctx, out2);
    for (unsigned i = 0; i < mpv->size(); ++i) {
        if (!mpv->values[i]->isTemp) {
            assert(out->values[i].ptr());
            assert(out2->values[i].ptr());
            codegenValueCopy(out->values[i], out2->values[i], env, ctx);
        }
    }
}

void codegenIntoOne(ExprPtr expr, EnvPtr env, CodegenContextPtr ctx,
                    CValuePtr out)
{
    codegenIntoValues(expr, env, ctx, new MultiCValue(out));
}

CValuePtr codegenOne(ExprPtr expr, EnvPtr env, CodegenContextPtr ctx,
                     CValuePtr out)
{
    PValuePtr pv = analyzeValue(expr, env);
    if (pv->isTemp) {
        assert(out.ptr());
        assert(out->type == pv->type);
        codegenIntoOne(expr, env, ctx, out);
        return out;
    }
    else {
        assert(!out);
        MultiCValuePtr mcv = new MultiCValue(NULL);
        codegenExpr(expr, env, ctx, mcv);
        assert(mcv->values[0].ptr());
        return mcv->values[0];
    }
}

CValuePtr codegenOneAsRef(ExprPtr expr, EnvPtr env, CodegenContextPtr ctx)
{
    PValuePtr pv = analyzeValue(expr, env);
    CValuePtr out;
    if (pv->isTemp)
        out = codegenAllocValue(pv->type);
    else
        out = NULL;
    return codegenOne(expr, env, ctx, out);
}

void codegenExpr(ExprPtr expr,
                 EnvPtr env,
                 CodegenContextPtr ctx,
                 MultiCValuePtr out)
{
    LocationContext loc(expr->location);

    switch (expr->exprKind) {

    case BOOL_LITERAL :
    case INT_LITERAL :
    case FLOAT_LITERAL : {
        ObjectPtr x = analyze(expr, env);
        assert(x->objKind == VALUE_HOLDER);
        ValueHolder *y = (ValueHolder *)x.ptr();
        codegenValueHolder(y, out);
        break;
    }

    case CHAR_LITERAL : {
        CharLiteral *x = (CharLiteral *)expr.ptr();
        if (!x->desugared)
            x->desugared = desugarCharLiteral(x->value);
        codegenExpr(x->desugared, env, ctx, out);
        break;
    }

    case STRING_LITERAL : {
        StringLiteral *x = (StringLiteral *)expr.ptr();
        llvm::Constant *initializer =
            llvm::ConstantArray::get(llvm::getGlobalContext(),
                                     x->value,
                                     false);
        TypePtr type = arrayType(int8Type, x->value.size());
        llvm::GlobalVariable *gvar = new llvm::GlobalVariable(
            *llvmModule, llvmType(type), true,
            llvm::GlobalVariable::PrivateLinkage,
            initializer, "clayliteral_str");
        CValuePtr cv = new CValue(type, gvar);
        vector<ExprPtr> args;
        args.push_back(new ObjectExpr(cv.ptr()));
        codegenInvoke(kernelName("stringRef"), args, env, ctx, out);
        break;
    }

    case NAME_REF : {
        NameRef *x = (NameRef *)expr.ptr();
        ObjectPtr y = lookupEnv(env, x->name);
        if (y->objKind == EXPRESSION)
            codegenExpr((Expr *)y.ptr(), env, ctx, out);
        else
            codegenStaticObject(y, env, ctx, out);
        break;
    }

    case TUPLE : {
        Tuple *x = (Tuple *)expr.ptr();
        vector<ExprPtr> args2;
        bool expanded = expandVarArgs(x->args, env, args2);
        if (!expanded && (args2.size() == 1))
            codegenExpr(args2[0], env, ctx, out);
        else
            codegenInvoke(primName("tuple"), args2, env, ctx, out);
        break;
    }

    case ARRAY : {
        Array *x = (Array *)expr.ptr();
        vector<ExprPtr> args2;
        expandVarArgs(x->args, env, args2);
        codegenInvoke(primName("array"), args2, env, ctx, out);
        break;
    }

    case INDEXING : {
        Indexing *x = (Indexing *)expr.ptr();
        ObjectPtr y = analyzeOne(x->expr, env);
        assert(y.ptr());
        vector<ExprPtr> args2;
        expandVarArgs(x->args, env, args2);
        if (y->objKind == PVALUE) {
            CValuePtr cv = codegenOneAsRef(x->expr, env, ctx);
            vector<ExprPtr> args3;
            args3.push_back(new ObjectExpr(cv.ptr()));
            args3.insert(args3.end(), args2.begin(), args2.end());
            ObjectPtr op = kernelName("index");
            codegenInvoke(op, args3, env, ctx, out);
        }
        else {
            ObjectPtr obj = analyzeIndexing(y, args2, env);
            codegenStaticObject(obj, env, ctx, out);
        }
        break;
    }

    case CALL : {
        Call *x = (Call *)expr.ptr();
        ObjectPtr y = analyzeOne(x->expr, env);
        assert(y.ptr());
        vector<ExprPtr> args2;
        expandVarArgs(x->args, env, args2);
        if (y->objKind == PVALUE) {
            PValue *z = (PValue *)y.ptr();
            if (z->type->typeKind == STATIC_TYPE) {
                StaticType *t = (StaticType *)z->type.ptr();
                codegenInvoke(t->obj, args2, env, ctx, out);
            }
            else {
                CValuePtr cv = codegenOneAsRef(x->expr, env, ctx);
                codegenInvokeValue(cv, args2, env, ctx, out);
            }
        }
        else {
            codegenInvoke(y, args2, env, ctx, out);
        }
        break;
    }

    case FIELD_REF : {
        FieldRef *x = (FieldRef *)expr.ptr();
        ObjectPtr y = analyzeOne(x->expr, env);
        if (y->objKind == PVALUE) {
            PValue *z = (PValue *)y.ptr();
            if (z->type->typeKind == STATIC_TYPE) {
                StaticType *t = (StaticType *)z->type.ptr();
                ExprPtr inner = new FieldRef(new ObjectExpr(t->obj), x->name);
                codegenExpr(inner, env, ctx, out);
            }
            else {
                CValuePtr cv = codegenOneAsRef(x->expr, env, ctx);
                vector<ExprPtr> args2;
                args2.push_back(new ObjectExpr(cv.ptr()));
                args2.push_back(new ObjectExpr(x->name.ptr()));
                ObjectPtr prim = primName("recordFieldRefByName");
                codegenInvoke(prim, args2, env, ctx, out);
            }
        }
        else if (y->objKind == MODULE_HOLDER) {
            ModuleHolderPtr z = (ModuleHolder *)y.ptr();
            ObjectPtr obj = lookupModuleMember(z, x->name);
            codegenStaticObject(obj, env, ctx, out);
        }
        else {
            error("invalid member access operation");
        }
        break;
    }

    case TUPLE_REF : {
        TupleRef *x = (TupleRef *)expr.ptr();
        vector<ExprPtr> args;
        args.push_back(x->expr);
        args.push_back(new ObjectExpr(sizeTToValueHolder(x->index).ptr()));
        ObjectPtr prim = primName("tupleRef");
        codegenInvoke(prim, args, env, ctx, out);
        break;
    }

    case UNARY_OP : {
        UnaryOp *x = (UnaryOp *)expr.ptr();
        if (!x->desugared)
            x->desugared = desugarUnaryOp(x);
        codegenExpr(x->desugared, env, ctx, out);
        break;
    }

    case BINARY_OP : {
        BinaryOp *x = (BinaryOp *)expr.ptr();
        if (!x->desugared)
            x->desugared = desugarBinaryOp(x);
        codegenExpr(x->desugared, env, ctx, out);
        break;
    }

    case AND : {
        And *x = (And *)expr.ptr();
        PValuePtr pv1 = analyzeValue(x->expr1, env);
        PValuePtr pv2 = analyzeValue(x->expr2, env);
        if (pv1->type != pv2->type)
            error("type mismatch in 'and' expression");
        assert(out->size() == 1);
        llvm::BasicBlock *trueBlock = newBasicBlock("andTrue1");
        llvm::BasicBlock *falseBlock = newBasicBlock("andFalse1");
        llvm::BasicBlock *mergeBlock = newBasicBlock("andMerge");
        if (pv1->isTemp || pv2->isTemp) {
            assert(out->values[0].ptr());
            CValuePtr out0 = out->values[0];

            codegenIntoOne(x->expr1, env, ctx, out0);
            llvm::Value *flag1 = codegenToBoolFlag(out0, env, ctx);
            llvmBuilder->CreateCondBr(flag1, trueBlock, falseBlock);

            llvmBuilder->SetInsertPoint(trueBlock);
            codegenValueDestroy(out0, env, ctx);
            codegenIntoOne(x->expr2, env, ctx, out0);
            llvmBuilder->CreateBr(mergeBlock);

            llvmBuilder->SetInsertPoint(falseBlock);
            llvmBuilder->CreateBr(mergeBlock);

            llvmBuilder->SetInsertPoint(mergeBlock);
        }
        else {
            assert(!out->values[0]);

            CValuePtr ref1 = codegenOneAsRef(x->expr1, env, ctx);
            llvm::Value *flag1 = codegenToBoolFlag(ref1, env, ctx);
            llvmBuilder->CreateCondBr(flag1, trueBlock, falseBlock);

            llvmBuilder->SetInsertPoint(trueBlock);
            CValuePtr ref2 = codegenOneAsRef(x->expr2, env, ctx);
            llvmBuilder->CreateBr(mergeBlock);
            trueBlock = llvmBuilder->GetInsertBlock();

            llvmBuilder->SetInsertPoint(falseBlock);
            llvmBuilder->CreateBr(mergeBlock);

            llvmBuilder->SetInsertPoint(mergeBlock);
            const llvm::Type *ptrType = llvmPointerType(pv1->type);
            llvm::PHINode *phi = llvmBuilder->CreatePHI(ptrType);
            phi->addIncoming(ref1->llValue, falseBlock);
            phi->addIncoming(ref2->llValue, trueBlock);

            out->values[0] = new CValue(pv1->type, phi);
        }
        break;
    }

    case OR : {
        Or *x = (Or *)expr.ptr();
        PValuePtr pv1 = analyzeValue(x->expr1, env);
        PValuePtr pv2 = analyzeValue(x->expr2, env);
        if (pv1->type != pv2->type)
            error("type mismatch in 'or' expression");
        assert(out->size() == 1);
        llvm::BasicBlock *trueBlock = newBasicBlock("orTrue1");
        llvm::BasicBlock *falseBlock = newBasicBlock("orFalse1");
        llvm::BasicBlock *mergeBlock = newBasicBlock("orMerge");
        if (pv1->isTemp || pv2->isTemp) {
            assert(out->values[0].ptr());
            CValuePtr out0 = out->values[0];

            codegenIntoOne(x->expr1, env, ctx, out0);
            llvm::Value *flag1 = codegenToBoolFlag(out0, env, ctx);
            llvmBuilder->CreateCondBr(flag1, trueBlock, falseBlock);

            llvmBuilder->SetInsertPoint(trueBlock);
            llvmBuilder->CreateBr(mergeBlock);

            llvmBuilder->SetInsertPoint(falseBlock);
            codegenValueDestroy(out0, env, ctx);
            codegenIntoOne(x->expr2, env, ctx, out0);
            llvmBuilder->CreateBr(mergeBlock);

            llvmBuilder->SetInsertPoint(mergeBlock);
        }
        else {
            assert(!out->values[0]);

            CValuePtr ref1 = codegenOneAsRef(x->expr1, env, ctx);
            llvm::Value *flag1 = codegenToBoolFlag(ref1, env, ctx);
            llvmBuilder->CreateCondBr(flag1, trueBlock, falseBlock);

            llvmBuilder->SetInsertPoint(trueBlock);
            llvmBuilder->CreateBr(mergeBlock);

            llvmBuilder->SetInsertPoint(falseBlock);
            CValuePtr ref2 = codegenOneAsRef(x->expr2, env, ctx);
            llvmBuilder->CreateBr(mergeBlock);
            falseBlock = llvmBuilder->GetInsertBlock();

            llvmBuilder->SetInsertPoint(mergeBlock);
            const llvm::Type *ptrType = llvmPointerType(pv1->type);
            llvm::PHINode *phi = llvmBuilder->CreatePHI(ptrType);
            phi->addIncoming(ref1->llValue, trueBlock);
            phi->addIncoming(ref2->llValue, falseBlock);

            out->values[0] = new CValue(pv1->type, phi);
        }
        break;
    }

    case LAMBDA : {
        Lambda *x = (Lambda *)expr.ptr();
        if (!x->initialized)
            initializeLambda(x, env);
        codegenExpr(x->converted, env, ctx, out);
        break;
    }

    case VAR_ARGS_REF :
        error("invalid use of '...'");
        break;

    case NEW : {
        New *x = (New *)expr.ptr();
        if (!x->desugared)
            x->desugared = desugarNew(x);
        codegenExpr(x->desugared, env, ctx, out);
        break;
    }

    case STATIC_EXPR : {
        StaticExpr *x = (StaticExpr *)expr.ptr();
        if (!x->desugared)
            x->desugared = desugarStaticExpr(x);
        codegenExpr(x->desugared, env, ctx, out);
        break;
    }

    case FOREIGN_EXPR : {
        ForeignExpr *x = (ForeignExpr *)expr.ptr();
        codegenExpr(x->expr, x->getEnv(), ctx, out);
        break;
    }

    case OBJECT_EXPR : {
        ObjectExpr *x = (ObjectExpr *)expr.ptr();
        codegenStaticObject(x->obj, env, ctx, out);
        break;
    }

    default :
        assert(false);
        break;

    }
}



//
// codegenStaticObject
//

void codegenStaticObject(ObjectPtr x,
                         EnvPtr env,
                         CodegenContextPtr ctx,
                         MultiCValuePtr out)
{
    switch (x->objKind) {
    case ENUM_MEMBER : {
        EnumMember *y = (EnumMember *)x.ptr();
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0.ptr());
        assert(out0->type == y->type);
        llvm::Value *llv = llvm::ConstantInt::getSigned(
            llvmType(y->type), y->index);
        llvmBuilder->CreateStore(llv, out0->llValue);
        break;
    }
    case GLOBAL_VARIABLE : {
        GlobalVariable *y = (GlobalVariable *)x.ptr();
        if (!y->llGlobal)
            codegenGlobalVariable(y);
        assert(out->size() == 1);
        assert(!out->values[0]);
        out->values[0] = new CValue(y->type, y->llGlobal);
        break;
    }
    case EXTERNAL_VARIABLE : {
        ExternalVariable *y = (ExternalVariable *)x.ptr();
        if (!y->llGlobal)
            codegenExternalVariable(y);
        assert(out->size() == 1);
        assert(!out->values[0]);
        out->values[0] = new CValue(y->type2, y->llGlobal);
        break;
    }
    case EXTERNAL_PROCEDURE : {
        ExternalProcedure *y = (ExternalProcedure *)x.ptr();
        if (!y->llvmFunc)
            codegenExternal(y);
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0.ptr());
        assert(out0->type == y->ptrType);
        llvmBuilder->CreateStore(y->llvmFunc, out0->llValue);
        break;
    }
    case STATIC_GLOBAL : {
        StaticGlobal *y = (StaticGlobal *)x.ptr();
        codegenExpr(y->expr, y->env, ctx, out);
        break;
    }
    case VALUE_HOLDER : {
        ValueHolder *y = (ValueHolder *)x.ptr();
        codegenValueHolder(y, out);
        break;
    }
    case CVALUE : {
        assert(out->size() == 1);
        assert(!out->values[0]);
        out->values[0] = (CValue *)x.ptr();
        break;
    }
    case TYPE :
    case PRIM_OP :
    case PROCEDURE :
    case RECORD :
    case MODULE_HOLDER :
    case IDENTIFIER : {
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0.ptr());
        assert(out0->type == staticType(x));
        break;
    }
    default :
        error("invalid static object");
        break;
    }
}



//
// codegenGlobalVariable
//

void codegenGlobalVariable(GlobalVariablePtr x)
{
    assert(!x->llGlobal);
    ObjectPtr y = analyzeStaticObject(x.ptr());
    assert(y->objKind == PVALUE);
    PValue *pv = (PValue *)y.ptr();
    llvm::Constant *initializer =
        llvm::Constant::getNullValue(llvmType(pv->type));
    x->llGlobal =
        new llvm::GlobalVariable(
            *llvmModule, llvmType(pv->type), false,
            llvm::GlobalVariable::InternalLinkage,
            initializer, "clay_" + x->name->str);
}



//
// codegenExternalVariable
//

void codegenExternalVariable(ExternalVariablePtr x)
{
    assert(!x->llGlobal);
    if (!x->attributesVerified)
        verifyAttributes(x);
    ObjectPtr y = analyzeStaticObject(x.ptr());
    assert(y->objKind == PVALUE);
    PValue *pv = (PValue *)y.ptr();
    llvm::GlobalVariable::LinkageTypes linkage;
    if (x->attrDLLImport)
        linkage = llvm::GlobalVariable::DLLImportLinkage;
    else if (x->attrDLLExport)
        linkage = llvm::GlobalVariable::DLLExportLinkage;
    else
        linkage = llvm::GlobalVariable::ExternalLinkage;
    x->llGlobal =
        new llvm::GlobalVariable(
            *llvmModule, llvmType(pv->type), false,
            linkage, NULL, x->name->str);
}



//
// codegenExternal
//

void codegenExternal(ExternalProcedurePtr x)
{
    if (x->llvmFunc != NULL)
        return;
    if (!x->analyzed)
        analyzeExternal(x);
    assert(x->analyzed);
    assert(!x->llvmFunc);
    vector<const llvm::Type *> llArgTypes;
    for (unsigned i = 0; i < x->args.size(); ++i)
        llArgTypes.push_back(llvmType(x->args[i]->type2));
    const llvm::Type *llRetType =
        x->returnType2.ptr() ? llvmType(x->returnType2) : llvmVoidType();
    llvm::FunctionType *llFuncType =
        llvm::FunctionType::get(llRetType, llArgTypes, x->hasVarArgs);
    llvm::GlobalVariable::LinkageTypes linkage;
    if (x->attrDLLImport)
        linkage = llvm::GlobalVariable::DLLImportLinkage;
    else if (x->attrDLLExport)
        linkage = llvm::GlobalVariable::DLLExportLinkage;
    else
        linkage = llvm::GlobalVariable::ExternalLinkage;

    string llvmFuncName;
    if (!x->attrAsmLabel.empty()) {
        // '\01' is the llvm marker to specify asm label
        llvmFuncName = "\01" + x->attrAsmLabel;
    }
    else {
        llvmFuncName = x->name->str;
    }

    x->llvmFunc = llvm::Function::Create(llFuncType,
                                         linkage,
                                         llvmFuncName,
                                         llvmModule);
    if (x->attrStdCall)
        x->llvmFunc->setCallingConv(llvm::CallingConv::X86_StdCall);
    else if (x->attrFastCall)
        x->llvmFunc->setCallingConv(llvm::CallingConv::X86_FastCall);

    if (!x->body) return;

    llvm::Function *savedLLVMFunction = llvmFunction;
    llvm::IRBuilder<> *savedLLVMInitBuilder = llvmInitBuilder;
    llvm::IRBuilder<> *savedLLVMBuilder = llvmBuilder;

    llvmFunction = x->llvmFunc;

    llvm::BasicBlock *initBlock = newBasicBlock("init");
    llvm::BasicBlock *codeBlock = newBasicBlock("code");
    llvm::BasicBlock *returnBlock = newBasicBlock("return");

    llvmInitBuilder = new llvm::IRBuilder<>(initBlock);
    llvmBuilder = new llvm::IRBuilder<>(codeBlock);

    EnvPtr env = new Env(x->env);

    llvm::Function::arg_iterator ai = llvmFunction->arg_begin();
    for (unsigned i = 0; i < x->args.size(); ++i, ++ai) {
        ExternalArgPtr arg = x->args[i];
        llvm::Argument *llArg = &(*ai);
        llArg->setName(arg->name->str);
        llvm::Value *llArgVar =
            llvmInitBuilder->CreateAlloca(llvmType(arg->type2));
        llArgVar->setName(arg->name->str);
        llvmBuilder->CreateStore(llArg, llArgVar);
        CValuePtr cvalue = new CValue(arg->type2, llArgVar);
        addLocal(env, arg->name, cvalue.ptr());
    }

    vector<CReturn> returns;
    if (x->returnType2.ptr()) {
        llvm::Value *llRetVal =
            llvmInitBuilder->CreateAlloca(llvmType(x->returnType2));
        CValuePtr cret = new CValue(x->returnType2, llRetVal);
        returns.push_back(CReturn(false, x->returnType2, cret));
    }

    JumpTarget returnTarget(returnBlock, cgMarkStack());
    CodegenContextPtr ctx = new CodegenContext(returns, returnTarget);

    bool terminated = codegenStatement(x->body, env, ctx);
    if (!terminated) {
        cgDestroyStack(returnTarget.stackMarker, env, ctx);
        llvmBuilder->CreateBr(returnBlock);
    }
    cgPopStack(returnTarget.stackMarker);

    llvmInitBuilder->CreateBr(codeBlock);

    llvmBuilder->SetInsertPoint(returnBlock);
    if (!x->returnType2) {
        llvmBuilder->CreateRetVoid();
    }
    else {
        CValuePtr retVal = returns[0].value;
        llvm::Value *v = llvmBuilder->CreateLoad(retVal->llValue);
        llvmBuilder->CreateRet(v);
    }

    delete llvmInitBuilder;
    delete llvmBuilder;

    llvmInitBuilder = savedLLVMInitBuilder;
    llvmBuilder = savedLLVMBuilder;
    llvmFunction = savedLLVMFunction;
}



//
// codegenValueHolder
//

void codegenValueHolder(ValueHolderPtr v, MultiCValuePtr out)
{
    assert(out->size() == 1);
    CValuePtr out0 = out->values[0];
    assert(out0.ptr());
    assert(v->type == out0->type);

    switch (v->type->typeKind) {

    case BOOL_TYPE :
    case INTEGER_TYPE :
    case FLOAT_TYPE : {
        llvm::Value *llv = codegenConstant(v);
        llvmBuilder->CreateStore(llv, out0->llValue);
        break;
    }

    default :
        assert(false);

    }
}



//
// codegenConstant
//

template <typename T>
llvm::Value *
_sintConstant(ValueHolderPtr v)
{
    return llvm::ConstantInt::getSigned(llvmType(v->type), *((T *)v->buf));
}

template <typename T>
llvm::Value *
_uintConstant(ValueHolderPtr v)
{
    return llvm::ConstantInt::get(llvmType(v->type), *((T *)v->buf));
}

llvm::Value *codegenConstant(ValueHolderPtr v) 
{
    llvm::Value *val = NULL;
    switch (v->type->typeKind) {
    case BOOL_TYPE : {
        int bv = (*(bool *)v->buf) ? 1 : 0;
        return llvm::ConstantInt::get(llvmType(boolType), bv);
    }
    case INTEGER_TYPE : {
        IntegerType *t = (IntegerType *)v->type.ptr();
        if (t->isSigned) {
            switch (t->bits) {
            case 8 :
                val = _sintConstant<char>(v);
                break;
            case 16 :
                val = _sintConstant<short>(v);
                break;
            case 32 :
                val = _sintConstant<long>(v);
                break;
            case 64 :
                val = _sintConstant<long long>(v);
                break;
            default :
                assert(false);
            }
        }
        else {
            switch (t->bits) {
            case 8 :
                val = _uintConstant<unsigned char>(v);
                break;
            case 16 :
                val = _uintConstant<unsigned short>(v);
                break;
            case 32 :
                val = _uintConstant<unsigned long>(v);
                break;
            case 64 :
                val = _uintConstant<unsigned long long>(v);
                break;
            default :
                assert(false);
            }
        }
        break;
    }
    case FLOAT_TYPE : {
        FloatType *t = (FloatType *)v->type.ptr();
        switch (t->bits) {
        case 32 :
            val = llvm::ConstantFP::get(llvmType(t), *((float *)v->buf));
            break;
        case 64 :
            val = llvm::ConstantFP::get(llvmType(t), *((double *)v->buf));
            break;
        default :
            assert(false);
        }
        break;
    }
    default :
        assert(false);
    }
    return val;
}



//
// codegenInvokeValue
//

void codegenInvokeValue(CValuePtr x,
                        const vector<ExprPtr> &args,
                        EnvPtr env,
                        CodegenContextPtr ctx,
                        MultiCValuePtr out)
{
    switch (x->type->typeKind) {

    case CODE_POINTER_TYPE : {
        CodePointerType *t = (CodePointerType *)x->type.ptr();
        ensureArity(args, t->argTypes.size());
        assert(out->size() == t->returnTypes.size());

        llvm::Value *llCallable = llvmBuilder->CreateLoad(x->llValue);
        vector<llvm::Value *> llArgs;
        for (unsigned i = 0; i < args.size(); ++i) {
            CValuePtr carg = codegenOneAsRef(args[i], env, ctx);
            if (carg->type != t->argTypes[i])
                error(args[i], "argument type mismatch");
            llArgs.push_back(carg->llValue);
        }
        vector<CValuePtr> refReturns;
        for (unsigned i = 0; i < t->returnTypes.size(); ++i) {
            TypePtr rt = t->returnTypes[i];
            if (t->returnIsRef[i]) {
                assert(!out->values[i]);
                CValuePtr cret = codegenAllocValue(pointerType(rt));
                refReturns.push_back(cret);
                llArgs.push_back(cret->llValue);
            }
            else {
                assert(out->values[i].ptr());
                refReturns.push_back(NULL);
                llArgs.push_back(out->values[i]->llValue);
            }
        }
        createCall(llCallable, llArgs.begin(), llArgs.end(), env, ctx);
        for (unsigned i = 0; i < t->returnTypes.size(); ++i) {
            if (t->returnIsRef[i]) {
                CValuePtr cret = refReturns[i];
                llvm::Value *v = llvmBuilder->CreateLoad(cret->llValue);
                assert(!out->values[i]);
                out->values[i] = new CValue(t->returnTypes[i], v);
            }
        }
        break;
    }

    case CCODE_POINTER_TYPE : {
        CCodePointerType *t = (CCodePointerType *)x->type.ptr();
        ensureArity2(args, t->argTypes.size(), t->hasVarArgs);

        llvm::Value *llCallable = llvmBuilder->CreateLoad(x->llValue);
        vector<llvm::Value *> llArgs;
        for (unsigned i = 0; i < t->argTypes.size(); ++i) {
            CValuePtr carg = codegenOneAsRef(args[i], env, ctx);
            if (carg->type != t->argTypes[i])
                error(args[i], "argument type mismatch");
            llvm::Value *llArg = llvmBuilder->CreateLoad(carg->llValue);
            llArgs.push_back(llArg);
        }
        if (t->hasVarArgs) {
            for (unsigned i = t->argTypes.size(); i < args.size(); ++i) {
                CValuePtr carg = codegenOneAsRef(args[i], env, ctx);
                llvm::Value *llArg = llvmBuilder->CreateLoad(carg->llValue);
                llArgs.push_back(llArg);
            }
        }
        llvm::CallInst *callInst =
            llvmBuilder->CreateCall(llCallable, llArgs.begin(), llArgs.end());
        switch (t->callingConv) {
        case CC_DEFAULT :
            break;
        case CC_STDCALL :
            callInst->setCallingConv(llvm::CallingConv::X86_StdCall);
            break;
        case CC_FASTCALL :
            callInst->setCallingConv(llvm::CallingConv::X86_FastCall);
            break;
        }
        llvm::Value *result = callInst;
        if (!t->returnType) {
            assert(out->size() == 0);
        }
        else {
            assert(out->size() == 1);
            CValuePtr out0 = out->values[0];
            assert(out0.ptr());
            assert(t->returnType == out0->type);
            llvmBuilder->CreateStore(result, out0->llValue);
        }
        break;
    }

    default : {
        vector<ExprPtr> args2;
        args2.push_back(new ObjectExpr(x.ptr()));
        args2.insert(args2.end(), args.begin(), args.end());
        codegenInvoke(kernelName("call"), args2, env, ctx, out);
        break;
    }

    }
}



//
// codegenInvoke
//

void codegenInvokeVoid(ObjectPtr x,
                       const vector<ExprPtr> &args,
                       EnvPtr env,
                       CodegenContextPtr ctx)
{
    ObjectPtr y = analyzeInvoke(x, args, env);
    assert(y.ptr());
    if (y->objKind != MULTI_PVALUE)
        error("void return expected");
    MultiPValuePtr mpv = (MultiPValue *)y.ptr();
    if (mpv->size() != 0)
        error("void return expected");
    codegenInvoke(x, args, env, ctx, new MultiCValue());
}

void codegenInvoke(ObjectPtr x,
                   const vector<ExprPtr> &args,
                   EnvPtr env,
                   CodegenContextPtr ctx,
                   MultiCValuePtr out)
{
    switch (x->objKind) {
    case TYPE :
    case RECORD :
    case PROCEDURE :
        codegenInvokeCallable(x, args, env, ctx, out);
        break;
    case PRIM_OP :
        codegenInvokePrimOp((PrimOp *)x.ptr(), args, env, ctx, out);
        break;
    default :
        error("invalid call operation");
    }
}



//
// codegenInvokeCallable, codegenInvokeSpecialCase, codegenCallable
//

void codegenInvokeCallable(ObjectPtr x,
                           const vector<ExprPtr> &args,
                           EnvPtr env,
                           CodegenContextPtr ctx,
                           MultiCValuePtr out)
{
    vector<ObjectPtr> argsKey;
    vector<ValueTempness> argsTempness;
    vector<LocationPtr> argLocations;
    bool result = computeArgsKey(args, env,
                                 argsKey, argsTempness,
                                 argLocations);
    assert(result);
    if (codegenInvokeSpecialCase(x, argsKey))
        return;
    InvokeStackContext invokeStackContext(x, argsKey);
    InvokeEntryPtr entry =
        codegenCallable(x, argsKey, argsTempness, argLocations);
    if (entry->inlined)
        codegenInvokeInlined(entry, args, env, ctx, out);
    else
        codegenInvokeCode(entry, args, env, ctx, out);
}

bool codegenInvokeSpecialCase(ObjectPtr x,
                              const vector<ObjectPtr> &argsKey)
{
    switch (x->objKind) {
    case TYPE : {
        Type *y = (Type *)x.ptr();
        if (isPrimitiveType(y) && argsKey.empty())
            return true;
        break;
    }
    case PROCEDURE : {
        if ((x == kernelName("destroy")) &&
            (argsKey.size() == 1))
        {
            ObjectPtr y = argsKey[0];
            assert(y->objKind == TYPE);
            if (isPrimitiveType((Type *)y.ptr()))
                return true;
        }
        break;
    }
    }
    return false;
}

InvokeEntryPtr codegenCallable(ObjectPtr x,
                               const vector<ObjectPtr> &argsKey,
                               const vector<ValueTempness> &argsTempness,
                               const vector<LocationPtr> &argLocations)
{
    InvokeEntryPtr entry =
        analyzeCallable(x, argsKey, argsTempness, argLocations);
    if (!entry->inlined)
        codegenCodeBody(entry, getCodeName(x));
    return entry;
}



//
// codegenInvokeCode
//

void codegenInvokeCode(InvokeEntryPtr entry,
                       const vector<ExprPtr> &args,
                       EnvPtr env,
                       CodegenContextPtr ctx,
                       MultiCValuePtr out)
{
    vector<llvm::Value *> llArgs;
    for (unsigned i = 0; i < args.size(); ++i) {
        assert(entry->argsKey[i]->objKind == TYPE);
        TypePtr argType = (Type *)entry->argsKey[i].ptr();
        CValuePtr carg = codegenOneAsRef(args[i], env, ctx);
        assert(carg->type == argType);
        llArgs.push_back(carg->llValue);
    }
    assert(out->size() == entry->returnTypes.size());
    vector<CValuePtr> refReturns;
    for (unsigned i = 0; i < entry->returnTypes.size(); ++i) {
        TypePtr rt = entry->returnTypes[i];
        TypePtr t = entry->returnTypes[i];
        if (entry->returnIsRef[i]) {
            assert(!out->values[i]);
            CValuePtr cret = codegenAllocValue(pointerType(rt));
            refReturns.push_back(cret);
            llArgs.push_back(cret->llValue);
        }
        else {
            assert(out->values[i].ptr());
            refReturns.push_back(NULL);
            llArgs.push_back(out->values[i]->llValue);
        }
    }
    createCall(entry->llvmFunc, llArgs.begin(), llArgs.end(), env, ctx);
    for (unsigned i = 0; i < entry->returnTypes.size(); ++i) {
        if (entry->returnIsRef[i]) {
            CValuePtr cret = refReturns[i];
            llvm::Value *v = llvmBuilder->CreateLoad(cret->llValue);
            assert(!out->values[i]);
            out->values[i] = new CValue(entry->returnTypes[i], v);
        }
    }
}



//
// codegenInvokeInlined
//

void codegenInvokeInlined(InvokeEntryPtr entry,
                          const vector<ExprPtr> &args,
                          EnvPtr env,
                          CodegenContextPtr ctx,
                          MultiCValuePtr out)
{
    assert(entry->inlined);

    CodePtr code = entry->code;

    if (entry->hasVarArgs)
        assert(args.size() >= entry->fixedArgNames.size());
    else
        assert(args.size() == entry->fixedArgNames.size());

    EnvPtr bodyEnv = new Env(entry->env);

    for (unsigned i = 0; i < entry->fixedArgNames.size(); ++i) {
        ExprPtr expr = new ForeignExpr(env, args[i]);
        addLocal(bodyEnv, entry->fixedArgNames[i], expr.ptr());
    }

    VarArgsInfoPtr vaInfo = new VarArgsInfo(entry->hasVarArgs);
    if (entry->hasVarArgs) {
        for (unsigned i = entry->fixedArgNames.size(); i < args.size(); ++i) {
            ExprPtr expr = new ForeignExpr(env, args[i]);
            vaInfo->varArgs.push_back(expr);
        }
    }
    addLocal(bodyEnv, new Identifier("%varArgs"), vaInfo.ptr());

    ObjectPtr analysis = analyzeInvokeInlined(entry, args, env);
    assert(analysis.ptr());
    MultiPValuePtr mpv = analysisToMultiPValue(analysis);
    assert(mpv->size() == out->size());

    const vector<ReturnSpecPtr> &returnSpecs = entry->code->returnSpecs;
    if (!returnSpecs.empty()) {
        assert(returnSpecs.size() == mpv->size());
    }

    vector<CReturn> returns;
    for (unsigned i = 0; i < mpv->size(); ++i) {
        PValuePtr pv = mpv->values[i];
        if (pv->isTemp) {
            CValuePtr cv = out->values[i];
            assert(cv.ptr());
            assert(cv->type == pv->type);
            returns.push_back(CReturn(false, pv->type, cv));
            if (!returnSpecs.empty()) {
                ReturnSpecPtr rspec = returnSpecs[i];
                if (rspec.ptr() && rspec->name.ptr()) {
                    addLocal(bodyEnv, rspec->name, cv.ptr());
                }
            }
        }
        else {
            assert(!out->values[i]);
            CValuePtr cv = codegenAllocValue(pointerType(pv->type));
            returns.push_back(CReturn(true, pv->type, cv));
        }
    }

    llvm::BasicBlock *returnBlock = newBasicBlock("return");

    JumpTarget returnTarget(returnBlock, cgMarkStack());
    CodegenContextPtr bodyCtx = new CodegenContext(returns, returnTarget);

    bool terminated = codegenStatement(entry->code->body, bodyEnv, bodyCtx);
    if (!terminated) {
        cgDestroyStack(returnTarget.stackMarker, bodyEnv, bodyCtx);
        llvmBuilder->CreateBr(returnBlock);
    }
    cgPopStack(returnTarget.stackMarker);

    llvmBuilder->SetInsertPoint(returnBlock);

    for (unsigned i = 0; i < mpv->size(); ++i) {
        PValuePtr pv = mpv->values[i];
        if (!pv->isTemp) {
            CValuePtr cv = returns[i].value;
            llvm::Value *v = llvmBuilder->CreateLoad(cv->llValue);
            assert(!out->values[i]);
            out->values[i] = new CValue(pv->type, v);
        }
    }
}



//
// codegenCWrapper
//

void codegenCWrapper(InvokeEntryPtr entry, const string &callableName)
{
    assert(!entry->llvmCWrapper);

    vector<const llvm::Type *> llArgTypes;
    for (unsigned i = 0; i < entry->fixedArgTypes.size(); ++i)
        llArgTypes.push_back(llvmType(entry->fixedArgTypes[i]));
    if (entry->hasVarArgs) {
        for (unsigned i = 0; i < entry->varArgTypes.size(); ++i)
            llArgTypes.push_back(llvmType(entry->varArgTypes[i]));
    }

    TypePtr returnType;
    const llvm::Type *llReturnType = NULL;
    if (entry->returnTypes.empty()) {
        returnType = NULL;
        llReturnType = llvmVoidType();
    }
    else {
        assert(entry->returnTypes.size() == 1);
        assert(!entry->returnIsRef[0]);
        returnType = entry->returnTypes[0];
        llReturnType = llvmType(returnType);
    }

    llvm::FunctionType *llFuncType =
        llvm::FunctionType::get(llReturnType, llArgTypes, false);

    llvm::Function *llCWrapper =
        llvm::Function::Create(llFuncType,
                               llvm::Function::InternalLinkage,
                               "clay_cwrapper_" + callableName,
                               llvmModule);

    entry->llvmCWrapper = llCWrapper;

    llvm::BasicBlock *llBlock =
        llvm::BasicBlock::Create(llvm::getGlobalContext(),
                                 "body",
                                 llCWrapper);
    llvm::IRBuilder<> llBuilder(llBlock);

    vector<llvm::Value *> innerArgs;
    llvm::Function::arg_iterator ai = llCWrapper->arg_begin();
    for (unsigned i = 0; i < llArgTypes.size(); ++i, ++ai) {
        llvm::Argument *llArg = &(*ai);
        llvm::Value *llv = llBuilder.CreateAlloca(llArgTypes[i]);
        llBuilder.CreateStore(llArg, llv);
        innerArgs.push_back(llv);
    }

    if (!returnType) {
        llBuilder.CreateCall(entry->llvmFunc,
                             innerArgs.begin(), innerArgs.end());
        llBuilder.CreateRetVoid();
    }
    else {
        llvm::Value *llRetVal = llBuilder.CreateAlloca(llvmType(returnType));
        innerArgs.push_back(llRetVal);
        llBuilder.CreateCall(entry->llvmFunc,
                             innerArgs.begin(), innerArgs.end());
        llvm::Value *llRet = llBuilder.CreateLoad(llRetVal);
        llBuilder.CreateRet(llRet);
    }
}



//
// codegenInvokePrimOp
//

static llvm::Value *_cgNumeric(ExprPtr expr,
                               EnvPtr env,
                               CodegenContextPtr ctx,
                               TypePtr &type)
{
    CValuePtr cv = codegenOneAsRef(expr, env, ctx);
    if (type.ptr()) {
        if (cv->type != type)
            error(expr, "argument type mismatch");
    }
    else {
        switch (cv->type->typeKind) {
        case INTEGER_TYPE :
        case FLOAT_TYPE :
            break;
        default :
            error(expr, "expecting numeric type");
        }
        type = cv->type;
    }
    return llvmBuilder->CreateLoad(cv->llValue);
}

static llvm::Value *_cgInteger(ExprPtr expr,
                               EnvPtr env,
                               CodegenContextPtr ctx,
                               TypePtr &type)
{
    CValuePtr cv = codegenOneAsRef(expr, env, ctx);
    if (type.ptr()) {
        if (cv->type != type)
            error(expr, "argument type mismatch");
    }
    else {
        if (cv->type->typeKind != INTEGER_TYPE)
            error(expr, "expecting integer type");
        type = cv->type;
    }
    return llvmBuilder->CreateLoad(cv->llValue);
}

static llvm::Value *_cgPointer(ExprPtr expr,
                               EnvPtr env,
                               CodegenContextPtr ctx,
                               PointerTypePtr &type)
{
    CValuePtr cv = codegenOneAsRef(expr, env, ctx);
    if (type.ptr()) {
        if (cv->type != (Type *)type.ptr())
            error(expr, "argument type mismatch");
    }
    else {
        if (cv->type->typeKind != POINTER_TYPE)
            error(expr, "expecting pointer type");
        type = (PointerType *)cv->type.ptr();
    }
    return llvmBuilder->CreateLoad(cv->llValue);
}

static llvm::Value *_cgPointerLike(ExprPtr expr,
                                   EnvPtr env,
                                   CodegenContextPtr ctx,
                                   TypePtr &type)
{
    CValuePtr cv = codegenOneAsRef(expr, env, ctx);
    if (type.ptr()) {
        if (cv->type != type)
            error(expr, "argument type mismatch");
    }
    else {
        switch (cv->type->typeKind) {
        case POINTER_TYPE :
        case CODE_POINTER_TYPE :
        case CCODE_POINTER_TYPE :
            break;
        default :
            error(expr, "expecting a pointer or a code pointer");
        }
        type = cv->type;
    }
    return llvmBuilder->CreateLoad(cv->llValue);
}

void codegenInvokePrimOp(PrimOpPtr x,
                         const vector<ExprPtr> &args,
                         EnvPtr env,
                         CodegenContextPtr ctx,
                         MultiCValuePtr out)
{
    switch (x->primOpCode) {

    case PRIM_Type : {
        ensureArity(args, 1);
        PValuePtr y = analyzeValue(args[0], env);
        codegenStaticObject(y->type.ptr(), env, ctx, out);
        break;
    }

    case PRIM_TypeP : {
        ensureArity(args, 1);
        ObjectPtr y = evaluateStatic(args[0], env);
        ObjectPtr obj = boolToValueHolder(y->objKind == TYPE).ptr();
        codegenStaticObject(obj, env, ctx, out);
        break;
    }

    case PRIM_TypeSize : {
        ensureArity(args, 1);
        TypePtr t = evaluateType(args[0], env);
        ObjectPtr obj = sizeTToValueHolder(typeSize(t)).ptr();
        codegenStaticObject(obj, env, ctx, out);
        break;
    }

    case PRIM_primitiveCopy : {
        ensureArity(args, 2);
        CValuePtr cv0 = codegenOneAsRef(args[0], env, ctx);
        CValuePtr cv1 = codegenOneAsRef(args[1], env, ctx);
        if (!isPrimitiveType(cv0->type))
            error(args[0], "expecting primitive type");
        if (cv0->type != cv1->type)
            error(args[1], "argument type mismatch");
        llvm::Value *v = llvmBuilder->CreateLoad(cv1->llValue);
        llvmBuilder->CreateStore(v, cv0->llValue);
        assert(out->size() == 0);
        break;
    }

    case PRIM_boolNot : {
        ensureArity(args, 1);
        CValuePtr cv = codegenOneAsRef(args[0], env, ctx);
        if (cv->type != boolType)
            error(args[0], "expecting bool type");
        llvm::Value *v = llvmBuilder->CreateLoad(cv->llValue);
        llvm::Value *zero = llvm::ConstantInt::get(llvmType(boolType), 0);
        llvm::Value *flag = llvmBuilder->CreateICmpEQ(v, zero);
        llvm::Value *v2 = llvmBuilder->CreateZExt(flag, llvmType(boolType));
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0.ptr());
        assert(out0->type == boolType);
        llvmBuilder->CreateStore(v2, out0->llValue);
        break;
    }

    case PRIM_numericEqualsP : {
        ensureArity(args, 2);
        TypePtr t;
        llvm::Value *v0 = _cgNumeric(args[0], env, ctx, t);
        llvm::Value *v1 = _cgNumeric(args[1], env, ctx, t);
        llvm::Value *flag;
        switch (t->typeKind) {
        case INTEGER_TYPE :
            flag = llvmBuilder->CreateICmpEQ(v0, v1);
            break;
        case FLOAT_TYPE :
            flag = llvmBuilder->CreateFCmpUEQ(v0, v1);
            break;
        default :
            assert(false);
        }
        llvm::Value *result =
            llvmBuilder->CreateZExt(flag, llvmType(boolType));
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0.ptr());
        assert(out0->type == boolType);
        llvmBuilder->CreateStore(result, out0->llValue);
        break;
    }

    case PRIM_numericLesserP : {
        ensureArity(args, 2);
        TypePtr t;
        llvm::Value *v0 = _cgNumeric(args[0], env, ctx, t);
        llvm::Value *v1 = _cgNumeric(args[1], env, ctx, t);
        llvm::Value *flag;
        switch (t->typeKind) {
        case INTEGER_TYPE : {
            IntegerType *it = (IntegerType *)t.ptr();
            if (it->isSigned)
                flag = llvmBuilder->CreateICmpSLT(v0, v1);
            else
                flag = llvmBuilder->CreateICmpULT(v0, v1);
            break;
        }
        case FLOAT_TYPE :
            flag = llvmBuilder->CreateFCmpULT(v0, v1);
            break;
        default :
            assert(false);
        }
        llvm::Value *result =
            llvmBuilder->CreateZExt(flag, llvmType(boolType));
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0.ptr());
        assert(out0->type == boolType);
        llvmBuilder->CreateStore(result, out0->llValue);
        break;
    }

    case PRIM_numericAdd : {
        ensureArity(args, 2);
        TypePtr t;
        llvm::Value *v0 = _cgNumeric(args[0], env, ctx, t);
        llvm::Value *v1 = _cgNumeric(args[1], env, ctx, t);
        llvm::Value *result;
        switch (t->typeKind) {
        case INTEGER_TYPE :
            result = llvmBuilder->CreateAdd(v0, v1);
            break;
        case FLOAT_TYPE :
            result = llvmBuilder->CreateFAdd(v0, v1);
            break;
        default :
            assert(false);
        }
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0.ptr());
        assert(out0->type == t);
        llvmBuilder->CreateStore(result, out0->llValue);
        break;
    }

    case PRIM_numericSubtract : {
        ensureArity(args, 2);
        TypePtr t;
        llvm::Value *v0 = _cgNumeric(args[0], env, ctx, t);
        llvm::Value *v1 = _cgNumeric(args[1], env, ctx, t);
        llvm::Value *result;
        switch (t->typeKind) {
        case INTEGER_TYPE :
            result = llvmBuilder->CreateSub(v0, v1);
            break;
        case FLOAT_TYPE :
            result = llvmBuilder->CreateFSub(v0, v1);
            break;
        default :
            assert(false);
        }
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0.ptr());
        assert(out0->type == t);
        llvmBuilder->CreateStore(result, out0->llValue);
        break;
    }

    case PRIM_numericMultiply : {
        ensureArity(args, 2);
        TypePtr t;
        llvm::Value *v0 = _cgNumeric(args[0], env, ctx, t);
        llvm::Value *v1 = _cgNumeric(args[1], env, ctx, t);
        llvm::Value *result;
        switch (t->typeKind) {
        case INTEGER_TYPE :
            result = llvmBuilder->CreateMul(v0, v1);
            break;
        case FLOAT_TYPE :
            result = llvmBuilder->CreateFMul(v0, v1);
            break;
        default :
            assert(false);
        }
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0.ptr());
        assert(out0->type == t);
        llvmBuilder->CreateStore(result, out0->llValue);
        break;
    }

    case PRIM_numericDivide : {
        ensureArity(args, 2);
        TypePtr t;
        llvm::Value *v0 = _cgNumeric(args[0], env, ctx, t);
        llvm::Value *v1 = _cgNumeric(args[1], env, ctx, t);
        llvm::Value *result;
        switch (t->typeKind) {
        case INTEGER_TYPE : {
            IntegerType *it = (IntegerType *)t.ptr();
            if (it->isSigned)
                result = llvmBuilder->CreateSDiv(v0, v1);
            else
                result = llvmBuilder->CreateUDiv(v0, v1);
            break;
        }
        case FLOAT_TYPE :
            result = llvmBuilder->CreateFDiv(v0, v1);
            break;
        default :
            assert(false);
        }
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0.ptr());
        assert(out0->type == t);
        llvmBuilder->CreateStore(result, out0->llValue);
        break;
    }

    case PRIM_numericNegate : {
        ensureArity(args, 1);
        TypePtr t;
        llvm::Value *v = _cgNumeric(args[0], env, ctx, t);
        llvm::Value *result;
        switch (t->typeKind) {
        case INTEGER_TYPE :
            result = llvmBuilder->CreateNeg(v);
            break;
        case FLOAT_TYPE :
            result = llvmBuilder->CreateFNeg(v);
            break;
        default :
            assert(false);
        }
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0.ptr());
        assert(out0->type == t);
        llvmBuilder->CreateStore(result, out0->llValue);
        break;
    }

    case PRIM_integerRemainder : {
        ensureArity(args, 2);
        TypePtr t;
        llvm::Value *v0 = _cgInteger(args[0], env, ctx, t);
        llvm::Value *v1 = _cgInteger(args[1], env, ctx, t);
        llvm::Value *result;
        IntegerType *it = (IntegerType *)t.ptr();
        if (it->isSigned)
            result = llvmBuilder->CreateSRem(v0, v1);
        else
            result = llvmBuilder->CreateURem(v0, v1);
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0.ptr());
        assert(out0->type == t);
        llvmBuilder->CreateStore(result, out0->llValue);
        break;
    }

    case PRIM_integerShiftLeft : {
        ensureArity(args, 2);
        TypePtr t;
        llvm::Value *v0 = _cgInteger(args[0], env, ctx, t);
        llvm::Value *v1 = _cgInteger(args[1], env, ctx, t);
        llvm::Value *result = llvmBuilder->CreateShl(v0, v1);
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0.ptr());
        assert(out0->type == t);
        llvmBuilder->CreateStore(result, out0->llValue);
        break;
    }

    case PRIM_integerShiftRight : {
        ensureArity(args, 2);
        TypePtr t;
        llvm::Value *v0 = _cgInteger(args[0], env, ctx, t);
        llvm::Value *v1 = _cgInteger(args[1], env, ctx, t);
        llvm::Value *result;
        IntegerType *it = (IntegerType *)t.ptr();
        if (it->isSigned)
            result = llvmBuilder->CreateAShr(v0, v1);
        else
            result = llvmBuilder->CreateLShr(v0, v1);
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0.ptr());
        assert(out0->type == t);
        llvmBuilder->CreateStore(result, out0->llValue);
        break;
    }

    case PRIM_integerBitwiseAnd : {
        ensureArity(args, 2);
        TypePtr t;
        llvm::Value *v0 = _cgInteger(args[0], env, ctx, t);
        llvm::Value *v1 = _cgInteger(args[1], env, ctx, t);
        llvm::Value *result = llvmBuilder->CreateAnd(v0, v1);
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0.ptr());
        assert(out0->type == t);
        llvmBuilder->CreateStore(result, out0->llValue);
        break;
    }

    case PRIM_integerBitwiseOr : {
        ensureArity(args, 2);
        TypePtr t;
        llvm::Value *v0 = _cgInteger(args[0], env, ctx, t);
        llvm::Value *v1 = _cgInteger(args[1], env, ctx, t);
        llvm::Value *result = llvmBuilder->CreateOr(v0, v1);
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0.ptr());
        assert(out0->type == t);
        llvmBuilder->CreateStore(result, out0->llValue);
        break;
    }

    case PRIM_integerBitwiseXor : {
        ensureArity(args, 2);
        TypePtr t;
        llvm::Value *v0 = _cgInteger(args[0], env, ctx, t);
        llvm::Value *v1 = _cgInteger(args[1], env, ctx, t);
        llvm::Value *result = llvmBuilder->CreateXor(v0, v1);
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0.ptr());
        assert(out0->type == t);
        llvmBuilder->CreateStore(result, out0->llValue);
        break;
    }

    case PRIM_integerBitwiseNot : {
        ensureArity(args, 1);
        TypePtr t;
        llvm::Value *v0 = _cgInteger(args[0], env, ctx, t);
        llvm::Value *result = llvmBuilder->CreateNot(v0);
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0.ptr());
        assert(out0->type == t);
        llvmBuilder->CreateStore(result, out0->llValue);
        break;
    }

    case PRIM_numericConvert : {
        ensureArity(args, 2);
        TypePtr dest = evaluateType(args[0], env);
        TypePtr src;
        llvm::Value *v = _cgNumeric(args[1], env, ctx, src);
        if (src == dest) {
            assert(out->size() == 1);
            CValuePtr out0 = out->values[0];
            assert(out0.ptr());
            assert(out0->type == dest);
            llvmBuilder->CreateStore(v, out0->llValue);
        }
        else {
            llvm::Value *result;
            switch (dest->typeKind) {
            case INTEGER_TYPE : {
                IntegerType *dest2 = (IntegerType *)dest.ptr();
                if (src->typeKind == INTEGER_TYPE) {
                    IntegerType *src2 = (IntegerType *)src.ptr();
                    if (dest2->bits < src2->bits)
                        result = llvmBuilder->CreateTrunc(v, llvmType(dest));
                    else if (src2->isSigned)
                        result = llvmBuilder->CreateSExt(v, llvmType(dest));
                    else
                        result = llvmBuilder->CreateZExt(v, llvmType(dest));
                }
                else if (src->typeKind == FLOAT_TYPE) {
                    if (dest2->isSigned)
                        result = llvmBuilder->CreateFPToSI(v, llvmType(dest));
                    else
                        result = llvmBuilder->CreateFPToUI(v, llvmType(dest));
                }
                else {
                    error(args[1], "expecting numeric type");
                    result = NULL;
                }
                break;
            }
            case FLOAT_TYPE : {
                FloatType *dest2 = (FloatType *)dest.ptr();
                if (src->typeKind == INTEGER_TYPE) {
                    IntegerType *src2 = (IntegerType *)src.ptr();
                    if (src2->isSigned)
                        result = llvmBuilder->CreateSIToFP(v, llvmType(dest));
                    else
                        result = llvmBuilder->CreateUIToFP(v, llvmType(dest));
                }
                else if (src->typeKind == FLOAT_TYPE) {
                    FloatType *src2 = (FloatType *)src.ptr();
                    if (dest2->bits < src2->bits)
                        result = llvmBuilder->CreateFPTrunc(v, llvmType(dest));
                    else
                        result = llvmBuilder->CreateFPExt(v, llvmType(dest));
                }
                else {
                    error(args[1], "expecting numeric type");
                    result = NULL;
                }
                break;
            }
            default :
                error(args[0], "expecting numeric type");
                result = NULL;
                break;
            }
            assert(out->size() == 1);
            CValuePtr out0 = out->values[0];
            assert(out0.ptr());
            assert(out0->type == dest);
            llvmBuilder->CreateStore(result, out0->llValue);
        }
        break;
    }

    case PRIM_Pointer :
        error("Pointer type constructor cannot be called");
        break;

    case PRIM_addressOf : {
        ensureArity(args, 1);
        PValuePtr pv = analyzeValue(args[0], env);
        if (pv->isTemp)
            error("cannot take address of a temporary");
        CValuePtr cv = codegenOneAsRef(args[0], env, ctx);
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0.ptr());
        assert(out0->type == pointerType(cv->type));
        llvmBuilder->CreateStore(cv->llValue, out0->llValue);
        break;
    }

    case PRIM_pointerDereference : {
        ensureArity(args, 1);
        PointerTypePtr t;
        llvm::Value *v = _cgPointer(args[0], env, ctx, t);
        assert(out->size() == 1);
        assert(!out->values[0]);
        out->values[0] = new CValue(t->pointeeType, v);
        break;
    }

    case PRIM_pointerEqualsP : {
        ensureArity(args, 2);
        PointerTypePtr t;
        llvm::Value *v0 = _cgPointer(args[0], env, ctx, t);
        llvm::Value *v1 = _cgPointer(args[1], env, ctx, t);
        llvm::Value *flag = llvmBuilder->CreateICmpEQ(v0, v1);
        llvm::Value *result = llvmBuilder->CreateZExt(flag, llvmType(boolType));
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0.ptr());
        assert(out0->type == boolType);
        llvmBuilder->CreateStore(result, out0->llValue);
        break;
    }

    case PRIM_pointerLesserP : {
        ensureArity(args, 2);
        PointerTypePtr t;
        llvm::Value *v0 = _cgPointer(args[0], env, ctx, t);
        llvm::Value *v1 = _cgPointer(args[1], env, ctx, t);
        llvm::Value *flag = llvmBuilder->CreateICmpULT(v0, v1);
        llvm::Value *result = llvmBuilder->CreateZExt(flag, llvmType(boolType));
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0.ptr());
        assert(out0->type == boolType);
        llvmBuilder->CreateStore(result, out0->llValue);
        break;
    }

    case PRIM_pointerOffset : {
        ensureArity(args, 2);
        PointerTypePtr t;
        llvm::Value *v0 = _cgPointer(args[0], env, ctx, t);
        TypePtr offsetT;
        llvm::Value *v1 = _cgInteger(args[1], env, ctx, offsetT);
        vector<llvm::Value *> indices;
        indices.push_back(v1);
        llvm::Value *result = llvmBuilder->CreateGEP(v0,
                                                     indices.begin(),
                                                     indices.end());
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0.ptr());
        assert(out0->type == t.ptr());
        llvmBuilder->CreateStore(result, out0->llValue);
        break;
    }

    case PRIM_pointerToInt : {
        ensureArity(args, 2);
        TypePtr dest = evaluateType(args[0], env);
        if (dest->typeKind != INTEGER_TYPE)
            error(args[0], "invalid integer type");
        PointerTypePtr pt;
        llvm::Value *v = _cgPointer(args[1], env, ctx, pt);
        llvm::Value *result = llvmBuilder->CreatePtrToInt(v, llvmType(dest));
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0.ptr());
        assert(out0->type == dest);
        llvmBuilder->CreateStore(result, out0->llValue);
        break;
    }

    case PRIM_intToPointer : {
        ensureArity(args, 2);
        TypePtr pointeeType = evaluateType(args[0], env);
        TypePtr dest = pointerType(pointeeType);
        TypePtr t;
        llvm::Value *v = _cgInteger(args[1], env, ctx, t);
        llvm::Value *result = llvmBuilder->CreateIntToPtr(v, llvmType(dest));
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0.ptr());
        assert(out0->type == dest);
        llvmBuilder->CreateStore(result, out0->llValue);
        break;
    }

    case PRIM_CodePointerP : {
        ensureArity(args, 1);
        ObjectPtr y = evaluateStatic(args[0], env);
        bool isCPType = false;
        if (y->objKind == TYPE) {
            Type *t = (Type *)y.ptr();
            if (t->typeKind == CODE_POINTER_TYPE)
                isCPType = true;
        }
        ObjectPtr obj = boolToValueHolder(isCPType).ptr();
        codegenStaticObject(obj, env, ctx, out);
        break;
    }

    case PRIM_CodePointer :
        error("CodePointer type constructor cannot be called");

    case PRIM_RefCodePointer :
        error("RefCodePointer type constructor cannot be called");

    case PRIM_makeCodePointer : {
        if (args.size() < 1)
            error("incorrect number of arguments");
        ObjectPtr callable = evaluateStatic(args[0], env);
        switch (callable->objKind) {
        case TYPE :
        case RECORD :
        case PROCEDURE :
            break;
        default :
            error(args[0], "invalid callable");
        }
        vector<ObjectPtr> argsKey;
        vector<ValueTempness> argsTempness;
        vector<LocationPtr> argLocations;
        for (unsigned i = 1; i < args.size(); ++i) {
            TypePtr t = evaluateType(args[i], env);
            argsKey.push_back(t.ptr());
            argsTempness.push_back(LVALUE);
            argLocations.push_back(args[i]->location);
        }

        InvokeStackContext invokeStackContext(callable, argsKey);

        InvokeEntryPtr entry =
            codegenCallable(callable, argsKey, argsTempness, argLocations);
        if (entry->inlined)
            error(args[0], "cannot create pointer to inlined code");
        vector<TypePtr> argTypes = entry->fixedArgTypes;
        if (entry->hasVarArgs) {
            argTypes.insert(argTypes.end(),
                            entry->varArgTypes.begin(),
                            entry->varArgTypes.end());
        }
        TypePtr cpType = codePointerType(argTypes,
                                         entry->returnIsRef,
                                         entry->returnTypes);
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0.ptr());
        assert(out0->type == cpType);
        llvmBuilder->CreateStore(entry->llvmFunc, out0->llValue);
        break;
    }

    case PRIM_CCodePointerP : {
        ensureArity(args, 1);
        ObjectPtr y = evaluateStatic(args[0], env);
        bool isCCPType = false;
        if (y->objKind == TYPE) {
            Type *t = (Type *)y.ptr();
            if (t->typeKind == CCODE_POINTER_TYPE)
                isCCPType = true;
        }
        ObjectPtr obj = boolToValueHolder(isCCPType).ptr();
        codegenStaticObject(obj, env, ctx, out);
        break;
    }

    case PRIM_CCodePointer :
        error("CCodePointer type constructor cannot be called");

    case PRIM_StdCallCodePointer :
        error("StdCallCodePointer type constructor cannot be called");

    case PRIM_FastCallCodePointer :
        error("FastCallCodePointer type constructor cannot be called");

    case PRIM_makeCCodePointer : {
        if (args.size() < 1)
            error("incorrect number of arguments");
        ObjectPtr callable = evaluateStatic(args[0], env);
        switch (callable->objKind) {
        case TYPE :
        case RECORD :
        case PROCEDURE :
            break;
        default :
            error(args[0], "invalid callable");
        }
        vector<ObjectPtr> argsKey;
        vector<ValueTempness> argsTempness;
        vector<LocationPtr> argLocations;
        for (unsigned i = 1; i < args.size(); ++i) {
            TypePtr t = evaluateType(args[i], env);
            argsKey.push_back(t.ptr());
            argsTempness.push_back(LVALUE);
            argLocations.push_back(args[i]->location);
        }

        InvokeStackContext invokeStackContext(callable, argsKey);

        InvokeEntryPtr entry =
            codegenCallable(callable, argsKey, argsTempness, argLocations);
        if (entry->inlined)
            error(args[0], "cannot create pointer to inlined code");
        vector<TypePtr> argTypes = entry->fixedArgTypes;
        if (entry->hasVarArgs) {
            argTypes.insert(argTypes.end(),
                            entry->varArgTypes.begin(),
                            entry->varArgTypes.end());
        }
        TypePtr returnType;
        if (entry->returnTypes.size() == 0) {
            returnType = NULL;
        }
        else if (entry->returnTypes.size() == 1) {
            if (entry->returnIsRef[0])
                error(args[0], "cannot create C compatible pointer "
                      "to return-by-reference code");
            returnType = entry->returnTypes[0];
        }
        else {
            error(args[0], "cannot create C compatible pointer "
                  "to multi-return code");
        }
        TypePtr ccpType = cCodePointerType(CC_DEFAULT,
                                           argTypes,
                                           false,
                                           returnType);
        string callableName = getCodeName(callable);
        if (!entry->llvmCWrapper)
            codegenCWrapper(entry, callableName);
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0.ptr());
        assert(out0->type == ccpType);
        llvmBuilder->CreateStore(entry->llvmCWrapper, out0->llValue);
        break;
    }

    case PRIM_pointerCast : {
        ensureArity(args, 2);
        TypePtr dest = evaluatePointerLikeType(args[0], env);
        TypePtr t;
        llvm::Value *v = _cgPointerLike(args[1], env, ctx, t);
        llvm::Value *result = llvmBuilder->CreateBitCast(v, llvmType(dest));
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0.ptr());
        assert(out0->type == dest);
        llvmBuilder->CreateStore(result, out0->llValue);
        break;
    }

    case PRIM_Array :
        error("Array type constructor cannot be called");

    case PRIM_array : {
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0.ptr());
        assert(out0->type->typeKind == ARRAY_TYPE);
        ArrayType *t = (ArrayType *)out0->type.ptr();
        assert((int)args.size() == t->size);
        TypePtr etype = t->elementType;
        for (unsigned i = 0; i < args.size(); ++i) {
            PValuePtr parg = analyzeValue(args[i], env);
            if (parg->type != etype)
                error(args[i], "array element type mismatch");
            llvm::Value *ptr =
                llvmBuilder->CreateConstGEP2_32(out0->llValue, 0, i);
            CValuePtr carg = new CValue(etype, ptr);
            codegenIntoOne(args[i], env, ctx, carg);
        }
        break;
    }

    case PRIM_arrayRef : {
        ensureArity(args, 2);
        CValuePtr carray = codegenOneAsRef(args[0], env, ctx);
        if (carray->type->typeKind != ARRAY_TYPE)
            error(args[0], "expecting array type");
        ArrayType *at = (ArrayType *)carray->type.ptr();
        TypePtr indexType;
        llvm::Value *i = _cgInteger(args[1], env, ctx, indexType);
        vector<llvm::Value *> indices;
        indices.push_back(llvm::ConstantInt::get(llvmIntType(32), 0));
        indices.push_back(i);
        llvm::Value *ptr =
            llvmBuilder->CreateGEP(carray->llValue,
                                   indices.begin(),
                                   indices.end());
        assert(out->size() == 1);
        assert(!out->values[0]);
        out->values[0] = new CValue(at->elementType, ptr);
        break;
    }

    case PRIM_TupleP : {
        ensureArity(args, 1);
        ObjectPtr y = evaluateStatic(args[0], env);
        bool isTupleType = false;
        if (y->objKind == TYPE) {
            Type *t = (Type *)y.ptr();
            if (t->typeKind == TUPLE_TYPE)
                isTupleType = true;
        }
        ObjectPtr obj = boolToValueHolder(isTupleType).ptr();
        codegenStaticObject(obj, env, ctx, out);
        break;
    }

    case PRIM_Tuple :
        error("Tuple type constructor cannot be called");

    case PRIM_TupleElementCount : {
        ensureArity(args, 1);
        TypePtr y = evaluateTupleType(args[0], env);
        TupleType *z = (TupleType *)y.ptr();
        ObjectPtr obj = sizeTToValueHolder(z->elementTypes.size()).ptr();
        codegenStaticObject(obj, env, ctx, out);
        break;
    }

    case PRIM_TupleElementOffset : {
        ensureArity(args, 2);
        TypePtr y = evaluateTupleType(args[0], env);
        TupleType *z = (TupleType *)y.ptr();
        size_t i = evaluateSizeT(args[1], env);
        const llvm::StructLayout *layout = tupleTypeLayout(z);
        ObjectPtr obj =  sizeTToValueHolder(layout->getElementOffset(i)).ptr();
        codegenStaticObject(obj, env, ctx, out);
        break;
    }

    case PRIM_tuple : {
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0.ptr());
        assert(out0->type->typeKind == TUPLE_TYPE);
        TupleType *tt = (TupleType *)out0->type.ptr();
        assert(args.size() == tt->elementTypes.size());
        for (unsigned i = 0; i < args.size(); ++i) {
            PValuePtr parg = analyzeValue(args[i], env);
            if (parg->type != tt->elementTypes[i])
                error(args[i], "argument type mismatch");
            llvm::Value *ptr =
                llvmBuilder->CreateConstGEP2_32(out0->llValue, 0, i);
            CValuePtr cargDest = new CValue(tt->elementTypes[i], ptr);
            codegenIntoOne(args[i], env, ctx, cargDest);
        }
        break;
    }

    case PRIM_tupleRef : {
        ensureArity(args, 2);
        CValuePtr ctuple = codegenOneAsRef(args[0], env, ctx);
        if (ctuple->type->typeKind != TUPLE_TYPE)
            error(args[0], "expecting a tuple");
        TupleType *tt = (TupleType *)ctuple->type.ptr();
        size_t i = evaluateSizeT(args[1], env);
        if (i >= tt->elementTypes.size())
            error(args[1], "tuple index out of range");
        llvm::Value *ptr =
            llvmBuilder->CreateConstGEP2_32(ctuple->llValue, 0, i);
        assert(out->size() == 1);
        assert(!out->values[0]);
        out->values[0] = new CValue(tt->elementTypes[i], ptr);
        break;
    }

    case PRIM_RecordP : {
        ensureArity(args, 1);
        ObjectPtr y = evaluateStatic(args[0], env);
        bool isRecordType = false;
        if (y->objKind == TYPE) {
            Type *t = (Type *)y.ptr();
            if (t->typeKind == RECORD_TYPE)
                isRecordType = true;
        }
        ObjectPtr obj = boolToValueHolder(isRecordType).ptr();
        codegenStaticObject(obj, env, ctx, out);
        break;
    }

    case PRIM_RecordFieldCount : {
        ensureArity(args, 1);
        TypePtr y = evaluateRecordType(args[0], env);
        RecordType *z = (RecordType *)y.ptr();
        const vector<TypePtr> &fieldTypes = recordFieldTypes(z);
        ObjectPtr obj = sizeTToValueHolder(fieldTypes.size()).ptr();
        codegenStaticObject(obj, env, ctx, out);
        break;
    }

    case PRIM_RecordFieldOffset : {
        ensureArity(args, 2);
        TypePtr y = evaluateRecordType(args[0], env);
        RecordType *z = (RecordType *)y.ptr();
        size_t i = evaluateSizeT(args[1], env);
        const vector<TypePtr> &fieldTypes = recordFieldTypes(z);
        if (i >= fieldTypes.size())
            error("field index out of range");
        const llvm::StructLayout *layout = recordTypeLayout(z);
        ObjectPtr obj = sizeTToValueHolder(layout->getElementOffset(i)).ptr();
        codegenStaticObject(obj, env, ctx, out);
        break;
    }

    case PRIM_RecordFieldIndex : {
        ensureArity(args, 2);
        TypePtr y = evaluateRecordType(args[0], env);
        RecordType *z = (RecordType *)y.ptr();
        IdentifierPtr fname = evaluateIdentifier(args[1], env);
        const map<string, size_t> &fieldIndexMap = recordFieldIndexMap(z);
        map<string, size_t>::const_iterator fi =
            fieldIndexMap.find(fname->str);
        if (fi == fieldIndexMap.end())
            error("field not in record");
        ObjectPtr obj = sizeTToValueHolder(fi->second).ptr();
        codegenStaticObject(obj, env, ctx, out);
        break;
    }

    case PRIM_recordFieldRef : {
        ensureArity(args, 2);
        CValuePtr crec = codegenOneAsRef(args[0], env, ctx);
        if (crec->type->typeKind != RECORD_TYPE)
            error(args[0], "expecting a record");
        RecordType *rt = (RecordType *)crec->type.ptr();
        const vector<TypePtr> &fieldTypes = recordFieldTypes(rt);
        size_t i = evaluateSizeT(args[1], env);
        llvm::Value *ptr =
            llvmBuilder->CreateConstGEP2_32(crec->llValue, 0, i);
        assert(out->size() == 1);
        assert(!out->values[0]);
        out->values[0] = new CValue(fieldTypes[i], ptr);
        break;
    }

    case PRIM_recordFieldRefByName : {
        ensureArity(args, 2);
        CValuePtr crec = codegenOneAsRef(args[0], env, ctx);
        if (crec->type->typeKind != RECORD_TYPE)
            error(args[0], "expecting a record");
        RecordType *rt = (RecordType *)crec->type.ptr();
        const vector<TypePtr> &fieldTypes = recordFieldTypes(rt);
        IdentifierPtr fname = evaluateIdentifier(args[1], env);
        const map<string, size_t> &fieldIndexMap = recordFieldIndexMap(rt);
        map<string, size_t>::const_iterator fi = fieldIndexMap.find(fname->str);
        if (fi == fieldIndexMap.end())
            error(args[1], "field not in record");
        size_t i = fi->second;
        llvm::Value *ptr =
            llvmBuilder->CreateConstGEP2_32(crec->llValue, 0, i);
        assert(out->size() == 1);
        assert(!out->values[0]);
        out->values[0] = new CValue(fieldTypes[i], ptr);
        break;
    }

    case PRIM_Static :
        error("Static type constructor cannot be called");

    case PRIM_StaticName : {
        ensureArity(args, 1);
        ObjectPtr y = evaluateStatic(args[0], env);
        ostringstream sout;
        printName(sout, y);
        ExprPtr z = new StringLiteral(sout.str());
        codegenExpr(z, env, ctx, out);
        break;
    }

    default : {
        void codegenInvokePrimOp2(PrimOpPtr x,
                                  const vector<ExprPtr> &args,
                                  EnvPtr env,
                                  CodegenContextPtr ctx,
                                  MultiCValuePtr out);
        codegenInvokePrimOp2(x, args, env, ctx, out);
        break;
    }

    }
}

void codegenInvokePrimOp2(PrimOpPtr x,
                          const vector<ExprPtr> &args,
                          EnvPtr env,
                          CodegenContextPtr ctx,
                          MultiCValuePtr out)
{
    switch (x->primOpCode) {

    case PRIM_EnumP : {
        ensureArity(args, 1);
        ObjectPtr y = evaluateStatic(args[0], env);
        bool isEnumType = false;
        if (y->objKind == TYPE) {
            Type *t = (Type *)y.ptr();
            if (t->typeKind == ENUM_TYPE)
                isEnumType = true;
        }
        ObjectPtr obj = boolToValueHolder(isEnumType).ptr();
        codegenStaticObject(obj, env, ctx, out);
        break;
    }

    case PRIM_enumToInt : {
        ensureArity(args, 1);
        CValuePtr cv = codegenOneAsRef(args[0], env, ctx);
        if (cv->type->typeKind != ENUM_TYPE)
            error(args[0], "expecting enum value");
        llvm::Value *llv = llvmBuilder->CreateLoad(cv->llValue);
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0.ptr());
        assert(out0->type == cIntType);
        llvmBuilder->CreateStore(llv, out0->llValue);
        break;
    }

    case PRIM_intToEnum : {
        ensureArity(args, 2);
        TypePtr t = evaluateEnumerationType(args[0], env);
        TypePtr t2 = cIntType;
        llvm::Value *v = _cgInteger(args[1], env, ctx, t2);
        assert(out->size() == 1);
        CValuePtr out0 = out->values[0];
        assert(out0.ptr());
        assert(out0->type == t);
        llvmBuilder->CreateStore(v, out0->llValue);
        break;
    }

    default :
        assert(false);
        break;
    }
}



//
// codegenSharedLib, codegenExe
//

static ProcedurePtr makeInitializerProcedure() {
    CodePtr code = new Code();
    code->body = globalVarInitializers().ptr();
    IdentifierPtr name = new Identifier("%initGlobals");
    ExprPtr target = new NameRef(name);
    OverloadPtr overload = new Overload(target, code, false);
    EnvPtr env = new Env();
    overload->env = env;
    ProcedurePtr proc = new Procedure(name, PRIVATE);
    proc->overloads.push_back(overload);
    proc->env = env;
    env->entries[name->str] = proc.ptr();
    return proc;
}

static ProcedurePtr makeDestructorProcedure() {
    CodePtr code = new Code();
    code->body = globalVarDestructors().ptr();
    IdentifierPtr name = new Identifier("%destroyGlobals");
    ExprPtr target = new NameRef(name);
    OverloadPtr overload = new Overload(target, code, false);
    EnvPtr env = new Env();
    overload->env = env;
    ProcedurePtr proc = new Procedure(name, PRIVATE);
    proc->overloads.push_back(overload);
    proc->env = env;
    env->entries[name->str] = proc.ptr();
    return proc;
}

static void generateLLVMCtorsAndDtors() {
    ObjectPtr initializer = makeInitializerProcedure().ptr();
    InvokeEntryPtr entry1 = codegenCallable(initializer,
                                            vector<ObjectPtr>(),
                                            vector<ValueTempness>(),
                                            vector<LocationPtr>());
    codegenCWrapper(entry1, getCodeName(initializer));
    ObjectPtr destructor = makeDestructorProcedure().ptr();
    InvokeEntryPtr entry2 = codegenCallable(destructor,
                                            vector<ObjectPtr>(),
                                            vector<ValueTempness>(),
                                            vector<LocationPtr>());
    codegenCWrapper(entry2, getCodeName(destructor));

    // make types for llvm.global_ctors, llvm.global_dtors
    vector<const llvm::Type *> fieldTypes;
    fieldTypes.push_back(llvmIntType(32));
    const llvm::Type *funcType = entry1->llvmCWrapper->getFunctionType();
    const llvm::Type *funcPtrType = llvm::PointerType::getUnqual(funcType);
    fieldTypes.push_back(funcPtrType);
    const llvm::StructType *structType =
        llvm::StructType::get(llvm::getGlobalContext(), fieldTypes);
    const llvm::ArrayType *arrayType = llvm::ArrayType::get(structType, 1);

    // make constants for llvm.global_ctors
    vector<llvm::Constant*> structElems1;
    llvm::ConstantInt *prio1 =
        llvm::ConstantInt::get(llvm::getGlobalContext(),
                               llvm::APInt(32, llvm::StringRef("65535"), 10));
    structElems1.push_back(prio1);
    structElems1.push_back(entry1->llvmCWrapper);
    llvm::Constant *structVal1 = llvm::ConstantStruct::get(structType,
                                                           structElems1);
    vector<llvm::Constant*> arrayElems1;
    arrayElems1.push_back(structVal1);
    llvm::Constant *arrayVal1 = llvm::ConstantArray::get(arrayType,
                                                         arrayElems1);

    // make constants for llvm.global_dtors
    vector<llvm::Constant*> structElems2;
    llvm::ConstantInt *prio2 =
        llvm::ConstantInt::get(llvm::getGlobalContext(),
                               llvm::APInt(32, llvm::StringRef("65535"), 10));
    structElems2.push_back(prio2);
    structElems2.push_back(entry2->llvmCWrapper);
    llvm::Constant *structVal2 = llvm::ConstantStruct::get(structType,
                                                           structElems2);
    vector<llvm::Constant*> arrayElems2;
    arrayElems2.push_back(structVal2);
    llvm::Constant *arrayVal2 = llvm::ConstantArray::get(arrayType,
                                                         arrayElems2);

    // define llvm.global_ctors
    new llvm::GlobalVariable(*llvmModule, arrayType, true,
                             llvm::GlobalVariable::AppendingLinkage,
                             arrayVal1, "llvm.global_ctors");

    // define llvm.global_dtors
    new llvm::GlobalVariable(*llvmModule, arrayType, true,
                             llvm::GlobalVariable::AppendingLinkage,
                             arrayVal2, "llvm.global_dtors");
}


void codegenSharedLib(ModulePtr module)
{
    generateLLVMCtorsAndDtors();

    for (unsigned i = 0; i < module->topLevelItems.size(); ++i) {
        TopLevelItemPtr x = module->topLevelItems[i];
        if (x->objKind == EXTERNAL_PROCEDURE) {
            ExternalProcedurePtr y = (ExternalProcedure *)x.ptr();
            if (y->body.ptr())
                codegenExternal(y);
        }
    }
}

void codegenExe(ModulePtr module)
{
    generateLLVMCtorsAndDtors();

    BlockPtr mainBody = new Block();
    ExprPtr mainCall = new Call(new NameRef(new Identifier("main")));
    mainBody->statements.push_back(new ExprStatement(mainCall));
    vector<bool> isRef; isRef.push_back(false);
    vector<ExprPtr> exprs; exprs.push_back(new IntLiteral("0"));
    mainBody->statements.push_back(new Return(isRef, exprs));

    ExternalProcedurePtr entryProc =
        new ExternalProcedure(new Identifier("main"),
                              PUBLIC,
                              vector<ExternalArgPtr>(),
                              false,
                              new ObjectExpr(cIntType.ptr()),
                              mainBody.ptr(),
                              vector<ExprPtr>());

    entryProc->env = module->env;

    codegenExternal(entryProc);
}
