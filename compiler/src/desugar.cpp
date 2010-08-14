#include "clay.hpp"
#include "libclaynames.hpp"

ExprPtr desugarCharLiteral(char c) {
    ExprPtr nameRef = prelude_expr_Char();
    CallPtr call = new Call(nameRef, new ExprList());
    ostringstream out;
    out << (int)c;
    call->args->add(new IntLiteral(out.str(), "i8"));
    return call.ptr();
}

ExprPtr desugarFieldRef(FieldRefPtr x) {
    ExprListPtr args = new ExprList(x->expr);
    args->add(new ObjectExpr(x->name.ptr()));
    return new Call(prelude_expr_fieldRef(), args);
}

ExprPtr desugarStaticIndexing(StaticIndexingPtr x) {
    ExprListPtr args = new ExprList(x->expr);
    ValueHolderPtr vh = sizeTToValueHolder(x->index);
    args->add(new StaticExpr(new ObjectExpr(vh.ptr())));
    return new Call(prelude_expr_staticIndex(), args);
}

ExprPtr desugarUnaryOp(UnaryOpPtr x) {
    ExprPtr callable;
    switch (x->op) {
    case DEREFERENCE :
        callable = prelude_expr_dereference();
        break;
    case ADDRESS_OF :
        callable = primitive_expr_addressOf();
        break;
    case PLUS :
        callable = prelude_expr_plus();
        break;
    case MINUS :
        callable = prelude_expr_minus();
        break;
    case NOT :
        callable = primitive_expr_boolNot();
        break;
    default :
        assert(false);
    }
    CallPtr call = new Call(callable, new ExprList());
    call->args->add(x->expr);
    return call.ptr();
}

ExprPtr desugarBinaryOp(BinaryOpPtr x) {
    ExprPtr callable;
    switch (x->op) {
    case ADD :
        callable = prelude_expr_add();
        break;
    case SUBTRACT :
        callable = prelude_expr_subtract();
        break;
    case MULTIPLY :
        callable = prelude_expr_multiply();
        break;
    case DIVIDE :
        callable = prelude_expr_divide();
        break;
    case REMAINDER :
        callable = prelude_expr_remainder();
        break;
    case EQUALS :
        callable = prelude_expr_equalsP();
        break;
    case NOT_EQUALS :
        callable = prelude_expr_notEqualsP();
        break;
    case LESSER :
        callable = prelude_expr_lesserP();
        break;
    case LESSER_EQUALS :
        callable = prelude_expr_lesserEqualsP();
        break;
    case GREATER :
        callable = prelude_expr_greaterP();
        break;
    case GREATER_EQUALS :
        callable = prelude_expr_greaterEqualsP();
        break;
    default :
        assert(false);
    }
    CallPtr call = new Call(callable, new ExprList());
    call->args->add(x->expr1);
    call->args->add(x->expr2);
    return call.ptr();
}

ExprPtr desugarNew(NewPtr x) {
    ExprPtr callable = prelude_expr_allocateShared();
    CallPtr call = new Call(callable, new ExprList());
    call->args->add(x->expr);
    return call.ptr();
}

ExprPtr desugarStaticExpr(StaticExprPtr x) {
    ExprPtr callable = prelude_expr_wrapStatic();
    CallPtr call = new Call(callable, new ExprList());
    call->args->add(x->expr);
    return call.ptr();
}

ExprPtr updateOperatorExpr(int op) {
    switch (op) {
    case UPDATE_ADD : return prelude_expr_addAssign();
    case UPDATE_SUBTRACT : return prelude_expr_subtractAssign();
    case UPDATE_MULTIPLY : return prelude_expr_multiplyAssign();
    case UPDATE_DIVIDE : return prelude_expr_divideAssign();
    case UPDATE_REMAINDER : return prelude_expr_remainderAssign();
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
    IdentifierPtr iterVar = new Identifier("%iter");

    BlockPtr block = new Block();
    vector<StatementPtr> &bs = block->statements;
    bs.push_back(new Binding(REF, identV(exprVar), new ExprList(x->expr)));

    CallPtr iteratorCall = new Call(prelude_expr_iterator(), new ExprList());
    iteratorCall->args->add(new NameRef(exprVar));
    bs.push_back(new Binding(VAR,
                             identV(iterVar),
                             new ExprList(iteratorCall.ptr())));

    CallPtr hasNextCall = new Call(prelude_expr_hasNextP(), new ExprList());
    hasNextCall->args->add(new NameRef(iterVar));
    CallPtr nextCall = new Call(prelude_expr_next(), new ExprList());
    nextCall->args->add(new NameRef(iterVar));
    ExprPtr unpackNext = new Unpack(nextCall.ptr());
    BlockPtr whileBody = new Block();
    vector<StatementPtr> &ws = whileBody->statements;
    ws.push_back(new Binding(REF, x->variables, new ExprList(unpackNext)));
    ws.push_back(x->body);

    bs.push_back(new While(hasNextCall.ptr(), whileBody.ptr()));
    return block.ptr();
}

StatementPtr desugarCatchBlocks(const vector<CatchPtr> &catchBlocks) {
    bool lastWasAny = false;
    IfPtr lastIf;
    StatementPtr result;
    for (unsigned i = 0; i < catchBlocks.size(); ++i) {
        CatchPtr x = catchBlocks[i];
        if (lastWasAny)
            error(x, "unreachable catch block");
        if (x->exceptionType.ptr()) {
            ExprListPtr typeArg = new ExprList(x->exceptionType);
            CallPtr cond = new Call(prelude_expr_exceptionIsP(), typeArg);
            cond->location = x->exceptionType->location;

            BlockPtr block = new Block();
            CallPtr getter = new Call(prelude_expr_exceptionAs(), typeArg);
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
            CallPtr getter = new Call(prelude_expr_exceptionAsAny(),
                                      new ExprList());
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
        CallPtr continueException = new Call(prelude_expr_continueException(),
                                             new ExprList());
        lastIf->elsePart = new ExprStatement(continueException.ptr());
    }
    return result;
}
