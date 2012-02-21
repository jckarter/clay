#include "clay.hpp"

namespace clay {

struct LambdaContext {
    bool captureByRef;
    EnvPtr nonLocalEnv;
    const string &closureDataName;
    vector<string> &freeVars;
    LambdaContext(bool captureByRef,
                  EnvPtr nonLocalEnv,
                  const string &closureDataName,
                  vector<string> &freeVars)
        : captureByRef(captureByRef), nonLocalEnv(nonLocalEnv),
          closureDataName(closureDataName), freeVars(freeVars) {}
};

void convertFreeVars(LambdaPtr x, EnvPtr env,
                     const string &closureDataName,
                     vector<string> &freeVars);

void convertFreeVars(StatementPtr x, EnvPtr env, LambdaContext &ctx);

void convertFreeVars(ExprPtr &x, EnvPtr env, LambdaContext &ctx);
void convertFreeVars(ExprListPtr x, EnvPtr env, LambdaContext &ctx);



//
// initializeLambda
//

static TypePtr typeOfValue(ObjectPtr obj) {
    switch (obj->objKind) {
    case PVALUE : return ((PValue *)obj.ptr())->type;
    case CVALUE : return ((CValue *)obj.ptr())->type;
    default : assert(false); return NULL;
    }
}

static vector<TypePtr> typesOfValues(ObjectPtr obj) {
    vector<TypePtr> types;
    switch (obj->objKind) {
    case MULTI_PVALUE : {
        MultiPValue *mpv = (MultiPValue *)obj.ptr();
        for (unsigned i = 0; i < mpv->size(); ++i)
            types.push_back(mpv->values[i]->type);
        break;
    }
    case MULTI_CVALUE : {
        MultiCValue *mcv = (MultiCValue *)obj.ptr();
        for (unsigned i = 0; i < mcv->size(); ++i)
            types.push_back(mcv->values[i]->type);
        break;
    }
    default :
        assert(false);
    }
    return types;
}

static void initializeLambdaWithFreeVars(LambdaPtr x, EnvPtr env,
    string const &closureDataName, string const &lname);
static void initializeLambdaWithoutFreeVars(LambdaPtr x, EnvPtr env,
    string const &closureDataName, string const &lname);

static string lambdaName(LambdaPtr x)
{
    string fullName = shortString(x->asString());

    if (fullName.size() <= 80)
        return "(" + fullName + ")";
    else {
        ostringstream shortName;
        shortName << "<lambda ";
        printFileLineCol(shortName, x->location);
        shortName << ">";

        return shortName.str();
    }
}

void initializeLambda(LambdaPtr x, EnvPtr env)
{
    assert(!x->initialized);
    x->initialized = true;

    string lname = lambdaName(x);

    ostringstream ostr;
    ostr << "%closureData:" << lname;
    string closureDataName = ostr.str();

    convertFreeVars(x, env, closureDataName, x->freeVars);
    getProcedureMonoTypes(x->mono, env, x->formalArgs, x->formalVarArg);
    if (x->freeVars.empty()) {
        initializeLambdaWithoutFreeVars(x, env, closureDataName, lname);
    }
    else {
        initializeLambdaWithFreeVars(x, env, closureDataName, lname);
    }
}

static void initializeLambdaWithFreeVars(LambdaPtr x, EnvPtr env,
    string const &closureDataName, string const &lname)
{
    RecordPtr r = new Record(PRIVATE);
    r->location = x->location;
    r->name = new Identifier(lname);
    r->env = env;
    x->lambdaRecord = r;
    r->lambda = x;
    vector<RecordFieldPtr> fields;

    CallPtr converted = new Call(NULL, new ExprList());
    for (unsigned i = 0; i < x->freeVars.size(); ++i) {
        IdentifierPtr ident = new Identifier(x->freeVars[i]);
        NameRefPtr nameRef = new NameRef(ident);

        TypePtr type;
        ObjectPtr obj = safeLookupEnv(env, ident);
        switch (obj->objKind) {
        case PVALUE :
        case CVALUE : {
            type = typeOfValue(obj);
            if (x->captureByRef) {
                type = pointerType(type);
                ExprPtr addr = new UnaryOp(ADDRESS_OF, nameRef.ptr());
                converted->parenArgs->add(addr);
            }
            else {
                converted->parenArgs->add(nameRef.ptr());
            }
            break;
        }
        case MULTI_PVALUE :
        case MULTI_CVALUE : {
            vector<TypePtr> types = typesOfValues(obj);
            vector<TypePtr> elementTypes;
            for (unsigned j = 0; j < types.size(); ++j) {
                TypePtr t = types[j];
                if (x->captureByRef)
                    t = pointerType(t);
                elementTypes.push_back(t);
            }
            type = tupleType(elementTypes);
            if (x->captureByRef) {
                ExprPtr e = operator_expr_packMultiValuedFreeVarByRef();
                CallPtr call = new Call(e, new ExprList());
                call->parenArgs->add(new Unpack(nameRef.ptr()));
                converted->parenArgs->add(call.ptr());
            }
            else {
                ExprPtr e = operator_expr_packMultiValuedFreeVar();
                CallPtr call = new Call(e, new ExprList());
                call->parenArgs->add(new Unpack(nameRef.ptr()));
                converted->parenArgs->add(call.ptr());
            }
            break;
        }
        default :
            assert(false);
        }

        ExprPtr fieldType = new ObjectExpr(type.ptr());
        RecordFieldPtr field = new RecordField(ident, fieldType);
        fields.push_back(field);
    }
    r->body = new RecordBody(fields);

    TypePtr t = recordType(r, vector<ObjectPtr>());
    x->lambdaType = t;
    ExprPtr typeExpr = new ObjectExpr(t.ptr());
    converted->expr = typeExpr;
    x->converted = converted.ptr();

    CodePtr code = new Code();
    code->location = x->location;
    IdentifierPtr closureDataIdent = new Identifier(closureDataName);
    FormalArgPtr closureDataArg = new FormalArg(closureDataIdent, typeExpr);
    code->formalArgs.push_back(closureDataArg.ptr());
    for (unsigned i = 0; i < x->formalArgs.size(); ++i) {
        code->formalArgs.push_back(x->formalArgs[i]);
    }
    if (x->formalVarArg.ptr()) {
        code->formalVarArg = x->formalVarArg;
    }
    code->body = x->body;

    OverloadPtr overload = new Overload(
        operator_expr_call(), code, false, false
    );
    overload->env = env;
    overload->location = x->location;
    ObjectPtr obj = operator_call();
    if (obj->objKind != PROCEDURE)
        error("'call' operator not found!");
    Procedure *callObj = (Procedure *)obj.ptr();
    callObj->overloads.insert(callObj->overloads.begin(), overload);
}

static void initializeLambdaWithoutFreeVars(LambdaPtr x, EnvPtr env,
    string const &closureDataName, string const &lname)
{
    IdentifierPtr name = new Identifier(lname);
    name->location = x->location;
    x->lambdaProc = new Procedure(name, PRIVATE);
    x->lambdaProc->lambda = x;

    CodePtr code = new Code();
    code->location = x->location;
    for (unsigned i = 0; i < x->formalArgs.size(); ++i) {
        code->formalArgs.push_back(x->formalArgs[i]);
    }
    if (x->formalVarArg.ptr()) {
        code->formalVarArg = x->formalVarArg;
    }
    code->body = x->body;

    ExprPtr procRef = new ObjectExpr(x->lambdaProc.ptr());
    procRef->location = x->location;
    OverloadPtr overload = new Overload(procRef, code, false, false);
    overload->env = env;
    overload->location = x->location;
    addProcedureOverload(x->lambdaProc, overload);

    x->converted = procRef;
}



//
// addFreeVar, typeOfValue, typesOfValues
//

static void addFreeVar(LambdaContext &ctx, const string &str)
{
    vector<string>::iterator i;
    i = std::find(ctx.freeVars.begin(), ctx.freeVars.end(), str);
    if (i == ctx.freeVars.end()) {
        ctx.freeVars.push_back(str);
    }
}



//
// convertFreeVars
//

void convertFreeVars(LambdaPtr x, EnvPtr env,
                     const string &closureDataName,
                     vector<string> &freeVars)
{
    EnvPtr env2 = new Env(env);
    for (unsigned i = 0; i < x->formalArgs.size(); ++i) {
        FormalArgPtr arg = x->formalArgs[i];
        addLocal(env2, arg->name, arg->name.ptr());
    }
    if (x->formalVarArg.ptr()) {
        addLocal(env2, x->formalVarArg->name, x->formalVarArg->name.ptr());
    }
    LambdaContext ctx(x->captureByRef, env, closureDataName, freeVars);
    convertFreeVars(x->body, env2, ctx);
}

void convertFreeVars(StatementPtr x, EnvPtr env, LambdaContext &ctx)
{
    switch (x->stmtKind) {

    case BLOCK : {
        Block *y = (Block *)x.ptr();
        if (!y->desugared)
            y->desugared = desugarBlock(y);
        for (unsigned i = 0; i < y->desugared->statements.size(); ++i) {
            StatementPtr z = y->desugared->statements[i];
            if (z->stmtKind == BINDING) {
                Binding *a = (Binding *)z.ptr();
                convertFreeVars(a->values, env, ctx);
                EnvPtr env2 = new Env(env);
                for (unsigned j = 0; j < a->names.size(); ++j)
                    addLocal(env2, a->names[j], a->names[j].ptr());
                env = env2;
            }
            else {
                convertFreeVars(z, env, ctx);
            }
        }
        break;
    }

    case LABEL :
    case BINDING : {
        break;
    }

    case ASSIGNMENT : {
        Assignment *y = (Assignment *)x.ptr();
        convertFreeVars(y->left, env, ctx);
        convertFreeVars(y->right, env, ctx);
        break;
    }

    case INIT_ASSIGNMENT : {
        InitAssignment *y = (InitAssignment *)x.ptr();
        convertFreeVars(y->left, env, ctx);
        convertFreeVars(y->right, env, ctx);
        break;
    }

    case UPDATE_ASSIGNMENT : {
        UpdateAssignment *y = (UpdateAssignment *)x.ptr();
        convertFreeVars(y->left, env, ctx);
        convertFreeVars(y->right, env, ctx);
        break;
    }

    case GOTO : {
        break;
    }

    case RETURN : {
        Return *y = (Return *)x.ptr();
        convertFreeVars(y->values, env, ctx);
        break;
    }

    case IF : {
        If *y = (If *)x.ptr();
        convertFreeVars(y->condition, env, ctx);
        convertFreeVars(y->thenPart, env, ctx);
        if (y->elsePart.ptr())
            convertFreeVars(y->elsePart, env, ctx);
        break;
    }

    case SWITCH : {
        Switch *y = (Switch *)x.ptr();
        convertFreeVars(y->expr, env, ctx);
        for (unsigned i = 0; i < y->caseBlocks.size(); ++i) {
            CaseBlockPtr z = y->caseBlocks[i];
            convertFreeVars(z->caseLabels, env, ctx);
            convertFreeVars(z->body, env, ctx);
        }
        if (y->defaultCase.ptr())
            convertFreeVars(y->defaultCase, env, ctx);
        break;
    }

    case EVAL_STATEMENT : {
        EvalStatement *eval = (EvalStatement *)x.ptr();
        convertFreeVars(eval->args, env, ctx);
        break;
    }

    case EXPR_STATEMENT : {
        ExprStatement *y = (ExprStatement *)x.ptr();
        convertFreeVars(y->expr, env, ctx);
        break;
    }

    case WHILE : {
        While *y = (While *)x.ptr();
        convertFreeVars(y->condition, env, ctx);
        convertFreeVars(y->body, env, ctx);
        break;
    }

    case BREAK :
    case CONTINUE : {
        break;
    }

    case FOR : {
        For *y = (For *)x.ptr();
        convertFreeVars(y->expr, env, ctx);
        EnvPtr env2 = new Env(env);
        for (unsigned j = 0; j < y->variables.size(); ++j)
            addLocal(env2, y->variables[j], y->variables[j].ptr());
        convertFreeVars(y->body, env2, ctx);
        break;
    }

    case FOREIGN_STATEMENT : {
        break;
    }

    case TRY : {
        Try *y = (Try *)x.ptr();
        convertFreeVars(y->tryBlock, env, ctx);
        for (unsigned i = 0; i < y->catchBlocks.size(); ++i) {
            EnvPtr env2 = new Env(env);
            addLocal(env2,
                y->catchBlocks[i]->exceptionVar,
                y->catchBlocks[i]->exceptionVar.ptr()
            );
            if (y->catchBlocks[i]->exceptionType.ptr())
                convertFreeVars(y->catchBlocks[i]->exceptionType, env, ctx);
            convertFreeVars(y->catchBlocks[i]->body, env2, ctx);
        }

        break;
    }

    case THROW : {
        Throw *y = (Throw *)x.ptr();
        convertFreeVars(y->expr, env, ctx);
        break;
    }

    case STATIC_FOR : {
        StaticFor *y = (StaticFor *)x.ptr();
        convertFreeVars(y->values, env, ctx);
        EnvPtr env2 = new Env(env);
        addLocal(env2, y->variable, y->variable.ptr());
        convertFreeVars(y->body, env2, ctx);
        break;
    }

    case FINALLY : {
        Finally *y = (Finally *)x.ptr();
        convertFreeVars(y->body, env, ctx);
        break;
    }

    case ONERROR : {
        OnError *y = (OnError *)x.ptr();
        convertFreeVars(y->body, env, ctx);
        break;
    }

    case UNREACHABLE :
        break;

    default :
        assert(false);

    }
}

void convertFreeVars(ExprPtr &x, EnvPtr env, LambdaContext &ctx)
{
    switch (x->exprKind) {

    case BOOL_LITERAL :
    case INT_LITERAL :
    case FLOAT_LITERAL :
    case CHAR_LITERAL :
    case STRING_LITERAL :
    case IDENTIFIER_LITERAL :
        break;

    case NAME_REF : {
        NameRef *y = (NameRef *)x.ptr();
        bool isNonLocal = false;
        bool isGlobal = false;
        ObjectPtr z = lookupEnvEx(env, y->name, ctx.nonLocalEnv,
                                  isNonLocal, isGlobal);
        if (isNonLocal && !isGlobal) {
            if ((z->objKind == PVALUE) || (z->objKind == CVALUE)) {
                TypePtr t = typeOfValue(z);
                if (isStaticOrTupleOfStatics(t)) {
                    ExprListPtr args = new ExprList();
                    args->add(new ObjectExpr(t.ptr()));
                    CallPtr call = new Call(operator_expr_typeToRValue(), args);
                    call->location = y->location;
                    x = call.ptr();
                }
                else {
                    addFreeVar(ctx, y->name->str);
                    IdentifierPtr a = new Identifier(ctx.closureDataName);
                    a->location = y->location;
                    NameRefPtr b = new NameRef(a);
                    b->location = y->location;
                    FieldRefPtr c = new FieldRef(b.ptr(), y->name);
                    c->location = y->location;
                    if (ctx.captureByRef) {
                        ExprPtr d = new UnaryOp(DEREFERENCE, c.ptr());
                        d->location = y->location;
                        x = d.ptr();
                    }
                    else {
                        x = c.ptr();
                    }
                }
            }
            else if ((z->objKind == MULTI_PVALUE) || (z->objKind == MULTI_CVALUE)) {
                vector<TypePtr> types = typesOfValues(z);
                bool allStatic = true;
                for (unsigned i = 0; i < types.size(); ++i) {
                    if (!isStaticOrTupleOfStatics(types[i])) {
                        allStatic = false;
                        break;
                    }
                }
                if (allStatic) {
                    ExprListPtr args = new ExprList();
                    for (unsigned i = 0; i < types.size(); ++i)
                        args->add(new ObjectExpr(types[i].ptr()));
                    CallPtr call = new Call(operator_expr_typesToRValues(), args);
                    call->location = y->location;
                    x = call.ptr();
                }
                else {
                    addFreeVar(ctx, y->name->str);
                    IdentifierPtr a = new Identifier(ctx.closureDataName);
                    a->location = y->location;
                    NameRefPtr b = new NameRef(a);
                    b->location = y->location;
                    FieldRefPtr c = new FieldRef(b.ptr(), y->name);
                    c->location = y->location;
                    if (ctx.captureByRef) {
                        ExprPtr f =
                            operator_expr_unpackMultiValuedFreeVarAndDereference();
                        CallPtr d = new Call(f, new ExprList());
                        d->parenArgs->add(c.ptr());
                        x = d.ptr();
                    }
                    else {
                        ExprPtr f = operator_expr_unpackMultiValuedFreeVar();
                        CallPtr d = new Call(f, new ExprList());
                        d->parenArgs->add(c.ptr());
                        x = d.ptr();
                    }
                }
            }
        }
        break;
    }

    case TUPLE : {
        Tuple *y = (Tuple *)x.ptr();
        convertFreeVars(y->args, env, ctx);
        break;
    }

    case PAREN : {
        Paren *y = (Paren *)x.ptr();
        convertFreeVars(y->args, env, ctx);
        break;
    }

    case INDEXING : {
        Indexing *y = (Indexing *)x.ptr();
        convertFreeVars(y->expr, env, ctx);
        convertFreeVars(y->args, env, ctx);
        break;
    }

    case CALL : {
        Call *y = (Call *)x.ptr();
        convertFreeVars(y->expr, env, ctx);
        convertFreeVars(y->parenArgs, env, ctx);
        convertFreeVars(y->lambdaArgs, env, ctx);
        break;
    }

    case FIELD_REF : {
        FieldRef *y = (FieldRef *)x.ptr();
        convertFreeVars(y->expr, env, ctx);
        break;
    }

    case STATIC_INDEXING : {
        StaticIndexing *y = (StaticIndexing *)x.ptr();
        convertFreeVars(y->expr, env, ctx);
        break;
    }

    case UNARY_OP : {
        UnaryOp *y = (UnaryOp *)x.ptr();
        convertFreeVars(y->expr, env, ctx);
        break;
    }

    case BINARY_OP : {
        BinaryOp *y = (BinaryOp *)x.ptr();
        convertFreeVars(y->expr1, env, ctx);
        convertFreeVars(y->expr2, env, ctx);
        break;
    }

    case VARIADIC_OP : {
        VariadicOp *y = (VariadicOp *)x.ptr();
        convertFreeVars(y->exprs, env, ctx);
        break;
    }

    case AND : {
        And *y = (And *)x.ptr();
        convertFreeVars(y->expr1, env, ctx);
        convertFreeVars(y->expr2, env, ctx);
        break;
    }

    case OR : {
        Or *y = (Or *)x.ptr();
        convertFreeVars(y->expr1, env, ctx);
        convertFreeVars(y->expr2, env, ctx);
        break;
    }

    case LAMBDA : {
        Lambda *y = (Lambda *)x.ptr();
        EnvPtr env2 = new Env(env);
        for (unsigned i = 0; i < y->formalArgs.size(); ++i)
            addLocal(env2, y->formalArgs[i]->name, y->formalArgs[i]->name.ptr());
        if (y->formalVarArg.ptr())
            addLocal(env2, y->formalVarArg->name, y->formalVarArg->name.ptr());
        convertFreeVars(y->body, env2, ctx);
        break;
    }

    case UNPACK : {
        Unpack *y = (Unpack *)x.ptr();
        convertFreeVars(y->expr, env, ctx);
        break;
    }

    case STATIC_EXPR : {
        StaticExpr *y = (StaticExpr *)x.ptr();
        convertFreeVars(y->expr, env, ctx);
        break;
    }

    case DISPATCH_EXPR : {
        DispatchExpr *y = (DispatchExpr *)x.ptr();
        convertFreeVars(y->expr, env, ctx);
        break;
    }

    case EVAL_EXPR : {
        EvalExpr *eval = (EvalExpr *)x.ptr();
        convertFreeVars(eval->args, env, ctx);
        break;
    }

    case FOREIGN_EXPR :
    case OBJECT_EXPR :
    case FILE_EXPR :
    case LINE_EXPR :
    case COLUMN_EXPR :
    case ARG_EXPR :
        break;
    }
}

void convertFreeVars(ExprListPtr x, EnvPtr env, LambdaContext &ctx)
{
    for (unsigned i = 0; i < x->size(); ++i)
        convertFreeVars(x->exprs[i], env, ctx);
}

}
