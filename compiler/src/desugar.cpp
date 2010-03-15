#include "clay.hpp"

ExprPtr desugarCharLiteral(char c) {
    ExprPtr nameRef = kernelNameRef("Char");
    CallPtr call = new Call(nameRef);
    ostringstream out;
    out << (int)c;
    call->args.push_back(new IntLiteral(out.str(), "i8"));
    return call.ptr();
}

ExprPtr desugarStringLiteral(const string &s) {
    ArrayPtr charArray = new Array();
    for (unsigned i = 0; i < s.size(); ++i)
        charArray->args.push_back(desugarCharLiteral(s[i]));
    ExprPtr nameRef = kernelNameRef("String");
    CallPtr call = new Call(nameRef);
    call->args.push_back(charArray.ptr());
    return call.ptr();
}

ExprPtr desugarTuple(TuplePtr x) {
    if (x->args.size() == 1)
        return x->args[0];
    return new Call(primNameRef("tuple"), x->args);
}

ExprPtr desugarArray(ArrayPtr x) {
    return new Call(primNameRef("array"), x->args);
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

StatementPtr desugarForStatement(ForPtr x) {
    IdentifierPtr exprVar = new Identifier("%expr");
    IdentifierPtr iterVar = new Identifier("%iter");

    BlockPtr block = new Block();
    block->statements.push_back(new Binding(REF, exprVar, x->expr));

    CallPtr iteratorCall = new Call(kernelNameRef("iterator"));
    iteratorCall->args.push_back(new NameRef(exprVar));
    block->statements.push_back(new Binding(VAR, iterVar, iteratorCall.ptr()));

    CallPtr hasNextCall = new Call(kernelNameRef("hasNext?"));
    hasNextCall->args.push_back(new NameRef(iterVar));
    CallPtr nextCall = new Call(kernelNameRef("next"));
    nextCall->args.push_back(new NameRef(iterVar));
    BlockPtr whileBody = new Block();
    whileBody->statements.push_back(new Binding(REF, x->variable, nextCall.ptr()));
    whileBody->statements.push_back(x->body);

    block->statements.push_back(new While(hasNextCall.ptr(), whileBody.ptr()));
    return block.ptr();
}
