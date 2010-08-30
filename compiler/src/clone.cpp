#include "clay.hpp"

CodePtr clone(CodePtr x)
{
    CodePtr y = new Code();
    y->location = x->location;
    clone(x->patternVars, y->patternVars);
    y->predicate = cloneOpt(x->predicate);
    clone(x->formalArgs, y->formalArgs);
    y->formalVarArg = cloneOpt(x->formalVarArg);
    clone(x->returnSpecs, y->returnSpecs);
    y->varReturnSpec = cloneOpt(x->varReturnSpec);
    y->body = cloneOpt(x->body);
    y->llvmBody = x->llvmBody;
    return y;
}

void clone(const vector<PatternVar> &x, vector<PatternVar> &out)
{
    out = x;
}

void clone(const vector<IdentifierPtr> &x, vector<IdentifierPtr> &out)
{
    for (unsigned i = 0; i < x.size(); ++i)
        out.push_back(x[i]);
}

ExprPtr clone(ExprPtr x)
{
    ExprPtr out;

    switch (x->exprKind) {

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

    case IDENTIFIER_LITERAL : {
        IdentifierLiteral *y = (IdentifierLiteral *)x.ptr();
        out = new IdentifierLiteral(y->value);
        break;
    }

    case NAME_REF : {
        NameRef *y = (NameRef *)x.ptr();
        out = new NameRef(y->name);
        break;
    }

    case TUPLE : {
        Tuple *y = (Tuple *)x.ptr();
        out = new Tuple(clone(y->args));
        break;
    }

    case ARRAY : {
        Array *y = (Array *)x.ptr();
        out = new Array(clone(y->args));
        break;
    }

    case INDEXING : {
        Indexing *y = (Indexing *)x.ptr();
        out = new Indexing(clone(y->expr), clone(y->args));
        break;
    }

    case CALL : {
        Call *y = (Call *)x.ptr();
        out = new Call(clone(y->expr), clone(y->args));
        break;
    }

    case FIELD_REF : {
        FieldRef *y = (FieldRef *)x.ptr();
        out = new FieldRef(clone(y->expr), y->name);
        break;
    }

    case STATIC_INDEXING : {
        StaticIndexing *y = (StaticIndexing *)x.ptr();
        out = new StaticIndexing(clone(y->expr), y->index);
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

    case IF_EXPR : {
        IfExpr *y = (IfExpr *)x.ptr();
        out = new IfExpr(clone(y->condition),
                         clone(y->thenPart),
                         clone(y->elsePart));
        break;
    }

    case LAMBDA : {
        Lambda *y = (Lambda *)x.ptr();
        LambdaPtr z = new Lambda(y->captureByRef);
        clone(y->formalArgs, z->formalArgs);
        z->body = clone(y->body);
        out = z.ptr();
        break;
    }

    case UNPACK : {
        Unpack *y = (Unpack *)x.ptr();
        out = new Unpack(clone(y->expr));
        break;
    }

    case NEW : {
        New *y = (New *)x.ptr();
        out = new New(clone(y->expr));
        break;
    }

    case STATIC_EXPR : {
        StaticExpr *y = (StaticExpr *)x.ptr();
        out = new StaticExpr(clone(y->expr));
        break;
    }

    case DISPATCH_EXPR : {
        DispatchExpr *y = (DispatchExpr *)x.ptr();
        out = new DispatchExpr(clone(y->expr));
        break;
    }

    case FOREIGN_EXPR : {
        out = x;
        break;
    }

    case OBJECT_EXPR : {
        ObjectExpr *y = (ObjectExpr *)x.ptr();
        out = new ObjectExpr(y->obj);
        break;
    }

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

ExprListPtr clone(ExprListPtr x)
{
    ExprListPtr out = new ExprList();
    for (unsigned i = 0; i < x->size(); ++i)
        out->add(clone(x->exprs[i]));
    return out;
}

void clone(const vector<FormalArgPtr> &x, vector<FormalArgPtr> &out)
{
    for (unsigned i = 0; i < x.size(); ++i)
        out.push_back(clone(x[i]));
}

FormalArgPtr clone(FormalArgPtr x)
{
    FormalArgPtr out = new FormalArg(x->name, cloneOpt(x->type), x->tempness);
    out->location = x->location;
    return out;
}

FormalArgPtr cloneOpt(FormalArgPtr x)
{
    if (!x)
        return NULL;
    return clone(x);
}

void clone(const vector<ReturnSpecPtr> &x, vector<ReturnSpecPtr> &out)
{
    for (unsigned i = 0; i < x.size(); ++i)
        out.push_back(clone(x[i]));
}

ReturnSpecPtr clone(ReturnSpecPtr x)
{
    return new ReturnSpec(clone(x->type), x->name);
}

ReturnSpecPtr cloneOpt(ReturnSpecPtr x)
{
    if (!x)
        return NULL;
    return clone(x);
}

StatementPtr clone(StatementPtr x)
{
    StatementPtr out;

    switch (x->stmtKind) {

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
        vector<IdentifierPtr> names;
        clone(y->names, names);
        out = new Binding(y->bindingKind, names, clone(y->values));
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
        out = new Return(y->returnKind, clone(y->values));
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
        vector<IdentifierPtr> variables;
        clone(y->variables, variables);
        out = new For(variables, clone(y->expr), clone(y->body));
        break;
    }

    case FOREIGN_STATEMENT : {
        out = x;
        break;
    }

    case TRY : {
        Try *y = (Try *)x.ptr();
        vector<CatchPtr> catchBlocks;
        clone(y->catchBlocks, catchBlocks);
        out = new Try(clone(y->tryBlock), catchBlocks);
        break;
    }

    case THROW : {
        Throw *y = (Throw *)x.ptr();
        out = new Throw(cloneOpt(y->expr));
        break;
    }

    case STATIC_FOR : {
        StaticFor *y = (StaticFor *)x.ptr();
        out = new StaticFor(y->variable, clone(y->values), clone(y->body));
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

CatchPtr clone(CatchPtr x)
{
    return new Catch(x->exceptionVar,
                     cloneOpt(x->exceptionType),
                     clone(x->body));
}

void clone(const vector<CatchPtr> &x, vector<CatchPtr> &out)
{
    for (unsigned i = 0; i < x.size(); ++i)
        out.push_back(clone(x[i]));
}
