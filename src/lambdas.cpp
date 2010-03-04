#include "clay.hpp"


void convertClosure(LambdaPtr x, EnvPtr env,
                    const string &closureDataName,
                    vector<string> &freeVars);

void convertClosure(StatementPtr x, EnvPtr env, EnvPtr nonLocalEnv,
                    const string &closureDataName,
                    vector<string> &freeVars);

void  convertClosure(ExprPtr &x, EnvPtr env, EnvPtr nonLocalEnv,
                     const string &closureDataName,
                     vector<string> &freeVars);



//
// initializeLambda
//

static int closureDataIndex = 0;

void initializeLambda(LambdaPtr x, EnvPtr env)
{
    assert(!x->initialized);
    x->initialized = true;

    RecordPtr r = new Record();
    r->location = x->location;
    r->name = new Identifier("LambdaFreeVars");
    x->lambdaRecord = r;

    TypePtr t = recordType(r, vector<ObjectPtr>());
    x->lambdaType = t;
    ExprPtr typeExpr = new ObjectExpr(t.ptr());

    ostringstream ostr;
    ostr << "%closureData" << closureDataIndex;
    ++closureDataIndex;
    string closureDataName = ostr.str();

    vector<string> freeVars;
    convertClosure(x, env, closureDataName, freeVars);

    CallPtr converted = new Call(typeExpr);
    for (unsigned i = 0; i < freeVars.size(); ++i) {
        IdentifierPtr ident = new Identifier(freeVars[i]);
        NameRefPtr nameRef = new NameRef(ident);
        converted->args.push_back(nameRef.ptr());

        TypePtr type;
        ObjectPtr obj = lookupEnv(env, ident);
        switch (obj->objKind) {
        case PVALUE : {
            PValue *y = (PValue *)obj.ptr();
            type = y->type;
            break;
        }
        case CVALUE : {
            CValue *y = (CValue *)obj.ptr();
            type = y->type;
            break;
        }
        default :
            assert(false);
        }

        ExprPtr fieldType = new ObjectExpr(type.ptr());
        RecordFieldPtr field = new RecordField(ident, fieldType);
        r->fields.push_back(field);
    }

    x->converted = converted.ptr();

    CodePtr code = new Code();
    code->location = x->location;
    IdentifierPtr closureDataIdent = new Identifier(closureDataName);
    ValueArgPtr closureDataArg = new ValueArg(closureDataIdent, typeExpr);
    code->formalArgs.push_back(closureDataArg.ptr());
    for (unsigned i = 0; i < x->formalArgs.size(); ++i) {
        ValueArgPtr y = new ValueArg(x->formalArgs[i], NULL);
        y->location = x->formalArgs[i]->location;
        code->formalArgs.push_back(y.ptr());
    }
    code->body = x->body;

    OverloadPtr overload = new Overload(kernelNameRef("call"), code);
    overload->env = env;
    overload->location = x->location;
    ObjectPtr obj = kernelName("call");
    if (obj->objKind != OVERLOADABLE)
        error("'call' operator not found!");
    Overloadable *callObj = (Overloadable *)obj.ptr();
    callObj->overloads.insert(callObj->overloads.begin(), overload);
    if (!callObj->staticFlagsInitialized)
        initIsStaticFlags(callObj);
    else
        updateIsStaticFlags(obj, overload);
}



//
// convertClosure
//

void convertClosure(LambdaPtr x, EnvPtr env,
                    const string &closureDataName,
                    vector<string> &freeVars)
{
    EnvPtr env2 = new Env(env);
    for (unsigned i = 0; i < x->formalArgs.size(); ++i) {
        IdentifierPtr name = x->formalArgs[i];
        addLocal(env2, name, name.ptr());
    }
    convertClosure(x->body, env2, env, closureDataName, freeVars);
}

void convertClosure(StatementPtr x, EnvPtr env, EnvPtr nonLocalEnv,
                    const string &closureDataName,
                    vector<string> &freeVars)
{
    switch (x->objKind) {

    case BLOCK : {
        Block *y = (Block *)x.ptr();
        for (unsigned i = 0; i < y->statements.size(); ++i) {
            StatementPtr z = y->statements[i];
            if (z->objKind == BINDING) {
                Binding *a = (Binding *)z.ptr();
                convertClosure(a->expr, env, nonLocalEnv,
                               closureDataName, freeVars);
                EnvPtr env2 = new Env(env);
                addLocal(env2, a->name, a->name.ptr());
                env = env2;
            }
            else {
                convertClosure(z, env, nonLocalEnv,
                               closureDataName, freeVars);
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
        convertClosure(y->left, env, nonLocalEnv, closureDataName, freeVars);
        convertClosure(y->right, env, nonLocalEnv, closureDataName, freeVars);
        break;
    }

    case INIT_ASSIGNMENT : {
        InitAssignment *y = (InitAssignment *)x.ptr();
        convertClosure(y->left, env, nonLocalEnv, closureDataName, freeVars);
        convertClosure(y->right, env, nonLocalEnv, closureDataName, freeVars);
        break;
    }

    case UPDATE_ASSIGNMENT : {
        UpdateAssignment *y = (UpdateAssignment *)x.ptr();
        convertClosure(y->left, env, nonLocalEnv, closureDataName, freeVars);
        convertClosure(y->right, env, nonLocalEnv, closureDataName, freeVars);
        break;
    }

    case GOTO : {
        break;
    }

    case RETURN : {
        Return *y = (Return *)x.ptr();
        if (y->expr.ptr())
            convertClosure(y->expr, env, nonLocalEnv,
                           closureDataName, freeVars);
        break;
    }

    case RETURN_REF : {
        ReturnRef *y = (ReturnRef *)x.ptr();
        convertClosure(y->expr, env, nonLocalEnv, closureDataName, freeVars);
        break;
    }

    case IF : {
        If *y = (If *)x.ptr();
        convertClosure(y->condition, env, nonLocalEnv,
                       closureDataName, freeVars);
        convertClosure(y->thenPart, env, nonLocalEnv,
                       closureDataName, freeVars);
        if (y->elsePart.ptr())
            convertClosure(y->elsePart, env, nonLocalEnv,
                           closureDataName, freeVars);
        break;
    }

    case EXPR_STATEMENT : {
        ExprStatement *y = (ExprStatement *)x.ptr();
        convertClosure(y->expr, env, nonLocalEnv, closureDataName, freeVars);
        break;
    }

    case WHILE : {
        While *y = (While *)x.ptr();
        convertClosure(y->condition, env, nonLocalEnv,
                       closureDataName, freeVars);
        convertClosure(y->body, env, nonLocalEnv, closureDataName, freeVars);
        break;
    }

    case BREAK :
    case CONTINUE : {
        break;
    }

    case FOR : {
        For *y = (For *)x.ptr();
        convertClosure(y->expr, env, nonLocalEnv, closureDataName, freeVars);
        EnvPtr env2 = new Env(env);
        addLocal(env2, y->variable, y->variable.ptr());
        convertClosure(y->body, env2, nonLocalEnv, closureDataName, freeVars);
        break;
    }

    default :
        assert(false);

    }
}

void convertClosure(ExprPtr &x, EnvPtr env, EnvPtr nonLocalEnv,
                    const string &closureDataName,
                    vector<string> &freeVars)
{
    switch (x->objKind) {

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
        ObjectPtr z = lookupEnvEx(env, y->name, nonLocalEnv,
                                  isNonLocal, isGlobal);
        if (isNonLocal && !isGlobal) {
            if ((z->objKind == PVALUE) || (z->objKind == CVALUE)) {
                vector<string>::iterator i =
                    std::find(freeVars.begin(), freeVars.end(), y->name->str);
                if (i == freeVars.end()) {
                    freeVars.push_back(y->name->str);
                }
                IdentifierPtr a = new Identifier(closureDataName);
                a->location = y->location;
                NameRefPtr b = new NameRef(a);
                b->location = y->location;
                FieldRefPtr c = new FieldRef(b.ptr(), y->name);
                c->location = y->location;
                x = c.ptr();
            }
        }
        break;
    }

    case RETURNED :
        break;

    case TUPLE : {
        Tuple *y = (Tuple *)x.ptr();
        for (unsigned i = 0; i < y->args.size(); ++i)
            convertClosure(y->args[i], env, nonLocalEnv,
                           closureDataName, freeVars);
        break;
    }

    case ARRAY : {
        Array *y = (Array *)x.ptr();
        for (unsigned i = 0; i < y->args.size(); ++i)
            convertClosure(y->args[i], env, nonLocalEnv,
                           closureDataName, freeVars);
        break;
    }

    case INDEXING : {
        Indexing *y = (Indexing *)x.ptr();
        convertClosure(y->expr, env, nonLocalEnv, closureDataName, freeVars);
        for (unsigned i = 0; i < y->args.size(); ++i)
            convertClosure(y->args[i], env, nonLocalEnv,
                           closureDataName, freeVars);
        break;
    }

    case CALL : {
        Call *y = (Call *)x.ptr();
        convertClosure(y->expr, env, nonLocalEnv, closureDataName, freeVars);
        for (unsigned i = 0; i < y->args.size(); ++i)
            convertClosure(y->args[i], env, nonLocalEnv,
                           closureDataName, freeVars);
        break;
    }

    case FIELD_REF : {
        FieldRef *y = (FieldRef *)x.ptr();
        convertClosure(y->expr, env, nonLocalEnv, closureDataName, freeVars);
        break;
    }

    case TUPLE_REF : {
        TupleRef *y = (TupleRef *)x.ptr();
        convertClosure(y->expr, env, nonLocalEnv, closureDataName, freeVars);
        break;
    }

    case UNARY_OP : {
        UnaryOp *y = (UnaryOp *)x.ptr();
        convertClosure(y->expr, env, nonLocalEnv, closureDataName, freeVars);
        break;
    }

    case BINARY_OP : {
        BinaryOp *y = (BinaryOp *)x.ptr();
        convertClosure(y->expr1, env, nonLocalEnv, closureDataName, freeVars);
        convertClosure(y->expr2, env, nonLocalEnv, closureDataName, freeVars);
        break;
    }

    case AND : {
        And *y = (And *)x.ptr();
        convertClosure(y->expr1, env, nonLocalEnv, closureDataName, freeVars);
        convertClosure(y->expr2, env, nonLocalEnv, closureDataName, freeVars);
        break;
    }

    case OR : {
        Or *y = (Or *)x.ptr();
        convertClosure(y->expr1, env, nonLocalEnv, closureDataName, freeVars);
        convertClosure(y->expr2, env, nonLocalEnv, closureDataName, freeVars);
        break;
    }

    case LAMBDA : {
        Lambda *y = (Lambda *)x.ptr();
        EnvPtr env2 = new Env(env);
        for (unsigned i = 0; i < y->formalArgs.size(); ++i)
            addLocal(env2, y->formalArgs[i], y->formalArgs[i].ptr());
        convertClosure(y->body, env2, nonLocalEnv, closureDataName, freeVars);
        break;
    }

    case SC_EXPR :
    case OBJECT_EXPR :
    case CVALUE_EXPR :
        break;

    default :
        assert(false);

    }
}
