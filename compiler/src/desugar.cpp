#include "clay.hpp"

namespace clay {

ExprPtr desugarCharLiteral(char c) {
    ExprPtr nameRef = operator_expr_charLiteral();
    CallPtr call = new Call(nameRef, new ExprList());
    llvm::SmallString<128> buf;
    llvm::raw_svector_ostream out(buf);
    out << (int)c;
    call->parenArgs->add(new IntLiteral(out.str(), "ss"));
    return call.ptr();
}

ExprPtr desugarFieldRef(FieldRefPtr x) {
    ExprListPtr args = new ExprList(x->expr);
    args->add(new ObjectExpr(x->name.ptr()));
    CallPtr call = new Call(operator_expr_fieldRef(), args);
    call->location = x->location;
    return call.ptr();
}

ExprPtr desugarStaticIndexing(StaticIndexingPtr x) {
    ExprListPtr args = new ExprList(x->expr);
    ValueHolderPtr vh = sizeTToValueHolder(x->index);
    args->add(new StaticExpr(new ObjectExpr(vh.ptr())));
    CallPtr call = new Call(operator_expr_staticIndex(), args);
    call->location = x->location;
    return call.ptr();
}

ExprPtr lookupCallable(int op) {
    ExprPtr callable;
    switch (op) {
    case DEREFERENCE :
        callable = operator_expr_dereference();
        break;
    case ADDRESS_OF :
        callable = primitive_expr_addressOf();
        break;
    case NOT :
        callable = primitive_expr_boolNot();
        break;
    case PREFIX_OP :
        callable = operator_expr_prefixOperator();
        break;
    case INFIX_OP :
        callable = operator_expr_infixOperator();
        break;
    case IF_EXPR :
        callable = operator_expr_ifExpression();
        break;
    default :
        assert(false);
    }
    return callable;
}

ExprPtr desugarVariadicOp(VariadicOpPtr x) {
    ExprPtr callable = lookupCallable(x->op);
    CallPtr call = new Call(callable, new ExprList());
    call->parenArgs->add(x->exprs);  
    call->location = x->location;
    return call.ptr();
}

static vector<IdentifierPtr> identV(IdentifierPtr x) {
    vector<IdentifierPtr> v;
    v.push_back(x);
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

StatementPtr desugarForStatement(ForPtr x) {
    IdentifierPtr exprVar = Identifier::get("%expr", x->location);
    IdentifierPtr iterVar = Identifier::get("%iter", x->location);
    IdentifierPtr valueVar = Identifier::get("%value", x->location);

    BlockPtr block = new Block();
    block->location = x->body->location;
    vector<StatementPtr> &bs = block->statements;
    BindingPtr exprBinding = new Binding(FORWARD, identV(exprVar), new ExprList(x->expr));
    exprBinding->location = x->body->location;
    bs.push_back(exprBinding.ptr());

    CallPtr iteratorCall = new Call(operator_expr_iterator(), new ExprList());
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
    CallPtr nextValueCall = new Call(operator_expr_nextValue(), new ExprList());
    nextValueCall->location = x->body->location;
    ExprPtr iterName = new NameRef(iterVar);
    iterName->location = x->body->location;
    nextValueCall->parenArgs->add(iterName);
    BindingPtr nextValueBinding = new Binding(VAR,
        identV(valueVar),
        new ExprList(nextValueCall.ptr()));
    nextValueBinding->location = x->body->location;
    valueStatements.push_back(nextValueBinding.ptr());

    CallPtr hasValueCall = new Call(operator_expr_hasValueP(), new ExprList());
    hasValueCall->location = x->body->location;
    ExprPtr valueName = new NameRef(valueVar);
    valueName->location = x->body->location;
    hasValueCall->parenArgs->add(valueName);

    CallPtr getValueCall = new Call(operator_expr_getValue(), new ExprList());
    getValueCall->location = x->body->location;
    getValueCall->parenArgs->add(valueName);
    BlockPtr whileBody = new Block();
    whileBody->location = x->body->location;
    vector<StatementPtr> &ws = whileBody->statements;
    ws.push_back(new Binding(FORWARD, x->variables, new ExprList(getValueCall.ptr())));
    ws.push_back(x->body);

    StatementPtr whileStmt = new While(valueStatements, hasValueCall.ptr(), whileBody.ptr());
    whileStmt->location = x->location;
    bs.push_back(whileStmt);
    return block.ptr();
}

StatementPtr desugarCatchBlocks(const vector<CatchPtr> &catchBlocks) {
    assert(!catchBlocks.empty());
    Location firstCatchLocation = catchBlocks.front()->location;
    IdentifierPtr expVar = Identifier::get("%exp", firstCatchLocation);

    CallPtr activeException = new Call(primitive_expr_activeException(), new ExprList());
    activeException->location = firstCatchLocation;

    BindingPtr expBinding =
        new Binding(VAR,
                    vector<IdentifierPtr>(1, expVar),
                    new ExprList(activeException.ptr()));
    expBinding->location = firstCatchLocation;

    bool lastWasAny = false;
    IfPtr lastIf;
    StatementPtr result;
    for (unsigned i = 0; i < catchBlocks.size(); ++i) {
        CatchPtr x = catchBlocks[i];
        if (lastWasAny)
            error(x, "unreachable catch block");
        if (x->exceptionType.ptr()) {
            ExprListPtr asTypeArgs = new ExprList(x->exceptionType);
            ExprPtr expVarName = new NameRef(expVar);
            expVarName->location = x->exceptionVar->location;
            asTypeArgs->add(expVarName);
            CallPtr cond = new Call(operator_expr_exceptionIsP(), asTypeArgs);
            cond->location = x->exceptionType->location;

            BlockPtr block = new Block();
            block->location = x->location;
            CallPtr getter = new Call(operator_expr_exceptionAs(), asTypeArgs);
            getter->location = x->exceptionVar->location;
            BindingPtr binding =
                new Binding(VAR,
                            vector<IdentifierPtr>(1, x->exceptionVar),
                            new ExprList(getter.ptr()));
            binding->location = x->exceptionVar->location;

            block->statements.push_back(binding.ptr());
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
            ExprListPtr asAnyArgs = new ExprList(new NameRef(expVar));
            CallPtr getter = new Call(operator_expr_exceptionAsAny(),
                                      asAnyArgs);
            getter->location = x->exceptionVar->location;
            BindingPtr binding =
                new Binding(VAR,
                            vector<IdentifierPtr>(1, x->exceptionVar),
                            new ExprList(getter.ptr()));
            binding->location = x->exceptionVar->location;
            block->statements.push_back(binding.ptr());
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
        CallPtr continueException = new Call(operator_expr_continueException(),
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

StatementPtr desugarSwitchStatement(SwitchPtr x) {
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

    ExprPtr caseCallable = operator_expr_caseP();

    StatementPtr root;
    StatementPtr *nextPtr = &root;

    // dispatch logic
    for (unsigned i = 0; i < x->caseBlocks.size(); ++i) {
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

static SourcePtr evalToSource(Location const &location, ExprListPtr args, EnvPtr env)
{
    llvm::SmallString<128> sourceTextBuf;
    llvm::raw_svector_ostream sourceTextOut(sourceTextBuf);
    MultiStaticPtr values = evaluateMultiStatic(args, env);
    for (unsigned i = 0; i < values->size(); ++i) {
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

ExprListPtr desugarEvalExpr(EvalExprPtr eval, EnvPtr env)
{
    if (eval->evaled)
        return eval->value;
    else {
        SourcePtr source = evalToSource(eval->location, new ExprList(eval->args), env);
        eval->value = parseExprList(source, 0, source->size());
        eval->evaled = true;
        return eval->value;
    }
}

vector<StatementPtr> const &desugarEvalStatement(EvalStatementPtr eval, EnvPtr env)
{
    if (eval->evaled)
        return eval->value;
    else {
        SourcePtr source = evalToSource(eval->location, eval->args, env);
        parseStatements(source, 0, source->size(), eval->value);
        eval->evaled = true;
        return eval->value;
    }
}

vector<TopLevelItemPtr> const &desugarEvalTopLevel(EvalTopLevelPtr eval, EnvPtr env)
{
    if (eval->evaled)
        return eval->value;
    else {
        SourcePtr source = evalToSource(eval->location, eval->args, env);
        parseTopLevelItems(source, 0, source->size(), eval->value);
        eval->evaled = true;
        return eval->value;
    }
}

static StatementPtr desugarWithBlock(WithStatementPtr with,
        const vector<StatementPtr> & statements, unsigned int i)
{
    ++i;
    BlockPtr b = new Block();
    for (; i < statements.size(); ++i) {
        StatementPtr stmt = statements[i];
        if (stmt->stmtKind == WITH) {
            b->statements.push_back(desugarWithBlock((WithStatement *)stmt.ptr(), statements, i));
            break;
        }
        b->statements.push_back(statements[i]);
    }

    vector<FormalArgPtr> formalArgs;
    for(int i = 0; i < with->lhs.size(); i++) {
        formalArgs.push_back(new FormalArg(with->lhs.at(i), NULL, TEMPNESS_DONTCARE));
    }

    FormalArgPtr formalVarArg = NULL;
    bool captureByRef = false;

    ExprPtr la = new Lambda(captureByRef, formalArgs, formalVarArg, b.ptr());
    la->location = with->withLocation;


    if (with->rhs->exprKind != CALL) {
        error("the right hand side of a with statement must be a call.");
    }

    CallPtr call = (Call*)with->rhs.ptr();

    //prepend the lambda to the arguments for monad
    call->parenArgs->exprs.insert(call->parenArgs->exprs.begin(),la);

    //form the return yield expression

    ExprListPtr rexprs = new ExprList();

    rexprs->add(new Unpack(call.ptr()));

    StatementPtr r = new Return(RETURN_VALUE, rexprs);
    r->location = with->location;

    return r.ptr();
}

BlockPtr desugarBlock(BlockPtr block)
{
    for (unsigned i = 0; i < block->statements.size(); ++i) {
        StatementPtr stmt = block->statements[i];
        if (stmt->stmtKind == WITH) {
            StatementPtr stn = desugarWithBlock((WithStatement *)stmt.ptr(), block->statements, i);
            block->statements[i] = stn;
            block->statements.resize( i + 1);
            break;
        }
    }
    return block;
}

}
