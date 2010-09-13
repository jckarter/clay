#include "clay.hpp"
#include "libclaynames.hpp"


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

static int _lambdaObjectIndex = 0;

static int nextLambdaObjectIndex() {
    int x = _lambdaObjectIndex;
    ++ _lambdaObjectIndex;
    return x;
}

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

static void initializeLambdaWithFreeVars(LambdaPtr x,
                                         EnvPtr env,
                                         const string &closureDataName,
                                         int lambdaObjectIndex);
static void initializeLambdaWithoutFreeVars(LambdaPtr x,
                                            EnvPtr env);

void initializeLambda(LambdaPtr x, EnvPtr env)
{
    assert(!x->initialized);
    x->initialized = true;

    int lambdaObjectIndex = nextLambdaObjectIndex();

    ostringstream ostr;
    ostr << "%closureData" << lambdaObjectIndex;
    string closureDataName = ostr.str();

    convertFreeVars(x, env, closureDataName, x->freeVars);
    if (x->freeVars.empty()) {
        initializeLambdaWithoutFreeVars(x, env);
    }
    else {
        initializeLambdaWithFreeVars(
            x, env, closureDataName, lambdaObjectIndex);
    }
}

static void initializeLambdaWithFreeVars(LambdaPtr x,
                                         EnvPtr env,
                                         const string &closureDataName,
                                         int lambdaObjectIndex)
{
    ostringstream sout;
    sout << "LambdaObject" << lambdaObjectIndex;

    RecordPtr r = new Record(PRIVATE);
    r->location = x->location;
    r->name = new Identifier(sout.str());
    x->lambdaRecord = r;
    vector<RecordFieldPtr> fields;

    TypePtr t = recordType(r, vector<ObjectPtr>());
    x->lambdaType = t;
    ExprPtr typeExpr = new ObjectExpr(t.ptr());

    CallPtr converted = new Call(typeExpr, new ExprList());
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
                converted->args->add(addr);
            }
            else {
                converted->args->add(nameRef.ptr());
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
                ExprPtr e = prelude_expr_packMultiValuedFreeVarByRef();
                CallPtr call = new Call(e, new ExprList());
                call->args->add(new Unpack(nameRef.ptr()));
                converted->args->add(call.ptr());
            }
            else {
                ExprPtr e = prelude_expr_packMultiValuedFreeVar();
                CallPtr call = new Call(e, new ExprList());
                call->args->add(new Unpack(nameRef.ptr()));
                converted->args->add(call.ptr());
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

    x->converted = converted.ptr();

    CodePtr code = new Code();
    code->location = x->location;
    IdentifierPtr closureDataIdent = new Identifier(closureDataName);
    FormalArgPtr closureDataArg = new FormalArg(closureDataIdent, typeExpr);
    code->formalArgs.push_back(closureDataArg.ptr());
    for (unsigned i = 0; i < x->formalArgs.size(); ++i) {
        FormalArgPtr y = new FormalArg(x->formalArgs[i], NULL);
        y->location = x->formalArgs[i]->location;
        code->formalArgs.push_back(y.ptr());
    }
    code->body = x->body;

    OverloadPtr overload = new Overload(prelude_expr_call(), code, false);
    overload->env = env;
    overload->location = x->location;
    ObjectPtr obj = prelude_call();
    if (obj->objKind != PROCEDURE)
        error("'call' operator not found!");
    Procedure *callObj = (Procedure *)obj.ptr();
    callObj->overloads.insert(callObj->overloads.begin(), overload);
}

static void initializeLambdaWithoutFreeVars(LambdaPtr x, EnvPtr env)
{
    IdentifierPtr name = new Identifier("LambdaProcedure");
    name->location = x->location;
    x->lambdaProc = new Procedure(name, PRIVATE);

    CodePtr code = new Code();
    code->location = x->location;
    for (unsigned i = 0; i < x->formalArgs.size(); ++i) {
        FormalArgPtr y = new FormalArg(x->formalArgs[i], NULL);
        y->location = x->formalArgs[i]->location;
        code->formalArgs.push_back(y.ptr());
    }
    code->body = x->body;

    ExprPtr procRef = new ObjectExpr(x->lambdaProc.ptr());
    procRef->location = x->location;
    OverloadPtr overload = new Overload(procRef, code, false);
    overload->env = env;
    overload->location = x->location;
    x->lambdaProc->overloads.push_back(overload);

    x->converted = procRef;
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
        IdentifierPtr name = x->formalArgs[i];
        addLocal(env2, name, name.ptr());
    }
    LambdaContext ctx(x->captureByRef, env, closureDataName, freeVars);
    convertFreeVars(x->body, env2, ctx);
}

void convertFreeVars(StatementPtr x, EnvPtr env, LambdaContext &ctx)
{
    switch (x->stmtKind) {

    case BLOCK : {
        Block *y = (Block *)x.ptr();
        for (unsigned i = 0; i < y->statements.size(); ++i) {
            StatementPtr z = y->statements[i];
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

    case CASE_BODY : {
        CaseBody *y = (CaseBody *)x.ptr();
        for (unsigned i = 0; i < y->statements.size(); ++i)
            convertFreeVars(y->statements[i], env, ctx);
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
                vector<string>::iterator i =
                    std::find(ctx.freeVars.begin(), ctx.freeVars.end(),
                              y->name->str);
                if (i == ctx.freeVars.end()) {
                    ctx.freeVars.push_back(y->name->str);
                }
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
            else if ((z->objKind == MULTI_PVALUE) || (z->objKind == MULTI_CVALUE)) {
                vector<string>::iterator i =
                    std::find(ctx.freeVars.begin(), ctx.freeVars.end(),
                              y->name->str);
                if (i == ctx.freeVars.end()) {
                    ctx.freeVars.push_back(y->name->str);
                }
                IdentifierPtr a = new Identifier(ctx.closureDataName);
                a->location = y->location;
                NameRefPtr b = new NameRef(a);
                b->location = y->location;
                FieldRefPtr c = new FieldRef(b.ptr(), y->name);
                c->location = y->location;
                if (ctx.captureByRef) {
                    ExprPtr f =
                        prelude_expr_unpackMultiValuedFreeVarAndDereference();
                    CallPtr d = new Call(f, new ExprList());
                    d->args->add(c.ptr());
                    x = d.ptr();
                }
                else {
                    ExprPtr f = prelude_expr_unpackMultiValuedFreeVar();
                    CallPtr d = new Call(f, new ExprList());
                    d->args->add(c.ptr());
                    x = d.ptr();
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

    case ARRAY : {
        Array *y = (Array *)x.ptr();
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
        convertFreeVars(y->args, env, ctx);
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

    case IF_EXPR : {
        IfExpr *y = (IfExpr *)x.ptr();
        convertFreeVars(y->condition, env, ctx);
        convertFreeVars(y->thenPart, env, ctx);
        convertFreeVars(y->elsePart, env, ctx);
        break;
    }

    case LAMBDA : {
        Lambda *y = (Lambda *)x.ptr();
        EnvPtr env2 = new Env(env);
        for (unsigned i = 0; i < y->formalArgs.size(); ++i)
            addLocal(env2, y->formalArgs[i], y->formalArgs[i].ptr());
        convertFreeVars(y->body, env2, ctx);
        break;
    }

    case UNPACK : {
        Unpack *y = (Unpack *)x.ptr();
        convertFreeVars(y->expr, env, ctx);
        break;
    }

    case NEW : {
        New *y = (New *)x.ptr();
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

    case FOREIGN_EXPR :
    case OBJECT_EXPR :
        break;
    }
}

void convertFreeVars(ExprListPtr x, EnvPtr env, LambdaContext &ctx)
{
    for (unsigned i = 0; i < x->size(); ++i)
        convertFreeVars(x->exprs[i], env, ctx);
}
