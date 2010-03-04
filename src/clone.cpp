#include "clay.hpp"

CodePtr clone(CodePtr x)
{
    CodePtr y = new Code();
    y->location = x->location;
    clone(x->patternVars, y->patternVars);
    y->predicate = cloneOpt(x->predicate);
    clone(x->formalArgs, y->formalArgs);
    y->body = clone(x->body);
    y->returnType = cloneOpt(x->returnType);
    y->returnRef = x->returnRef;
    return y;
}

void clone(const vector<IdentifierPtr> &x, vector<IdentifierPtr> &out)
{
    for (unsigned i = 0; i < x.size(); ++i)
        out.push_back(x[i]);
}

ExprPtr clone(ExprPtr x)
{
    ExprPtr out;

    switch (x->objKind) {

    case BOOL_LITERAL : {
        BoolLiteral *y = (BoolLiteral *)x.ptr();
        out = new BoolLiteral(y->value);
        break;
    }

    case INT_LITERAL : {
        IntLiteral *y = (IntLiteral *)x.ptr();
        out = new IntLiteral(y->value, y->suffix);
        break;
    }

    case FLOAT_LITERAL : {
        FloatLiteral *y = (FloatLiteral *)x.ptr();
        out = new FloatLiteral(y->value, y->suffix);
        break;
    }

    case CHAR_LITERAL : {
        CharLiteral *y = (CharLiteral *)x.ptr();
        out = new CharLiteral(y->value);
        break;
    }

    case STRING_LITERAL : {
        StringLiteral *y = (StringLiteral *)x.ptr();
        out = new StringLiteral(y->value);
        break;
    }

    case NAME_REF : {
        NameRef *y = (NameRef *)x.ptr();
        out = new NameRef(y->name);
        break;
    }

    case RETURNED : {
        out = new Returned();
        break;
    }

    case TUPLE : {
        Tuple *y = (Tuple *)x.ptr();
        TuplePtr z = new Tuple();
        clone(y->args, z->args);
        out = z.ptr();
        break;
    }

    case ARRAY : {
        Array *y = (Array *)x.ptr();
        ArrayPtr z = new Array();
        clone(y->args, z->args);
        out = z.ptr();
        break;
    }

    case INDEXING : {
        Indexing *y = (Indexing *)x.ptr();
        IndexingPtr z = new Indexing(clone(y->expr));
        clone(y->args, z->args);
        out = z.ptr();
        break;
    }

    case CALL : {
        Call *y = (Call *)x.ptr();
        CallPtr z = new Call(clone(y->expr));
        clone(y->args, z->args);
        out = z.ptr();
        break;
    }

    case FIELD_REF : {
        FieldRef *y = (FieldRef *)x.ptr();
        out = new FieldRef(clone(y->expr), y->name);
        break;
    }

    case TUPLE_REF : {
        TupleRef *y = (TupleRef *)x.ptr();
        out = new TupleRef(clone(y->expr), y->index);
        break;
    }

    case UNARY_OP : {
        UnaryOp *y = (UnaryOp *)x.ptr();
        out = new UnaryOp(y->op, clone(y->expr));
        break;
    }

    case BINARY_OP : {
        BinaryOp *y = (BinaryOp *)x.ptr();
        out = new BinaryOp(y->op, clone(y->expr1), clone(y->expr2));
        break;
    }

    case AND : {
        And *y = (And *)x.ptr();
        out = new And(clone(y->expr1), clone(y->expr2));
        break;
    }

    case OR : {
        Or *y = (Or *)x.ptr();
        out = new Or(clone(y->expr1), clone(y->expr2));
        break;
    }

    case LAMBDA : {
        Lambda *y = (Lambda *)x.ptr();
        LambdaPtr z = new Lambda();
        clone(y->formalArgs, z->formalArgs);
        z->body = clone(y->body);
        out = z.ptr();
        break;
    }

    case SC_EXPR : {
        SCExpr *y = (SCExpr *)x.ptr();
        out = new SCExpr(y->env, clone(y->expr));
        break;
    }

    case OBJECT_EXPR : {
        ObjectExpr *y = (ObjectExpr *)x.ptr();
        out = new ObjectExpr(y->obj);
        break;
    }

    case CVALUE_EXPR :
        assert(false);
        break;

    default :
        assert(false);
        break;

    }

    out->location = x->location;
    return out;
}

ExprPtr cloneOpt(ExprPtr x)
{
    if (!x)
        return NULL;
    return clone(x);
}

void clone(const vector<ExprPtr> &x, vector<ExprPtr> &out)
{
    for (unsigned i = 0; i < x.size(); ++i)
        out.push_back(clone(x[i]));
}

void clone(const vector<FormalArgPtr> &x, vector<FormalArgPtr> &out)
{
    for (unsigned i = 0; i < x.size(); ++i)
        out.push_back(clone(x[i]));
}

FormalArgPtr clone(FormalArgPtr x)
{
    FormalArgPtr out;

    switch (x->objKind) {

    case VALUE_ARG : {
        ValueArg *y = (ValueArg *)x.ptr();
        out = new ValueArg(y->name, cloneOpt(y->type));
        break;
    }

    case STATIC_ARG : {
        StaticArg *y = (StaticArg *)x.ptr();
        out = new StaticArg(clone(y->pattern));
        break;
    }

    default :
        assert(false);

    }

    out->location = x->location;
    return out;
}

StatementPtr clone(StatementPtr x)
{
    StatementPtr out;

    switch (x->objKind) {

    case BLOCK : {
        Block *y = (Block *)x.ptr();
        BlockPtr z = new Block();
        clone(y->statements, z->statements);
        out = z.ptr();
        break;
    }

    case LABEL : {
        Label *y = (Label *)x.ptr();
        out = new Label(y->name);
        break;
    }

    case BINDING : {
        Binding *y = (Binding *)x.ptr();
        out = new Binding(y->bindingKind, y->name, clone(y->expr));
        break;
    }

    case ASSIGNMENT : {
        Assignment *y = (Assignment *)x.ptr();
        out = new Assignment(clone(y->left), clone(y->right));
        break;
    }

    case INIT_ASSIGNMENT : {
        InitAssignment *y = (InitAssignment *)x.ptr();
        out = new InitAssignment(clone(y->left), clone(y->right));
        break;
    }

    case UPDATE_ASSIGNMENT : {
        UpdateAssignment *y = (UpdateAssignment *)x.ptr();
        out = new UpdateAssignment(y->op, clone(y->left), clone(y->right));
        break;
    }

    case GOTO : {
        Goto *y = (Goto *)x.ptr();
        out = new Goto(y->labelName);
        break;
    }

    case RETURN : {
        Return *y = (Return *)x.ptr();
        out = new Return(cloneOpt(y->expr));
        break;
    }

    case RETURN_REF : {
        ReturnRef *y = (ReturnRef *)x.ptr();
        out = new ReturnRef(clone(y->expr));
        break;
    }

    case IF : {
        If *y = (If *)x.ptr();
        out = new If(clone(y->condition), clone(y->thenPart),
                     cloneOpt(y->elsePart));
        break;
    }

    case EXPR_STATEMENT : {
        ExprStatement *y = (ExprStatement *)x.ptr();
        out = new ExprStatement(clone(y->expr));
        break;
    }

    case WHILE : {
        While *y = (While *)x.ptr();
        out = new While(clone(y->condition), clone(y->body));
        break;
    }

    case BREAK : {
        out = new Break();
        break;
    }

    case CONTINUE : {
        out = new Continue();
        break;
    }

    case FOR : {
        For *y = (For *)x.ptr();
        out = new For(y->variable, clone(y->expr), clone(y->body));
        break;
    }

    default :
        assert(false);

    }

    out->location = x->location;
    return out;
}

StatementPtr cloneOpt(StatementPtr x)
{
    if (!x)
        return NULL;
    return clone(x);
}

void clone(const vector<StatementPtr> &x, vector<StatementPtr> &out)
{
    for (unsigned i = 0; i < x.size(); ++i)
        out.push_back(clone(x[i]));
}
