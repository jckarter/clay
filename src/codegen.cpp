#include "clay.hpp"


vector<CValuePtr> stackCValues;


//
// codegen value ops
//

void codegenValueInit(CValuePtr dest)
{
    codegenInvokeType(dest->type, vector<ExprPtr>(), new Env(), dest);
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
    args.push_back(new CValueExpr(src));
    codegenInvokeType(dest->type, args, new Env(), dest);
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

int cgMarkStack()
{
    return stackCValues.size();
}

void cgDestroyStack(int marker)
{
    int i = (int)stackCValues.size();
    assert(marker <= i);
    while (marker < i) {
        --i;
        codegenValueDestroy(stackCValues[i]);
    }
}

void cgPopStack(int marker)
{
    assert(marker <= (int)stackCValues.size());
    while (marker < (int)stackCValues.size())
        stackCValues.pop_back();
}

void cgDestroyAndPopStack(int marker)
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
                                const vector<bool> &isStaticFlags,
                                const vector<ObjectPtr> &argsKey,
                                const vector<LocationPtr> &argLocations)
{
    
    InvokeEntryPtr entry =
        analyzeProcedure(x, isStaticFlags, argsKey, argLocations);
    codegenCodeBody(entry, x->name->str);
    return entry;
}

InvokeEntryPtr codegenOverloadable(OverloadablePtr x,
                                   const vector<bool> &isStaticFlags,
                                   const vector<ObjectPtr> &argsKey,
                                   const vector<LocationPtr> &argLocations)
{
    InvokeEntryPtr entry =
        analyzeOverloadable(x, isStaticFlags, argsKey, argLocations);
    codegenCodeBody(entry, x->name->str);
    return entry;
}

void codegenCodeBody(InvokeEntryPtr entry, const string &callableName)
{
    assert(entry->analyzed);
    if (entry->llvmFunc)
        return;

    vector<const llvm::Type *> llArgTypes;
    for (unsigned i = 0; i < entry->argTypes.size(); ++i)
        llArgTypes.push_back(llvmPointerType(entry->argTypes[i]));

    const llvm::Type *llReturnType;
    if (!entry->returnType) {
        llReturnType = llvmVoidType();
    }
    else if (entry->returnIsTemp) {
        llArgTypes.push_back(llvmPointerType(entry->returnType));
        llReturnType = llvmVoidType();
    }
    else {
        llReturnType = llvmPointerType(entry->returnType);
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
    for (unsigned i = 0; i < entry->argTypes.size(); ++i, ++ai) {
        llvm::Argument *llArgValue = &(*ai);
        llArgValue->setName(entry->argNames[i]->str);
        CValuePtr cvalue = new CValue(entry->argTypes[i], llArgValue);
        addLocal(env, entry->argNames[i], cvalue.ptr());
    }

    CValuePtr returnVal;
    if (entry->returnType.ptr()) {
        if (entry->returnIsTemp) {
            returnVal = new CValue(entry->returnType, &(*ai));
        }
        else {
            const llvm::Type *llt = llvmPointerType(entry->returnType);
            llvm::Value *llv = llvmInitBuilder->CreateAlloca(llt);
            returnVal = new CValue(entry->returnType, llv);
        }
    }
    ObjectPtr rinfo = new ReturnedInfo(entry->returnType,
                                       entry->returnIsTemp,
                                       returnVal);
    addLocal(env, new Identifier("%returned"), rinfo);

    JumpTarget returnTarget(returnBlock, cgMarkStack());
    CodeContext ctx(entry, returnVal, returnTarget);

    bool terminated = codegenStatement(entry->code->body, env, ctx);
    if (!terminated) {
        cgDestroyStack(returnTarget.stackMarker);
        llvmBuilder->CreateBr(returnBlock);
    }
    cgPopStack(returnTarget.stackMarker);

    llvmInitBuilder->CreateBr(codeBlock);

    llvmBuilder->SetInsertPoint(returnBlock);

    if (!entry->returnType || entry->returnIsTemp) {
        llvmBuilder->CreateRetVoid();
    }
    else {
        llvm::Value *v = llvmBuilder->CreateLoad(returnVal->llValue);
        llvmBuilder->CreateRet(v);
    }

    delete llvmInitBuilder;
    delete llvmBuilder;

    llvmInitBuilder = savedLLVMInitBuilder;
    llvmBuilder = savedLLVMBuilder;
    llvmFunction = savedLLVMFunction;
}



//
// _getUpdateOperator
//

static const char *_getUpdateOperator(int op) {
    switch (op) {
    case UPDATE_ADD : return "addAssign";
    case UPDATE_SUBTRACT : return "subtractAssign";
    case UPDATE_MULTIPLY : return "multiplyAssign";
    case UPDATE_DIVIDE : return "divideAssign";
    case UPDATE_REMAINDER : return "remainderAssign";
    default :
        assert(false);
        return NULL;
    }
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
        int blockMarker = cgMarkStack();
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
            cgDestroyStack(blockMarker);
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
        CValuePtr cvLeft = codegenValue(x->left, env, NULL);
        CValuePtr cvRight = codegenAsRef(x->right, env, pvRight);
        codegenValueAssign(cvLeft, cvRight);
        cgDestroyAndPopStack(marker);
        return false;
    }

    case INIT_ASSIGNMENT : {
        InitAssignment *x = (InitAssignment *)stmt.ptr();
        PValuePtr pvLeft = analyzeValue(x->left, env);
        PValuePtr pvRight = analyzeValue(x->right, env);
        if (pvLeft->type != pvRight->type)
            error("type mismatch in assignment");
        if (pvLeft->isTemp)
            error(x->left, "cannot assign to a temporary");
        int marker = cgMarkStack();
        CValuePtr cvLeft = codegenValue(x->left, env, NULL);
        codegenIntoValue(x->right, env, pvRight, cvLeft);
        cgDestroyAndPopStack(marker);
        return false;
    }

    case UPDATE_ASSIGNMENT : {
        UpdateAssignment *x = (UpdateAssignment *)stmt.ptr();
        PValuePtr pvLeft = analyzeValue(x->left, env);
        if (pvLeft->isTemp)
            error(x->left, "cannot assign to a temporary");
        CallPtr call = new Call(kernelNameRef(_getUpdateOperator(x->op)));
        call->args.push_back(x->left);
        call->args.push_back(x->right);
        int marker = cgMarkStack();
        ObjectPtr obj = analyzeMaybeVoidValue(call.ptr(), env);
        switch (obj->objKind) {
        case VOID_VALUE :
            codegenMaybeVoid(call.ptr(), env, NULL);
            break;
        case PVALUE :
            codegenAsRef(call.ptr(), env, (PValue *)obj.ptr());
            break;
        default :
            assert(false);
        }
        cgDestroyAndPopStack(marker);
        return false;
    }

    case GOTO : {
        Goto *x = (Goto *)stmt.ptr();
        map<string, JumpTarget>::iterator li =
            ctx.labels.find(x->labelName->str);
        if (li == ctx.labels.end())
            error("goto label not found");
        const JumpTarget &jt = li->second;
        cgDestroyStack(jt.stackMarker);
        llvmBuilder->CreateBr(jt.block);
        return true;
    }

    case RETURN : {
        Return *x = (Return *)stmt.ptr();
        InvokeEntryPtr entry = ctx.entry;
        if (!x->expr) {
            if (entry->returnType.ptr())
                error("non-void return expected");
        }
        else {
            if (!entry->returnType)
                error("void return expected");
            if (!entry->returnIsTemp)
                error("return by reference expected");
            PValuePtr pv = analyzeValue(x->expr, env);
            if (pv->type != entry->returnType)
                error("type mismatch in return");
            codegenRootIntoValue(x->expr, env, pv, ctx.returnVal);
        }
        const JumpTarget &jt = ctx.returnTarget;
        cgDestroyStack(jt.stackMarker);
        llvmBuilder->CreateBr(jt.block);
        return true;
    }

    case RETURN_REF : {
        ReturnRef *x = (ReturnRef *)stmt.ptr();
        InvokeEntryPtr entry = ctx.entry;
        if (!entry->returnType)
            error("void return expected");
        if (entry->returnIsTemp)
            error("return by value expected");
        PValuePtr pv = analyzeValue(x->expr, env);
        if (pv->type != entry->returnType)
            error("type mismatch in return");
        if (pv->isTemp)
            error("cannot return a temporary by reference");
        CValuePtr ref = codegenRootValue(x->expr, env, NULL);
        llvmBuilder->CreateStore(ref->llValue, ctx.returnVal->llValue);
        const JumpTarget &jt = ctx.returnTarget;
        cgDestroyStack(jt.stackMarker);
        llvmBuilder->CreateBr(jt.block);
        return true;
    }

    case IF : {
        If *x = (If *)stmt.ptr();
        PValuePtr pv = analyzeValue(x->condition, env);
        int marker = cgMarkStack();
        CValuePtr cv = codegenAsRef(x->condition, env, pv);
        llvm::Value *cond = codegenToBoolFlag(cv);
        cgDestroyAndPopStack(marker);

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
        int marker = cgMarkStack();
        if (a->objKind == VOID_VALUE) {
            codegenMaybeVoid(x->expr, env, NULL);
        }
        else if (a->objKind == PVALUE) {
            PValuePtr pv = (PValue *)a.ptr();
            codegenAsRef(x->expr, env, pv);
        }
        else {
            error("invalid expression statement");
        }
        cgDestroyAndPopStack(marker);
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
        int marker = cgMarkStack();
        CValuePtr cv = codegenAsRef(x->condition, env, pv);
        llvm::Value *cond = codegenToBoolFlag(cv);
        cgDestroyAndPopStack(marker);

        llvmBuilder->CreateCondBr(cond, whileBody, whileEnd);

        ctx.breaks.push_back(JumpTarget(whileEnd, cgMarkStack()));
        ctx.continues.push_back(JumpTarget(whileBegin, cgMarkStack()));
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
        cgDestroyStack(jt.stackMarker);
        llvmBuilder->CreateBr(jt.block);
        return true;
    }

    case CONTINUE : {
        if (ctx.continues.empty())
            error("invalid continue statement");
        const JumpTarget &jt = ctx.breaks.back();
        cgDestroyStack(jt.stackMarker);
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
            ctx.labels[y->name->str] = JumpTarget(bb, cgMarkStack());
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



//
// codegen expressions
//

void codegenRootIntoValue(ExprPtr expr,
                          EnvPtr env,
                          PValuePtr pv,
                          CValuePtr out)
{
    int marker = cgMarkStack();
    codegenIntoValue(expr, env, pv, out);
    cgDestroyAndPopStack(marker);
}

CValuePtr codegenRootValue(ExprPtr expr, EnvPtr env, CValuePtr out)
{
    int marker = cgMarkStack();
    CValuePtr cv = codegenValue(expr, env, out);
    cgDestroyAndPopStack(marker);
    return cv;
}

void codegenIntoValue(ExprPtr expr, EnvPtr env, PValuePtr pv, CValuePtr out)
{
    if (pv->isTemp) {
        codegenValue(expr, env, out);
    }
    else {
        CValuePtr ref = codegenValue(expr, env, NULL);
        codegenValueCopy(out, ref);
    }
}

CValuePtr codegenAsRef(ExprPtr expr, EnvPtr env, PValuePtr pv)
{
    CValuePtr result;
    if (pv->isTemp) {
        result = codegenAllocValue(pv->type);
        codegenValue(expr, env, result);
    }
    else {
        result = codegenValue(expr, env, NULL);
    }
    return result;
}

CValuePtr codegenValue(ExprPtr expr, EnvPtr env, CValuePtr out)
{
    CValuePtr result = codegenMaybeVoid(expr, env, out);
    if (!result)
        error(expr, "expected non-void value");
    return result;
}

CValuePtr codegenMaybeVoid(ExprPtr expr, EnvPtr env, CValuePtr out)
{
    return codegenExpr(expr, env, out);
}

CValuePtr codegenExpr(ExprPtr expr, EnvPtr env, CValuePtr out)
{
    LocationContext loc(expr->location);

    switch (expr->objKind) {

    case BOOL_LITERAL : {
        BoolLiteral *x = (BoolLiteral *)expr.ptr();
        int bv = (x->value ? 1 : 0);
        llvm::Value *llv = llvm::ConstantInt::get(llvmType(boolType), bv);
        llvmBuilder->CreateStore(llv, out->llValue);
        return out;
    }

    case INT_LITERAL : {
        IntLiteral *x = (IntLiteral *)expr.ptr();
        char *ptr = (char *)x->value.c_str();
        char *end = ptr;
        llvm::Value *llv;
        if (x->suffix == "i8") {
            long y = strtol(ptr, &end, 0);
            if (*end != 0)
                error("invalid int8 literal");
            if ((errno == ERANGE) || (y < SCHAR_MIN) || (y > SCHAR_MAX))
                error("int8 literal out of range");
            llv = llvm::ConstantInt::getSigned(llvmIntType(8), y);
        }
        else if (x->suffix == "i16") {
            long y = strtol(ptr, &end, 0);
            if (*end != 0)
                error("invalid int16 literal");
            if ((errno == ERANGE) || (y < SHRT_MIN) || (y > SHRT_MAX))
                error("int16 literal out of range");
            llv = llvm::ConstantInt::getSigned(llvmIntType(16), y);
        }
        else if ((x->suffix == "i32") || x->suffix.empty()) {
            long y = strtol(ptr, &end, 0);
            if (*end != 0)
                error("invalid int32 literal");
            if (errno == ERANGE)
                error("int32 literal out of range");
            llv = llvm::ConstantInt::getSigned(llvmIntType(32), y);
        }
        else if (x->suffix == "i64") {
            long long y = strtoll(ptr, &end, 0);
            if (*end != 0)
                error("invalid int64 literal");
            if (errno == ERANGE)
                error("int64 literal out of range");
            llv = llvm::ConstantInt::getSigned(llvmIntType(64), y);
        }
        else if (x->suffix == "u8") {
            unsigned long y = strtoul(ptr, &end, 0);
            if (*end != 0)
                error("invalid uint8 literal");
            if ((errno == ERANGE) || (y > UCHAR_MAX))
                error("uint8 literal out of range");
            llv = llvm::ConstantInt::get(llvmIntType(8), y);
        }
        else if (x->suffix == "u16") {
            unsigned long y = strtoul(ptr, &end, 0);
            if (*end != 0)
                error("invalid uint16 literal");
            if ((errno == ERANGE) || (y > USHRT_MAX))
                error("uint16 literal out of range");
            llv = llvm::ConstantInt::get(llvmIntType(16), y);
        }
        else if (x->suffix == "u32") {
            unsigned long y = strtoul(ptr, &end, 0);
            if (*end != 0)
                error("invalid uint32 literal");
            if (errno == ERANGE)
                error("uint32 literal out of range");
            llv = llvm::ConstantInt::get(llvmIntType(32), y);
        }
        else if (x->suffix == "u64") {
            unsigned long long y = strtoull(ptr, &end, 0);
            if (*end != 0)
                error("invalid uint64 literal");
            if (errno == ERANGE)
                error("uint64 literal out of range");
            llv = llvm::ConstantInt::get(llvmIntType(64), y);
        }
        else if (x->suffix == "f32") {
            float y = strtof(ptr, &end);
            if (*end != 0)
                error("invalid float32 literal");
            if (errno == ERANGE)
                error("float32 literal out of range");
            llv = llvm::ConstantFP::get(llvmFloatType(32), y);
        }
        else if (x->suffix == "f64") {
            double y = strtod(ptr, &end);
            if (*end != 0)
                error("invalid float64 literal");
            if (errno == ERANGE)
                error("float64 literal out of range");
            llv = llvm::ConstantFP::get(llvmFloatType(64), y);
        }
        else {
            error("invalid literal suffix: " + x->suffix);
        }
        llvmBuilder->CreateStore(llv, out->llValue);
        return out;
    }

    case FLOAT_LITERAL : {
        FloatLiteral *x = (FloatLiteral *)expr.ptr();
        char *ptr = (char *)x->value.c_str();
        char *end = ptr;
        llvm::Value *llv;
        if (x->suffix == "f32") {
            float y = strtof(ptr, &end);
            if (*end != 0)
                error("invalid float32 literal");
            if (errno == ERANGE)
                error("float32 literal out of range");
            llv = llvm::ConstantFP::get(llvmFloatType(32), y);
        }
        else if ((x->suffix == "f64") || x->suffix.empty()) {
            double y = strtod(ptr, &end);
            if (*end != 0)
                error("invalid float64 literal");
            if (errno == ERANGE)
                error("float64 literal out of range");
            llv = llvm::ConstantFP::get(llvmFloatType(64), y);
        }
        else {
            error("invalid float literal suffix: " + x->suffix);
        }
        llvmBuilder->CreateStore(llv, out->llValue);
        return out;
    }

    case CHAR_LITERAL : {
        CharLiteral *x = (CharLiteral *)expr.ptr();
        if (!x->desugared)
            x->desugared = desugarCharLiteral(x->value);
        return codegenExpr(x->desugared, env, out);
    }

    case STRING_LITERAL : {
        StringLiteral *x = (StringLiteral *)expr.ptr();
        if (!x->desugared)
            x->desugared = desugarStringLiteral(x->value);
        return codegenExpr(x->desugared, env, out);
    }

    case NAME_REF : {
        NameRef *x = (NameRef *)expr.ptr();
        ObjectPtr y = lookupEnv(env, x->name);
        switch (y->objKind) {
        case STATIC_GLOBAL : {
            y = evaluateStatic(x, env);
            if (y->objKind != VALUE_HOLDER)
                error("invalid expression");
            ValueHolder *z = (ValueHolder *)y.ptr();
            codegenValueHolder(z, out);
            return out;
        }
        case VALUE_HOLDER : {
            ValueHolder *z = (ValueHolder *)y.ptr();
            codegenValueHolder(z, out);
            return out;
        }
        case CVALUE : {
            CValue *z = (CValue *)y.ptr();
            return z;
        }
        }
        error("invalid name ref");
        return NULL;
    }

    case RETURNED : {
        ObjectPtr y = lookupEnv(env, new Identifier("%returned"));
        assert(y.ptr());
        ReturnedInfo *z = (ReturnedInfo *)y.ptr();
        if (!z->returnType)
            error("'returned' cannot be used when returning void");
        if (!z->returnIsTemp)
            error("'returned' cannot be used when returning by reference");
        return z->returnVal;
    }

    case TUPLE : {
        Tuple *x = (Tuple *)expr.ptr();
        if (!x->desugared)
            x->desugared = desugarTuple(x);
        return codegenExpr(x->desugared, env, out);
    }

    case ARRAY : {
        Array *x = (Array *)expr.ptr();
        if (!x->desugared)
            x->desugared = desugarArray(x);
        return codegenExpr(x->desugared, env, out);
    }

    case INDEXING : {
        Indexing *x = (Indexing *)expr.ptr();
        PValuePtr y = analyzeValue(x->expr, env);
        CValuePtr z = codegenAsRef(x->expr, env, y);
        vector<ExprPtr> args2;
        args2.push_back(new CValueExpr(z));
        args2.insert(args2.end(), x->args.begin(), x->args.end());
        ObjectPtr op = kernelName("index");
        return codegenInvoke(op, args2, env, out);
    }

    case CALL : {
        Call *x = (Call *)expr.ptr();
        ObjectPtr y = analyze(x->expr, env);
        assert(y.ptr());
        if (y->objKind == PVALUE) {
            CValuePtr cv = codegenAsRef(x->expr, env, (PValue *)y.ptr());
            return codegenInvokeValue(cv, x->args, env, out);
        }
        return codegenInvoke(y, x->args, env, out);
    }

    case FIELD_REF : {
        FieldRef *x = (FieldRef *)expr.ptr();
        vector<ExprPtr> args;
        args.push_back(x->expr);
        args.push_back(new ObjectExpr(x->name.ptr()));
        ObjectPtr prim = primName("recordFieldRefByName");
        return codegenInvoke(prim, args, env, out);
    }

    case TUPLE_REF : {
        TupleRef *x = (TupleRef *)expr.ptr();
        vector<ExprPtr> args;
        args.push_back(x->expr);
        args.push_back(new ObjectExpr(intToValueHolder(x->index).ptr()));
        ObjectPtr prim = primName("tupleRef");
        return codegenInvoke(prim, args, env, out);
    }

    case UNARY_OP : {
        UnaryOp *x = (UnaryOp *)expr.ptr();
        if (!x->desugared)
            x->desugared = desugarUnaryOp(x);
        return codegenExpr(x->desugared, env, out);
    }

    case BINARY_OP : {
        BinaryOp *x = (BinaryOp *)expr.ptr();
        if (!x->desugared)
            x->desugared = desugarBinaryOp(x);
        return codegenExpr(x->desugared, env, out);
    }

    case AND : {
        And *x = (And *)expr.ptr();
        PValuePtr pv1 = analyzeValue(x->expr1, env);
        PValuePtr pv2 = analyzeValue(x->expr2, env);
        if (pv1->type != pv2->type)
            error("type mismatch in 'and' expression");
        llvm::BasicBlock *trueBlock = newBasicBlock("andTrue1");
        llvm::BasicBlock *falseBlock = newBasicBlock("andFalse1");
        llvm::BasicBlock *mergeBlock = newBasicBlock("andMerge");
        CValuePtr result;
        if (pv1->isTemp) {
            codegenIntoValue(x->expr1, env, pv1, out);
            llvm::Value *flag1 = codegenToBoolFlag(out);
            llvmBuilder->CreateCondBr(flag1, trueBlock, falseBlock);

            llvmBuilder->SetInsertPoint(trueBlock);
            codegenValueDestroy(out);
            codegenIntoValue(x->expr2, env, pv2, out);
            llvmBuilder->CreateBr(mergeBlock);

            llvmBuilder->SetInsertPoint(falseBlock);
            llvmBuilder->CreateBr(mergeBlock);

            llvmBuilder->SetInsertPoint(mergeBlock);
            result = out;
        }
        else {
            if (pv2->isTemp) {
                CValuePtr ref1 = codegenExpr(x->expr1, env, NULL);
                llvm::Value *flag1 = codegenToBoolFlag(ref1);
                llvmBuilder->CreateCondBr(flag1, trueBlock, falseBlock);

                llvmBuilder->SetInsertPoint(trueBlock);
                codegenIntoValue(x->expr2, env, pv2, out);
                llvmBuilder->CreateBr(mergeBlock);

                llvmBuilder->SetInsertPoint(falseBlock);
                codegenValueCopy(out, ref1);
                llvmBuilder->CreateBr(mergeBlock);

                llvmBuilder->SetInsertPoint(mergeBlock);
                result = out;
            }
            else {
                CValuePtr ref1 = codegenExpr(x->expr1, env, NULL);
                llvm::Value *flag1 = codegenToBoolFlag(ref1);
                llvmBuilder->CreateCondBr(flag1, trueBlock, falseBlock);

                llvmBuilder->SetInsertPoint(trueBlock);
                CValuePtr ref2 = codegenExpr(x->expr2, env, NULL);
                llvmBuilder->CreateBr(mergeBlock);
                trueBlock = llvmBuilder->GetInsertBlock();

                llvmBuilder->SetInsertPoint(falseBlock);
                llvmBuilder->CreateBr(mergeBlock);

                llvmBuilder->SetInsertPoint(mergeBlock);
                const llvm::Type *ptrType = llvmPointerType(pv1->type);
                llvm::PHINode *phi = llvmBuilder->CreatePHI(ptrType);
                phi->addIncoming(ref1->llValue, falseBlock);
                phi->addIncoming(ref2->llValue, trueBlock);
                result = new CValue(pv1->type, phi);
            }
        }
        return result;
    }

    case OR : {
        Or *x = (Or *)expr.ptr();
        PValuePtr pv1 = analyzeValue(x->expr1, env);
        PValuePtr pv2 = analyzeValue(x->expr2, env);
        if (pv1->type != pv2->type)
            error("type mismatch in 'or' expression");
        llvm::BasicBlock *trueBlock = newBasicBlock("orTrue1");
        llvm::BasicBlock *falseBlock = newBasicBlock("orFalse1");
        llvm::BasicBlock *mergeBlock = newBasicBlock("orMerge");
        CValuePtr result;
        if (pv1->isTemp) {
            codegenIntoValue(x->expr1, env, pv1, out);
            llvm::Value *flag1 = codegenToBoolFlag(out);
            llvmBuilder->CreateCondBr(flag1, trueBlock, falseBlock);

            llvmBuilder->SetInsertPoint(trueBlock);
            llvmBuilder->CreateBr(mergeBlock);

            llvmBuilder->SetInsertPoint(falseBlock);
            codegenValueDestroy(out);
            codegenIntoValue(x->expr2, env, pv2, out);
            llvmBuilder->CreateBr(mergeBlock);

            llvmBuilder->SetInsertPoint(mergeBlock);
            result = out;
        }
        else {
            if (pv2->isTemp) {
                CValuePtr ref1 = codegenExpr(x->expr1, env, NULL);
                llvm::Value *flag1 = codegenToBoolFlag(ref1);
                llvmBuilder->CreateCondBr(flag1, trueBlock, falseBlock);

                llvmBuilder->SetInsertPoint(trueBlock);
                codegenValueCopy(out, ref1);
                llvmBuilder->CreateBr(mergeBlock);

                llvmBuilder->SetInsertPoint(falseBlock);
                codegenIntoValue(x->expr2, env, pv2, out);
                llvmBuilder->CreateBr(mergeBlock);

                llvmBuilder->SetInsertPoint(mergeBlock);
                result = out;
            }
            else {
                CValuePtr ref1 = codegenExpr(x->expr1, env, NULL);
                llvm::Value *flag1 = codegenToBoolFlag(ref1);
                llvmBuilder->CreateCondBr(flag1, trueBlock, falseBlock);

                llvmBuilder->SetInsertPoint(trueBlock);
                llvmBuilder->CreateBr(mergeBlock);

                llvmBuilder->SetInsertPoint(falseBlock);
                CValuePtr ref2 = codegenExpr(x->expr2, env, NULL);
                llvmBuilder->CreateBr(mergeBlock);
                falseBlock = llvmBuilder->GetInsertBlock();

                llvmBuilder->SetInsertPoint(mergeBlock);
                const llvm::Type *ptrType = llvmPointerType(pv1->type);
                llvm::PHINode *phi = llvmBuilder->CreatePHI(ptrType);
                phi->addIncoming(ref1->llValue, trueBlock);
                phi->addIncoming(ref2->llValue, falseBlock);
                result = new CValue(pv1->type, phi);
            }
        }
        return result;
    }

    case SC_EXPR : {
        SCExpr *x = (SCExpr *)expr.ptr();
        return codegenExpr(x->expr, x->env, out);
    }

    case OBJECT_EXPR : {
        ObjectExpr *x = (ObjectExpr *)expr.ptr();
        switch (x->obj->objKind) {
        case VALUE_HOLDER : {
            ValueHolder *y = (ValueHolder *)x->obj.ptr();
            codegenValueHolder(y, out);
            return out;
        }
        }
        error("invalid expression");
        return NULL;
    }

    case CVALUE_EXPR : {
        CValueExpr *x = (CValueExpr *)expr.ptr();
        return x->cv;
    }

    default :
        assert(false);
        return NULL;

    }
}



//
// codegenValueHolder
//

void codegenValueHolder(ValueHolderPtr v, CValuePtr out)
{
    assert(v->type == out->type);

    switch (v->type->typeKind) {

    case BOOL_TYPE :
    case INTEGER_TYPE :
    case FLOAT_TYPE : {
        llvm::Value *llv = codegenConstant(v);
        llvmBuilder->CreateStore(llv, out->llValue);
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

CValuePtr codegenInvokeValue(CValuePtr x,
                             const vector<ExprPtr> &args,
                             EnvPtr env,
                             CValuePtr out)
{
    switch (x->type->typeKind) {

    case CODE_POINTER_TYPE : {
        CodePointerType *t = (CodePointerType *)x->type.ptr();
        ensureArity(args, t->argTypes.size());

        llvm::Value *llCallable = llvmBuilder->CreateLoad(x->llValue);
        vector<llvm::Value *> llArgs;
        for (unsigned i = 0; i < args.size(); ++i) {
            PValuePtr parg = analyzeValue(args[i], env);
            if (parg->type != t->argTypes[i])
                error(args[i], "argument type mismatch");
            CValuePtr carg = codegenAsRef(args[i], env, parg);
            llArgs.push_back(carg->llValue);
        }
        if (!t->returnType) {
            assert(!out);
            llvmBuilder->CreateCall(llCallable, llArgs.begin(), llArgs.end());
            return NULL;
        }
        else if (t->returnIsTemp) {
            assert(out.ptr());
            assert(t->returnType == out->type);
            llArgs.push_back(out->llValue);
            llvmBuilder->CreateCall(llCallable, llArgs.begin(), llArgs.end());
            return out;
        }
        else {
            assert(!out);
            llvm::Value *result =
                llvmBuilder->CreateCall(llCallable, llArgs.begin(),
                                        llArgs.end());
            return new CValue(t->returnType, result);
        }
        assert(false);
    }

    case CCODE_POINTER_TYPE : {
        CCodePointerType *t = (CCodePointerType *)x->type.ptr();
        ensureArity(args, t->argTypes.size());

        llvm::Value *llCallable = llvmBuilder->CreateLoad(x->llValue);
        vector<llvm::Value *> llArgs;
        for (unsigned i = 0; i < args.size(); ++i) {
            PValuePtr parg = analyzeValue(args[i], env);
            if (parg->type != t->argTypes[i])
                error(args[i], "argument type mismatch");
            CValuePtr carg = codegenAsRef(args[i], env, parg);
            llvm::Value *llArg = llvmBuilder->CreateLoad(carg->llValue);
            llArgs.push_back(llArg);
        }
        llvm::Value *result =
            llvmBuilder->CreateCall(llCallable, llArgs.begin(), llArgs.end());
        if (!t->returnType) {
            assert(!out);
            return NULL;
        }
        else {
            assert(out.ptr());
            assert(t->returnType == out->type);
            llvmBuilder->CreateStore(result, out->llValue);
            return out;
        }
        assert(false);
    }

    default :
        error("invalid call operation");
        return NULL;
    }
}



//
// codegenInvoke
//

void codegenInvokeVoid(ObjectPtr x,
                       const vector<ExprPtr> &args,
                       EnvPtr env)
{
    CValuePtr result = codegenInvoke(x, args, env, NULL);
    if (result.ptr())
        error("void expression expected");
}

CValuePtr codegenInvoke(ObjectPtr x,
                        const vector<ExprPtr> &args,
                        EnvPtr env,
                        CValuePtr out)
{
    switch (x->objKind) {
    case TYPE :
        return codegenInvokeType((Type *)x.ptr(), args, env, out);
    case RECORD :
        return codegenInvokeRecord((Record *)x.ptr(), args, env, out);
    case PROCEDURE :
        return codegenInvokeProcedure((Procedure *)x.ptr(), args, env, out);
    case OVERLOADABLE :
        return codegenInvokeOverloadable((Overloadable *)x.ptr(),
                                         args, env, out);
    case EXTERNAL_PROCEDURE :
        return codegenInvokeExternal((ExternalProcedure *)x.ptr(),
                                     args, env, out);

    case STATIC_PROCEDURE :
    case STATIC_OVERLOADABLE : {
        ObjectPtr y = analyzeInvoke(x, args, env);
        if (y->objKind != VALUE_HOLDER)
            error("invalid expression");
        ValueHolder *z = (ValueHolder *)y.ptr();
        codegenValueHolder(z, out);
        return out;
    }

    case PRIM_OP :
        return codegenInvokePrimOp((PrimOp *)x.ptr(), args, env, out);
    default :
        error("invalid call operation");
        return NULL;
    }
}



//
// codegenInvokeType
//

CValuePtr codegenInvokeType(TypePtr x,
                            const vector<ExprPtr> &args,
                            EnvPtr env,
                            CValuePtr out)
{
    const vector<bool> &isStaticFlags =
        lookupIsStaticFlags(x.ptr(), args.size());
    vector<ObjectPtr> argsKey;
    vector<LocationPtr> argLocations;
    bool result = computeArgsKey(isStaticFlags, args, env,
                                 argsKey, argLocations);
    assert(result);
    InvokeEntryPtr entry =
        analyzeConstructor(x, isStaticFlags, argsKey, argLocations);
    if (entry->isBuiltin) {
        return codegenInvokeBuiltinConstructor(x, args, env, out);
    }
    else {
        codegenCodeBody(entry, "constructor");
        return codegenInvokeCode(entry, args, env, out);
    }
}

CValuePtr codegenInvokeBuiltinConstructor(TypePtr x,
                                          const vector<ExprPtr> &args,
                                          EnvPtr env,
                                          CValuePtr out)
{
    switch (x->typeKind) {

    case ARRAY_TYPE : {
        ArrayType *y = (ArrayType *)x.ptr();
        ensureArity(args, y->size);
        for (unsigned i = 0; i < args.size(); ++i) {
            PValuePtr z = analyzeValue(args[i], env);
            if (z->type != y->elementType)
                error(args[i], "argument type mismatch");
            llvm::Value *ptr =
                llvmBuilder->CreateConstGEP2_32(out->llValue, 0, i);
            codegenIntoValue(args[i], env, z, new CValue(z->type, ptr));
        }
        return out;
    }

    case TUPLE_TYPE : {
        TupleType *y = (TupleType *)x.ptr();
        ensureArity(args, y->elementTypes.size());
        for (unsigned i = 0; i < args.size(); ++i) {
            PValuePtr z = analyzeValue(args[i], env);
            if (z->type != y->elementTypes[i])
                error(args[i], "argument type mismatch");
            llvm::Value *ptr =
                llvmBuilder->CreateConstGEP2_32(out->llValue, 0, i);
            codegenIntoValue(args[i], env, z, new CValue(z->type, ptr));
        }
        return out;
    }

    case RECORD_TYPE : {
        RecordType *y = (RecordType *)x.ptr();
        const vector<TypePtr> &fieldTypes = recordFieldTypes(y);
        ensureArity(args, fieldTypes.size());
        for (unsigned i = 0; i < args.size(); ++i) {
            PValuePtr z = analyzeValue(args[i], env);
            if (z->type != fieldTypes[i])
                error(args[i], "argument type mismatch");
            llvm::Value *ptr =
                llvmBuilder->CreateConstGEP2_32(out->llValue, 0, i);
            codegenIntoValue(args[i], env, z, new CValue(z->type, ptr));
        }
        return out;
    }

    }

    error("invalid constructor");
    return NULL;
}



//
// codegenInvokeRecord
//

CValuePtr codegenInvokeRecord(RecordPtr x,
                              const vector<ExprPtr> &args,
                              EnvPtr env,
                              CValuePtr out)
{
    const vector<bool> &isStaticFlags =
        lookupIsStaticFlags(x.ptr(), args.size());
    vector<ObjectPtr> argsKey;
    vector<LocationPtr> argLocations;
    bool result = computeArgsKey(isStaticFlags, args, env,
                                 argsKey, argLocations);
    assert(result);
    InvokeEntryPtr entry =
        analyzeRecordConstructor(x, isStaticFlags, argsKey, argLocations);
    codegenCodeBody(entry, x->name->str);
    return codegenInvokeCode(entry, args, env, out);
}



//
// codegenInvokeProcedure, codegenInvokeOverloadable
//

CValuePtr codegenInvokeProcedure(ProcedurePtr x,
                                 const vector<ExprPtr> &args,
                                 EnvPtr env,
                                 CValuePtr out)
{
    const vector<bool> &isStaticFlags =
        lookupIsStaticFlags(x.ptr(), args.size());
    vector<ObjectPtr> argsKey;
    vector<LocationPtr> argLocations;
    bool result = computeArgsKey(isStaticFlags, args, env,
                                 argsKey, argLocations);
    assert(result);
    InvokeEntryPtr entry =
        codegenProcedure(x, isStaticFlags, argsKey, argLocations);
    return codegenInvokeCode(entry, args, env, out);
}

CValuePtr codegenInvokeOverloadable(OverloadablePtr x,
                                    const vector<ExprPtr> &args,
                                    EnvPtr env,
                                    CValuePtr out)
{
    const vector<bool> &isStaticFlags =
        lookupIsStaticFlags(x.ptr(), args.size());
    vector<ObjectPtr> argsKey;
    vector<LocationPtr> argLocations;
    bool result = computeArgsKey(isStaticFlags, args, env,
                                 argsKey, argLocations);
    assert(result);
    InvokeEntryPtr entry =
        codegenOverloadable(x, isStaticFlags, argsKey, argLocations);
    return codegenInvokeCode(entry, args, env, out);
}



//
// codegenInvokeCode
//

CValuePtr codegenInvokeCode(InvokeEntryPtr entry,
                            const vector<ExprPtr> &args,
                            EnvPtr env,
                            CValuePtr out)
{
    vector<llvm::Value *> llArgs;
    for (unsigned i = 0; i < args.size(); ++i) {
        if (entry->isStaticFlags[i])
            continue;
        assert(entry->argsKey[i]->objKind == TYPE);
        TypePtr argType = (Type *)entry->argsKey[i].ptr();
        PValuePtr parg = analyzeValue(args[i], env);
        assert(parg->type == argType);
        CValuePtr carg = codegenAsRef(args[i], env, parg);
        llArgs.push_back(carg->llValue);
    }
    if (!entry->returnType) {
        llvmBuilder->CreateCall(entry->llvmFunc, llArgs.begin(), llArgs.end());
        return NULL;
    }
    else if (entry->returnIsTemp) {
        llArgs.push_back(out->llValue);
        llvmBuilder->CreateCall(entry->llvmFunc, llArgs.begin(), llArgs.end());
        return out;
    }
    else {
        llvm::Value *result =
            llvmBuilder->CreateCall(entry->llvmFunc, llArgs.begin(),
                                    llArgs.end());
        return new CValue(entry->returnType, result);
    }
}



//
// codegenCWrapper
//

void codegenCWrapper(InvokeEntryPtr entry, const string &callableName)
{
    assert(!entry->llvmCWrapper);

    vector<const llvm::Type *> llArgTypes;
    for (unsigned i = 0; i < entry->argTypes.size(); ++i)
        llArgTypes.push_back(llvmType(entry->argTypes[i]));
    const llvm::Type *llReturnType =
        entry->returnType.ptr() ? llvmType(entry->returnType) : llvmVoidType();

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

    if (!entry->returnType) {
        llBuilder.CreateCall(entry->llvmFunc, innerArgs.begin(),
                             innerArgs.end());
        llBuilder.CreateRetVoid();
    }
    else {
        if (!entry->returnIsTemp)
            error("c-code pointers don't support return by reference");
        llvm::Value *llRetVal =
            llBuilder.CreateAlloca(llvmType(entry->returnType));
        innerArgs.push_back(llRetVal);
        llBuilder.CreateCall(entry->llvmFunc, innerArgs.begin(),
                             innerArgs.end());
        llvm::Value *llRet = llBuilder.CreateLoad(llRetVal);
        llBuilder.CreateRet(llRet);
    }
}



//
// codegenInvokeExternal
//

CValuePtr codegenInvokeExternal(ExternalProcedurePtr x,
                                const vector<ExprPtr> &args,
                                EnvPtr env,
                                CValuePtr out)
{
    assert(x->analyzed);
    if (!x->llvmFunc) {
        vector<const llvm::Type *> llArgTypes;
        for (unsigned i = 0; i < x->args.size(); ++i)
            llArgTypes.push_back(llvmType(x->args[i]->type2));
        const llvm::Type *llRetType =
            x->returnType2.ptr() ? llvmType(x->returnType2) : llvmVoidType();
        llvm::FunctionType *llFuncType =
            llvm::FunctionType::get(llRetType, llArgTypes, x->hasVarArgs);
        x->llvmFunc = llvm::Function::Create(llFuncType,
                                             llvm::Function::ExternalLinkage,
                                             x->name->str.c_str(),
                                             llvmModule);
    }
    ensureArity2(args, x->args.size(), x->hasVarArgs);
    vector<llvm::Value *> llArgs;
    for (unsigned i = 0; i < x->args.size(); ++i) {
        PValuePtr parg = analyzeValue(args[i], env);
        if (parg->type != x->args[i]->type2)
            error(args[i], "argument type mismatch");
        CValuePtr carg = codegenAsRef(args[i], env, parg);
        llvm::Value *llArg = llvmBuilder->CreateLoad(carg->llValue);
        llArgs.push_back(llArg);
    }
    if (x->hasVarArgs) {
        for (unsigned i = x->args.size(); i < args.size(); ++i) {
            PValuePtr parg = analyzeValue(args[i], env);
            CValuePtr carg = codegenAsRef(args[i], env, parg);
            llvm::Value *llArg = llvmBuilder->CreateLoad(carg->llValue);
            llArgs.push_back(llArg);
        }
    }
    llvm::Value *result =
        llvmBuilder->CreateCall(x->llvmFunc, llArgs.begin(), llArgs.end());
    if (!x->returnType2) {
        assert(!out);
        return NULL;
    }
    else {
        llvmBuilder->CreateStore(result, out->llValue);
        return out;
    }
}



//
// codegenInvokePrimOp
//

static llvm::Value *_cgNumeric(ExprPtr expr, EnvPtr env, TypePtr &type)
{
    PValuePtr pv = analyzeValue(expr, env);
    if (type.ptr()) {
        if (pv->type != type)
            error(expr, "argument type mismatch");
    }
    else {
        switch (pv->type->typeKind) {
        case INTEGER_TYPE :
        case FLOAT_TYPE :
            break;
        default :
            error(expr, "expecting numeric type");
        }
        type = pv->type;
    }
    CValuePtr cv = codegenAsRef(expr, env, pv);
    return llvmBuilder->CreateLoad(cv->llValue);
}

static llvm::Value *_cgInteger(ExprPtr expr, EnvPtr env, TypePtr &type)
{
    PValuePtr pv = analyzeValue(expr, env);
    if (type.ptr()) {
        if (pv->type != type)
            error(expr, "argument type mismatch");
    }
    else {
        if (pv->type->typeKind != INTEGER_TYPE)
            error(expr, "expecting integer type");
        type = pv->type;
    }
    CValuePtr cv = codegenAsRef(expr, env, pv);
    return llvmBuilder->CreateLoad(cv->llValue);
}

static llvm::Value *_cgPointer(ExprPtr expr, EnvPtr env, PointerTypePtr &type)
{
    PValuePtr pv = analyzeValue(expr, env);
    if (type.ptr()) {
        if (pv->type != (Type *)type.ptr())
            error(expr, "argument type mismatch");
    }
    else {
        if (pv->type->typeKind != POINTER_TYPE)
            error(expr, "expecting pointer type");
        type = (PointerType *)pv->type.ptr();
    }
    CValuePtr cv = codegenAsRef(expr, env, pv);
    return llvmBuilder->CreateLoad(cv->llValue);
}

static llvm::Value *_cgPointerLike(ExprPtr expr, EnvPtr env, TypePtr &type)
{
    PValuePtr pv = analyzeValue(expr, env);
    if (type.ptr()) {
        if (pv->type != type)
            error(expr, "argument type mismatch");
    }
    else {
        switch (pv->type->typeKind) {
        case POINTER_TYPE :
        case CODE_POINTER_TYPE :
        case CCODE_POINTER_TYPE :
            break;
        default :
            error(expr, "expecting a pointer or a code pointer");
        }
        type = pv->type;
    }
    CValuePtr cv = codegenAsRef(expr, env, pv);
    return llvmBuilder->CreateLoad(cv->llValue);
}

CValuePtr codegenInvokePrimOp(PrimOpPtr x,
                              const vector<ExprPtr> &args,
                              EnvPtr env,
                              CValuePtr out)
{
    switch (x->primOpCode) {

    case PRIM_TypeP :
    case PRIM_TypeSize :
    case PRIM_CodePointerTypeP :
    case PRIM_CCodePointerTypeP :
    case PRIM_TupleTypeP :
    case PRIM_TupleElementCount :
    case PRIM_TupleElementOffset :
    case PRIM_RecordTypeP :
    case PRIM_RecordFieldCount :
    case PRIM_RecordFieldOffset :
    case PRIM_RecordFieldIndex : {
        // handle static primitives
        ObjectPtr obj = analyzeInvokePrimOp(x, args, env);
        assert(obj->objKind == VALUE_HOLDER);
        ValueHolderPtr vh = (ValueHolder *)obj.ptr();
        codegenValueHolder(vh, out);
        return out;
    }

    case PRIM_primitiveCopy : {
        ensureArity(args, 2);
        PValuePtr pv0 = analyzeValue(args[0], env);
        PValuePtr pv1 = analyzeValue(args[1], env);
        switch (pv0->type->typeKind) {
        case BOOL_TYPE :
        case INTEGER_TYPE :
        case FLOAT_TYPE :
        case POINTER_TYPE :
        case CODE_POINTER_TYPE :
        case CCODE_POINTER_TYPE :
            break;
        default :
            error(args[0], "expecting primitive type");
        }
        if (pv0->type != pv1->type)
            error(args[1], "argument type mismatch");
        CValuePtr cv0 = codegenAsRef(args[0], env, pv0);
        CValuePtr cv1 = codegenAsRef(args[1], env, pv1);
        llvm::Value *v = llvmBuilder->CreateLoad(cv1->llValue);
        llvmBuilder->CreateStore(v, cv0->llValue);
        return NULL;
    }

    case PRIM_boolNot : {
        ensureArity(args, 1);
        PValuePtr pv = analyzeValue(args[0], env);
        if (pv->type != boolType)
            error(args[0], "expecting bool type");
        CValuePtr cv = codegenAsRef(args[0], env, pv);
        llvm::Value *v = llvmBuilder->CreateLoad(cv->llValue);
        llvm::Value *zero = llvm::ConstantInt::get(llvmType(boolType), 0);
        llvm::Value *flag = llvmBuilder->CreateICmpEQ(v, zero);
        llvm::Value *v2 = llvmBuilder->CreateZExt(flag, llvmType(boolType));
        llvmBuilder->CreateStore(v2, out->llValue);
        return out;
    }

    case PRIM_numericEqualsP : {
        ensureArity(args, 2);
        TypePtr t;
        llvm::Value *v0 = _cgNumeric(args[0], env, t);
        llvm::Value *v1 = _cgNumeric(args[1], env, t);
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
        llvmBuilder->CreateStore(result, out->llValue);
        return out;
    }

    case PRIM_numericLesserP : {
        ensureArity(args, 2);
        TypePtr t;
        llvm::Value *v0 = _cgNumeric(args[0], env, t);
        llvm::Value *v1 = _cgNumeric(args[1], env, t);
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
            flag = llvmBuilder->CreateFCmpUEQ(v0, v1);
            break;
        default :
            assert(false);
        }
        llvm::Value *result =
            llvmBuilder->CreateZExt(flag, llvmType(boolType));
        llvmBuilder->CreateStore(result, out->llValue);
        return out;
    }

    case PRIM_numericAdd : {
        ensureArity(args, 2);
        TypePtr t;
        llvm::Value *v0 = _cgNumeric(args[0], env, t);
        llvm::Value *v1 = _cgNumeric(args[1], env, t);
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
        llvmBuilder->CreateStore(result, out->llValue);
        return out;
    }

    case PRIM_numericSubtract : {
        ensureArity(args, 2);
        TypePtr t;
        llvm::Value *v0 = _cgNumeric(args[0], env, t);
        llvm::Value *v1 = _cgNumeric(args[1], env, t);
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
        llvmBuilder->CreateStore(result, out->llValue);
        return out;
    }

    case PRIM_numericMultiply : {
        ensureArity(args, 2);
        TypePtr t;
        llvm::Value *v0 = _cgNumeric(args[0], env, t);
        llvm::Value *v1 = _cgNumeric(args[1], env, t);
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
        llvmBuilder->CreateStore(result, out->llValue);
        return out;
    }

    case PRIM_numericDivide : {
        ensureArity(args, 2);
        TypePtr t;
        llvm::Value *v0 = _cgNumeric(args[0], env, t);
        llvm::Value *v1 = _cgNumeric(args[1], env, t);
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
        llvmBuilder->CreateStore(result, out->llValue);
        return out;
    }

    case PRIM_numericNegate : {
        ensureArity(args, 1);
        TypePtr t;
        llvm::Value *v = _cgNumeric(args[0], env, t);
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
        llvmBuilder->CreateStore(result, out->llValue);
        return out;
    }

    case PRIM_integerRemainder : {
        ensureArity(args, 2);
        TypePtr t;
        llvm::Value *v0 = _cgInteger(args[0], env, t);
        llvm::Value *v1 = _cgInteger(args[1], env, t);
        llvm::Value *result;
        IntegerType *it = (IntegerType *)t.ptr();
        if (it->isSigned)
            result = llvmBuilder->CreateSRem(v0, v1);
        else
            result = llvmBuilder->CreateUDiv(v0, v1);
        llvmBuilder->CreateStore(result, out->llValue);
        return out;
    }

    case PRIM_integerShiftLeft : {
        ensureArity(args, 2);
        TypePtr t;
        llvm::Value *v0 = _cgInteger(args[0], env, t);
        llvm::Value *v1 = _cgInteger(args[1], env, t);
        llvm::Value *result = llvmBuilder->CreateShl(v0, v1);
        llvmBuilder->CreateStore(result, out->llValue);
        return out;
    }

    case PRIM_integerShiftRight : {
        ensureArity(args, 2);
        TypePtr t;
        llvm::Value *v0 = _cgInteger(args[0], env, t);
        llvm::Value *v1 = _cgInteger(args[1], env, t);
        llvm::Value *result;
        IntegerType *it = (IntegerType *)t.ptr();
        if (it->isSigned)
            result = llvmBuilder->CreateAShr(v0, v1);
        else
            result = llvmBuilder->CreateLShr(v0, v1);
        llvmBuilder->CreateStore(result, out->llValue);
        return out;
    }

    case PRIM_integerBitwiseAnd : {
        ensureArity(args, 2);
        TypePtr t;
        llvm::Value *v0 = _cgInteger(args[0], env, t);
        llvm::Value *v1 = _cgInteger(args[1], env, t);
        llvm::Value *result = llvmBuilder->CreateAnd(v0, v1);
        llvmBuilder->CreateStore(result, out->llValue);
        return out;
    }

    case PRIM_integerBitwiseOr : {
        ensureArity(args, 2);
        TypePtr t;
        llvm::Value *v0 = _cgInteger(args[0], env, t);
        llvm::Value *v1 = _cgInteger(args[1], env, t);
        llvm::Value *result = llvmBuilder->CreateOr(v0, v1);
        llvmBuilder->CreateStore(result, out->llValue);
        return out;
    }

    case PRIM_integerBitwiseXor : {
        ensureArity(args, 2);
        TypePtr t;
        llvm::Value *v0 = _cgInteger(args[0], env, t);
        llvm::Value *v1 = _cgInteger(args[1], env, t);
        llvm::Value *result = llvmBuilder->CreateXor(v0, v1);
        llvmBuilder->CreateStore(result, out->llValue);
        return out;
    }

    case PRIM_integerBitwiseNot : {
        ensureArity(args, 1);
        TypePtr t;
        llvm::Value *v0 = _cgInteger(args[0], env, t);
        llvm::Value *result = llvmBuilder->CreateNot(v0);
        llvmBuilder->CreateStore(result, out->llValue);
        return out;
    }

    case PRIM_numericConvert : {
        ensureArity(args, 2);
        TypePtr dest = evaluateType(args[0], env);
        TypePtr src;
        llvm::Value *v = _cgNumeric(args[1], env, src);
        if (src == dest) {
            llvmBuilder->CreateStore(v, out->llValue);
            return out;
        }
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
            }
            break;
        }
        default :
            error(args[0], "expecting numeric type");
            return NULL;
        }
        llvmBuilder->CreateStore(result, out->llValue);
        return out;
    }

    case PRIM_addressOf : {
        ensureArity(args, 1);
        PValuePtr pv = analyzeValue(args[0], env);
        if (pv->isTemp)
            error("cannot take address of a temporary");
        CValuePtr cv = codegenAsRef(args[0], env, pv);
        llvmBuilder->CreateStore(cv->llValue, out->llValue);
        return out;
    }

    case PRIM_pointerDereference : {
        ensureArity(args, 1);
        PointerTypePtr t;
        llvm::Value *v = _cgPointer(args[0], env, t);
        return new CValue(t->pointeeType, v);
    }

    case PRIM_pointerToInt : {
        ensureArity(args, 2);
        TypePtr dest = evaluateType(args[0], env);
        if (dest->typeKind != INTEGER_TYPE)
            error(args[0], "invalid integer type");
        PointerTypePtr pt;
        llvm::Value *v = _cgPointer(args[1], env, pt);
        llvm::Value *result = llvmBuilder->CreatePtrToInt(v, llvmType(dest));
        llvmBuilder->CreateStore(result, out->llValue);
        return out;
    }

    case PRIM_intToPointer : {
        ensureArity(args, 2);
        TypePtr pointeeType = evaluateType(args[0], env);
        TypePtr dest = pointerType(pointeeType);
        TypePtr t;
        llvm::Value *v = _cgInteger(args[1], env, t);
        llvm::Value *result = llvmBuilder->CreateIntToPtr(v, llvmType(dest));
        llvmBuilder->CreateStore(result, out->llValue);
        return out;
    }

    case PRIM_makeCodePointer : {
        if (args.size() < 1)
            error("incorrect number of arguments");
        ObjectPtr callable = evaluateStatic(args[0], env);
        switch (callable->objKind) {
        case PROCEDURE :
        case OVERLOADABLE :
            break;
        default :
            error(args[0], "invalid procedure or overloadable");
        }
        const vector<bool> &isStaticFlags =
            lookupIsStaticFlags(callable, args.size()-1);
        vector<ObjectPtr> argsKey;
        vector<LocationPtr> argLocations;
        for (unsigned i = 1; i < args.size(); ++i) {
            if (!isStaticFlags[i-1]) {
                TypePtr t = evaluateType(args[i], env);
                argsKey.push_back(t.ptr());
            }
            else {
                ObjectPtr param = evaluateStatic(args[i], env);
                argsKey.push_back(param);
            }
            argLocations.push_back(args[i]->location);
        }
        InvokeEntryPtr entry;
        switch (callable->objKind) {
        case PROCEDURE : {
            Procedure *y = (Procedure *)callable.ptr();
            entry = codegenProcedure(y, isStaticFlags,
                                     argsKey, argLocations);
            break;
        }
        case OVERLOADABLE : {
            Overloadable *y = (Overloadable *)callable.ptr();
            entry = codegenOverloadable(y, isStaticFlags,
                                        argsKey, argLocations);
            break;
        }
        default :
            assert(false);
        }
        llvmBuilder->CreateStore(entry->llvmFunc, out->llValue);
        return out;
    }

    case PRIM_makeCCodePointer : {
        if (args.size() < 1)
            error("incorrect number of arguments");
        ObjectPtr callable = evaluateStatic(args[0], env);
        switch (callable->objKind) {
        case PROCEDURE :
        case OVERLOADABLE :
            break;
        default :
            error(args[0], "invalid procedure or overloadable");
        }
        const vector<bool> &isStaticFlags =
            lookupIsStaticFlags(callable, args.size()-1);
        vector<ObjectPtr> argsKey;
        vector<LocationPtr> argLocations;
        for (unsigned i = 1; i < args.size(); ++i) {
            if (!isStaticFlags[i-1]) {
                TypePtr t = evaluateType(args[i], env);
                argsKey.push_back(t.ptr());
            }
            else {
                ObjectPtr param = evaluateStatic(args[i], env);
                argsKey.push_back(param);
            }
            argLocations.push_back(args[i]->location);
        }
        InvokeEntryPtr entry;
        string callableName;
        switch (callable->objKind) {
        case PROCEDURE : {
            Procedure *y = (Procedure *)callable.ptr();
            entry = codegenProcedure(y, isStaticFlags,
                                     argsKey, argLocations);
            callableName = y->name->str;
            break;
        }
        case OVERLOADABLE : {
            Overloadable *y = (Overloadable *)callable.ptr();
            entry = codegenOverloadable(y, isStaticFlags,
                                        argsKey, argLocations);
            callableName = y->name->str;
            break;
        }
        default :
            assert(false);
        }
        if (!entry->llvmCWrapper)
            codegenCWrapper(entry, callableName);
        llvmBuilder->CreateStore(entry->llvmCWrapper, out->llValue);
        return out;
    }

    case PRIM_pointerCast : {
        ensureArity(args, 2);
        TypePtr dest = evaluatePointerLikeType(args[0], env);
        TypePtr t;
        llvm::Value *v = _cgPointerLike(args[1], env, t);
        llvm::Value *result = llvmBuilder->CreateBitCast(v, llvmType(dest));
        assert(out.ptr());
        assert(out->type == dest);
        llvmBuilder->CreateStore(result, out->llValue);
        return out;
    }

    case PRIM_array : {
        if (args.empty())
            error("atleast one element required for creating an array");
        PValuePtr pv = analyzeValue(args[0], env);
        TypePtr etype = pv->type;
        int n = (int)args.size();
        assert(out.ptr());
        assert(arrayType(etype, n) == out->type);
        for (unsigned i = 0; i < args.size(); ++i) {
            PValuePtr parg = analyzeValue(args[i], env);
            if (parg->type != etype)
                error(args[i], "array element type mismatch");
            llvm::Value *ptr =
                llvmBuilder->CreateConstGEP2_32(out->llValue, 0, i);
            codegenIntoValue(args[i], env, parg, new CValue(etype, ptr));
        }
        return out;
    }

    case PRIM_arrayRef : {
        ensureArity(args, 2);
        PValuePtr parray = analyzeValue(args[0], env);
        if (parray->type->typeKind != ARRAY_TYPE)
            error(args[0], "expecting array type");
        ArrayType *at = (ArrayType *)parray->type.ptr();
        CValuePtr carray = codegenAsRef(args[0], env, parray);
        TypePtr indexType;
        llvm::Value *i = _cgInteger(args[1], env, indexType);
        vector<llvm::Value *> indices;
        indices.push_back(llvm::ConstantInt::get(llvmIntType(32), 0));
        indices.push_back(i);
        llvm::Value *ptr =
            llvmBuilder->CreateGEP(carray->llValue,
                                   indices.begin(),
                                   indices.end());
        return new CValue(at->elementType, ptr);
    }

    case PRIM_tuple : {
        assert(out.ptr());
        assert(out->type->typeKind == TUPLE_TYPE);
        TupleType *tt = (TupleType *)out->type.ptr();
        ensureArity(args, tt->elementTypes.size());
        for (unsigned i = 0; i < args.size(); ++i) {
            PValuePtr parg = analyzeValue(args[i], env);
            if (parg->type != tt->elementTypes[i])
                error(args[i], "argument type mismatch");
            llvm::Value *ptr =
                llvmBuilder->CreateConstGEP2_32(out->llValue, 0, i);
            CValuePtr cargDest = new CValue(tt->elementTypes[i], ptr);
            codegenIntoValue(args[i], env, parg, cargDest);
        }
        return out;
    }

    case PRIM_tupleRef : {
        ensureArity(args, 2);
        PValuePtr ptuple = analyzeValue(args[0], env);
        if (ptuple->type->typeKind != TUPLE_TYPE)
            error(args[0], "expecting a tuple");
        TupleType *tt = (TupleType *)ptuple->type.ptr();
        CValuePtr ctuple = codegenAsRef(args[0], env, ptuple);
        int i = evaluateInt(args[1], env);
        llvm::Value *ptr =
            llvmBuilder->CreateConstGEP2_32(ctuple->llValue, 0, i);
        return new CValue(tt->elementTypes[i], ptr);
    }

    case PRIM_recordFieldRef : {
        ensureArity(args, 2);
        PValuePtr prec = analyzeValue(args[0], env);
        if (prec->type->typeKind != RECORD_TYPE)
            error(args[0], "expecting a record");
        RecordType *rt = (RecordType *)prec->type.ptr();
        const vector<TypePtr> &fieldTypes = recordFieldTypes(rt);
        CValuePtr crec = codegenAsRef(args[0], env, prec);
        int i = evaluateInt(args[1], env);
        llvm::Value *ptr =
            llvmBuilder->CreateConstGEP2_32(crec->llValue, 0, i);
        return new CValue(fieldTypes[i], ptr);
    }

    case PRIM_recordFieldRefByName : {
        ensureArity(args, 2);
        PValuePtr prec = analyzeValue(args[0], env);
        if (prec->type->typeKind != RECORD_TYPE)
            error(args[0], "expecting a record");
        RecordType *rt = (RecordType *)prec->type.ptr();
        const vector<TypePtr> &fieldTypes = recordFieldTypes(rt);
        CValuePtr crec = codegenAsRef(args[0], env, prec);
        IdentifierPtr fname = evaluateIdentifier(args[1], env);
        const map<string, int> &fieldIndexMap = recordFieldIndexMap(rt);
        map<string, int>::const_iterator fi = fieldIndexMap.find(fname->str);
        if (fi == fieldIndexMap.end())
            error(args[1], "field not in record");
        int i = fi->second;
        llvm::Value *ptr =
            llvmBuilder->CreateConstGEP2_32(crec->llValue, 0, i);
        return new CValue(fieldTypes[i], ptr);
    }

    default :
        assert(false);
        return NULL;
    }
}



//
// codegenMain
//

llvm::Function *codegenMain(ModulePtr module)
{
    llvm::FunctionType *llMainType =
        llvm::FunctionType::get(llvmIntType(32),
                                vector<const llvm::Type *>(),
                                false);
    llvm::Function *llMain =
        llvm::Function::Create(llMainType,
                               llvm::Function::ExternalLinkage,
                               "main",
                               llvmModule);

    llvm::Function *savedLLVMFunction = llvmFunction;
    llvm::IRBuilder<> *savedLLVMInitBuilder = llvmInitBuilder;
    llvm::IRBuilder<> *savedLLVMBuilder = llvmBuilder;

    llvmFunction = llMain;

    llvm::BasicBlock *initBlock = newBasicBlock("initBlock");
    llvm::BasicBlock *codeBlock = newBasicBlock("codeBlock");

    llvmInitBuilder = new llvm::IRBuilder<>(initBlock);
    llvmBuilder = new llvm::IRBuilder<>(codeBlock);

    ObjectPtr mainObj = lookupEnv(module->env, new Identifier("main"));
    if (mainObj->objKind != PROCEDURE)
        error("expecting 'main' to be a procedure");
    ProcedurePtr mainProc = (Procedure *)mainObj.ptr();
    InvokeEntryPtr entry = codegenProcedure(mainProc,
                                            vector<bool>(),
                                            vector<ObjectPtr>(),
                                            vector<LocationPtr>());
    if (!entry->returnType) {
        codegenInvokeCode(entry, vector<ExprPtr>(), new Env(), NULL);
        llvm::Value *zero = llvm::ConstantInt::get(llvmIntType(32), 0);
        llvmBuilder->CreateRet(zero);
    }
    else if (entry->returnIsTemp) {
        CValuePtr result = codegenAllocValue(entry->returnType);
        codegenInvokeCode(entry, vector<ExprPtr>(), new Env(), result);
        if (entry->returnType == int32Type) {
            llvm::Value *v = llvmBuilder->CreateLoad(result->llValue);
            codegenValueDestroy(result);
            llvmBuilder->CreateRet(v);
        }
        else {
            codegenValueDestroy(result);
            llvm::Value *zero = llvm::ConstantInt::get(llvmIntType(32), 0);
            llvmBuilder->CreateRet(zero);
        }
    }
    else {
        CValuePtr result =
            codegenInvokeCode(entry, vector<ExprPtr>(), new Env(), NULL);
        if (entry->returnType == int32Type) {
            llvm::Value *v = llvmBuilder->CreateLoad(result->llValue);
            llvmBuilder->CreateRet(v);
        }
        else {
            llvm::Value *zero = llvm::ConstantInt::get(llvmIntType(32), 0);
            llvmBuilder->CreateRet(zero);
        }
    }
    llvmInitBuilder->CreateBr(codeBlock);

    delete llvmInitBuilder;
    delete llvmBuilder;

    llvmBuilder = savedLLVMBuilder;
    llvmInitBuilder = savedLLVMInitBuilder;
    llvmFunction = savedLLVMFunction;

    return llMain;
}
