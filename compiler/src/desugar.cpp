#include "clay.hpp"

ExprPtr desugarCharLiteral(char c) {
    ExprPtr nameRef = kernelNameRef("Char");
    CallPtr call = new Call(nameRef);
    ostringstream out;
    out << (int)c;
    call->args.push_back(new IntLiteral(out.str(), "i8"));
    return call.ptr();
}

ExprPtr desugarUnaryOp(UnaryOpPtr x) {
    ExprPtr callable;
    switch (x->op) {
    case DEREFERENCE :
        callable = kernelNameRef("dereference");
        break;
    case ADDRESS_OF :
        callable = primNameRef("addressOf");
        break;
    case PLUS :
        callable = kernelNameRef("plus");
        break;
    case MINUS :
        callable = kernelNameRef("minus");
        break;
    case NOT :
        callable = primNameRef("boolNot");
        break;
    default :
        assert(false);
    }
    CallPtr call = new Call(callable);
    call->args.push_back(x->expr);
    return call.ptr();
}

ExprPtr desugarBinaryOp(BinaryOpPtr x) {
    ExprPtr callable;
    switch (x->op) {
    case ADD :
        callable = kernelNameRef("add");
        break;
    case SUBTRACT :
        callable = kernelNameRef("subtract");
        break;
    case MULTIPLY :
        callable = kernelNameRef("multiply");
        break;
    case DIVIDE :
        callable = kernelNameRef("divide");
        break;
    case REMAINDER :
        callable = kernelNameRef("remainder");
        break;
    case EQUALS :
        callable = kernelNameRef("equals?");
        break;
    case NOT_EQUALS :
        callable = kernelNameRef("notEquals?");
        break;
    case LESSER :
        callable = kernelNameRef("lesser?");
        break;
    case LESSER_EQUALS :
        callable = kernelNameRef("lesserEquals?");
        break;
    case GREATER :
        callable = kernelNameRef("greater?");
        break;
    case GREATER_EQUALS :
        callable = kernelNameRef("greaterEquals?");
        break;
    default :
        assert(false);
    }
    CallPtr call = new Call(callable);
    call->args.push_back(x->expr1);
    call->args.push_back(x->expr2);
    return call.ptr();
}

ExprPtr desugarNew(NewPtr x) {
    ExprPtr callable = kernelNameRef("allocateShared");
    CallPtr call = new Call(callable);
    call->args.push_back(x->expr);
    return call.ptr();
}

ExprPtr desugarStaticExpr(StaticExprPtr x) {
    ExprPtr callable = kernelNameRef("wrapStatic");
    CallPtr call = new Call(callable);
    call->args.push_back(x->expr);
    return call.ptr();
}

const char *updateOperatorName(int op) {
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


static vector<IdentifierPtr> identV(IdentifierPtr x) {
    vector<IdentifierPtr> v;
    v.push_back(x);
    return v;
}

static vector<ExprPtr> exprV(ExprPtr x) {
    vector<ExprPtr> v;
    v.push_back(x);
    return v;
}

StatementPtr desugarForStatement(ForPtr x) {
    IdentifierPtr exprVar = new Identifier("%expr");
    IdentifierPtr iterVar = new Identifier("%iter");

    BlockPtr block = new Block();
    vector<StatementPtr> &bs = block->statements;
    bs.push_back(new Binding(REF, identV(exprVar), exprV(x->expr)));

    CallPtr iteratorCall = new Call(kernelNameRef("iterator"));
    iteratorCall->args.push_back(new NameRef(exprVar));
    bs.push_back(new Binding(VAR, identV(iterVar), exprV(iteratorCall.ptr())));

    CallPtr hasNextCall = new Call(kernelNameRef("hasNext?"));
    hasNextCall->args.push_back(new NameRef(iterVar));
    CallPtr nextCall = new Call(kernelNameRef("next"));
    nextCall->args.push_back(new NameRef(iterVar));
    ExprPtr unpackNext = new Unpack(nextCall.ptr());
    BlockPtr whileBody = new Block();
    vector<StatementPtr> &ws = whileBody->statements;
    ws.push_back(new Binding(REF, x->variables, exprV(unpackNext)));
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
            vector<ExprPtr> typeArg(1, x->exceptionType);
            CallPtr cond = new Call(kernelNameRef("exceptionIs?"), typeArg);
            cond->location = x->exceptionType->location;

            BlockPtr block = new Block();
            CallPtr getter = new Call(kernelNameRef("exceptionAs"), typeArg);
            getter->location = x->exceptionVar->location;
            BindingPtr binding =
                new Binding(VAR,
                            vector<IdentifierPtr>(1, x->exceptionVar),
                            vector<ExprPtr>(1, getter.ptr()));
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
            CallPtr getter = new Call(kernelNameRef("exceptionAsAny"));
            getter->location = x->exceptionVar->location;
            BindingPtr binding =
                new Binding(VAR,
                            vector<IdentifierPtr>(1, x->exceptionVar),
                            vector<ExprPtr>(1, getter.ptr()));
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
        CallPtr continueException = new Call(kernelNameRef("continueException"));
        lastIf->elsePart = new ExprStatement(continueException.ptr());
    }
    return result;
}
