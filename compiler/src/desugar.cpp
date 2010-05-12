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

StatementPtr desugarForStatement(ForPtr x) {
    IdentifierPtr exprVar = new Identifier("%expr");
    IdentifierPtr iterVar = new Identifier("%iter");

    BlockPtr block = new Block();
    block->statements.push_back(new Binding(REF, identV(exprVar), x->expr));

    CallPtr iteratorCall = new Call(kernelNameRef("iterator"));
    iteratorCall->args.push_back(new NameRef(exprVar));
    block->statements.push_back(new Binding(VAR, identV(iterVar),
                                            iteratorCall.ptr()));

    CallPtr hasNextCall = new Call(kernelNameRef("hasNext?"));
    hasNextCall->args.push_back(new NameRef(iterVar));
    CallPtr nextCall = new Call(kernelNameRef("next"));
    nextCall->args.push_back(new NameRef(iterVar));
    BlockPtr whileBody = new Block();
    whileBody->statements.push_back(new Binding(REF, identV(x->variable),
                                                nextCall.ptr()));
    whileBody->statements.push_back(x->body);

    block->statements.push_back(new While(hasNextCall.ptr(), whileBody.ptr()));
    return block.ptr();
}
