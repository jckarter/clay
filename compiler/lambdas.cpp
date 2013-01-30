#include "clay.hpp"
#include "codegen.hpp"
#include "operators.hpp"
#include "lambdas.hpp"
#include "desugar.hpp"
#include "analyzer.hpp"
#include "loader.hpp"
#include "env.hpp"


namespace clay {

struct LambdaContext {
    LambdaCapture captureBy;
    EnvPtr nonLocalEnv;
    llvm::StringRef closureDataName;
    vector<string> &freeVars;
    CompilerState* cst;
    LambdaContext(LambdaCapture captureBy,
                  EnvPtr nonLocalEnv,
                  llvm::StringRef closureDataName,
                  vector<string> &freeVars,
                  CompilerState* cst)
        : captureBy(captureBy), nonLocalEnv(nonLocalEnv),
          closureDataName(closureDataName), freeVars(freeVars) ,
          cst(cst) {}
};

void convertFreeVars(LambdaPtr x, EnvPtr env,
                     llvm::StringRef closureDataName,
                     vector<string> &freeVars);

void convertFreeVars(StatementPtr x, EnvPtr env, LambdaContext &ctx);

void convertFreeVars(ExprPtr &x, EnvPtr env, LambdaContext &ctx);
void convertFreeVars(ExprListPtr x, EnvPtr env, LambdaContext &ctx);



//
// initializeLambda
//

static TypePtr typeOfValue(ObjectPtr obj) {
    switch (obj->objKind) {
    case PVALUE : return ((PValue *)obj.ptr())->data.type;
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
            types.push_back(mpv->values[i].type);
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
    llvm::StringRef closureDataName, llvm::StringRef lname,
    CompilerState* cst);
static void initializeLambdaWithoutFreeVars(LambdaPtr x, EnvPtr env,
    llvm::StringRef lname);

static string lambdaName(LambdaPtr x)
{
    string fullName = shortString(x->asString());

    if (fullName.size() <= 80)
        return "(" + fullName + ")";
    else {
        llvm::SmallString<128> shortNameBuf;
        llvm::raw_svector_ostream shortName(shortNameBuf);
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

    CompilerState* cst = safeLookupModule(env)->cst;

    string lname = lambdaName(x);

    llvm::SmallString<128> buf;
    llvm::raw_svector_ostream ostr(buf);
    ostr << "%closureData:" << lname;
    string closureDataName = ostr.str();

    convertFreeVars(x, env, closureDataName, x->freeVars);
    getProcedureMonoTypes(x->mono, env, x->formalArgs, x->hasVarArg);
        
    switch (x->captureBy) {
    case REF_CAPTURE :
    case VALUE_CAPTURE : {
        if (x->freeVars.empty()) {
            initializeLambdaWithoutFreeVars(x, env, lname);
        }
        else {
            initializeLambdaWithFreeVars(x, env, closureDataName, lname, cst);
        }
        break;
    }
    case STATELESS : {
        assert(x->freeVars.empty());
        initializeLambdaWithoutFreeVars(x, env, lname);
        break;
    }
    default :
        assert(false);
    }
}

static void checkForeignExpr(ObjectPtr &obj, EnvPtr env, CompilerState* cst)
{
    if (obj->objKind == EXPRESSION) {
        ExprPtr expr = (Expr *)obj.ptr();
        if (expr->exprKind == FOREIGN_EXPR) {
            MultiPValuePtr mpv = safeAnalyzeMulti(new ExprList(expr), env, 0, cst);
            obj = mpv.ptr();
        }
    }
    else if (obj->objKind == EXPR_LIST) {
        ExprListPtr expr = (ExprList *)obj.ptr();
        switch (expr->exprs[0]->exprKind) {
        case FOREIGN_EXPR :
        case UNPACK : {
            MultiPValuePtr mpv = safeAnalyzeMulti(
                new ExprList(foreignExpr(env, expr->exprs[0])), env, 0, cst);
            obj = mpv.ptr();
        }
        default: {} // make compiler happy
        }
    }
}

static void initializeLambdaWithFreeVars(LambdaPtr x, EnvPtr env,
    llvm::StringRef closureDataName, llvm::StringRef lname, CompilerState* cst)
{
    RecordDeclPtr r = new RecordDecl(NULL, PRIVATE);
    r->location = x->location;
    r->name = Identifier::get(lname);
    r->env = env;
    x->lambdaRecord = r;
    r->lambda = x;
    vector<RecordFieldPtr> fields;

    CallPtr converted = new Call(NULL, new ExprList());
    for (size_t i = 0; i < x->freeVars.size(); ++i) {
        IdentifierPtr ident = Identifier::get(x->freeVars[i]);
        NameRefPtr nameRef = new NameRef(ident);

        TypePtr type;
        ObjectPtr obj = safeLookupEnv(env, ident);
        checkForeignExpr(obj, env, cst);
        switch (obj->objKind) {
        case PVALUE :
        case CVALUE : {
            type = typeOfValue(obj);
            if (x->captureBy == REF_CAPTURE) {
                type = pointerType(type);
                ExprPtr addr = new VariadicOp(ADDRESS_OF, new ExprList(nameRef.ptr()));
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
            for (size_t j = 0; j < types.size(); ++j) {
                TypePtr t = types[j];
                if (x->captureBy == REF_CAPTURE)
                    t = pointerType(t);
                elementTypes.push_back(t);
            }
            type = tupleType(elementTypes, cst);
            if (x->captureBy == REF_CAPTURE) {
                ExprPtr e = operator_expr_packMultiValuedFreeVarByRef(cst);
                CallPtr call = new Call(e, new ExprList());
                call->parenArgs->add(new Unpack(nameRef.ptr()));
                converted->parenArgs->add(call.ptr());
            }
            else {
                ExprPtr e = operator_expr_packMultiValuedFreeVar(cst);
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
    IdentifierPtr closureDataIdent = Identifier::get(closureDataName);
    FormalArgPtr closureDataArg = new FormalArg(closureDataIdent, typeExpr);
    code->formalArgs.push_back(closureDataArg.ptr());
    for (size_t i = 0; i < x->formalArgs.size(); ++i) {
        code->formalArgs.push_back(x->formalArgs[i]);
    }
    code->hasVarArg = x->hasVarArg;
    code->body = x->body;

    OverloadPtr overload = new Overload(
        NULL, operator_expr_call(cst), code, false, IGNORE, x->hasAsConversion
    );
    overload->env = env;
    overload->location = x->location;
    ObjectPtr obj = operator_call(cst);
    if (obj->objKind != PROCEDURE)
        error("'call' operator not found!");
    Procedure *callObj = (Procedure *)obj.ptr();
    addOverload(callObj->overloads, overload);
}

static void initializeLambdaWithoutFreeVars(LambdaPtr x, EnvPtr env, 
    llvm::StringRef lname)
{
    IdentifierPtr name = Identifier::get(lname, x->location);
    x->lambdaProc = new Procedure(NULL, name, PRIVATE, false);
    x->lambdaProc->lambda = x;

    CodePtr code = new Code();
    code->location = x->location;
    for (size_t i = 0; i < x->formalArgs.size(); ++i) {
        code->formalArgs.push_back(x->formalArgs[i]);
    }
    code->hasVarArg = x->hasVarArg;
    code->body = x->body;

    ExprPtr procRef = new ObjectExpr(x->lambdaProc.ptr());
    procRef->location = x->location;
    OverloadPtr overload = new Overload(NULL, procRef, code, false, IGNORE, x->hasAsConversion);
    overload->env = env;
    overload->location = x->location;
    addProcedureOverload(x->lambdaProc, env, overload);

    x->converted = procRef;
}



//
// addFreeVar, typeOfValue, typesOfValues
//

static void addFreeVar(LambdaContext &ctx, llvm::StringRef str)
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
                     llvm::StringRef closureDataName,
                     vector<string> &freeVars)
{
    EnvPtr env2 = new Env(env);
    for (size_t i = 0; i < x->formalArgs.size(); ++i) {
        FormalArgPtr arg = x->formalArgs[i];
        addLocal(env2, arg->name, arg->name.ptr());
    }
    LambdaContext ctx(x->captureBy, env, closureDataName, freeVars, env->cst);
    convertFreeVars(x->body, env2, ctx);
}

static EnvPtr convertFreeVarsFromBinding(BindingPtr binding, EnvPtr env, LambdaContext &ctx)
{
    convertFreeVars(binding->values, env, ctx);
    EnvPtr env2 = new Env(env);
    for (size_t j = 0; j < binding->args.size(); ++j)
        addLocal(env2, binding->args[j]->name, binding->args[j]->name.ptr());
    return env2;
}

static EnvPtr convertFreeVarsFromStatementExpressionStatements(
    llvm::ArrayRef<StatementPtr> stmts,
    EnvPtr env,
    LambdaContext &ctx)
{
    EnvPtr env2 = env;
    for (StatementPtr const *i = stmts.begin(), *end = stmts.end();
         i != end;
         ++i)
    {
        switch ((*i)->stmtKind) {
        case BINDING:
            env2 = convertFreeVarsFromBinding((Binding*)i->ptr(), env2, ctx);
            break;

        case ASSIGNMENT:
        case VARIADIC_ASSIGNMENT:
        case INIT_ASSIGNMENT:
        case EXPR_STATEMENT:
            convertFreeVars(*i, env2, ctx);
            break;

        default:
            assert(false);
            return NULL;
        }
    }
    return env2;
}

void convertFreeVars(StatementPtr x, EnvPtr env, LambdaContext &ctx)
{
    switch (x->stmtKind) {

    case BLOCK : {
        Block *y = (Block *)x.ptr();
        for (size_t i = 0; i < y->statements.size(); ++i) {
            StatementPtr z = y->statements[i];
            if (z->stmtKind == BINDING)
                env = convertFreeVarsFromBinding((Binding *)z.ptr(), env, ctx);
            else
                convertFreeVars(z, env, ctx);
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

    case VARIADIC_ASSIGNMENT : {
        VariadicAssignment *y = (VariadicAssignment *)x.ptr();
        convertFreeVars(y->exprs, env, ctx);
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
        env = convertFreeVarsFromStatementExpressionStatements(y->conditionStatements, env, ctx);
        convertFreeVars(y->condition, env, ctx);
        convertFreeVars(y->thenPart, env, ctx);
        if (y->elsePart.ptr())
            convertFreeVars(y->elsePart, env, ctx);
        break;
    }

    case SWITCH : {
        Switch *y = (Switch *)x.ptr();
        env = convertFreeVarsFromStatementExpressionStatements(y->exprStatements, env, ctx);
        convertFreeVars(y->expr, env, ctx);
        for (size_t i = 0; i < y->caseBlocks.size(); ++i) {
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
        env = convertFreeVarsFromStatementExpressionStatements(y->conditionStatements, env, ctx);
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
        for (size_t j = 0; j < y->variables.size(); ++j)
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
        for (size_t i = 0; i < y->catchBlocks.size(); ++i) {
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
        desugarThrow(y);
        convertFreeVars(y->desugaredExpr, env, ctx);
        if (y->desugaredContext != NULL)
            convertFreeVars(y->desugaredContext, env, ctx);
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

    case STATIC_ASSERT_STATEMENT :
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
        break;

    case NAME_REF : {
        NameRef *y = (NameRef *)x.ptr();
        bool isNonLocal = false;
        bool isGlobal = false;
        ObjectPtr z = lookupEnvEx(env, y->name, ctx.nonLocalEnv,
                                  isNonLocal, isGlobal);
        if (isNonLocal && !isGlobal) {
            checkForeignExpr(z, env, ctx.cst);
            if ((z->objKind == PVALUE) || (z->objKind == CVALUE)) {
                TypePtr t = typeOfValue(z);
                if (isStaticOrTupleOfStatics(t)) {
                    ExprListPtr args = new ExprList();
                    args->add(new ObjectExpr(t.ptr()));
                    CallPtr call = new Call(operator_expr_typeToRValue(ctx.cst), args);
                    call->location = y->location;
                    x = call.ptr();
                }
                else {
                    addFreeVar(ctx, y->name->str);
                    IdentifierPtr a = Identifier::get(ctx.closureDataName, y->location);
                    NameRefPtr b = new NameRef(a);
                    b->location = y->location;
                    FieldRefPtr c = new FieldRef(b.ptr(), y->name);
                    c->location = y->location;
                    switch (ctx.captureBy) {
                    case REF_CAPTURE : {
                        ExprPtr d = new VariadicOp(DEREFERENCE, new ExprList(c.ptr()));
                        d->location = y->location;
                        x = d.ptr();
                        break;
                    }
                    case VALUE_CAPTURE : {
                        x = c.ptr();
                        break;
                    }
                    case STATELESS :
                        error(c->location, 
                            "cannot capture free variable in stateless lambda function");
                    default :
                        assert(false);
                    }
                }
            }
            else if ((z->objKind == MULTI_PVALUE) || (z->objKind == MULTI_CVALUE)) {
                vector<TypePtr> types = typesOfValues(z);
                bool allStatic = true;
                for (size_t i = 0; i < types.size(); ++i) {
                    if (!isStaticOrTupleOfStatics(types[i])) {
                        allStatic = false;
                        break;
                    }
                }
                if (allStatic) {
                    ExprListPtr args = new ExprList();
                    for (size_t i = 0; i < types.size(); ++i)
                        args->add(new ObjectExpr(types[i].ptr()));
                    CallPtr call = new Call(operator_expr_typesToRValues(ctx.cst), args);
                    call->location = y->location;
                    x = call.ptr();
                }
                else {
                    addFreeVar(ctx, y->name->str);
                    IdentifierPtr a = Identifier::get(ctx.closureDataName, y->location);
                    NameRefPtr b = new NameRef(a);
                    b->location = y->location;
                    FieldRefPtr c = new FieldRef(b.ptr(), y->name);
                    c->location = y->location;
                    switch (ctx.captureBy) {
                    case REF_CAPTURE : {
                        ExprPtr f =
                            operator_expr_unpackMultiValuedFreeVarAndDereference(ctx.cst);
                        CallPtr d = new Call(f, new ExprList());
                        d->parenArgs->add(c.ptr());
                        x = d.ptr();
                        break;
                    }
                    case VALUE_CAPTURE : {
                        ExprPtr f = operator_expr_unpackMultiValuedFreeVar(ctx.cst);
                        CallPtr d = new Call(f, new ExprList());
                        d->parenArgs->add(c.ptr());
                        x = d.ptr();
                        break;
                    }
                    case STATELESS :
                        error(c->location, 
                            "cannot capture free variable in stateless lambda function");
                    default :
                        assert(false);
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
        break;
    }

    case FIELD_REF : {
        FieldRef *y = (FieldRef *)x.ptr();
        if (y->desugared == NULL)
            desugarFieldRef(y, safeLookupModule(env));
        if (!y->isDottedModuleName)
            convertFreeVars(y->expr, env, ctx);
        break;
    }

    case STATIC_INDEXING : {
        StaticIndexing *y = (StaticIndexing *)x.ptr();
        convertFreeVars(y->expr, env, ctx);
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
        for (size_t i = 0; i < y->formalArgs.size(); ++i)
            addLocal(env2, y->formalArgs[i]->name, y->formalArgs[i]->name.ptr());
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
