#include "clay.hpp"
#include <cstdlib>
#include <climits>
#include <cassert>

static vector<TokenPtr> *tokens;
static int position;
static int maxPosition;

static bool next(TokenPtr &x) {
    if (position == (int)tokens->size())
        return false;
    x = (*tokens)[position];
    if (position > maxPosition)
        maxPosition = position;
    ++position;
    return true;
}

static int save() {
    return position;
}

static void restore(int p) {
    position = p;
}

static LocationPtr currentLocation() {
    if (position == (int)tokens->size())
        return NULL;
    return (*tokens)[position]->location;
}



//
// symbol, keyword
//

static bool symbol(const char *s) {
    TokenPtr t;
    if (!next(t) || (t->tokenKind != T_SYMBOL))
        return false;
    return t->str == s;
}

static bool keyword(const char *s) {
    TokenPtr t;
    if (!next(t) || (t->tokenKind != T_KEYWORD))
        return false;
    return t->str == s;
}



//
// identifier, identifierList,
// identifierListNoTail, dottedName
//

static bool identifier(IdentifierPtr &x) {
    LocationPtr location = currentLocation();
    TokenPtr t;
    if (!next(t) || (t->tokenKind != T_IDENTIFIER))
        return false;
    x = new Identifier(t->str);
    x->location = location;
    return true;
}

static bool identifierList(vector<IdentifierPtr> &x) {
    IdentifierPtr y;
    if (!identifier(y)) return false;
    x.clear();
    x.push_back(y);
    while (true) {
        int p = save();
        if (!symbol(",")) {
            restore(p);
            break;
        }
        p = save();
        if (!identifier(y)) {
            restore(p);
            break;
        }
        x.push_back(y);
    }
    return true;
}

static bool identifierListNoTail(vector<IdentifierPtr> &x) {
    IdentifierPtr y;
    if (!identifier(y)) return false;
    x.clear();
    x.push_back(y);
    while (true) {
        int p = save();
        if (!symbol(",") || !identifier(y)) {
            restore(p);
            break;
        }
        x.push_back(y);
    }
    return true;
}

static bool dottedName(DottedNamePtr &x) {
    LocationPtr location = currentLocation();
    DottedNamePtr y = new DottedName();
    IdentifierPtr ident;
    if (!identifier(ident)) return false;
    y->parts.push_back(ident);
    while (true) {
        int p = save();
        if (!symbol(".") || !identifier(ident)) {
            restore(p);
            break;
        }
        y->parts.push_back(ident);
    }
    x = y;
    x->location = location;
    return true;
}



//
// literals
//

static bool boolLiteral(ExprPtr &x) {
    LocationPtr location = currentLocation();
    int p = save();
    if (keyword("true"))
        x = new BoolLiteral(true);
    else if (restore(p), keyword("false"))
        x = new BoolLiteral(false);
    else
        return false;
    x->location = location;
    return true;
}

static string cleanNumericSeparator(const string &s) {
    string out;
    for (unsigned i = 0; i < s.size(); ++i) {
        if (s[i] != '_')
            out.push_back(s[i]);
    }
    return out;
}

static bool intLiteral(ExprPtr &x) {
    LocationPtr location = currentLocation();
    TokenPtr t;
    if (!next(t) || (t->tokenKind != T_INT_LITERAL))
        return false;
    TokenPtr t2;
    int p = save();
    if (next(t2) && (t2->tokenKind == T_IDENTIFIER)) {
        x = new IntLiteral(cleanNumericSeparator(t->str), t2->str);
    }
    else {
        restore(p);
        x = new IntLiteral(cleanNumericSeparator(t->str));
    }
    x->location = location;
    return true;
}

static bool floatLiteral(ExprPtr &x) {
    LocationPtr location = currentLocation();
    TokenPtr t;
    if (!next(t) || (t->tokenKind != T_FLOAT_LITERAL))
        return false;
    TokenPtr t2;
    int p = save();
    if (next(t2) && (t2->tokenKind == T_IDENTIFIER)) {
        x = new FloatLiteral(cleanNumericSeparator(t->str), t2->str);
    }
    else {
        restore(p);
        x = new FloatLiteral(cleanNumericSeparator(t->str));
    }
    x->location = location;
    return true;
}

static bool charLiteral(ExprPtr &x) {
    LocationPtr location = currentLocation();
    TokenPtr t;
    if (!next(t) || (t->tokenKind != T_CHAR_LITERAL))
        return false;
    x = new CharLiteral(t->str[0]);
    x->location = location;
    return true;
}

static bool stringLiteral(ExprPtr &x) {
    LocationPtr location = currentLocation();
    TokenPtr t;
    if (!next(t) || (t->tokenKind != T_STRING_LITERAL))
        return false;
    x = new StringLiteral(t->str);
    x->location = location;
    return true;
}

static bool identifierLiteral(ExprPtr &x) {
    LocationPtr location = currentLocation();
    if (!symbol("#")) return false;
    int p = save();
    IdentifierPtr y;
    TokenPtr t;
    if (identifier(y)) {
        // ok
    }
    else if (restore(p), (next(t) && (t->tokenKind == T_STRING_LITERAL))) {
        y = new Identifier(t->str);
        y->location = t->location;
    }
    else {
        return false;
    }
    x = new IdentifierLiteral(y);
    x->location = location;
    return true;
}

static bool literal(ExprPtr &x) {
    int p = save();
    if (boolLiteral(x)) return true;
    if (restore(p), intLiteral(x)) return true;
    if (restore(p), floatLiteral(x)) return true;
    if (restore(p), charLiteral(x)) return true;
    if (restore(p), stringLiteral(x)) return true;
    if (restore(p), identifierLiteral(x)) return true;
    return false;
}



//
// expression misc
//

static bool expression(ExprPtr &x);

static bool optExpression(ExprPtr &x) {
    int p = save();
    if (!expression(x)) {
        restore(p);
        x = NULL;
    }
    return true;
}

static bool expressionList(ExprListPtr &x) {
    ExprListPtr a;
    ExprPtr b;
    if (!expression(b)) return false;
    a = new ExprList(b);
    while (true) {
        int p = save();
        if (!symbol(",")) {
            restore(p);
            break;
        }
        p = save();
        if (!expression(b)) {
            restore(p);
            break;
        }
        a->add(b);
    }
    x = a;
    return true;
}

static bool optExpressionList(ExprListPtr &x) {
    int p = save();
    if (!expressionList(x)) {
        restore(p);
        x = new ExprList();
    }
    return true;
}



//
// atomic expr
//

static bool arrayExpr(ExprPtr &x) {
    LocationPtr location = currentLocation();
    if (!symbol("[")) return false;
    ExprListPtr args;
    if (!expressionList(args)) return false;
    if (!symbol("]")) return false;
    x = new Array(args);
    x->location = location;
    return true;
}

static bool tupleExpr(ExprPtr &x) {
    LocationPtr location = currentLocation();
    if (!symbol("(")) return false;
    ExprListPtr args;
    if (!optExpressionList(args)) return false;
    if (!symbol(")")) return false;
    x = new Tuple(args);
    x->location = location;
    return true;
}

static bool nameRef(ExprPtr &x) {
    LocationPtr location = currentLocation();
    IdentifierPtr a;
    if (!identifier(a)) return false;
    x = new NameRef(a);
    x->location = location;
    return true;
}

static bool atomicExpr(ExprPtr &x) {
    int p = save();
    if (arrayExpr(x)) return true;
    if (restore(p), tupleExpr(x)) return true;
    if (restore(p), nameRef(x)) return true;
    if (restore(p), literal(x)) return true;
    return false;
}



//
// suffix expr
//

static bool indexingSuffix(ExprPtr &x) {
    LocationPtr location = currentLocation();
    if (!symbol("[")) return false;
    ExprListPtr args;
    if (!optExpressionList(args)) return false;
    if (!symbol("]")) return false;
    x = new Indexing(NULL, args);
    x->location = location;
    return true;
}

static bool callSuffix(ExprPtr &x) {
    LocationPtr location = currentLocation();
    if (!symbol("(")) return false;
    ExprListPtr args;
    if (!optExpressionList(args)) return false;
    if (!symbol(")")) return false;
    x = new Call(NULL, args);
    x->location = location;
    return true;
}

static bool fieldRefSuffix(ExprPtr &x) {
    LocationPtr location = currentLocation();
    if (!symbol(".")) return false;
    IdentifierPtr a;
    if (!identifier(a)) return false;
    x = new FieldRef(NULL, a);
    x->location = location;
    return true;
}

static bool staticIndexingSuffix(ExprPtr &x) {
    LocationPtr location = currentLocation();
    if (!symbol(".")) return false;
    TokenPtr t;
    if (!next(t) || (t->tokenKind != T_INT_LITERAL))
        return false;
    char *b = const_cast<char *>(t->str.c_str());
    char *end = b;
    unsigned long c = strtoul(b, &end, 0);
    if (*end != 0)
        error(t, "invalid static index value");
    x = new StaticIndexing(NULL, (size_t)c);
    x->location = location;
    return true;
}

static bool dereferenceSuffix(ExprPtr &x) {
    LocationPtr location = currentLocation();
    if (!symbol("^")) return false;
    x = new UnaryOp(DEREFERENCE, NULL);
    x->location = location;
    return true;
}

static bool suffix(ExprPtr &x) {
    int p = save();
    if (indexingSuffix(x)) return true;
    if (restore(p), callSuffix(x)) return true;
    if (restore(p), fieldRefSuffix(x)) return true;
    if (restore(p), staticIndexingSuffix(x)) return true;
    if (restore(p), dereferenceSuffix(x)) return true;
    return false;
}

static void setSuffixBase(Expr *a, ExprPtr base) {
    switch (a->exprKind) {
    case INDEXING : {
        Indexing *b = (Indexing *)a;
        b->expr = base;
        break;
    }
    case CALL : {
        Call *b = (Call *)a;
        b->expr = base;
        break;
    }
    case FIELD_REF : {
        FieldRef *b = (FieldRef *)a;
        b->expr = base;
        break;
    }
    case STATIC_INDEXING : {
        StaticIndexing *b = (StaticIndexing *)a;
        b->expr = base;
        break;
    }
    case UNARY_OP : {
        UnaryOp *b = (UnaryOp *)a;
        assert(b->op == DEREFERENCE);
        b->expr = base;
        break;
    }
    default :
        assert(false);
    }
}

static bool suffixExpr(ExprPtr &x) {
    if (!atomicExpr(x)) return false;
    while (true) {
        int p = save();
        ExprPtr y;
        if (!suffix(y)) {
            restore(p);
            break;
        }
        setSuffixBase(y.ptr(), x);
        x = y;
    }
    return true;
}



//
// prefix expr
//

static bool addressOfExpr(ExprPtr &x) {
    LocationPtr location = currentLocation();
    if (!symbol("&")) return false;
    ExprPtr a;
    if (!suffixExpr(a)) return false;
    x = new UnaryOp(ADDRESS_OF, a);
    x->location = location;
    return true;
}

static bool plusOrMinus(int &op) {
    int p = save();
    if (symbol("+"))
        op = PLUS;
    else if (restore(p), symbol("-"))
        op = MINUS;
    else
        return false;
    return true;
}

static bool signExpr(ExprPtr &x) {
    LocationPtr location = currentLocation();
    int op;
    if (!plusOrMinus(op)) return false;
    ExprPtr b;
    if (!suffixExpr(b)) return false;
    x = new UnaryOp(op, b);
    x->location = location;
    return true;
}

static bool dispatchExpr(ExprPtr &x) {
    LocationPtr location = currentLocation();
    if (!symbol("*")) return false;
    ExprPtr a;
    if (!suffixExpr(a)) return false;
    x = new DispatchExpr(a);
    x->location = location;
    return true;
}

static bool prefixExpr(ExprPtr &x) {
    int p = save();
    if (signExpr(x)) return true;
    if (restore(p), addressOfExpr(x)) return true;
    if (restore(p), dispatchExpr(x)) return true;
    if (restore(p), suffixExpr(x)) return true;
    return false;
}



//
// arithmetic expr
//

static bool mulDivOp(int &op) {
    int p = save();
    if (symbol("*"))
        op = MULTIPLY;
    else if (restore(p), symbol("/"))
        op = DIVIDE;
    else if (restore(p), symbol("%"))
        op = REMAINDER;
    else
        return false;
    return true;
}

static bool mulDivTail(BinaryOpPtr &x) {
    LocationPtr location = currentLocation();
    int op;
    if (!mulDivOp(op)) return false;
    ExprPtr b;
    if (!prefixExpr(b)) return false;
    x = new BinaryOp(op, NULL, b);
    x->location = location;
    return true;
}

static bool mulDivExpr(ExprPtr &x) {
    if (!prefixExpr(x)) return false;
    while (true) {
        int p = save();
        BinaryOpPtr y;
        if (!mulDivTail(y)) {
            restore(p);
            break;
        }
        y->expr1 = x;
        x = y.ptr();
    }
    return true;
}

static bool addSubOp(int &op) {
    int p = save();
    if (symbol("+"))
        op = ADD;
    else if (restore(p), symbol("-"))
        op = SUBTRACT;
    else
        return false;
    return true;
}

static bool addSubTail(BinaryOpPtr &x) {
    LocationPtr location = currentLocation();
    int op;
    if (!addSubOp(op)) return false;
    ExprPtr y;
    if (!mulDivExpr(y)) return false;
    x = new BinaryOp(op, NULL, y);
    x->location = location;
    return true;
}

static bool addSubExpr(ExprPtr &x) {
    if (!mulDivExpr(x)) return false;
    while (true) {
        int p = save();
        BinaryOpPtr y;
        if (!addSubTail(y)) {
            restore(p);
            break;
        }
        y->expr1 = x;
        x = y.ptr();
    }
    return true;
}



//
// compare expr
//

static bool compareOp(int &op) {
    int p = save();
    const char *s[] = {"<", "<=", ">", ">=", NULL};
    const int ops[] = {LESSER, LESSER_EQUALS,
                       GREATER, GREATER_EQUALS};
    for (const char **a = s; *a; ++a) {
        restore(p);
        if (symbol(*a)) {
            int i = a - s;
            op = ops[i];
            return true;
        }
    }
    return false;
}

static bool compareTail(BinaryOpPtr &x) {
    LocationPtr location = currentLocation();
    int op;
    if (!compareOp(op)) return false;
    ExprPtr y;
    if (!addSubExpr(y)) return false;
    x = new BinaryOp(op, NULL, y);
    x->location = location;
    return true;
}

static bool compareExpr(ExprPtr &x) {
    if (!addSubExpr(x)) return false;
    while (true) {
        int p = save();
        BinaryOpPtr y;
        if (!compareTail(y)) {
            restore(p);
            break;
        }
        y->expr1 = x;
        x = y.ptr();
    }
    return true;
}



//
// equal expr
//

static bool equalOp(int &op) {
    int p = save();
    if (symbol("==")) {
        op = EQUALS;
        return true;
    }
    restore(p);
    if (symbol("!=")) {
        op = NOT_EQUALS;
        return true;
    }
    return false;
}


static bool equalTail(BinaryOpPtr &x) {
    LocationPtr location = currentLocation();
    int op;
    if (!equalOp(op)) return false;
    ExprPtr y;
    if (!compareExpr(y)) return false;
    x = new BinaryOp(op, NULL, y);
    x->location = location;
    return true;
}

static bool equalExpr(ExprPtr &x) {
    if (!compareExpr(x)) return false;
    while (true) {
        int p = save();
        BinaryOpPtr y;
        if (!equalTail(y)) {
            restore(p);
            break;
        }
        y->expr1 = x;
        x = y.ptr();
    }
    return true;
}



//
// not, and, or
//

static bool notExpr(ExprPtr &x) {
    LocationPtr location = currentLocation();
    int p = save();
    if (!keyword("not")) {
        restore(p);
        return equalExpr(x);
    }
    ExprPtr y;
    if (!equalExpr(y)) return false;
    x = new UnaryOp(NOT, y);
    x->location = location;
    return true;
}

static bool andExprTail(AndPtr &x) {
    LocationPtr location = currentLocation();
    if (!keyword("and")) return false;
    ExprPtr y;
    if (!notExpr(y)) return false;
    x = new And(NULL, y);
    x->location = location;
    return true;
}

static bool andExpr(ExprPtr &x) {
    if (!notExpr(x)) return false;
    while (true) {
        int p = save();
        AndPtr y;
        if (!andExprTail(y)) {
            restore(p);
            break;
        }
        y->expr1 = x;
        x = y.ptr();
    }
    return true;
}

static bool orExprTail(OrPtr &x) {
    LocationPtr location = currentLocation();
    if (!keyword("or")) return false;
    ExprPtr y;
    if (!andExpr(y)) return false;
    x = new Or(NULL, y);
    x->location = location;
    return true;
}

static bool orExpr(ExprPtr &x) {
    if (!andExpr(x)) return false;
    while (true) {
        int p = save();
        OrPtr y;
        if (!orExprTail(y)) {
            restore(p);
            break;
        }
        y->expr1 = x;
        x = y.ptr();
    }
    return true;
}



//
// ifExpr
//

static bool ifExpr(ExprPtr &x) {
    LocationPtr location = currentLocation();
    ExprPtr condition;
    ExprPtr thenPart, elsePart;
    if (!keyword("if")) return false;
    if (!symbol("(")) return false;
    if (!expression(condition)) return false;
    if (!symbol(")")) return false;
    if (!expression(thenPart)) return false;
    if (!keyword("else")) return false;
    if (!expression(elsePart)) return false;
    x = new IfExpr(condition, thenPart, elsePart);
    x->location = location;
    return true;
}



//
// returnKind, returnExprList, returnExpr
//

static bool returnKind(ReturnKind &x) {
    int p = save();
    if (keyword("ref")) {
        x = RETURN_REF;
    }
    else if (restore(p), keyword("forward")) {
        x = RETURN_FORWARD;
    }
    else {
        restore(p);
        x = RETURN_VALUE;
    }
    return true;
}

static bool returnExprList(ReturnKind &rkind, ExprListPtr &exprs) {
    if (!returnKind(rkind)) return false;
    if (!optExpressionList(exprs)) return false;
    return true;
}

static bool returnExpr(ReturnKind &rkind, ExprPtr &expr) {
    if (!returnKind(rkind)) return false;
    if (!expression(expr)) return false;
    return true;
}



//
// lambda
//

static bool block(StatementPtr &x);

static bool lambdaArgs4(vector<IdentifierPtr> &formalArgs,
                        IdentifierPtr &formalVarArg) {
    if (!symbol("...")) return false;
    if (!identifier(formalVarArg)) return false;
    formalArgs.clear();
    return true;
}

static bool lambdaArgs3(vector<IdentifierPtr> &formalArgs,
                        IdentifierPtr &formalVarArg) {
    if (!identifierListNoTail(formalArgs)) return false;
    int p = save();
    if (!symbol(",") || !symbol("...") || !identifier(formalVarArg)) {
        restore(p);
        formalVarArg = NULL;
    }
    return true;
}

static bool lambdaArgs2(vector<IdentifierPtr> &formalArgs,
                        IdentifierPtr &formalVarArg) {
    int p = save();
    if (lambdaArgs3(formalArgs, formalVarArg)) return true;
    restore(p);
    if (lambdaArgs4(formalArgs, formalVarArg)) return true;
    restore(p);
    formalArgs.clear();
    formalVarArg = NULL;
    return true;
}

static bool lambdaArgs(vector<IdentifierPtr> &formalArgs,
                       IdentifierPtr &formalVarArg) {
    int p = save();
    IdentifierPtr name;
    if (identifier(name)) {
        formalArgs.clear();
        formalArgs.push_back(name);
        formalVarArg = NULL;
        return true;
    }
    restore(p);
    if (!symbol("(")) return false;
    if (!lambdaArgs2(formalArgs, formalVarArg)) return false;
    if (!symbol(")")) return false;
    return true;
}

static bool lambdaExprBody(StatementPtr &x) {
    LocationPtr location = currentLocation();
    ReturnKind rkind;
    ExprPtr expr;
    if (!returnExpr(rkind, expr)) return false;
    x = new Return(rkind, new ExprList(expr));
    x->location = location;
    return true;
}

static bool optCaptureByRef(bool &captureByRef) {
    int p = save();
    if (keyword("ref")) {
        captureByRef = true;
    }
    else {
        restore(p);
        captureByRef = false;
    }
    return true;
}

static bool lambdaBody(StatementPtr &x) {
    int p = save();
    if (lambdaExprBody(x)) return true;
    if (restore(p), block(x)) return true;
    return false;
}

static bool lambda(ExprPtr &x) {
    LocationPtr location = currentLocation();
    vector<IdentifierPtr> formalArgs;
    IdentifierPtr formalVarArg;
    if (!lambdaArgs(formalArgs, formalVarArg)) return false;
    bool captureByRef;
    if (!optCaptureByRef(captureByRef)) return false;
    if (!symbol("=>")) return false;
    StatementPtr body;
    if (!lambdaBody(body)) return false;
    x = new Lambda(captureByRef, formalArgs, formalVarArg, body);
    x->location = location;
    return true;
}



//
// unpack
//

static bool unpack(ExprPtr &x) {
    LocationPtr location = currentLocation();
    if (!symbol("...")) return false;
    ExprPtr y;
    if (!expression(y)) return false;
    x = new Unpack(y);
    x->location = location;
    return true;
}



//
// newExpr, staticExpr, pairExpr
//

static bool newExpr(ExprPtr &x) {
    LocationPtr location = currentLocation();
    if (!keyword("new")) return false;
    ExprPtr y;
    if (!expression(y)) return false;
    x = new New(y);
    x->location = location;
    return true;
}

static bool staticExpr(ExprPtr &x) {
    LocationPtr location = currentLocation();
    if (!keyword("static")) return false;
    ExprPtr y;
    if (!expression(y)) return false;
    x = new StaticExpr(y);
    x->location = location;
    return true;
}

static bool pairExpr(ExprPtr &x) {
    LocationPtr location = currentLocation();
    IdentifierPtr y;
    if (!identifier(y)) return false;
    if (!symbol(":")) return false;
    ExprPtr z;
    if (!expression(z)) return false;
    ExprPtr ident = new IdentifierLiteral(y);
    ident->location = location;
    ExprListPtr args = new ExprList();
    args->add(ident);
    args->add(z);
    x = new Tuple(args);
    x->location = location;
    return true;
}



//
// expression, expressionNoPair
//

static bool expression(ExprPtr &x) {
    int p = save();
    if (lambda(x)) return true;
    if (restore(p), pairExpr(x)) return true;
    if (restore(p), orExpr(x)) return true;
    if (restore(p), ifExpr(x)) return true;
    if (restore(p), unpack(x)) return true;
    if (restore(p), newExpr(x)) return true;
    if (restore(p), staticExpr(x)) return true;
    return false;
}

static bool expressionNoPair(ExprPtr &x) {
    int p = save();
    if (lambda(x)) return true;
    if (restore(p), orExpr(x)) return true;
    if (restore(p), ifExpr(x)) return true;
    if (restore(p), unpack(x)) return true;
    if (restore(p), newExpr(x)) return true;
    if (restore(p), staticExpr(x)) return true;
    return false;
}



//
// pattern
//

static bool pattern(ExprPtr &x);

static bool dottedNameRef(ExprPtr &x) {
    if (!nameRef(x)) return false;
    while (true) {
        int p = save();
        ExprPtr y;
        if (!fieldRefSuffix(y)) {
            restore(p);
            break;
        }
        setSuffixBase(y.ptr(), x);
        x = y;
    }
    return true;
}

static bool atomicPattern(ExprPtr &x) {
    int p = save();
    if (dottedNameRef(x)) return true;
    if (restore(p), intLiteral(x)) return true;
    return false;
}

static bool patternSuffix(IndexingPtr &x) {
    LocationPtr location = currentLocation();
    if (!symbol("[")) return false;
    ExprListPtr args;
    if (!optExpressionList(args)) return false;
    if (!symbol("]")) return false;
    x = new Indexing(NULL, args);
    x->location = location;
    return true;
}

static bool pattern(ExprPtr &x) {
    if (!atomicPattern(x)) return false;
    int p = save();
    IndexingPtr y;
    if (!patternSuffix(y)) {
        restore(p);
        return true;
    }
    y->expr = x;
    x = y.ptr();
    return true;
}



//
// typeSpec, optTypeSpec, exprTypeSpec, optExprTypeSpec
//

static bool typeSpec(ExprPtr &x) {
    if (!symbol(":")) return false;
    return pattern(x);
}

static bool optTypeSpec(ExprPtr &x) {
    int p = save();
    if (!typeSpec(x)) {
        restore(p);
        x = NULL;
    }
    return true;
}

static bool exprTypeSpec(ExprPtr &x) {
    if (!symbol(":")) return false;
    return expression(x);
}

static bool optExprTypeSpec(ExprPtr &x) {
    int p = save();
    if (!exprTypeSpec(x)) {
        restore(p);
        x = NULL;
    }
    return true;
}



//
// statements
//

static bool statement(StatementPtr &x);

static bool labelDef(StatementPtr &x) {
    LocationPtr location = currentLocation();
    IdentifierPtr y;
    if (!identifier(y)) return false;
    if (!symbol(":")) return false;
    x = new Label(y);
    x->location = location;
    return true;
}

static bool bindingKind(int &bindingKind) {
    int p = save();
    if (keyword("var"))
        bindingKind = VAR;
    else if (restore(p), keyword("ref"))
        bindingKind = REF;
    else if (restore(p), keyword("alias"))
        bindingKind = ALIAS;
    else
        return false;
    return true;
}

static bool localBinding(StatementPtr &x) {
    LocationPtr location = currentLocation();
    int bk;
    if (!bindingKind(bk)) return false;
    vector<IdentifierPtr> y;
    if (!identifierList(y)) return false;
    if (!symbol("=")) return false;
    ExprListPtr z;
    if (!expressionList(z)) return false;
    if (!symbol(";")) return false;
    x = new Binding(bk, y, z);
    x->location = location;
    return true;
}

static bool blockItem(StatementPtr &x) {
    int p = save();
    if (labelDef(x)) return true;
    if (restore(p), localBinding(x)) return true;
    if (restore(p), statement(x)) return true;
    return false;
}

static bool block(StatementPtr &x) {
    LocationPtr location = currentLocation();
    if (!symbol("{")) return false;
    BlockPtr y = new Block();
    while (true) {
        int p = save();
        StatementPtr z;
        if (!blockItem(z)) {
            restore(p);
            break;
        }
        y->statements.push_back(z);
    }
    if (!symbol("}")) return false;
    x = y.ptr();
    x->location = location;
    return true;
}

static bool assignment(StatementPtr &x) {
    LocationPtr location = currentLocation();
    ExprListPtr y, z;
    if (!expressionList(y)) return false;
    if (!symbol("=")) return false;
    if (!expressionList(z)) return false;
    if (!symbol(";")) return false;
    x = new Assignment(y, z);
    x->location = location;
    return true;
}

static bool initAssignment(StatementPtr &x) {
    LocationPtr location = currentLocation();
    ExprListPtr y, z;
    if (!expressionList(y)) return false;
    if (!symbol("<--")) return false;
    if (!expressionList(z)) return false;
    if (!symbol(";")) return false;
    x = new InitAssignment(y, z);
    x->location = location;
    return true;
}

static bool updateOp(int &op) {
    int p = save();
    const char *s[] = {"+=", "-=", "*=", "/=", "%=", NULL};
    const int ops[] = {UPDATE_ADD, UPDATE_SUBTRACT, UPDATE_MULTIPLY,
                       UPDATE_DIVIDE, UPDATE_REMAINDER};
    for (const char **a = s; *a; ++a) {
        restore(p);
        if (symbol(*a)) {
            int i = a - s;
            op = ops[i];
            return true;
        }
    }
    return false;
}

static bool updateAssignment(StatementPtr &x) {
    LocationPtr location = currentLocation();
    ExprPtr y, z;
    if (!expression(y)) return false;
    int op;
    if (!updateOp(op)) return false;
    if (!expression(z)) return false;
    if (!symbol(";")) return false;
    x = new UpdateAssignment(op, y, z);
    x->location = location;
    return true;
}

static bool gotoStatement(StatementPtr &x) {
    LocationPtr location = currentLocation();
    IdentifierPtr y;
    if (!keyword("goto")) return false;
    if (!identifier(y)) return false;
    if (!symbol(";")) return false;
    x = new Goto(y);
    x->location = location;
    return true;
}

static bool returnStatement(StatementPtr &x) {
    LocationPtr location = currentLocation();
    if (!keyword("return")) return false;
    ReturnKind rkind;
    ExprListPtr exprs;
    if (!returnExprList(rkind, exprs)) return false;
    if (!symbol(";")) return false;
    x = new Return(rkind, exprs);
    x->location = location;
    return true;
}

static bool ifStatement(StatementPtr &x) {
    LocationPtr location = currentLocation();
    ExprPtr y;
    StatementPtr z, z2;
    if (!keyword("if")) return false;
    if (!symbol("(")) return false;
    if (!expression(y)) return false;
    if (!symbol(")")) return false;
    if (!statement(z)) return false;
    int p = save();
    if (!keyword("else") || !statement(z2))
        restore(p);
    x = new If(y, z, z2);
    x->location = location;
    return true;
}

static bool caseLabel(ExprPtr &x) {
    if (!keyword("case")) return false;
    ExprPtr expr;
    if (!expressionNoPair(expr)) return false;
    if (!symbol(":")) return false;
    x = expr;
    return true;
}

static bool caseLabelList(ExprListPtr &x) {
    ExprPtr a;
    if (!caseLabel(a)) return false;
    x = new ExprList(a);
    while (true) {
        int p = save();
        if (!caseLabel(a)) {
            restore(p);
            break;
        }
        x->add(a);
    }
    return true;
}

static bool caseBody(StatementPtr &x) {
    LocationPtr location = currentLocation();
    StatementPtr a;
    if (!blockItem(a)) return false;
    vector<StatementPtr> statements;
    statements.push_back(a);
    while (true) {
        int p = save();
        if (!blockItem(a)) {
            restore(p);
            break;
        }
        statements.push_back(a);
    }
    x = new CaseBody(statements);
    x->location = location;
    return true;
}

static bool caseBlock(CaseBlockPtr &x) {
    LocationPtr location = currentLocation();
    ExprListPtr caseLabels;
    StatementPtr body;
    if (!caseLabelList(caseLabels)) return false;
    if (!caseBody(body)) return false;
    x = new CaseBlock(caseLabels, body);
    x->location = location;
    return true;
}

static bool caseBlockList(vector<CaseBlockPtr> &x) {
    CaseBlockPtr a;
    if (!caseBlock(a)) return false;
    x.clear();
    x.push_back(a);
    while (true) {
        int p = save();
        if (!caseBlock(a)) {
            restore(p);
            break;
        }
        x.push_back(a);
    }
    return true;
}

static bool defaultCase(StatementPtr &x) {
    if (!keyword("default")) return false;
    if (!symbol(":")) return false;
    return caseBody(x);
}

static bool optDefaultCase(StatementPtr &x) {
    int p = save();
    if (!defaultCase(x)) {
        restore(p);
        x = NULL;
    }
    return true;
}

static bool switchStatement(StatementPtr &x) {
    LocationPtr location = currentLocation();
    ExprPtr expr;
    if (!keyword("switch")) return false;
    if (!symbol("(")) return false;
    if (!expression(expr)) return false;
    if (!symbol(")")) return false;
    if (!symbol("{")) return false;
    vector<CaseBlockPtr> caseBlocks;
    if (!caseBlockList(caseBlocks)) return false;
    StatementPtr defaultCase;
    if (!optDefaultCase(defaultCase)) return false;
    if (!symbol("}")) return false;
    x = new Switch(expr, caseBlocks, defaultCase);
    x->location = location;
    return true;
}

static bool exprStatement(StatementPtr &x) {
    LocationPtr location = currentLocation();
    ExprPtr y;
    if (!expression(y)) return false;
    if (!symbol(";")) return false;
    x = new ExprStatement(y);
    x->location = location;
    return true;
}

static bool whileStatement(StatementPtr &x) {
    LocationPtr location = currentLocation();
    ExprPtr y;
    StatementPtr z;
    if (!keyword("while")) return false;
    if (!symbol("(")) return false;
    if (!expression(y)) return false;
    if (!symbol(")")) return false;
    if (!statement(z)) return false;
    x = new While(y, z);
    x->location = location;
    return true;
}

static bool breakStatement(StatementPtr &x) {
    LocationPtr location = currentLocation();
    if (!keyword("break")) return false;
    if (!symbol(";")) return false;
    x = new Break();
    x->location = location;
    return true;
}

static bool continueStatement(StatementPtr &x) {
    LocationPtr location = currentLocation();
    if (!keyword("continue")) return false;
    if (!symbol(";")) return false;
    x = new Continue();
    x->location = location;
    return true;
}

static bool forStatement(StatementPtr &x) {
    LocationPtr location = currentLocation();
    vector<IdentifierPtr> a;
    ExprPtr b;
    StatementPtr c;
    if (!keyword("for")) return false;
    if (!symbol("(")) return false;
    if (!identifierList(a)) return false;
    if (!keyword("in")) return false;
    if (!expression(b)) return false;
    if (!symbol(")")) return false;
    if (!statement(c)) return false;
    x = new For(a, b, c);
    x->location = location;
    return true;
}

static bool catchBlock(CatchPtr &x) {
    LocationPtr location = currentLocation();
    if (!keyword("catch")) return false;
    if (!symbol("(")) return false;
    IdentifierPtr evar;
    if (!identifier(evar)) return false;
    ExprPtr etype;
    if (!optExprTypeSpec(etype)) return false;
    if (!symbol(")")) return false;
    StatementPtr body;
    if (!block(body)) return false;
    x = new Catch(evar, etype, body);
    x->location = location;
    return true;
}

static bool catchBlockList(vector<CatchPtr> &x) {
    CatchPtr a;
    if (!catchBlock(a)) return false;
    x.clear();
    x.push_back(a);
    while (true) {
        int p = save();
        if (!catchBlock(a)) {
            restore(p);
            break;
        }
        x.push_back(a);
    }
    return true;
}

static bool tryStatement(StatementPtr &x) {
    LocationPtr location = currentLocation();
    StatementPtr a;
    if (!keyword("try")) return false;
    if (!block(a)) return false;
    vector<CatchPtr> b;
    if (!catchBlockList(b)) return false;
    x = new Try(a, b);
    x->location = location;
    return true;
}

static bool throwStatement(StatementPtr &x) {
    LocationPtr location = currentLocation();
    ExprPtr a;
    if (!keyword("throw")) return false;
    if (!optExpression(a)) return false;
    if (!symbol(";")) return false;
    x = new Throw(a);
    x->location = location;
    return true;
}

static bool staticFor(StatementPtr &x) {
    LocationPtr location = currentLocation();
    if (!keyword("static")) return false;
    if (!keyword("for")) return false;
    if (!symbol("(")) return false;
    IdentifierPtr a;
    if (!identifier(a)) return false;
    if (!keyword("in")) return false;
    ExprListPtr b;
    if (!expressionList(b)) return false;
    if (!symbol(")")) return false;
    StatementPtr c;
    if (!statement(c)) return false;
    x = new StaticFor(a, b, c);
    x->location = location;
    return true;
}

static bool statement(StatementPtr &x) {
    int p = save();
    if (block(x)) return true;
    if (restore(p), assignment(x)) return true;
    if (restore(p), initAssignment(x)) return true;
    if (restore(p), updateAssignment(x)) return true;
    if (restore(p), ifStatement(x)) return true;
    if (restore(p), gotoStatement(x)) return true;
    if (restore(p), switchStatement(x)) return true;
    if (restore(p), returnStatement(x)) return true;
    if (restore(p), exprStatement(x)) return true;
    if (restore(p), whileStatement(x)) return true;
    if (restore(p), breakStatement(x)) return true;
    if (restore(p), continueStatement(x)) return true;
    if (restore(p), forStatement(x)) return true;
    if (restore(p), tryStatement(x)) return true;
    if (restore(p), throwStatement(x)) return true;
    if (restore(p), staticFor(x)) return true;

    return false;
}



//
// staticParams
//

static bool staticVarParam(IdentifierPtr &varParam) {
    if (!symbol("...")) return false;
    if (!identifier(varParam)) return false;
    return true;
}

static bool optStaticVarParam(IdentifierPtr &varParam) {
    int p = save();
    if (!staticVarParam(varParam)) {
        restore(p);
        varParam = NULL;
    }
    return true;
}

static bool trailingVarParam(IdentifierPtr &varParam) {
    if (!symbol(",")) return false;
    if (!staticVarParam(varParam)) return false;
    return true;
}

static bool optTrailingVarParam(IdentifierPtr &varParam) {
    int p = save();
    if (!trailingVarParam(varParam)) {
        restore(p);
        varParam = NULL;
    }
    return true;
}

static bool paramsAndVarParam(vector<IdentifierPtr> &params,
                              IdentifierPtr &varParam) {
    if (!identifierListNoTail(params)) return false;
    if (!optTrailingVarParam(varParam)) return false;
    int p = save();
    if (!symbol(","))
        restore(p);
    return true;
}

static bool staticParamsInner(vector<IdentifierPtr> &params,
                              IdentifierPtr &varParam) {
    int p = save();
    if (paramsAndVarParam(params, varParam)) return true;
    restore(p);
    params.clear(); varParam = NULL;
    return optStaticVarParam(varParam);
}

static bool staticParams(vector<IdentifierPtr> &params,
                         IdentifierPtr &varParam) {
    if (!symbol("[")) return false;
    if (!staticParamsInner(params, varParam)) return false;
    if (!symbol("]")) return false;
    return true;
}

static bool optStaticParams(vector<IdentifierPtr> &params,
                            IdentifierPtr &varParam) {
    int p = save();
    if (!staticParams(params, varParam)) {
        restore(p);
        params.clear();
        varParam = NULL;
    }
    return true;
}



//
// code
//

static bool optArgTempness(ValueTempness &tempness) {
    int p = save();
    if (keyword("rvalue")) {
        tempness = TEMPNESS_RVALUE;
        return true;
    }
    restore(p);
    if (keyword("lvalue")) {
        tempness = TEMPNESS_LVALUE;
        return true;
    }
    restore(p);
    if (keyword("forward")) {
        tempness = TEMPNESS_FORWARD;
        return true;
    }
    restore(p);
    tempness = TEMPNESS_DONTCARE;
    return true;
}

static bool valueFormalArg(FormalArgPtr &x) {
    LocationPtr location = currentLocation();
    ValueTempness tempness;
    if (!optArgTempness(tempness)) return false;
    IdentifierPtr y;
    ExprPtr z;
    if (!identifier(y)) return false;
    if (!optTypeSpec(z)) return false;
    x = new FormalArg(y, z, tempness);
    x->location = location;
    return true;
}

static bool staticFormalArg(unsigned index, FormalArgPtr &x) {
    LocationPtr location = currentLocation();
    ExprPtr y;
    if (!keyword("static")) return false;
    if (!expression(y)) return false;

    if (y->exprKind == UNPACK) {
        error(y, "static keyword cannot be used with variadic arguments");
    }

    // desugar static args
    ostringstream sout;
    sout << "%arg" << index;
    IdentifierPtr argName = new Identifier(sout.str());

    ExprPtr staticName =
        new ForeignExpr("prelude",
                        new NameRef(new Identifier("Static")));

    IndexingPtr indexing = new Indexing(staticName, new ExprList(y));
    
    x = new FormalArg(argName, indexing.ptr());
    x->location = location;
    return true;
}

static bool formalArg(unsigned index, FormalArgPtr &x) {
    int p = save();
    if (valueFormalArg(x)) return true;
    if (restore(p), staticFormalArg(index, x)) return true;
    return false;
}

static bool formalArgs(vector<FormalArgPtr> &x) {
    FormalArgPtr y;
    if (!formalArg(x.size(), y)) return false;
    x.clear();
    x.push_back(y);
    while (true) {
        int p = save();
        if (!symbol(",") || !formalArg(x.size(), y)) {
            restore(p);
            break;
        }
        x.push_back(y);
    }
    return true;
}

static bool varArgTypeSpec(ExprPtr &vargType) {
    if (!symbol(":")) return false;
    if (!nameRef(vargType)) return false;
    return true;
}

static bool optVarArgTypeSpec(ExprPtr &vargType) {
    int p = save();
    if (!varArgTypeSpec(vargType)) {
        restore(p);
        vargType = NULL;
    }
    return true;
}

static bool varArg(FormalArgPtr &x) {
    LocationPtr location = currentLocation();
    ValueTempness tempness;
    if (!optArgTempness(tempness)) return false;
    if (!symbol("...")) return false;
    IdentifierPtr name;
    if (!identifier(name)) return false;
    ExprPtr type;
    if (!optVarArgTypeSpec(type)) return false;
    x = new FormalArg(name, type, tempness);
    x->location = location;
    return true;
}

static bool optVarArg(FormalArgPtr &x) {
    int p = save();
    if (!varArg(x)) {
        restore(p);
        x = NULL;
    }
    return true;
}

static bool trailingVarArg(FormalArgPtr &x) {
    if (!symbol(",")) return false;
    if (!varArg(x)) return false;
    return true;
}

static bool optTrailingVarArg(FormalArgPtr &x) {
    int p = save();
    if (!trailingVarArg(x)) {
        restore(p);
        x = NULL;
    }
    return true;
}

static bool formalArgsWithVArgs(vector<FormalArgPtr> &args,
                                FormalArgPtr &varg) {
    if (!formalArgs(args)) return false;
    if (!optTrailingVarArg(varg)) return false;
    return true;
}

static bool argumentsBody(vector<FormalArgPtr> &args,
                          FormalArgPtr &varg) {
    int p = save();
    if (formalArgsWithVArgs(args, varg)) return true;
    restore(p);
    args.clear();
    varg = NULL;
    return optVarArg(varg);
}

static bool arguments(vector<FormalArgPtr> &args,
                      FormalArgPtr &varg) {
    if (!symbol("(")) return false;
    if (!argumentsBody(args, varg)) return false;
    if (!symbol(")")) return false;
    return true;
}

static bool predicate(ExprPtr &x) {
    if (!symbol("|")) return false;
    return expression(x);
}

static bool optPredicate(ExprPtr &x) {
    int p = save();
    if (!predicate(x)) {
        restore(p);
        x = NULL;
    }
    return true;
}

static bool patternVar(PatternVar &x) {
    int p = save();
    x.isMulti = true;
    if (!symbol("...")) {
        restore(p);
        x.isMulti = false;
    }
    if (!identifier(x.name)) return false;
    return true;
}

static bool patternVarList(vector<PatternVar> &x) {
    PatternVar y;
    if (!patternVar(y)) return false;
    x.clear();
    x.push_back(y);
    while (true) {
        int p = save();
        if (!symbol(",") || !patternVar(y)) {
            restore(p);
            break;
        }
        x.push_back(y);
    }
    return true;
}

static bool optPatternVarList(vector<PatternVar> &x) {
    int p = save();
    if (!patternVarList(x)) {
        restore(p);
        x.clear();
    }
    return true;
}

static bool patternVarsWithCond(vector<PatternVar> &x, ExprPtr &y) {
    if (!symbol("[")) return false;
    if (!optPatternVarList(x)) return false;
    if (!optPredicate(y) || !symbol("]")) {
        x.clear();
        y = NULL;
        return false;
    }
    return true;
}

static bool optPatternVarsWithCond(vector<PatternVar> &x, ExprPtr &y) {
    int p = save();
    if (!patternVarsWithCond(x, y)) {
        restore(p);
        x.clear();
        y = NULL;
    }
    return true;
}

static bool exprBody(StatementPtr &x) {
    if (!symbol("=")) return false;
    LocationPtr location = currentLocation();
    ReturnKind rkind;
    ExprListPtr exprs;
    if (!returnExprList(rkind, exprs)) return false;
    if (!symbol(";")) return false;
    x = new Return(rkind, exprs);
    x->location = location;
    return true;
}

static bool body(StatementPtr &x) {
    int p = save();
    if (exprBody(x)) return true;
    if (restore(p), block(x)) return true;
    return false;
}

static bool optBody(StatementPtr &x) {
    int p = save();
    if (body(x)) return true;
    restore(p);
    x = NULL;
    if (symbol(";")) return true;
    return false;
}



//
// topLevelVisibility, importVisibility
//

bool optVisibility(Visibility defaultVisibility, Visibility &x) {
    int p = save();
    if (keyword("public")) {
        x = PUBLIC;
        return true;
    }
    restore(p);
    if (keyword("private")) {
        x = PRIVATE;
        return true;
    }
    restore(p);
    x = defaultVisibility;
    return true;
}

bool topLevelVisibility(Visibility &x) {
    return optVisibility(PUBLIC, x);
}

bool importVisibility(Visibility &x) {
    return optVisibility(PRIVATE, x);
}



//
// records
//

static bool recordField(RecordFieldPtr &x) {
    LocationPtr location = currentLocation();
    IdentifierPtr y;
    if (!identifier(y)) return false;
    ExprPtr z;
    if (!exprTypeSpec(z)) return false;
    x = new RecordField(y, z);
    x->location = location;
    return true;
}

static bool recordFields(vector<RecordFieldPtr> &x) {
    RecordFieldPtr y;
    if (!recordField(y)) return false;
    x.clear();
    x.push_back(y);
    while (true) {
        int p = save();
        if (!symbol(",")) {
            restore(p);
            break;
        }
        p = save();
        if (!recordField(y)) {
            restore(p);
            break;
        }
        x.push_back(y);
    }
    return true;
}

static bool optRecordFields(vector<RecordFieldPtr> &x) {
    int p = save();
    if (!recordFields(x)) {
        restore(p);
        x.clear();
    }
    return true;
}

static bool recordBodyFields(RecordBodyPtr &x) {
    LocationPtr location = currentLocation();
    if (!symbol("(")) return false;
    vector<RecordFieldPtr> y;
    if (!optRecordFields(y)) return false;
    if (!symbol(")")) return false;
    if (!symbol(";")) return false;
    x = new RecordBody(y);
    x->location = location;
    return true;
}

static bool recordBodyComputed(RecordBodyPtr &x) {
    LocationPtr location = currentLocation();
    if (!symbol("=")) return false;
    ExprListPtr y;
    if (!optExpressionList(y)) return false;
    if (!symbol(";")) return false;
    x = new RecordBody(y);
    x->location = location;
    return true;
}

static bool recordBody(RecordBodyPtr &x) {
    int p = save();
    if (recordBodyFields(x)) return true;
    if (restore(p), recordBodyComputed(x)) return true;
    return false;
}

static bool record(TopLevelItemPtr &x) {
    LocationPtr location = currentLocation();
    Visibility vis;
    if (!topLevelVisibility(vis)) return false;
    if (!keyword("record")) return false;
    RecordPtr y = new Record(vis);
    if (!identifier(y->name)) return false;
    if (!optStaticParams(y->params, y->varParam)) return false;
    if (!recordBody(y->body)) return false;
    x = y.ptr();
    x->location = location;
    return true;
}



//
// variant, instance
//

static bool defaultInstances(ExprListPtr &x) {
    ExprListPtr a;
    ExprPtr y;
    if (!expression(y)) return false;
    a = new ExprList(y);
    while (true) {
        int p = save();
        if (!symbol("|") || !expression(y)) {
            restore(p);
            break;
        }
        a->add(y);
    }
    x = a;
    return true;
}

static bool optDefaultInstances(ExprListPtr &x) {
    int p = save();
    if (!symbol("=")) {
        restore(p);
        x = new ExprList();
        return true;
    }
    return defaultInstances(x);
}

static bool variant(TopLevelItemPtr &x) {
    LocationPtr location = currentLocation();
    Visibility vis;
    if (!topLevelVisibility(vis)) return false;
    if (!keyword("variant")) return false;
    IdentifierPtr name;
    if (!identifier(name)) return false;
    vector<IdentifierPtr> params;
    IdentifierPtr varParam;
    if (!optStaticParams(params, varParam)) return false;
    ExprListPtr defaultInstances;
    if (!optDefaultInstances(defaultInstances)) return false;
    if (!symbol(";")) return false;
    x = new Variant(name, vis, params, varParam, defaultInstances);
    x->location = location;
    return true;
}

static bool instance(TopLevelItemPtr &x) {
    LocationPtr location = currentLocation();
    vector<PatternVar> patternVars;
    ExprPtr predicate;
    if (!optPatternVarsWithCond(patternVars, predicate)) return false;
    if (!keyword("instance")) return false;
    ExprPtr target;
    if (!pattern(target)) return false;
    if (!symbol("=")) return false;
    ExprPtr member;
    if (!expression(member)) return false;
    if (!symbol(";")) return false;
    x = new Instance(patternVars, predicate, target, member);
    x->location = location;
    return true;
}



//
// returnSpec
//

static bool namedReturn(IdentifierPtr &x) {
    IdentifierPtr y;
    if (!identifier(y)) return false;
    if (!symbol(":")) return false;
    x = y;
    return true;
}

static bool optNamedReturn(IdentifierPtr &x) {
    int p = save();
    if (!namedReturn(x)) {
        restore(p);
        x = NULL;
    }
    return true;
}

static bool returnTypeExpression(ExprPtr &x) {
    return orExpr(x);
}

static bool returnSpec(ReturnSpecPtr &x) {
    LocationPtr location = currentLocation();
    IdentifierPtr y;
    if (!optNamedReturn(y)) return false;
    ExprPtr z;
    if (!returnTypeExpression(z)) return false;
    x = new ReturnSpec(z, y);
    x->location = location;
    return true;
}

static bool returnSpecList(vector<ReturnSpecPtr> &x) {
    ReturnSpecPtr y;
    if (!returnSpec(y)) return false;
    x.clear();
    x.push_back(y);
    while (true) {
        int p = save();
        if (!symbol(",") || !returnSpec(y)) {
            restore(p);
            break;
        }
        x.push_back(y);
    }
    return true;
}

static bool optReturnSpecList(vector<ReturnSpecPtr> &x) {
    int p = save();
    if (!returnSpecList(x)) {
        restore(p);
        x.clear();
    }
    return true;
}

static bool varReturnSpec(ReturnSpecPtr &x) {
    LocationPtr location = currentLocation();
    if (!symbol("...")) return false;
    IdentifierPtr y;
    if (!optNamedReturn(y)) return false;
    ExprPtr z;
    if (!returnTypeExpression(z)) return false;
    x = new ReturnSpec(z, y);
    x->location = location;
    return true;
}

static bool optVarReturnSpec(ReturnSpecPtr &x) {
    int p = save();
    if (!varReturnSpec(x)) {
        restore(p);
        x = NULL;
    }
    return true;
}

static bool allReturnSpecs(vector<ReturnSpecPtr> &returnSpecs,
                           ReturnSpecPtr &varReturnSpec) {
    returnSpecs.clear();
    varReturnSpec = NULL;
    if (!optReturnSpecList(returnSpecs)) return false;
    if (!optVarReturnSpec(varReturnSpec)) return false;
    return true;
}



//
// procedure, overload
//

static bool optInline(bool &isInline) {
    int p = save();
    if (!keyword("inline")) {
        restore(p);
        isInline = false;
        return true;
    }
    isInline = true;
    return true;
}

static bool optCallByName(bool &callByName) {
    int p = save();
    if (!keyword("callbyname")) {
        restore(p);
        callByName = false;
        return true;
    }
    callByName = true;
    return true;
}
    
static bool llvmCode(LLVMCodePtr &b) {
    TokenPtr t;
    if (!next(t) || (t->tokenKind != T_LLVM))
        return false;
    b = new LLVMCode(t->str);
    b->location = t->location;
    return true;
}

static bool llvmProcedure(vector<TopLevelItemPtr> &x) {
    LocationPtr location = currentLocation();
    CodePtr y = new Code();
    if (!optPatternVarsWithCond(y->patternVars, y->predicate)) return false;
    Visibility vis;
    if (!topLevelVisibility(vis)) return false;
    IdentifierPtr z;
    if (!identifier(z)) return false;
    if (!arguments(y->formalArgs, y->formalVarArg)) return false;
    if (!optReturnSpecList(y->returnSpecs)) return false;
    if (!llvmCode(y->llvmBody)) return false;
    y->location = location;

    ProcedurePtr u = new Procedure(z, vis);
    u->location = location;
    x.push_back(u.ptr());

    ExprPtr target = new NameRef(z);
    target->location = location;
    OverloadPtr v = new Overload(target, y, false, false);
    v->location = location;
    x.push_back(v.ptr());

    return true;
}

static bool procedureWithBody(vector<TopLevelItemPtr> &x) {
    LocationPtr location = currentLocation();
    CodePtr y = new Code();
    if (!optPatternVarsWithCond(y->patternVars, y->predicate)) return false;
    Visibility vis;
    if (!topLevelVisibility(vis)) return false;
    bool isInline;
    if (!optInline(isInline)) return false;
    bool callByName;
    if (!optCallByName(callByName)) return false;
    int p = save();
    if (!keyword("procedure"))
        restore(p);
    IdentifierPtr z;
    if (!identifier(z)) return false;
    if (!arguments(y->formalArgs, y->formalVarArg)) return false;
    if (!allReturnSpecs(y->returnSpecs, y->varReturnSpec)) return false;
    if (!body(y->body)) return false;
    y->location = location;

    ProcedurePtr u = new Procedure(z, vis);
    u->location = location;
    x.push_back(u.ptr());

    ExprPtr target = new NameRef(z);
    target->location = location;
    OverloadPtr v = new Overload(target, y, callByName, isInline);
    v->location = location;
    x.push_back(v.ptr());

    return true;
}

static bool procedure(TopLevelItemPtr &x) {
    LocationPtr location = currentLocation();
    Visibility vis;
    if (!topLevelVisibility(vis)) return false;
    if (!keyword("procedure")) return false;
    IdentifierPtr y;
    if (!identifier(y)) return false;
    if (!symbol(";")) return false;
    x = new Procedure(y, vis);
    x->location = location;
    return true;
}

static bool overload(TopLevelItemPtr &x) {
    LocationPtr location = currentLocation();
    CodePtr y = new Code();
    if (!optPatternVarsWithCond(y->patternVars, y->predicate)) return false;
    bool isInline;
    if (!optInline(isInline)) return false;
    bool callByName;
    if (!optCallByName(callByName)) return false;
    if (!keyword("overload")) return false;
    ExprPtr z;
    if (!pattern(z)) return false;
    if (!arguments(y->formalArgs, y->formalVarArg)) return false;
    if (!allReturnSpecs(y->returnSpecs, y->varReturnSpec)) return false;
    int p = save();
    if (!optBody(y->body)) {
        restore(p);
        if (callByName) return false;
        if (!llvmCode(y->llvmBody)) return false;
    }
    y->location = location;
    x = new Overload(z, y, callByName, isInline);
    x->location = location;
    return true;
}



//
// enumerations
//

static bool enumMember(EnumMemberPtr &x) {
    LocationPtr location = currentLocation();
    IdentifierPtr y;
    if (!identifier(y)) return false;
    x = new EnumMember(y);
    x->location = location;
    return true;
}

static bool enumMemberList(vector<EnumMemberPtr> &x) {
    EnumMemberPtr y;
    if (!enumMember(y)) return false;
    x.clear();
    x.push_back(y);
    while (true) {
        int p = save();
        if (!symbol(",")) {
            restore(p);
            break;
        }
        p = save();
        if (!enumMember(y)) {
            restore(p);
            break;
        }
        x.push_back(y);
    }
    return true;
}

static bool enumeration(TopLevelItemPtr &x) {
    LocationPtr location = currentLocation();
    Visibility vis;
    if (!topLevelVisibility(vis)) return false;
    if (!keyword("enum")) return false;
    IdentifierPtr y;
    if (!identifier(y)) return false;
    EnumerationPtr z = new Enumeration(y, vis);
    if (!symbol("{")) return false;
    if (!enumMemberList(z->members)) return false;
    if (!symbol("}")) return false;
    x = z.ptr();
    x->location = location;
    return true;
}



//
// global variable
//

static bool globalVariable(TopLevelItemPtr &x) {
    LocationPtr location = currentLocation();
    Visibility vis;
    if (!topLevelVisibility(vis)) return false;
    if (!keyword("var")) return false;
    IdentifierPtr name;
    if (!identifier(name)) return false;
    vector<IdentifierPtr> params;
    IdentifierPtr varParam;
    if (!optStaticParams(params, varParam)) return false;
    if (!symbol("=")) return false;
    ExprPtr expr;
    if (!expression(expr)) return false;
    if (!symbol(";")) return false;
    x = new GlobalVariable(name, vis, params, varParam, expr);
    x->location = location;
    return true;
}



//
// external procedure, external variable
//

static bool externalAttributes(ExprListPtr &x) {
    if (!symbol("(")) return false;
    if (!expressionList(x)) return false;
    if (!symbol(")")) return false;
    return true;
}

static bool optExternalAttributes(ExprListPtr &x) {
    int p = save();
    if (!externalAttributes(x)) {
        restore(p);
        x = new ExprList();
    }
    return true;
}

static bool externalArg(ExternalArgPtr &x) {
    LocationPtr location = currentLocation();
    IdentifierPtr y;
    if (!identifier(y)) return false;
    ExprPtr z;
    if (!exprTypeSpec(z)) return false;
    x = new ExternalArg(y, z);
    x->location = location;
    return true;
}

static bool externalArgs(vector<ExternalArgPtr> &x) {
    ExternalArgPtr y;
    if (!externalArg(y)) return false;
    x.clear();
    x.push_back(y);
    while (true) {
        int p = save();
        if (!symbol(",") || !externalArg(y)) {
            restore(p);
            break;
        }
        x.push_back(y);
    }
    return true;
}

static bool externalVarArgs(bool &hasVarArgs) {
    if (!symbol("...")) return false;
    hasVarArgs = true;
    return true;
}

static bool optExternalVarArgs(bool &hasVarArgs) {
    int p = save();
    if (!externalVarArgs(hasVarArgs)) {
        restore(p);
        hasVarArgs = false;
    }
    return true;
}

static bool trailingExternalVarArgs(bool &hasVarArgs) {
    if (!symbol(",")) return false;
    if (!externalVarArgs(hasVarArgs)) return false;
    return true;
}

static bool optTrailingExternalVarArgs(bool &hasVarArgs) {
    int p = save();
    if (!trailingExternalVarArgs(hasVarArgs)) {
        restore(p);
        hasVarArgs = false;
    }
    return true;
}

static bool externalArgsWithVArgs(vector<ExternalArgPtr> &x,
                                  bool &hasVarArgs) {
    if (!externalArgs(x)) return false;
    if (!optTrailingExternalVarArgs(hasVarArgs)) return false;
    return true;
}

static bool externalArgsBody(vector<ExternalArgPtr> &x,
                             bool &hasVarArgs) {
    int p = save();
    if (externalArgsWithVArgs(x, hasVarArgs)) return true;
    restore(p);
    x.clear();
    hasVarArgs = false;
    if (!optExternalVarArgs(hasVarArgs)) return false;
    return true;
}

static bool externalBody(StatementPtr &x) {
    int p = save();
    if (exprBody(x)) return true;
    if (restore(p), block(x)) return true;
    if (restore(p), symbol(";")) {
        x = NULL;
        return true;
    }
    return false;
}

static bool external(TopLevelItemPtr &x) {
    LocationPtr location = currentLocation();
    Visibility vis;
    if (!topLevelVisibility(vis)) return false;
    if (!keyword("external")) return false;
    ExternalProcedurePtr y = new ExternalProcedure(vis);
    if (!optExternalAttributes(y->attributes)) return false;
    if (!identifier(y->name)) return false;
    if (!symbol("(")) return false;
    if (!externalArgsBody(y->args, y->hasVarArgs)) return false;
    if (!symbol(")")) return false;
    if (!optExpression(y->returnType)) return false;
    if (!externalBody(y->body)) return false;
    x = y.ptr();
    x->location = location;
    return true;
}

static bool externalVariable(TopLevelItemPtr &x) {
    LocationPtr location = currentLocation();
    Visibility vis;
    if (!topLevelVisibility(vis)) return false;
    if (!keyword("external")) return false;
    ExternalVariablePtr y = new ExternalVariable(vis);
    if (!optExternalAttributes(y->attributes)) return false;
    if (!identifier(y->name)) return false;
    if (!exprTypeSpec(y->type)) return false;
    if (!symbol(";")) return false;
    x = y.ptr();
    x->location = location;
    return true;
}



//
// global alias
//

static bool globalAlias(TopLevelItemPtr &x) {
    LocationPtr location = currentLocation();
    Visibility vis;
    if (!topLevelVisibility(vis)) return false;
    if (!keyword("alias")) return false;
    IdentifierPtr name;
    if (!identifier(name)) return false;
    vector<IdentifierPtr> params;
    IdentifierPtr varParam;
    if (!optStaticParams(params, varParam)) return false;
    if (!symbol("=")) return false;
    ExprPtr expr;
    if (!expression(expr)) return false;
    if (!symbol(";")) return false;
    x = new GlobalAlias(name, vis, params, varParam, expr);
    x->location = location;
    return true;
}



//
// imports
//

static bool importAlias(IdentifierPtr &x) {
    if (!keyword("as")) return false;
    if (!identifier(x)) return false;
    return true;
}

static bool optImportAlias(IdentifierPtr &x) {
    int p = save();
    if (!importAlias(x)) {
        restore(p);
        x = NULL;
    }
    return true;
}

static bool importModule(ImportPtr &x) {
    LocationPtr location = currentLocation();
    Visibility vis;
    if (!importVisibility(vis)) return false;
    if (!keyword("import")) return false;
    DottedNamePtr y;
    if (!dottedName(y)) return false;
    IdentifierPtr z;
    if (!optImportAlias(z)) return false;
    if (!symbol(";")) return false;
    x = new ImportModule(y, vis, z);
    x->location = location;
    return true;
}

static bool importStar(ImportPtr &x) {
    LocationPtr location = currentLocation();
    Visibility vis;
    if (!importVisibility(vis)) return false;
    if (!keyword("import")) return false;
    DottedNamePtr y;
    if (!dottedName(y)) return false;
    if (!symbol(".")) return false;
    if (!symbol("*")) return false;
    if (!symbol(";")) return false;
    x = new ImportStar(y, vis);
    x->location = location;
    return true;
}

static bool importedMember(ImportedMember &x) {
    if (!identifier(x.name)) return false;
    if (!optImportAlias(x.alias)) return false;
    return true;
}

static bool importedMemberList(vector<ImportedMember> &x) {
    ImportedMember y;
    if (!importedMember(y)) return false;
    x.clear();
    x.push_back(y);
    while (true) {
        int p = save();
        if (!symbol(",") || !importedMember(y)) {
            restore(p);
            break;
        }
        x.push_back(y);
    }
    return true;
}

static bool importMembers(ImportPtr &x) {
    LocationPtr location = currentLocation();
    Visibility vis;
    if (!importVisibility(vis)) return false;
    if (!keyword("import")) return false;
    DottedNamePtr y;
    if (!dottedName(y)) return false;
    if (!symbol(".")) return false;
    if (!symbol("(")) return false;
    ImportMembersPtr z = new ImportMembers(y, vis);
    if (!importedMemberList(z->members)) return false;
    if (!symbol(")")) return false;
    if (!symbol(";")) return false;
    x = z.ptr();
    x->location = location;
    return true;
}

static bool import(ImportPtr &x) {
    int p = save();
    if (importModule(x)) return true;
    if (restore(p), importStar(x)) return true;
    if (restore(p), importMembers(x)) return true;
    return false;
}

static bool imports(vector<ImportPtr> &x) {
    x.clear();
    while (true) {
        int p = save();
        ImportPtr y;
        if (!import(y)) {
            restore(p);
            break;
        }
        x.push_back(y);
    }
    return true;
}



//
// module
//

static bool topLevelItem(vector<TopLevelItemPtr> &x) {
    int p = save();
    TopLevelItemPtr y;
    if (record(y)) goto success;
    if (restore(p), variant(y)) goto success;
    if (restore(p), instance(y)) goto success;
    if (restore(p), procedure(y)) goto success;
    if (restore(p), procedureWithBody(x)) goto success2;
    if (restore(p), llvmProcedure(x)) goto success2;
    if (restore(p), overload(y)) goto success;
    if (restore(p), enumeration(y)) goto success;
    if (restore(p), globalVariable(y)) goto success;
    if (restore(p), external(y)) goto success;
    if (restore(p), externalVariable(y)) goto success;
    if (restore(p), globalAlias(y)) goto success;
    return false;
success :
    assert(y.ptr());
    x.push_back(y);
success2 :
    return true;
}

static bool topLevelItems(vector<TopLevelItemPtr> &x) {
    x.clear();
    while (true) {
        int p = save();
        if (!topLevelItem(x)) {
            restore(p);
            break;
        }
    }
    return true;
}

static bool optTopLevelLLVM(LLVMCodePtr &x) {
    int p = save();
    if (!llvmCode(x)) {
        restore(p);
        x = NULL;
    }
    return true;
}

static bool module(const string &moduleName, ModulePtr &x) {
    LocationPtr location = currentLocation();
    ModulePtr y = new Module(moduleName);
    if (!imports(y->imports)) return false;
    if (!optTopLevelLLVM(y->topLevelLLVM)) return false;
    if (!topLevelItems(y->topLevelItems)) return false;
    x = y.ptr();
    x->location = location;
    return true;
}



//
// parse
//

ModulePtr parse(const string &moduleName, SourcePtr source) {
    vector<TokenPtr> t;
    tokenize(source, t);

    tokens = &t;
    position = maxPosition = 0;

    ModulePtr m;
    if (!module(moduleName, m) || (position < (int)t.size())) {
        LocationPtr location;
        if (maxPosition == (int)t.size())
            location = new Location(source, source->size);
        else
            location = t[maxPosition]->location;
        pushLocation(location);
        error("parse error");
    }

    tokens = NULL;
    position = maxPosition = 0;

    return m;
}



//
// parseExpr
//

ExprPtr parseExpr(SourcePtr source, int offset, int length) {
    vector<TokenPtr> t;
    tokenize(source, offset, length, t);

    tokens = &t;
    position = maxPosition = 0;

    ExprPtr expr;
    if (!expression(expr) || (position < (int)t.size())) {
        LocationPtr location;
        if (maxPosition == (int)t.size())
            location = new Location(source, offset + length);
        else
            location = t[maxPosition]->location;
        pushLocation(location);
        error("parse error");
    }

    tokens = NULL;
    position = maxPosition = 0;

    return expr;
}
