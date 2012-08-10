#include "clay.hpp"

namespace clay {

map<llvm::StringRef, IdentifierPtr> Identifier::freeIdentifiers;

static vector<Token> *tokens;
static int position;
static int maxPosition;
static bool parserOptionKeepDocumentation = false;

static bool next(Token *&x) {
    if (position == (int)tokens->size())
        return false;
    x = &(*tokens)[position];
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

static Location currentLocation() {
    if (position == (int)tokens->size())
        return Location();
    return (*tokens)[position].location;
}



//
// symbol, keyword
//

static bool opstring(llvm::StringRef &op) {
    Token *t;
    if (!next(t) || (t->tokenKind != T_OPSTRING))
        return false;
    op = llvm::StringRef(t->str);
    return true;
}

static bool uopstring(llvm::StringRef &op) {
    Token* t;
    if (!next(t) || (t->tokenKind != T_UOPSTRING))
        return false;
    op = llvm::StringRef(t->str);
    return true;
}

static bool opsymbol(const char *s) {
    Token* t;
    if (!next(t) || (t->tokenKind != T_OPSTRING))
        return false;
    return t->str == s;
}

static bool symbol(const char *s) {
    Token* t;
    if (!next(t) || (t->tokenKind != T_SYMBOL))
        return false;
    return t->str == s;
}

static bool keyword(const char *s) {
    Token* t;
    if (!next(t) || (t->tokenKind != T_KEYWORD))
        return false;
    return t->str == s;
}

static bool ellipsis() {
    return symbol("..");
}


//
// identifier, identifierList,
// identifierListNoTail, dottedName
//

static bool identifier(IdentifierPtr &x) {
    Location location = currentLocation();
    Token* t;
    if (!next(t) || (t->tokenKind != T_IDENTIFIER))
        return false;
    x = Identifier::get(t->str, location);
    return true;
}

static bool identifierList(vector<IdentifierPtr> &x) {
    IdentifierPtr y;
    if (!identifier(y)) return false;
    x.clear();
    while (true) {
        x.push_back(y);
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
    }
    return true;
}

static bool identifierListNoTail(vector<IdentifierPtr> &x) {
    IdentifierPtr y;
    if (!identifier(y)) return false;
    x.clear();
    while (true) {
        x.push_back(y);
        int p = save();
        if (!symbol(",") || !identifier(y)) {
            restore(p);
            break;
        }
    }
    return true;
}

static bool dottedName(DottedNamePtr &x) {
    Location location = currentLocation();
    DottedNamePtr y = new DottedName();
    IdentifierPtr ident;
    if (!identifier(ident)) return false;
    while (true) {
        y->parts.push_back(ident);
        int p = save();
        if (!symbol(".") || !identifier(ident)) {
            restore(p);
            break;
        }
    }
    x = y;
    x->location = location;
    return true;
}



//
// literals
//

static bool boolLiteral(ExprPtr &x) {
    Location location = currentLocation();
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

static string cleanNumericSeparator(llvm::StringRef op, llvm::StringRef s) {
    string out;
    if (op == "-")
        out.push_back('-');
    for (unsigned i = 0; i < s.size(); ++i) {
        if (s[i] != '_')
            out.push_back(s[i]);
    }
    return out;
}

static bool intLiteral(llvm::StringRef op, ExprPtr &x) {
    Location location = currentLocation();
    Token* t;
    if (!next(t) || (t->tokenKind != T_INT_LITERAL))
        return false;
    Token* t2;
    int p = save();
    if (next(t2) && (t2->tokenKind == T_IDENTIFIER)) {
        x = new IntLiteral(cleanNumericSeparator(op, t->str), t2->str);
    }
    else {
        restore(p);
        x = new IntLiteral(cleanNumericSeparator(op, t->str));
    }
    x->location = location;
    return true;
}

static bool floatLiteral(llvm::StringRef op, ExprPtr &x) {
    Location location = currentLocation();
    Token* t;
    if (!next(t) || (t->tokenKind != T_FLOAT_LITERAL))
        return false;
    Token* t2;
    int p = save();
    if (next(t2) && (t2->tokenKind == T_IDENTIFIER)) {
        x = new FloatLiteral(cleanNumericSeparator(op, t->str), t2->str);
    }
    else {
        restore(p);
        x = new FloatLiteral(cleanNumericSeparator(op, t->str));
    }
    x->location = location;
    return true;
}


static bool charLiteral(ExprPtr &x) {
    Location location = currentLocation();
    Token* t;
    if (!next(t) || (t->tokenKind != T_CHAR_LITERAL))
        return false;
    x = new CharLiteral(t->str[0]);
    x->location = location;
    return true;
}

static bool stringLiteral(ExprPtr &x) {
    Location location = currentLocation();
    Token* t;
    if (!next(t) || (t->tokenKind != T_STRING_LITERAL))
        return false;
    IdentifierPtr id = Identifier::get(t->str, location);
    x = new StringLiteral(id);
    x->location = location;
    return true;
}

static bool literal(ExprPtr &x) {
    int p = save();
    if (boolLiteral(x)) return true;
    if (restore(p), intLiteral("+", x)) return true;
    if (restore(p), floatLiteral("+", x)) return true;
    if (restore(p), charLiteral(x)) return true;
    if (restore(p), stringLiteral(x)) return true;
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

static bool tupleExpr(ExprPtr &x) {
    Location location = currentLocation();
    if (!symbol("[")) return false;
    ExprListPtr args;
    if (!optExpressionList(args)) return false;
    if (!symbol("]")) return false;
    x = new Tuple(args);
    x->location = location;
    return true;
}

static bool parenExpr(ExprPtr &x) {
    Location location = currentLocation();
    if (!symbol("(")) return false;
    ExprListPtr args;
    if (!optExpressionList(args)) return false;
    if (!symbol(")")) return false;
    x = new Paren(args);
    x->location = location;
    return true;
}

static bool nameRef(ExprPtr &x) {
    Location location = currentLocation();
    IdentifierPtr a;
    if (!identifier(a)) return false;
    x = new NameRef(a);
    x->location = location;
    return true;
}

static bool fileExpr(ExprPtr &x) {
    Location location = currentLocation();
    if (!keyword("__FILE__")) return false;
    x = new FILEExpr();
    x->location = location;
    return true;
}

static bool lineExpr(ExprPtr &x) {
    Location location = currentLocation();
    if (!keyword("__LINE__")) return false;
    x = new LINEExpr();
    x->location = location;
    return true;
}

static bool columnExpr(ExprPtr &x) {
    Location location = currentLocation();
    if (!keyword("__COLUMN__")) return false;
    x = new COLUMNExpr();
    x->location = location;
    return true;
}

static bool argExpr(ExprPtr &x) {
    Location location = currentLocation();
    if (!keyword("__ARG__")) return false;
    IdentifierPtr name;
    if (!identifier(name)) return false;
    x = new ARGExpr(name);
    x->location = location;
    return true;
}

static bool evalExpr(ExprPtr &ev) {
    Location location = currentLocation();
    if (!keyword("eval")) return false;
    ExprPtr args;
    if (!expression(args)) return false;
    ev = new EvalExpr(args);
    ev->location = location;
    return true;
}

static bool atomicExpr(ExprPtr &x) {
    int p = save();
    if (parenExpr(x)) return true;
    if (restore(p), tupleExpr(x)) return true;
    if (restore(p), nameRef(x)) return true;
    if (restore(p), literal(x)) return true;
    if (restore(p), fileExpr(x)) return true;
    if (restore(p), lineExpr(x)) return true;
    if (restore(p), columnExpr(x)) return true;
    if (restore(p), argExpr(x)) return true;
    if (restore(p), evalExpr(x)) return true;
    return false;
}



//
// suffix expr
//

static bool stringLiteralSuffix(ExprPtr &x) {
    Location location = currentLocation();
    ExprPtr str;
    if (!stringLiteral(str)) return false;
    ExprListPtr strArgs = new ExprList(str);
    x = new Call(NULL, strArgs, new ExprList());
    x->location = location;
    return true;
}

static bool indexingSuffix(ExprPtr &x) {
    Location location = currentLocation();
    if (!symbol("[")) return false;
    ExprListPtr args;
    if (!optExpressionList(args)) return false;
    if (!symbol("]")) return false;
    x = new Indexing(NULL, args);
    x->location = location;
    return true;
}

static bool lambda(ExprPtr &x);

static bool optCallLambdaList(ExprListPtr &args) {
    args = new ExprList();

    int p = save();
    if (!symbol(":")) {
        restore(p);
        return true;
    }

    while (true) {
        ExprPtr lambdaArg;
        Location startLocation = currentLocation();
        if (!lambda(lambdaArg)) return false;
        lambdaArg->startLocation = startLocation;
        lambdaArg->endLocation = currentLocation();

        args->add(lambdaArg);
        p = save();
        if (!symbol("::")) {
            restore(p);
            return true;
        }
    }
}

static bool callSuffix(ExprPtr &x) {
    Location location = currentLocation();
    if (!symbol("(")) return false;
    ExprListPtr args;
    if (!optExpressionList(args)) return false;
    if (!symbol(")")) return false;
    ExprListPtr lambdaArgs;
    if (!optCallLambdaList(lambdaArgs)) return false;
    x = new Call(NULL, args, lambdaArgs);
    x->location = location;
    return true;
}

static bool fieldRefSuffix(ExprPtr &x) {
    Location location = currentLocation();
    if (!symbol(".")) return false;
    IdentifierPtr a;
    if (!identifier(a)) return false;
    x = new FieldRef(NULL, a);
    x->location = location;
    return true;
}

static bool staticIndexingSuffix(ExprPtr &x) {
    Location location = currentLocation();
    Token* t;
    if (!next(t) || (t->tokenKind != T_STATIC_INDEX))
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
    Location location = currentLocation();
    if (!symbol("^")) return false;
    x = new VariadicOp(DEREFERENCE, new ExprList());
    x->location = location;
    return true;
}

static bool suffix(ExprPtr &x) {
    int p = save();
    if (stringLiteralSuffix(x)) return true;
    if (restore(p), indexingSuffix(x)) return true;
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
    case VARIADIC_OP : {
        VariadicOp *b = (VariadicOp *)a;
        assert(b->op == DEREFERENCE);
        b->exprs->add(base);
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

static bool prefixExpr(ExprPtr &x);

static bool addressOfExpr(ExprPtr &x) {
    Location location = currentLocation();
    if (!symbol("@")) return false;
    ExprPtr a;
    if (!prefixExpr(a)) return false;
    x = new VariadicOp(ADDRESS_OF, new ExprList(a));
    x->location = location;
    return true;
}

static bool plusOrMinus(llvm::StringRef &op) {
    int p = save();
    if (opsymbol("+")) {
        op = llvm::StringRef("+");
        return true;
    } else if (restore(p), opsymbol("-")) {
        op = llvm::StringRef("-");
        return true;
    } else
        return false;
}

static bool signedLiteral(llvm::StringRef op, ExprPtr &x) {
    int p = save();
    if (restore(p), intLiteral(op, x)) return true;
    if (restore(p), floatLiteral(op, x)) return true;
    return false;
}

static bool dispatchExpr(ExprPtr &x) {
    Location location = currentLocation();
    if (!opsymbol("*")) return false;
    ExprPtr a;
    if (!prefixExpr(a)) return false;
    x = new DispatchExpr(a);
    x->location = location;
    return true;
}

static bool staticExpr(ExprPtr &x) {
    Location location = currentLocation();
    if (!symbol("#")) return false;
    ExprPtr y;
    if (!prefixExpr(y)) return false;
    x = new StaticExpr(y);
    x->location = location;
    return true;
}


static bool operatorOp(llvm::StringRef &op) {
    int p = save();

    const char *s[] = {
        "<--", "-->", "=>", "->", "=", NULL
    };
    for (const char **a = s; *a; ++a) {
        if (opsymbol(*a)) return false;
        restore(p);
    }
    if (!opstring(op)) return false;
    return true;
}

static bool preopExpr(ExprPtr &x) {
    Location location = currentLocation();
    llvm::StringRef op;
    int p = save();
    if (plusOrMinus(op)) {
        if (signedLiteral(op, x)) return true;
    }
    restore(p);
    if (!operatorOp(op)) return false;
    ExprPtr y;
    if (!prefixExpr(y)) return false;
    ExprListPtr exprs = new ExprList(new NameRef(Identifier::get(op)));
    exprs->add(y);
    x = new VariadicOp(PREFIX_OP, exprs);
    x->location = location;
    return true;
}

static bool prefixExpr(ExprPtr &x) {
    int p = save();
    if (addressOfExpr(x)) return true;
    if (restore(p), dispatchExpr(x)) return true;
    if (restore(p), preopExpr(x)) return true;
    if (restore(p), staticExpr(x)) return true;
    if (restore(p), suffixExpr(x)) return true;
    return false;
}


//
// infix binary operator expr
//

static bool operatorTail(VariadicOpPtr &x) {
    Location location = currentLocation();
    ExprListPtr exprs = new ExprList();
    ExprPtr b;
    llvm::StringRef op;
    if (!operatorOp(op)) return false;
    int p = save();
    while (true) {
        if (!prefixExpr(b)) return false;
        exprs->add(new NameRef(Identifier::get(op)));
        exprs->add(b);
        p = save();
        if (!operatorOp(op)) {
            restore(p);
            break;
        }
    }
    x = new VariadicOp(INFIX_OP, exprs);
    x->location = location;
    return true;
}

static bool operatorExpr(ExprPtr &x) {
    if (!prefixExpr(x)) return false;
    while (true) {
        int p = save();
        VariadicOpPtr y;
        if (!operatorTail(y)) {
            restore(p);
            break;
        }
        y->exprs->insert(x);
        x = y.ptr();
    }
    return true;
}



//
// not, and, or
//

static bool notExpr(ExprPtr &x) {
    Location location = currentLocation();
    int p = save();
    if (!keyword("not")) {
        restore(p);
        return operatorExpr(x);
    }
    ExprPtr y;
    if (!operatorExpr(y)) return false;
    x = new VariadicOp(NOT, new ExprList(y));
    x->location = location;
    return true;
}

static bool andExprTail(AndPtr &x) {
    Location location = currentLocation();
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
    Location location = currentLocation();
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
    Location location = currentLocation();
    ExprPtr expr;
    if (!keyword("if")) return false;
    if (!symbol("(")) return false;
    if (!expression(expr)) return false;
    ExprListPtr exprs = new ExprList(expr);
    if (!symbol(")")) return false;
    if (!expression(expr)) return false;
    exprs->add(expr);
    if (!keyword("else")) return false;
    if (!expression(expr)) return false;
    exprs->add(expr);
    x = new VariadicOp(IF_EXPR, exprs);
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

static bool arguments(vector<FormalArgPtr> &args,
                      FormalArgPtr &varg);

static bool lambdaArgs(vector<FormalArgPtr> &formalArgs,
                       FormalArgPtr &formalVarArg) {
    int p = save();
    IdentifierPtr name;
    if (identifier(name)) {
        formalArgs.clear();
        FormalArgPtr arg = new FormalArg(name, NULL, TEMPNESS_DONTCARE);
        arg->location = name->location;
        formalArgs.push_back(arg);
        formalVarArg = NULL;
        return true;
    }
    restore(p);
    if (!arguments(formalArgs, formalVarArg)) return false;
    return true;
}

static bool lambdaExprBody(StatementPtr &x) {
    Location location = currentLocation();
    ReturnKind rkind;
    ExprPtr expr;
    if (!returnExpr(rkind, expr)) return false;
    x = new Return(rkind, new ExprList(expr));
    x->location = location;
    return true;
}

static bool lambdaArrow(bool &captureByRef) {
    int p = save();
    if (opsymbol("->")) {
        captureByRef = true;
        return true;
    } else {
        restore(p);
        if (opsymbol("=>")) {
            captureByRef = false;
            return true;
        }
        return false;
    }
}

static bool lambdaBody(StatementPtr &x) {
    int p = save();
    if (lambdaExprBody(x)) return true;
    if (restore(p), block(x)) return true;
    return false;
}

static bool lambda(ExprPtr &x) {
    Location location = currentLocation();
    vector<FormalArgPtr> formalArgs;
    FormalArgPtr formalVarArg;
    bool captureByRef;
    StatementPtr body;
    if (!lambdaArgs(formalArgs, formalVarArg)) return false;
    if (!lambdaArrow(captureByRef)) return false;
    if (!lambdaBody(body)) return false;
    x = new Lambda(captureByRef, formalArgs, formalVarArg, body);
    x->location = location;
    return true;
}



//
// unpack
//

static bool unpack(ExprPtr &x) {
    Location location = currentLocation();
    if (!ellipsis()) return false;
    ExprPtr y;
    if (!expression(y)) return false;
    x = new Unpack(y);
    x->location = location;
    return true;
}



//
// pairExpr
//

static bool pairExpr(ExprPtr &x) {
    Location location = currentLocation();
    IdentifierPtr y;
    if (!identifier(y)) return false;
    if (!symbol(":")) return false;
    ExprPtr z;
    if (!expression(z)) return false;
    ExprPtr ident = new StringLiteral(y);
    ident->location = location;
    ExprListPtr args = new ExprList();
    args->add(ident);
    args->add(z);
    x = new Tuple(args);
    x->location = location;
    return true;
}


//
// expression
//

static bool expression(ExprPtr &x) {
    Location startLocation = currentLocation();
    int p = save();
    if (restore(p), lambda(x)) goto success;
    if (restore(p), pairExpr(x)) goto success;
    if (restore(p), orExpr(x)) goto success;
    if (restore(p), ifExpr(x)) goto success;
    if (restore(p), unpack(x)) goto success;
    return false;

success:
    x->startLocation = startLocation;
    x->endLocation = currentLocation();
    return true;
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
    if (restore(p), intLiteral("+", x)) return true;
    return false;
}

static bool patternSuffix(IndexingPtr &x) {
    Location location = currentLocation();
    if (!symbol("[")) return false;
    ExprListPtr args;
    if (!optExpressionList(args)) return false;
    if (!symbol("]")) return false;
    x = new Indexing(NULL, args);
    x->location = location;
    return true;
}

static bool pattern(ExprPtr &x) {
    Location start = currentLocation();
    if (!atomicPattern(x)) return false;
    int p = save();
    IndexingPtr y;
    if (!patternSuffix(y)) {
        restore(p);
        x->startLocation = start;
        x->endLocation = currentLocation();
        return true;
    }
    y->expr = x;
    x = y.ptr();
    x->startLocation = start;
    x->endLocation = currentLocation();
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
    Location location = currentLocation();
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
    else if (restore(p), keyword("forward"))
        bindingKind = FORWARD;
    else
        return false;
    return true;
}

static bool optPatternVarsWithCond(vector<PatternVar> &x, ExprPtr &y);
static bool bindingsBody(vector<FormalArgPtr> &args,
                          FormalArgPtr &varg);

static bool localBinding(StatementPtr &x) {
    Location location = currentLocation();
    int bk;
    if (!bindingKind(bk)) return false;
    vector<PatternVar> patternVars;
    ExprPtr predicate;
    if (!optPatternVarsWithCond(patternVars, predicate)) return false;
    vector<FormalArgPtr> args;
    FormalArgPtr varg;
    if (!bindingsBody(args,varg)) return false;
    if (!opsymbol("=")) return false;
    ExprListPtr z;
    if (!expressionList(z)) return false;
    if (!symbol(";")) return false;
    x = new Binding(bk, patternVars, predicate, args, varg, z);
    x->location = location;
    return true;
}

static bool blockItems(vector<StatementPtr> &stmts);
static bool withStatement(StatementPtr &x) {
    Location startLocation = currentLocation();

    if (!keyword("with")) return false;
    vector<IdentifierPtr> lhs;

    int p = save();
    if (!identifierList(lhs) || !opsymbol("=")) {
        lhs.clear();
        restore(p);
    }
    Location location = currentLocation();

    ExprPtr rhs = NULL;
    if (!suffixExpr(rhs)) return false;
    if (!symbol(";")) return false;

    WithStatementPtr w = new WithStatement(lhs, rhs, location);

    x = w.ptr();
    x->location = location;
    return true;
}

static bool blockItem(StatementPtr &x) {
    int p = save();
    if (labelDef(x)) return true;
    if (restore(p), withStatement(x)) return true;
    if (restore(p), localBinding(x)) return true;
    if (restore(p), statement(x)) return true;
    return false;
}

static bool blockItems(vector<StatementPtr> &stmts) {
    while (true) {
        int p = save();
        StatementPtr z;
        if (!blockItem(z)) {
            restore(p);
            break;
        }
        stmts.push_back(z);
    }
    return true;
}

static bool statementExprStatement(StatementPtr &stmt);

static bool statementExpression(vector<StatementPtr> &stmts, ExprPtr &expr) {
    ExprPtr tailExpr;
    StatementPtr stmt;
    while (true) {
        int p = save();
        if (statementExprStatement(stmt)) {
            stmts.push_back(stmt);
        } else if (restore(p), expression(tailExpr)) {
            int q = save();
            if (symbol(";")) {
                stmts.push_back(new ExprStatement(tailExpr));
            } else {
                restore(q);
                expr = tailExpr;
                return true;
            }
        } else {
            return false;
        }
    }
}

static bool block(StatementPtr &x) {
    Location location = currentLocation();
    if (!symbol("{")) return false;
    BlockPtr y = new Block();
    if (!blockItems(y->statements)) return false;
    if (!symbol("}")) return false;
    x = y.ptr();
    x->location = location;
    return true;
}

static bool assignment(StatementPtr &x) {
    Location location = currentLocation();
    ExprListPtr y, z;
    if (!expressionList(y)) return false;
    if (!opsymbol("=")) return false;
    if (!expressionList(z)) return false;
    if (!symbol(";")) return false;
    x = new Assignment(y, z);
    x->location = location;
    return true;
}

static bool initAssignment(StatementPtr &x) {
    Location location = currentLocation();
    ExprListPtr y, z;
    if (!expressionList(y)) return false;
    if (!opsymbol("<--")) return false;
    if (!expressionList(z)) return false;
    if (!symbol(";")) return false;
    x = new InitAssignment(y, z);
    x->location = location;
    return true;
}

static bool prefixUpdate(StatementPtr &x) {
    Location location = currentLocation();
    ExprPtr z;
    llvm::StringRef op;
    if (!uopstring(op)) return false;
    if (!expression(z)) return false;
    if (!symbol(";")) return false;
    ExprListPtr exprs = new ExprList(new NameRef(Identifier::get(op)));
    if (z->exprKind == VARIADIC_OP) {
        VariadicOp *y = (VariadicOp *)z.ptr();
        exprs->add(y->exprs);
    } else {
        exprs->add(z);
    }
    x = new VariadicAssignment(PREFIX_OP, exprs);
    x->location = location;
    return true;
}

static bool updateAssignment(StatementPtr &x) {
    Location location = currentLocation();
    ExprPtr y,z;
    if (!expression(y)) return false;
    llvm::StringRef op;
    if (!uopstring(op)) return false;
    if (!expression(z)) return false;
    if (!symbol(";")) return false;
    ExprListPtr exprs = new ExprList(new NameRef(Identifier::get(op)));
    exprs->add(y);
    if (z->exprKind == VARIADIC_OP) {
        VariadicOp *y = (VariadicOp *)z.ptr();
        exprs->add(y->exprs);
    } else {
        exprs->add(z);
    }
    x = new VariadicAssignment(INFIX_OP, exprs);
    x->location = location;
    return true;
}


static bool gotoStatement(StatementPtr &x) {
    Location location = currentLocation();
    IdentifierPtr y;
    if (!keyword("goto")) return false;
    if (!identifier(y)) return false;
    if (!symbol(";")) return false;
    x = new Goto(y);
    x->location = location;
    return true;
}

static bool returnStatement(StatementPtr &x) {
    Location location = currentLocation();
    if (!keyword("return")) return false;
    ReturnKind rkind;
    ExprListPtr exprs;
    if (!returnExprList(rkind, exprs)) return false;
    if (!symbol(";")) return false;
    x = new Return(rkind, exprs);
    x->location = location;
    return true;
}

static bool optElse(StatementPtr &x) {
    int p = save();
    if (!keyword("else")) {
        restore(p);
        return true;
    }
    return statement(x);
}

static bool ifStatement(StatementPtr &x) {
    Location location = currentLocation();
    vector<StatementPtr> condStmts;
    ExprPtr condition;
    StatementPtr thenPart, elsePart;
    if (!keyword("if")) return false;
    if (!symbol("(")) return false;
    if (!statementExpression(condStmts, condition)) return false;
    if (!symbol(")")) return false;
    if (!statement(thenPart)) return false;
    if (!optElse(elsePart)) return false;
    x = new If(condStmts, condition, thenPart, elsePart);
    x->location = location;
    return true;
}

static bool caseList(ExprListPtr &x) {
    if (!keyword("case")) return false;
    if (!symbol("(")) return false;
    if (!expressionList(x)) return false;
    if (!symbol(")")) return false;
    return true;
}

static bool caseBlock(CaseBlockPtr &x) {
    Location location = currentLocation();
    ExprListPtr caseLabels;
    StatementPtr body;
    if (!caseList(caseLabels)) return false;
    if (!statement(body)) return false;
    x = new CaseBlock(caseLabels, body);
    x->location = location;
    return true;
}

static bool caseBlockList(vector<CaseBlockPtr> &x) {
    CaseBlockPtr a;
    if (!caseBlock(a)) return false;
    x.clear();
    while (true) {
        x.push_back(a);
        int p = save();
        if (!caseBlock(a)) {
            restore(p);
            break;
        }
    }
    return true;
}

static bool switchStatement(StatementPtr &x) {
    Location location = currentLocation();
    vector<StatementPtr> exprStmts;
    ExprPtr expr;
    if (!keyword("switch")) return false;
    if (!symbol("(")) return false;
    if (!statementExpression(exprStmts, expr)) return false;
    if (!symbol(")")) return false;
    vector<CaseBlockPtr> caseBlocks;
    if (!caseBlockList(caseBlocks)) return false;
    StatementPtr defaultCase;
    if (!optElse(defaultCase)) return false;
    x = new Switch(exprStmts, expr, caseBlocks, defaultCase);
    x->location = location;
    return true;
}

static bool exprStatementNeedsSemicolon(ExprPtr expr) {
    if (expr->exprKind == CALL) {
        CallPtr call = (Call*)expr.ptr();
        unsigned lambdaCount = call->lambdaArgs->size();
        if (lambdaCount != 0) {
            LambdaPtr lastLambda = (Lambda*)call->lambdaArgs->exprs.back().ptr();
            assert(lastLambda->exprKind == LAMBDA);
            return lastLambda->body->stmtKind != BLOCK;
        }
    }
    return true;
}

static bool exprStatement(StatementPtr &x) {
    Location location = currentLocation();
    ExprPtr y;
    if (!expression(y)) return false;
    if (exprStatementNeedsSemicolon(y) && !symbol(";")) return false;
    x = new ExprStatement(y);
    x->location = location;
    return true;
}

static bool whileStatement(StatementPtr &x) {
    Location location = currentLocation();
    vector<StatementPtr> condStmts;
    ExprPtr cond;
    StatementPtr body;
    if (!keyword("while")) return false;
    if (!symbol("(")) return false;
    if (!statementExpression(condStmts, cond)) return false;
    if (!symbol(")")) return false;
    if (!statement(body)) return false;
    x = new While(condStmts, cond, body);
    x->location = location;
    return true;
}

static bool breakStatement(StatementPtr &x) {
    Location location = currentLocation();
    if (!keyword("break")) return false;
    if (!symbol(";")) return false;
    x = new Break();
    x->location = location;
    return true;
}

static bool continueStatement(StatementPtr &x) {
    Location location = currentLocation();
    if (!keyword("continue")) return false;
    if (!symbol(";")) return false;
    x = new Continue();
    x->location = location;
    return true;
}

static bool forStatement(StatementPtr &x) {
    Location location = currentLocation();
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
    Location location = currentLocation();
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
    while (true) {
        x.push_back(a);
        int p = save();
        if (!catchBlock(a)) {
            restore(p);
            break;
        }
    }
    return true;
}

static bool tryStatement(StatementPtr &x) {
    Location location = currentLocation();
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
    Location location = currentLocation();
    ExprPtr a;
    if (!keyword("throw")) return false;
    if (!optExpression(a)) return false;
    if (!symbol(";")) return false;
    x = new Throw(a);
    x->location = location;
    return true;
}

static bool staticFor(StatementPtr &x) {
    Location location = currentLocation();
    if (!ellipsis()) return false;
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

static bool finallyStatement(StatementPtr &x) {
    Location location = currentLocation();

    if (!keyword("finally")) return false;
    StatementPtr body;
    if (!statement(body)) return false;
    x = new Finally(body);
    x->location = location;
    return true;
}

static bool onerrorStatement(StatementPtr &x) {
    Location location = currentLocation();

    if (!keyword("onerror")) return false;
    StatementPtr body;
    if (!statement(body)) return false;
    x = new OnError(body);
    x->location = location;
    return true;
}

static bool evalStatement(StatementPtr &x) {
    Location location = currentLocation();

    if (!keyword("eval")) return false;
    ExprListPtr args;
    if (!expressionList(args)) return false;
    if (!symbol(";")) return false;
    x = new EvalStatement(args);
    x->location = location;
    return true;
}

static bool statement(StatementPtr &x) {
    int p = save();
    if (block(x)) return true;
    if (restore(p), assignment(x)) return true;
    if (restore(p), initAssignment(x)) return true;
    if (restore(p), updateAssignment(x)) return true;
    if (restore(p), prefixUpdate(x)) return true;
    if (restore(p), ifStatement(x)) return true;
    if (restore(p), gotoStatement(x)) return true;
    if (restore(p), switchStatement(x)) return true;
    if (restore(p), returnStatement(x)) return true;
    if (restore(p), evalStatement(x)) return true;
    if (restore(p), exprStatement(x)) return true;
    if (restore(p), whileStatement(x)) return true;
    if (restore(p), breakStatement(x)) return true;
    if (restore(p), continueStatement(x)) return true;
    if (restore(p), forStatement(x)) return true;
    if (restore(p), tryStatement(x)) return true;
    if (restore(p), throwStatement(x)) return true;
    if (restore(p), staticFor(x)) return true;
    if (restore(p), finallyStatement(x)) return true;
    if (restore(p), onerrorStatement(x)) return true;

    return false;
}

static bool statementExprStatement(StatementPtr &stmt) {
    int p = save();
    if (localBinding(stmt)) return true;
    if (restore(p), assignment(stmt)) return true;
    if (restore(p), initAssignment(stmt)) return true;
    if (restore(p), updateAssignment(stmt)) return true;
    return false;
}



//
// staticParams
//

static bool staticVarParam(IdentifierPtr &varParam) {
    if (!ellipsis()) return false;
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
    if (keyword("ref")) {
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
    Location location = currentLocation();
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

static FormalArgPtr makeStaticFormalArg(unsigned index, ExprPtr expr, Location const & location) {
    // desugar static args
    llvm::SmallString<128> buf;
    llvm::raw_svector_ostream sout(buf);
    sout << "%arg" << index;
    IdentifierPtr argName = Identifier::get(sout.str());

    ExprPtr staticName =
        new ForeignExpr("prelude",
                        new NameRef(Identifier::get("Static")));

    IndexingPtr indexing = new Indexing(staticName, new ExprList(expr));
    indexing->startLocation = location;
    indexing->endLocation = currentLocation();

    FormalArgPtr arg = new FormalArg(argName, indexing.ptr());
    arg->location = location;
    return arg;
}

static bool staticFormalArg(unsigned index, FormalArgPtr &x) {
    Location location = currentLocation();
    ExprPtr expr;
    if (!symbol("#")) return false;
    if (!expression(expr)) return false;

    if (expr->exprKind == UNPACK) {
        error(expr, "#static variadic arguments are not yet supported");
    }

    x = makeStaticFormalArg(index, expr, location);
    return true;
}

static bool stringFormalArg(unsigned index, FormalArgPtr &x) {
    Location location = currentLocation();
    ExprPtr expr;
    if (!stringLiteral(expr)) return false;
    x = makeStaticFormalArg(index, expr, location);
    return true;
}

static bool formalArg(unsigned index, FormalArgPtr &x) {
    int p = save();
    if (valueFormalArg(x)) return true;
    if (restore(p), staticFormalArg(index, x)) return true;
    if (restore(p), stringFormalArg(index, x)) return true;
    return false;
}

static bool formalArgs(vector<FormalArgPtr> &x) {
    FormalArgPtr y;
    if (!formalArg(x.size(), y)) return false;
    x.clear();
    while (true) {
        x.push_back(y);
        int p = save();
        if (!symbol(",") || !formalArg(x.size(), y)) {
            restore(p);
            break;
        }
    }
    return true;
}

static bool varArgTypeSpec(ExprPtr &vargType) {
    if (!symbol(":")) return false;
    Location start = currentLocation();
    if (!nameRef(vargType)) return false;
    vargType->startLocation = start;
    vargType->endLocation = currentLocation();
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
    Location location = currentLocation();
    ValueTempness tempness;
    if (!optArgTempness(tempness)) return false;
    if (!ellipsis()) return false;
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



static bool valueBindingArg(FormalArgPtr &x) {
    Location location = currentLocation();
    IdentifierPtr y;
    ExprPtr z;
    if (!identifier(y)) return false;
    if (!optTypeSpec(z)) return false;
    x = new FormalArg(y, z);
    x->location = location;
    return true;
}

static bool bindingArg(FormalArgPtr &x) {
    int p = save();
    if (valueBindingArg(x)) return true;
    return false;
}

static bool bindingArgs(vector<FormalArgPtr> &x) {
    FormalArgPtr y;
    if (!bindingArg(y)) return false;
    x.clear();
    while (true) {
        x.push_back(y);
        int p = save();
        if (!symbol(",") || !bindingArg(y)) {
            restore(p);
            break;
        }
    }
    return true;
}


static bool bindingVarArg(FormalArgPtr &x) {
    Location location = currentLocation();
    if (!ellipsis()) return false;
    IdentifierPtr name;
    if (!identifier(name)) return false;
    ExprPtr type;
    if (!optVarArgTypeSpec(type)) return false;
    x = new FormalArg(name, type);
    x->location = location;
    return true;
}

static bool optBindingVarArg(FormalArgPtr &x) {
    int p = save();
    if (!bindingVarArg(x)) {
        restore(p);
        x = NULL;
    }
    return true;
}

static bool trailingBindingVarArg(FormalArgPtr &x) {
    if (!symbol(",")) return false;
    if (!bindingVarArg(x)) return false;
    return true;
}

static bool optTrailingBindingVarArg(FormalArgPtr &x) {
    int p = save();
    if (!trailingBindingVarArg(x)) {
        restore(p);
        x = NULL;
    }
    return true;
}

static bool bindingArgsWithVArgs(vector<FormalArgPtr> &args,
                                FormalArgPtr &varg) {
    if (!bindingArgs(args)) return false;
    if (!optTrailingBindingVarArg(varg)) return false;
    return true;
}

static bool bindingsBody(vector<FormalArgPtr> &args,
                          FormalArgPtr &varg) {
    int p = save();
    if (bindingArgsWithVArgs(args, varg)) return true;
    restore(p);
    args.clear();
    varg = NULL;
    return optBindingVarArg(varg);
}

static bool predicate(ExprPtr &x) {
    if (!keyword("when")) return false;
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
    if (!ellipsis()) {
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
    while (true) {
        x.push_back(y);
        int p = save();
        if (!symbol(",") || !patternVar(y)) {
            restore(p);
            break;
        }
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

static bool patternVars(vector<PatternVar> &x) {
    if (!symbol("[")) return false;
    if (!optPatternVarList(x)) return false;
    if (!symbol("]")) {
        x.clear();
        return false;
    }
    return true;
}

static bool optPatternVars(vector<PatternVar> &x) {
    int p = save();
    if (!patternVars(x)) {
        restore(p);
        x.clear();
    }
    return true;
}

static bool exprBody(StatementPtr &x) {
    if (!opsymbol("=")) return false;
    Location location = currentLocation();
    ReturnKind rkind;
    ExprListPtr exprs;
    if (!returnExprList(rkind, exprs)) return false;
    if (!symbol(";")) return false;
    x = new Return(rkind, exprs, true);
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
    Location location = currentLocation();
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
    while (true) {
        x.push_back(y);
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
    Location location = currentLocation();
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
    Location location = currentLocation();
    if (!opsymbol("=")) return false;
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
    Location location = currentLocation();
    vector<PatternVar> patternVars;
    ExprPtr predicate;
    Visibility vis;
    if (!optPatternVarsWithCond(patternVars, predicate)) return false;
    if (!topLevelVisibility(vis)) return false;
    if (!keyword("record")) return false;
    RecordPtr y = new Record(vis, patternVars, predicate);
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

static bool instances(ExprListPtr &x) {
    return symbol("(") && optExpressionList(x) && symbol(")");
}

static bool optInstances(ExprListPtr &x) {
    int p = save();
    if (symbol("(")) {
        return optExpressionList(x) && symbol(")");
    } else {
        restore(p);
        x = new ExprList();
        return true;
    }
}

static bool variant(TopLevelItemPtr &x) {
    Location location = currentLocation();
    vector<PatternVar> patternVars;
    ExprPtr predicate;
    Visibility vis;
    if (!optPatternVarsWithCond(patternVars, predicate)) return false;
    if (!topLevelVisibility(vis)) return false;
    if (!keyword("variant")) return false;
    IdentifierPtr name;
    if (!identifier(name)) return false;
    vector<IdentifierPtr> params;
    IdentifierPtr varParam;
    if (!optStaticParams(params, varParam)) return false;
    ExprListPtr defaultInstances;
    if (!optInstances(defaultInstances)) return false;
    if (!symbol(";")) return false;
    x = new Variant(name, vis, patternVars, predicate, params, varParam, defaultInstances);
    x->location = location;
    return true;
}

static bool instance(TopLevelItemPtr &x) {
    Location location = currentLocation();
    vector<PatternVar> patternVars;
    ExprPtr predicate;
    if (!optPatternVarsWithCond(patternVars, predicate)) return false;
    if (!keyword("instance")) return false;
    ExprPtr target;
    if (!pattern(target)) return false;
    ExprListPtr members;
    if (!instances(members)) return false;
    if (!symbol(";")) return false;
    x = new Instance(patternVars, predicate, target, members);
    x->location = location;
    return true;
}



//
// returnSpec
//

static bool namedReturnName(IdentifierPtr &x) {
    IdentifierPtr y;
    if (!identifier(y)) return false;
    if (!symbol(":")) return false;
    x = y;
    return true;
}

static bool returnTypeExpression(ExprPtr &x) {
    return orExpr(x);
}

static bool returnType(ReturnSpecPtr &x) {
    Location location = currentLocation();
    ExprPtr z;
    if (!returnTypeExpression(z)) return false;
    x = new ReturnSpec(z, NULL);
    x->location = location;
    return true;
}

static bool returnTypeList(vector<ReturnSpecPtr> &x) {
    ReturnSpecPtr y;
    if (!returnType(y)) return false;
    x.clear();
    while (true) {
        x.push_back(y);
        int p = save();
        if (!symbol(",") || !returnType(y)) {
            restore(p);
            break;
        }
    }
    return true;
}

static bool optReturnTypeList(vector<ReturnSpecPtr> &x) {
    int p = save();
    if (!returnTypeList(x)) {
        restore(p);
        x.clear();
    }
    return true;
}

static bool namedReturn(ReturnSpecPtr &x) {
    Location location = currentLocation();
    IdentifierPtr y;
    if (!namedReturnName(y)) return false;
    ExprPtr z;
    if (!returnTypeExpression(z)) return false;
    x = new ReturnSpec(z, y);
    x->location = location;
    return true;
}

static bool namedReturnList(vector<ReturnSpecPtr> &x) {
    ReturnSpecPtr y;
    if (!namedReturn(y)) return false;
    x.clear();
    while (true) {
        x.push_back(y);
        int p = save();
        if (!symbol(",") || !namedReturn(y)) {
            restore(p);
            break;
        }
    }
    return true;
}

static bool optNamedReturnList(vector<ReturnSpecPtr> &x) {
    int p = save();
    if (!namedReturnList(x)) {
        restore(p);
        x.clear();
    }
    return true;
}

static bool varReturnType(ReturnSpecPtr &x) {
    Location location = currentLocation();
    if (!ellipsis()) return false;
    ExprPtr z;
    if (!returnTypeExpression(z)) return false;
    x = new ReturnSpec(z, NULL);
    x->location = location;
    return true;
}

static bool optVarReturnType(ReturnSpecPtr &x) {
    int p = save();
    if (!varReturnType(x)) {
        restore(p);
        x = NULL;
    }
    return true;
}

static bool varNamedReturn(ReturnSpecPtr &x) {
    Location location = currentLocation();
    if (!ellipsis()) return false;
    IdentifierPtr y;
    if (!namedReturnName(y)) return false;
    ExprPtr z;
    if (!returnTypeExpression(z)) return false;
    x = new ReturnSpec(z, y);
    x->location = location;
    return true;
}

static bool optVarNamedReturn(ReturnSpecPtr &x) {
    int p = save();
    if (!varNamedReturn(x)) {
        restore(p);
        x = NULL;
    }
    return true;
}

static bool allReturnSpecsWithFlag(vector<ReturnSpecPtr> &returnSpecs,
                           ReturnSpecPtr &varReturnSpec, bool &exprRetSpecs) {
    returnSpecs.clear();
    varReturnSpec = NULL;
    int p = save();
    if (symbol(":")) {
        if (!optReturnTypeList(returnSpecs)) return false;
        if (!optVarReturnType(varReturnSpec)) return false;
        if (returnSpecs.size()>0 || varReturnSpec!=NULL)
            exprRetSpecs = true;
        return true;
    } else {
        restore(p);
        if (opsymbol("-->")) {
            if (!optNamedReturnList(returnSpecs)) return false;
            if (!optVarNamedReturn(varReturnSpec)) return false;
            return true;
        } else {
            restore(p);
            return false;
        }
    }
}

static bool allReturnSpecs(vector<ReturnSpecPtr> &returnSpecs,
                           ReturnSpecPtr &varReturnSpec) {
    bool exprRetSpecs = false;
    return allReturnSpecsWithFlag(returnSpecs, varReturnSpec, exprRetSpecs);
}


//
// define, overload
//

static bool optInline(InlineAttribute &isInline) {
    int p = save();
    if (keyword("inline"))
        isInline = INLINE;
    else if (restore(p), keyword("forceinline"))
        isInline = FORCE_INLINE;
    else if (restore(p), keyword("noinline"))
        isInline = NEVER_INLINE;
    else {
        restore(p);
        isInline = IGNORE;
    }
    return true;
}

static bool optCallByName(bool &callByName) {
    int p = save();
    if (!keyword("alias")) {
        restore(p);
        callByName = false;
        return true;
    }
    callByName = true;
    return true;
}

static bool llvmCode(LLVMCodePtr &b) {
    Token* t;
    if (!next(t) || (t->tokenKind != T_LLVM))
        return false;
    b = new LLVMCode(t->str);
    b->location = t->location;
    return true;
}

static bool llvmProcedure(vector<TopLevelItemPtr> &x) {
    Location location = currentLocation();
    CodePtr y = new Code();
    if (!optPatternVarsWithCond(y->patternVars, y->predicate)) return false;
    Visibility vis;
    if (!topLevelVisibility(vis)) return false;
    InlineAttribute isInline;
    if (!optInline(isInline)) return false;
    IdentifierPtr z;
    Location targetStartLocation = currentLocation();
    if (!identifier(z)) return false;
    Location targetEndLocation = currentLocation();
    if (!arguments(y->formalArgs, y->formalVarArg)) return false;
    y->returnSpecsDeclared = allReturnSpecs(y->returnSpecs, y->varReturnSpec);
    if (!llvmCode(y->llvmBody)) return false;
    y->location = location;

    ProcedurePtr u = new Procedure(z, vis);
    u->location = location;
    x.push_back(u.ptr());

    ExprPtr target = new NameRef(z);
    target->location = location;
    target->startLocation = targetStartLocation;
    target->endLocation = targetEndLocation;
    OverloadPtr v = new Overload(target, y, false, isInline);
    v->location = location;
    x.push_back(v.ptr());

    return true;
}

static bool procedureWithInterface(vector<TopLevelItemPtr> &x) {
    Location location = currentLocation();
    CodePtr interfaceCode = new Code();
    if (!optPatternVarsWithCond(interfaceCode->patternVars, interfaceCode->predicate)) return false;
    Visibility vis;
    if (!topLevelVisibility(vis)) return false;
    if (!keyword("define")) return false;
    IdentifierPtr name;
    Location targetStartLocation = currentLocation();
    if (!identifier(name)) return false;
    Location targetEndLocation = currentLocation();
    if (!arguments(interfaceCode->formalArgs, interfaceCode->formalVarArg)) return false;
    interfaceCode->returnSpecsDeclared = allReturnSpecs(interfaceCode->returnSpecs, interfaceCode->varReturnSpec);
    if (!symbol(";")) return false;
    interfaceCode->location = location;

    ExprPtr target = new NameRef(name);
    target->location = location;
    target->startLocation = targetStartLocation;
    target->endLocation = targetEndLocation;
    OverloadPtr interface = new Overload(target, interfaceCode, false, IGNORE);
    interface->location = location;

    ProcedurePtr proc = new Procedure(name, vis, interface);
    proc->location = location;
    x.push_back(proc.ptr());

    return true;
}

static bool procedureWithBody(vector<TopLevelItemPtr> &x) {
    Location location = currentLocation();
    CodePtr code = new Code();
    if (!optPatternVarsWithCond(code->patternVars, code->predicate)) return false;
    Visibility vis;
    if (!topLevelVisibility(vis)) return false;
    InlineAttribute isInline;
    if (!optInline(isInline)) return false;
    bool callByName;
    if (!optCallByName(callByName)) return false;
    IdentifierPtr name;
    Location targetStartLocation = currentLocation();
    if (!identifier(name)) return false;
    Location targetEndLocation = currentLocation();
    if (!arguments(code->formalArgs, code->formalVarArg)) return false;
    bool exprRetSpecs = false;
    code->returnSpecsDeclared = allReturnSpecsWithFlag(code->returnSpecs, code->varReturnSpec, exprRetSpecs);
    if (!body(code->body)) return false;
    code->location = location;
    if(exprRetSpecs && code->body->stmtKind == RETURN){
        Return *x = (Return *)code->body.ptr();    
        if(x->isExprReturn)
            x->isReturnSpecs = true;
    }

    ProcedurePtr proc = new Procedure(name, vis);
    proc->location = location;
    x.push_back(proc.ptr());

    ExprPtr target = new NameRef(name);
    target->location = location;
    target->startLocation = targetStartLocation;
    target->endLocation = targetEndLocation;
    OverloadPtr oload = new Overload(target, code, callByName, isInline);
    oload->location = location;
    x.push_back(oload.ptr());

    return true;
}

static bool procedure(TopLevelItemPtr &x) {
    Location location = currentLocation();
    Visibility vis;
    if (!topLevelVisibility(vis)) return false;
    if (!keyword("define")) return false;
    IdentifierPtr name;
    if (!identifier(name)) return false;
    if (!symbol(";")) return false;
    x = new Procedure(name, vis);
    x->location = location;
    return true;
}

static bool overload(TopLevelItemPtr &x) {
    Location location = currentLocation();
    CodePtr code = new Code();
    if (!optPatternVarsWithCond(code->patternVars, code->predicate)) return false;
    InlineAttribute isInline;
    if (!optInline(isInline)) return false;
    bool callByName;
    if (!optCallByName(callByName)) return false;
    if (!keyword("overload")) return false;
    ExprPtr target;
    Location targetStartLocation = currentLocation();
    if (!pattern(target)) return false;
    Location targetEndLocation = currentLocation();
    if (!arguments(code->formalArgs, code->formalVarArg)) return false;
    bool exprRetSpecs = false;
    code->returnSpecsDeclared = allReturnSpecsWithFlag(code->returnSpecs, code->varReturnSpec, exprRetSpecs);
    int p = save();
    if (!optBody(code->body)) {
        restore(p);
        if (callByName) return false;
        if (!llvmCode(code->llvmBody)) return false;
    }
    if(exprRetSpecs && code->body->stmtKind == RETURN){
        Return *x = (Return *)code->body.ptr();    
        if(x->isExprReturn)
            x->isReturnSpecs = true;
    }
    target->location = location;
    target->startLocation = targetStartLocation;
    target->endLocation = targetEndLocation;
    code->location = location;
    x = new Overload(target, code, callByName, isInline);
    x->location = location;
    return true;
}



//
// enumerations
//

static bool enumMember(EnumMemberPtr &x) {
    Location location = currentLocation();
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
    while (true) {
        x.push_back(y);
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
    }
    return true;
}

static bool enumeration(TopLevelItemPtr &x) {
    Location location = currentLocation();
    vector<PatternVar> patternVars;
    ExprPtr predicate;
    Visibility vis;
    if (!optPatternVarsWithCond(patternVars, predicate)) return false;
    if (!topLevelVisibility(vis)) return false;
    if (!keyword("enum")) return false;
    IdentifierPtr y;
    if (!identifier(y)) return false;
    EnumerationPtr z = new Enumeration(y, vis, patternVars, predicate);
    if (!symbol("(")) return false;
    if (!enumMemberList(z->members)) return false;
    if (!symbol(")")) return false;
    if (!symbol(";")) return false;
    x = z.ptr();
    x->location = location;
    return true;
}



//
// global variable
//

static bool globalVariable(TopLevelItemPtr &x) {
    Location location = currentLocation();
    vector<PatternVar> patternVars;
    ExprPtr predicate;
    Visibility vis;
    if (!optPatternVarsWithCond(patternVars, predicate)) return false;
    if (!topLevelVisibility(vis)) return false;
    if (!keyword("var")) return false;
    IdentifierPtr name;
    if (!identifier(name)) return false;
    vector<IdentifierPtr> params;
    IdentifierPtr varParam;
    if (!optStaticParams(params, varParam)) return false;
    if (!opsymbol("=")) return false;
    ExprPtr expr;
    if (!expression(expr)) return false;
    if (!symbol(";")) return false;
    x = new GlobalVariable(name, vis, patternVars, predicate, params, varParam, expr);
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
    Location location = currentLocation();
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
    while (true) {
        x.push_back(y);
        int p = save();
        if (!symbol(",") || !externalArg(y)) {
            restore(p);
            break;
        }
    }
    return true;
}

static bool externalVarArgs(bool &hasVarArgs) {
    if (!ellipsis()) return false;
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

static bool optExternalReturn(ExprPtr &x) {
    int p = save();
    if (!symbol(":")) {
        restore(p);
        return true;
    }
    return optExpression(x);
}

static bool external(TopLevelItemPtr &x) {
    Location location = currentLocation();
    Visibility vis;
    if (!topLevelVisibility(vis)) return false;
    if (!keyword("external")) return false;
    ExternalProcedurePtr y = new ExternalProcedure(vis);
    if (!optExternalAttributes(y->attributes)) return false;
    if (!identifier(y->name)) return false;
    if (!symbol("(")) return false;
    if (!externalArgsBody(y->args, y->hasVarArgs)) return false;
    if (!symbol(")")) return false;
    if (!optExternalReturn(y->returnType)) return false;
    if (!externalBody(y->body)) return false;
    x = y.ptr();
    x->location = location;
    return true;
}

static bool externalVariable(TopLevelItemPtr &x) {
    Location location = currentLocation();
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
    Location location = currentLocation();
    vector<PatternVar> patternVars;
    ExprPtr predicate;
    Visibility vis;
    if (!optPatternVarsWithCond(patternVars, predicate)) return false;
    if (!topLevelVisibility(vis)) return false;
    if (!keyword("alias")) return false;
    IdentifierPtr name;
    if (!identifier(name)) return false;
    vector<IdentifierPtr> params;
    IdentifierPtr varParam;
    if (!optStaticParams(params, varParam)) return false;
    if (!opsymbol("=")) return false;
    ExprPtr expr;
    if (!expression(expr)) return false;
    if (!symbol(";")) return false;
    x = new GlobalAlias(name, vis, patternVars, predicate, params, varParam, expr);
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
    Location location = currentLocation();
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
    Location location = currentLocation();
    Visibility vis;
    if (!importVisibility(vis)) return false;
    if (!keyword("import")) return false;
    DottedNamePtr y;
    if (!dottedName(y)) return false;
    if (!symbol(".")) return false;
    if (!opsymbol("*")) return false;
    if (!symbol(";")) return false;
    x = new ImportStar(y, vis);
    x->location = location;
    return true;
}

static bool importedMember(ImportedMember &x) {
    if (!topLevelVisibility(x.visibility)) return false;
    if (!identifier(x.name)) return false;
    if (!optImportAlias(x.alias)) return false;
    return true;
}

static bool importedMemberList(vector<ImportedMember> &x) {
    ImportedMember y;
    if (!importedMember(y)) return false;
    x.clear();
    while (true) {
        x.push_back(y);
        int p = save();
        if (!symbol(",")) {
            restore(p);
            break;
        }
        p = save();
        if (!importedMember(y)) {
            restore(p);
            break;
        }
    }
    return true;
}

static bool importMembers(ImportPtr &x) {
    Location location = currentLocation();
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

static bool evalTopLevel(TopLevelItemPtr &x) {
    Location location = currentLocation();
    if (!keyword("eval")) return false;
    ExprListPtr args;
    if (!expressionList(args)) return false;
    if (!symbol(";")) return false;
    x = new EvalTopLevel(args);
    x->location = location;
    return true;
}

//
// documentation
//


static bool documentationAnnotation(std::map<DocumentationAnnotation, string> &an)
{
    Location location = currentLocation();
    Token* t;
    if (!next(t) || t->tokenKind != T_DOC_PROPERTY) return false;
    llvm::StringRef key = llvm::StringRef(t->str);

    DocumentationAnnotation ano;
    if (key == "section") {
        ano = SectionAnnotation;
    } else if (key == "module") {
        ano = ModuleAnnotation;
    } else if (key == "overload") {
        ano = OverloadAnnotation;
    } else if (key == "record") {
        ano = RecordAnnotion;
    } else {
        pushLocation(location);
        fmtError("invalid annotation '%s'\n", key.str().c_str());
    }

    if (!next(t) || t->tokenKind != T_DOC_TEXT) return false;
    llvm::StringRef value = llvm::StringRef(t->str);

    an.insert(std::pair<DocumentationAnnotation,string>(ano, value.str()));
    return true;
}

static bool documentationText(std::string &text)
{
    Token* t;
    if (!next(t) || t->tokenKind != T_DOC_TEXT) return false;
    if (parserOptionKeepDocumentation)
        text.append(t->str.str());
        text.append("\n");
    return true;
}


static bool documentation(TopLevelItemPtr &x)
{
    Location location = currentLocation();
    std::map<DocumentationAnnotation, string> annotation;
    std::string text;

    bool success = false;
    bool hasAttachmentAnnotation = false;

    for (;;) {
        int p = save();
        if (documentationAnnotation(annotation)) {
            if (hasAttachmentAnnotation) {
                restore (p);
                break;
            }
            hasAttachmentAnnotation = true;
            success = true;
            continue;
        }
        restore(p);
        if (documentationText(text)) {
            success = true;
            continue;
        }
        restore(p);
        break;
    }

    if (success) {
        x = new Documentation(annotation, text);
        return true;
    }
    return false;


}


//
// module
//

static bool topLevelItem(vector<TopLevelItemPtr> &x) {
    int p = save();

    TopLevelItemPtr y;
    if (documentation(y)) goto success;
    if (restore(p), record(y)) goto success;
    if (restore(p), variant(y)) goto success;
    if (restore(p), instance(y)) goto success;
    if (restore(p), procedure(y)) goto success;
    if (restore(p), procedureWithInterface(x)) goto success2;
    if (restore(p), procedureWithBody(x)) goto success2;
    if (restore(p), llvmProcedure(x)) goto success2;
    if (restore(p), overload(y)) goto success;
    if (restore(p), enumeration(y)) goto success;
    if (restore(p), globalVariable(y)) goto success;
    if (restore(p), external(y)) goto success;
    if (restore(p), externalVariable(y)) goto success;
    if (restore(p), globalAlias(y)) goto success;
    if (restore(p), evalTopLevel(y)) goto success;
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

static bool optModuleDeclarationAttributes(ExprListPtr &x) {
    int p = save();
    if (!symbol("(")) {
        restore(p);
        x = NULL;
        return true;
    }
    if (!optExpressionList(x))
        return false;
    if (!symbol(")"))
        return false;
    return true;
}

static bool optModuleDeclaration(ModuleDeclarationPtr &x) {
    Location location = currentLocation();

    int p = save();
    if (!keyword("in")) {
        restore(p);
        x = NULL;
        return true;
    }
    DottedNamePtr name;
    ExprListPtr attributes;
    if (!dottedName(name))
        return false;
    if (!optModuleDeclarationAttributes(attributes))
        return false;
    if (!symbol(";"))
        return false;

    x = new ModuleDeclaration(name, attributes);
    x->location = location;
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

static bool module(llvm::StringRef moduleName, ModulePtr &x) {
    Location location = currentLocation();
    ModulePtr y = new Module(moduleName);
    if (!imports(y->imports)) return false;
    if (!optModuleDeclaration(y->declaration)) return false;
    if (!optTopLevelLLVM(y->topLevelLLVM)) return false;
    if (!topLevelItems(y->topLevelItems)) return false;
    x = y.ptr();
    x->location = location;
    return true;
}



//
// parse
//

template<typename Parser, typename Node>
void applyParser(SourcePtr source, int offset, int length, Parser parser, Node &node)
{
    vector<Token> t;
    tokenize(source, offset, length, t);

    tokens = &t;
    position = maxPosition = 0;

    if (!parser(node) || (position < (int)t.size())) {
        Location location;
        if (maxPosition == (int)t.size())
            location = Location(source.ptr(), source->size());
        else
            location = t[maxPosition].location;
        pushLocation(location);
        error("parse error");
    }

    tokens = NULL;
    position = maxPosition = 0;
}

struct ModuleParser {
    llvm::StringRef moduleName;
    bool operator()(ModulePtr &m) { return module(moduleName, m); }
};

ModulePtr parse(llvm::StringRef moduleName, SourcePtr source, ParserFlags flags) {
    if (flags && ParserKeepDocumentation)
        parserOptionKeepDocumentation = true;
    ModulePtr m;
    ModuleParser p = { moduleName };
    applyParser(source, 0, source->size(), p, m);
    m->source = source;
    return m;
}


//
// parseExpr
//

ExprPtr parseExpr(SourcePtr source, int offset, int length) {
    ExprPtr expr;
    applyParser(source, offset, length, expression, expr);
    return expr;
}


//
// parseExprList
//

ExprListPtr parseExprList(SourcePtr source, int offset, int length) {
    ExprListPtr exprList;
    applyParser(source, offset, length, expressionList, exprList);
    return exprList;
}


//
// parseStatements
//

void parseStatements(SourcePtr source, int offset, int length,
        vector<StatementPtr> &stmts)
{
    applyParser(source, offset, length, blockItems, stmts);
}


//
// parseTopLevelItems
//

void parseTopLevelItems(SourcePtr source, int offset, int length,
        vector<TopLevelItemPtr> &topLevels)
{
    applyParser(source, offset, length, topLevelItems, topLevels);
}

}
