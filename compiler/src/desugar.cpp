#include "clay.hpp"

namespace clay {

ExprPtr desugarCharLiteral(char c) {
    ExprPtr nameRef = operator_expr_charLiteral();
    CallPtr call = new Call(nameRef, new ExprList());
    ostringstream out;
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

ExprPtr desugarUnaryOp(UnaryOpPtr x) {
    ExprPtr callable;
    switch (x->op) {
    case DEREFERENCE :
        callable = operator_expr_dereference();
        break;
    case ADDRESS_OF :
        callable = primitive_expr_addressOf();
        break;
    case PLUS :
        callable = operator_expr_plus();
        break;
    case MINUS :
        callable = operator_expr_minus();
        break;
    case NOT :
        callable = primitive_expr_boolNot();
        break;
    default :
        assert(false);
    }
    CallPtr call = new Call(callable, new ExprList());
    call->location = x->location;
    call->parenArgs->add(x->expr);
    return call.ptr();
}

ExprPtr desugarBinaryOp(BinaryOpPtr x) {
    ExprPtr callable;
    switch (x->op) {
    case EQUALS :
        callable = operator_expr_equalsP();
        break;
    case NOT_EQUALS :
        callable = operator_expr_notEqualsP();
        break;
    case LESSER :
        callable = operator_expr_lesserP();
        break;
    case LESSER_EQUALS :
        callable = operator_expr_lesserEqualsP();
        break;
    case GREATER :
        callable = operator_expr_greaterP();
        break;
    case GREATER_EQUALS :
        callable = operator_expr_greaterEqualsP();
        break;
    default :
        assert(false);
    }
    CallPtr call = new Call(callable, new ExprList());
    call->location = x->location;
    call->parenArgs->add(x->expr1);
    call->parenArgs->add(x->expr2);
    return call.ptr();
}

ExprPtr desugarVariadicOp(VariadicOpPtr x) {
    ExprPtr callable;
    switch (x->op) {
    case ADD :
        callable = operator_expr_add();
        break;
    case SUBTRACT :
        callable = operator_expr_subtract();
        break;
    case MULTIPLY :
        callable = operator_expr_multiply();
        break;
    case DIVIDE :
        callable = operator_expr_divide();
        break;
    case REMAINDER :
        callable = operator_expr_remainder();
        break;
    case CAT :
        callable = operator_expr_cat();
        break;
    case QUOTIENT :
        callable = operator_expr_quotient();
        break;
    default :
        assert(false);
    }
    CallPtr call = new Call(callable, new ExprList());
    call->location = x->location;
    call->parenArgs->add(x->exprs);
    return call.ptr();
}

ExprPtr desugarIfExpr(IfExprPtr x) {
    ExprPtr callable = operator_expr_ifExpression();
    CallPtr call = new Call(callable, new ExprList());
    call->parenArgs->add(x->condition);
    call->parenArgs->add(x->thenPart);
    call->parenArgs->add(x->elsePart);
    return call.ptr();
}

ExprPtr updateOperatorExpr(int op) {
    switch (op) {
    case UPDATE_ADD : return operator_expr_add();
    case UPDATE_SUBTRACT : return operator_expr_subtract();
    case UPDATE_MULTIPLY : return operator_expr_multiply();
    case UPDATE_DIVIDE : return operator_expr_divide();
    case UPDATE_QUOTIENT : return operator_expr_quotient();
    case UPDATE_REMAINDER : return operator_expr_remainder();
    case UPDATE_CAT : return operator_expr_cat();
    default :
        assert(false);
        return NULL;
    }
}


static vector<IdentifierPtr> identV(IdentifierPtr x) {
    vector<IdentifierPtr> v;
    v.push_back(x);
    return v;
}

StatementPtr desugarForStatement(ForPtr x) {
    IdentifierPtr exprVar = new Identifier("%expr");
    exprVar->location = x->location;
    IdentifierPtr iterVar = new Identifier("%iter");
    iterVar->location = x->location;

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
    BindingPtr iteratorBinding = new Binding(VAR,
        identV(iterVar),
        new ExprList(iteratorCall.ptr()));
    iteratorBinding->location = x->body->location;
    bs.push_back(iteratorBinding.ptr());

    CallPtr hasNextCall = new Call(operator_expr_hasNextP(), new ExprList());
    hasNextCall->location = x->body->location;

    ExprPtr iterName = new NameRef(iterVar);
    iterName->location = x->body->location;
    hasNextCall->parenArgs->add(iterName);
    CallPtr nextCall = new Call(operator_expr_next(), new ExprList());
    nextCall->location = x->body->location;
    nextCall->parenArgs->add(iterName);
    ExprPtr unpackNext = new Unpack(nextCall.ptr());
    unpackNext->location = x->body->location;
    BlockPtr whileBody = new Block();
    whileBody->location = x->body->location;
    vector<StatementPtr> &ws = whileBody->statements;
    ws.push_back(new Binding(FORWARD, x->variables, new ExprList(unpackNext)));
    ws.push_back(x->body);

    StatementPtr whileStmt = new While(hasNextCall.ptr(), whileBody.ptr());
    whileStmt->location = x->location;
    bs.push_back(whileStmt);
    return block.ptr();
}

StatementPtr desugarCatchBlocks(const vector<CatchPtr> &catchBlocks) {
    assert(!catchBlocks.empty());
    LocationPtr firstCatchLocation = catchBlocks.front()->location;
    IdentifierPtr expVar = new Identifier("%exp");

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

    // %thing is the value being switched on
    IdentifierPtr thing = new Identifier("%case");
    thing->location = x->expr->location;
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

static SourcePtr evalToSource(LocationPtr location, ExprListPtr args, EnvPtr env)
{
    ostringstream sourceTextOut;
    MultiStaticPtr values = evaluateMultiStatic(args, env);
    for (unsigned i = 0; i < values->size(); ++i) {
        printStaticName(sourceTextOut, values->values[i]);
    }

    ostringstream sourceNameOut;
    sourceNameOut << "<eval ";
    printFileLineCol(sourceNameOut, location);
    sourceNameOut << ">";

    string sourceText = sourceTextOut.str();
    char *sourceTextData = new char[sourceText.size() + 1];
    strcpy(sourceTextData, sourceText.c_str());

    return new Source(sourceNameOut.str(), sourceTextData, sourceText.size());
}

ExprListPtr desugarEvalExpr(EvalExprPtr eval, EnvPtr env)
{
    if (eval->evaled)
        return eval->value;
    else {
        SourcePtr source = evalToSource(eval->location, new ExprList(eval->args), env);
        eval->value = parseExprList(source, 0, source->size);
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
        parseStatements(source, 0, source->size, eval->value);
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
        parseTopLevelItems(source, 0, source->size, eval->value);
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
