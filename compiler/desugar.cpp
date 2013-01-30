#include "clay.hpp"
#include "operators.hpp"
#include "evaluator.hpp"
#include "analyzer.hpp"
#include "desugar.hpp"
#include "parser.hpp"
#include "clone.hpp"
#include "objects.hpp"


namespace clay {

ExprPtr desugarCharLiteral(char c, CompilerState* cst) {
    ExprPtr nameRef = operator_expr_charLiteral(cst);
    CallPtr call = new Call(nameRef, new ExprList());
    llvm::SmallString<128> buf;
    llvm::raw_svector_ostream out(buf);
    out << (int)c;
    call->parenArgs->add(new IntLiteral(out.str(), "ss"));
    return call.ptr();
}
    
static ModulePtr dottedImportedModule(FieldRef *x, Module *module) {
    llvm::StringMap<ModuleLookup> *moduleNode = &module->importedModuleNames;
    FieldRef *fieldRefNode = x;
    for (;;) {
        llvm::StringMap<ModuleLookup>::iterator parent = moduleNode->find(fieldRefNode->name->str);
        if (parent == moduleNode->end())
            return NULL;
        moduleNode = &parent->getValue().parents;
        if (fieldRefNode->expr->exprKind == FIELD_REF)
            fieldRefNode = (FieldRef*)fieldRefNode->expr.ptr();
        else
            break;
    }
    if (fieldRefNode->expr->exprKind == NAME_REF) {
        NameRef *nameNode = (NameRef*)fieldRefNode->expr.ptr();
        llvm::StringMap<ModuleLookup>::iterator parent = moduleNode->find(nameNode->name->str);
        if (parent == moduleNode->end())
            return NULL;
        return parent->getValue().module;
    }
    return NULL;
}

void desugarFieldRef(FieldRefPtr x, ModulePtr module) {
    if (x->expr->exprKind == FIELD_REF || x->expr->exprKind == NAME_REF) {
        ModulePtr m = dottedImportedModule(x.ptr(), module.ptr());
        if (m.ptr()) {
            x->isDottedModuleName = true;
            x->desugared = new ObjectExpr(m.ptr());
            return;
        }
    }
    
    ExprListPtr args = new ExprList(x->expr);
    args->add(new ObjectExpr(x->name.ptr()));
    CallPtr call = new Call(operator_expr_fieldRef(module->cst), args);
    call->location = x->location;
    x->desugared = call.ptr();
}

ExprPtr desugarStaticIndexing(StaticIndexingPtr x, CompilerState* cst) {
    ExprListPtr args = new ExprList(x->expr);
    ValueHolderPtr vh = sizeTToValueHolder(x->index, cst);
    args->add(new StaticExpr(new ObjectExpr(vh.ptr())));
    CallPtr call = new Call(operator_expr_staticIndex(cst), args);
    call->location = x->location;
    return call.ptr();
}

ExprPtr lookupCallable(int op, CompilerState* cst) {
    ExprPtr callable;
    switch (op) {
    case DEREFERENCE :
        callable = operator_expr_dereference(cst);
        break;
    case ADDRESS_OF :
        callable = primitive_expr_addressOf(cst);
        break;
    case NOT :
        callable = primitive_expr_boolNot(cst);
        break;
    case PREFIX_OP :
        callable = operator_expr_prefixOperator(cst);
        break;
    case INFIX_OP :
        callable = operator_expr_infixOperator(cst);
        break;
    case IF_EXPR :
        callable = operator_expr_ifExpression(cst);
        break;
    default :
        assert(false);
    }
    return callable;
}

ExprPtr desugarVariadicOp(VariadicOpPtr x, CompilerState* cst) {
    ExprPtr callable = lookupCallable(x->op, cst);
    CallPtr call = new Call(callable, new ExprList());
    call->parenArgs->add(x->exprs);  
    call->location = x->location;
    return call.ptr();
}

static vector<FormalArgPtr> identV(IdentifierPtr x) {
    vector<FormalArgPtr> v;
    v.push_back(new FormalArg(x, NULL));
    return v;
}

static vector<FormalArgPtr> identVtoFormalV(llvm::ArrayRef<IdentifierPtr> x) {
    vector<FormalArgPtr> v;
    for(size_t i=0; i<x.size(); ++i)
        v.push_back(new FormalArg(x[i], NULL));
    return v;
}

//  for (<vars> in <expr>) <body>
// becomes:
//  {
//      forward %expr = <expr>;
//      forward %iter = iterator(%expr);
//      while (var %value = nextValue(%iter); hasValue?(%value)) {
//          forward <vars> = getValue(%value);
//          <body>
//      }
//  }

StatementPtr desugarForStatement(ForPtr x, CompilerState* cst) {
    IdentifierPtr exprVar = Identifier::get("%expr", x->location);
    IdentifierPtr iterVar = Identifier::get("%iter", x->location);
    IdentifierPtr valueVar = Identifier::get("%value", x->location);

    BlockPtr block = new Block();
    block->location = x->body->location;
    vector<StatementPtr> &bs = block->statements;
    BindingPtr exprBinding = new Binding(FORWARD, identV(exprVar), new ExprList(x->expr));
    exprBinding->location = x->body->location;
    bs.push_back(exprBinding.ptr());

    CallPtr iteratorCall = new Call(operator_expr_iterator(cst), new ExprList());
    iteratorCall->location = x->body->location;
    ExprPtr exprName = new NameRef(exprVar);
    exprName->location = x->location;
    iteratorCall->parenArgs->add(exprName);
    BindingPtr iteratorBinding = new Binding(FORWARD,
        identV(iterVar),
        new ExprList(iteratorCall.ptr()));
    iteratorBinding->location = x->body->location;
    bs.push_back(iteratorBinding.ptr());

    vector<StatementPtr> valueStatements;
    CallPtr nextValueCall = new Call(operator_expr_nextValue(cst), new ExprList());
    nextValueCall->location = x->body->location;
    ExprPtr iterName = new NameRef(iterVar);
    iterName->location = x->body->location;
    nextValueCall->parenArgs->add(iterName);
    BindingPtr nextValueBinding = new Binding(VAR,
        identV(valueVar),
        new ExprList(nextValueCall.ptr()));
    nextValueBinding->location = x->body->location;
    valueStatements.push_back(nextValueBinding.ptr());

    CallPtr hasValueCall = new Call(operator_expr_hasValueP(cst), new ExprList());
    hasValueCall->location = x->body->location;
    ExprPtr valueName = new NameRef(valueVar);
    valueName->location = x->body->location;
    hasValueCall->parenArgs->add(valueName);

    CallPtr getValueCall = new Call(operator_expr_getValue(cst), new ExprList());
    getValueCall->location = x->body->location;
    getValueCall->parenArgs->add(valueName);
    BlockPtr whileBody = new Block();
    whileBody->location = x->body->location;
    vector<StatementPtr> &ws = whileBody->statements;
    ws.push_back(new Binding(FORWARD, identVtoFormalV(x->variables), new ExprList(getValueCall.ptr())));
    ws.push_back(x->body);

    StatementPtr whileStmt = new While(valueStatements, hasValueCall.ptr(), whileBody.ptr());
    whileStmt->location = x->location;
    bs.push_back(whileStmt);
    return block.ptr();
}

static void makeExceptionVars(vector<IdentifierPtr>& identifiers, CatchPtr x) {
    identifiers.push_back(x->exceptionVar);
    identifiers.push_back(
            !!x->contextVar ?
                    x->contextVar :
                    Identifier::get("%context", x->location));
}

StatementPtr desugarCatchBlocks(llvm::ArrayRef<CatchPtr> catchBlocks,
                                CompilerState* cst) {
    assert(!catchBlocks.empty());
    Location firstCatchLocation = catchBlocks.front()->location;
    IdentifierPtr expVar = Identifier::get("%exp", firstCatchLocation);

    CallPtr activeException = new Call(primitive_expr_activeException(cst), new ExprList());
    activeException->location = firstCatchLocation;

    BindingPtr expBinding =
        new Binding(VAR,
                    identVtoFormalV(vector<IdentifierPtr>(1, expVar)),
                    new ExprList(activeException.ptr()));
    expBinding->location = firstCatchLocation;

    bool lastWasAny = false;
    IfPtr lastIf;
    StatementPtr result;
    for (size_t i = 0; i < catchBlocks.size(); ++i) {
        CatchPtr x = catchBlocks[i];
        if (lastWasAny)
            error(x, "unreachable catch block");
        if (x->exceptionType.ptr()) {
            ExprListPtr asTypeArgs = new ExprList(x->exceptionType);
            ExprPtr expVarName = new NameRef(expVar);
            expVarName->location = x->exceptionVar->location;
            asTypeArgs->add(expVarName);
            CallPtr cond = new Call(operator_expr_exceptionIsP(cst), asTypeArgs);
            cond->location = x->exceptionType->location;

            BlockPtr block = new Block();
            block->location = x->location;

            vector<IdentifierPtr> identifiers;
            makeExceptionVars(identifiers, x);

            CallPtr getter = new Call(operator_expr_exceptionAs(cst), asTypeArgs);
            getter->location = x->exceptionVar->location;
            BindingPtr binding =
                new Binding(VAR,
                            identVtoFormalV(identifiers),
                            new ExprList(getter.ptr()));
            binding->location = x->exceptionVar->location;

            BindingPtr exceptionVarForRethrow =
                new Binding(REF,
                            identVtoFormalV(vector<IdentifierPtr>(1, Identifier::get("%exception", x->location))),
                            new ExprList(new NameRef(Identifier::get(x->exceptionVar->str, x->location))));
            exceptionVarForRethrow->location = x->exceptionVar->location;

            block->statements.push_back(binding.ptr());
            block->statements.push_back(exceptionVarForRethrow.ptr());
            block->statements.push_back(x->body);

            IfPtr ifStatement = new If(cond.ptr(), block.ptr());
            ifStatement->location = x->location;
            if (!result)
                result = ifStatement.ptr();
            if (lastIf.ptr())
                lastIf->elsePart = ifStatement.ptr();
            lastIf = ifStatement;
        }
        else {
            BlockPtr block = new Block();
            block->location = x->location;

            vector<IdentifierPtr> identifiers;
            makeExceptionVars(identifiers, x);

            ExprListPtr asAnyArgs = new ExprList(new NameRef(expVar));
            CallPtr getter = new Call(operator_expr_exceptionAsAny(cst),
                                      asAnyArgs);
            getter->location = x->exceptionVar->location;
            BindingPtr binding =
                new Binding(VAR,
                            identVtoFormalV(identifiers),
                            new ExprList(getter.ptr()));
            binding->location = x->exceptionVar->location;

            BindingPtr exceptionVarForRethrow =
                new Binding(REF,
                            identVtoFormalV(vector<IdentifierPtr>(1, Identifier::get("%exception", x->location))),
                            new ExprList(new NameRef(Identifier::get(x->exceptionVar->str, x->location))));
            exceptionVarForRethrow->location = x->exceptionVar->location;

            block->statements.push_back(binding.ptr());
            block->statements.push_back(exceptionVarForRethrow.ptr());
            block->statements.push_back(x->body);

            if (!result)
                result = block.ptr();
            if (lastIf.ptr())
                lastIf->elsePart = block.ptr();

            lastWasAny = true;
            lastIf = NULL;
        }
    }
    assert(result.ptr());
    if (!lastWasAny) {
        assert(lastIf.ptr());
        BlockPtr block = new Block();
        block->location = firstCatchLocation;
        ExprPtr expVarName = new NameRef(expVar);
        expVarName->location = firstCatchLocation;
        ExprListPtr continueArgs = new ExprList(expVarName);
        CallPtr continueException = new Call(operator_expr_continueException(cst),
                                             continueArgs);
        continueException->location = firstCatchLocation;
        StatementPtr stmt = new ExprStatement(continueException.ptr());
        stmt->location = firstCatchLocation;
        block->statements.push_back(stmt);
        block->statements.push_back(new Unreachable());
        lastIf->elsePart = block.ptr();
    }

    BlockPtr block = new Block();
    block->location = firstCatchLocation;
    block->statements.push_back(expBinding.ptr());
    block->statements.push_back(result.ptr());

    return block.ptr();
}

void desugarThrow(Throw* thro) {
    if (thro->desugaredExpr != NULL)
        return;

    if (thro->expr != NULL) {
        thro->desugaredExpr = thro->expr;
        thro->desugaredContext = thro->context;
    } else {
        thro->desugaredExpr = new NameRef(Identifier::get("%exception", thro->location));
        thro->desugaredContext = new NameRef(Identifier::get("%context", thro->location));
    }
}

StatementPtr desugarSwitchStatement(SwitchPtr x, CompilerState* cst) {
    BlockPtr block = new Block();
    block->location = x->location;

    block->statements.insert(block->statements.end(),
        x->exprStatements.begin(), x->exprStatements.end());

    // %thing is the value being switched on
    IdentifierPtr thing = Identifier::get("%case", x->expr->location);
    NameRefPtr thingRef = new NameRef(thing);
    thingRef->location = x->expr->location;

    // initialize %case
    {
        BindingPtr b = new Binding(FORWARD, identV(thing), new ExprList(x->expr));
        b->location = x->expr->location;
        block->statements.push_back(b.ptr());
    }

    ExprPtr caseCallable = operator_expr_caseP(cst);

    StatementPtr root;
    StatementPtr *nextPtr = &root;

    // dispatch logic
    for (size_t i = 0; i < x->caseBlocks.size(); ++i) {
        CaseBlockPtr caseBlock = x->caseBlocks[i];

        ExprListPtr caseArgs = new ExprList(thingRef.ptr());
        ExprPtr caseCompareArgs = new Paren(caseBlock->caseLabels);
        caseCompareArgs->location = caseBlock->location;
        caseArgs->add(caseCompareArgs);

        ExprPtr condition = new Call(caseCallable, caseArgs);
        condition->location = caseBlock->location;

        IfPtr ifStmt = new If(condition, caseBlock->body);
        ifStmt->location = caseBlock->location;
        *nextPtr = ifStmt.ptr();
        nextPtr = &(ifStmt->elsePart);
    }

    if (x->defaultCase.ptr())
        *nextPtr = x->defaultCase;
    else
        *nextPtr = NULL;

    block->statements.push_back(root);

    return block.ptr();
}

static SourcePtr evalToSource(Location const &location, ExprListPtr args, 
                              EnvPtr env, CompilerState* cst)
{
    llvm::SmallString<128> sourceTextBuf;
    llvm::raw_svector_ostream sourceTextOut(sourceTextBuf);
    MultiStaticPtr values = evaluateMultiStatic(args, env, cst);
    for (size_t i = 0; i < values->size(); ++i) {
        printStaticName(sourceTextOut, values->values[i]);
    }

    llvm::SmallString<128> sourceNameBuf;
    llvm::raw_svector_ostream sourceNameOut(sourceNameBuf);
    sourceNameOut << "<eval ";
    printFileLineCol(sourceNameOut, location);
    sourceNameOut << ">";

    sourceTextOut.flush();
    return new Source(sourceNameOut.str(),
        llvm::MemoryBuffer::getMemBufferCopy(sourceTextBuf));
}

ExprListPtr desugarEvalExpr(EvalExprPtr eval, EnvPtr env, CompilerState* cst)
{
    if (eval->evaled)
        return eval->value;
    else {
        SourcePtr source = evalToSource(eval->location, new ExprList(eval->args), env, cst);
        eval->value = parseExprList(source, 0, unsigned(source->size()), cst);
        eval->evaled = true;
        return eval->value;
    }
}

llvm::ArrayRef<StatementPtr> desugarEvalStatement(EvalStatementPtr eval, EnvPtr env,
                                                  CompilerState* cst)
{
    if (eval->evaled)
        return eval->value;
    else {
        SourcePtr source = evalToSource(eval->location, eval->args, env, cst);
        parseStatements(source, 0, unsigned(source->size()), eval->value, cst);
        eval->evaled = true;
        return eval->value;
    }
}

llvm::ArrayRef<TopLevelItemPtr> desugarEvalTopLevel(EvalTopLevelPtr eval, EnvPtr env,
                                                    CompilerState* cst)
{
    if (eval->evaled)
        return eval->value;
    else {
        SourcePtr source = evalToSource(eval->location, eval->args, env, cst);
        parseTopLevelItems(source, 0, unsigned(source->size()), eval->value, 
                           eval->module, cst);
        eval->evaled = true;
        return eval->value;
    }
}

OverloadPtr desugarAsOverload(OverloadPtr &x, CompilerState* cst) {
    assert(x->hasAsConversion);

    //Generate specialised overload
    CodePtr code = new Code();
    code->location = x->location;
    clone(x->code->patternVars, code->patternVars);
    
    //Generate stub body
    CallPtr returnExpr = new Call(x->target.ptr(), new ExprList());
    returnExpr->location = x->code->body->location;

    //Add patterns as static args
    for (unsigned i = 0; i < x->code->patternVars.size(); ++i) {
        llvm::SmallString<128> buf;
        llvm::raw_svector_ostream sout(buf);
        sout << "%asArg" << i;
        IdentifierPtr argName = Identifier::get(sout.str());

        ExprPtr staticName =
            new ForeignExpr("prelude",
                            new NameRef(Identifier::get("Static")));

        IndexingPtr indexing = new Indexing(staticName, new ExprList(new NameRef(x->code->patternVars[i].name)));
        indexing->startLocation = code->patternVars[i].name->location;
        indexing->endLocation = code->patternVars[i].name->location;

        FormalArgPtr arg = new FormalArg(argName, indexing.ptr());
        arg->location = x->code->patternVars[i].name->location;
        code->formalArgs.insert(code->formalArgs.begin(), arg);

        //Add patterns as static call args
        ExprPtr callArg = new NameRef(x->code->patternVars[i].name);
        returnExpr->parenArgs->add(callArg);
    }

    //Add args
    for (unsigned i = 0; i < x->code->formalArgs.size(); ++i) {
        FormalArgPtr arg = clone(x->code->formalArgs[i]);
        arg->tempness = TEMPNESS_FORWARD;
        if (x->code->formalArgs[i]->asArg) {
            arg->type = x->code->formalArgs[i]->asType;
            ExprPtr typeArg = x->code->formalArgs[i]->asType;
            CallPtr callArg = new Call(operator_expr_asExpression(cst), new ExprList());
            callArg->parenArgs->add(new NameRef(x->code->formalArgs[i]->name));
            callArg->parenArgs->add(x->code->formalArgs[i]->asType);
            callArg->location = x->code->formalArgs[i]->location;
            returnExpr->parenArgs->add(callArg.ptr());
        } else {
            ExprPtr arg0 = new NameRef(x->code->formalArgs[i]->name);
            arg0->location = x->code->formalArgs[i]->location;
            returnExpr->parenArgs->add(arg0);
        }
        code->formalArgs.push_back(arg.ptr());
    }

    code->body = x->code->body;
    OverloadPtr spec = new Overload(x->module, x->target, code, x->callByName, x->isInline);
    spec->location = x->location;
    spec->env = x->env;

    x->code->body = new Return(RETURN_FORWARD, new ExprList(new Unpack(returnExpr.ptr())), true);
    x->isInline = FORCE_INLINE;
    return spec;
}

}
