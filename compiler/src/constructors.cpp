#include "clay.hpp"

vector<OverloadPtr> typeOverloads;

void addTypeOverload(OverloadPtr x)
{
    typeOverloads.insert(typeOverloads.begin(), x);
}

void initTypeOverloads(TypePtr t)
{
    assert(!t->overloadsInitialized);

    for (unsigned i = 0; i < typeOverloads.size(); ++i) {
        OverloadPtr x = typeOverloads[i];
        EnvPtr env = new Env(x->env);
        const vector<IdentifierPtr> &pvars = x->code->patternVars;
        for (unsigned i = 0; i < pvars.size(); ++i) {
            PatternCellPtr cell = new PatternCell(pvars[i], NULL);
            addLocal(env, pvars[i], cell.ptr());
        }
        PatternPtr pattern = evaluatePattern(x->target, env);
        if (unify(pattern, t.ptr()))
            t->overloads.push_back(x);
    }

    switch (t->typeKind) {
    case ARRAY_TYPE :
        initBuiltinConstructor((ArrayType *)t.ptr());
        break;
    case TUPLE_TYPE :
        initBuiltinConstructor((TupleType *)t.ptr());
        break;
    case RECORD_TYPE :
        initBuiltinConstructor((RecordType *)t.ptr());
        break;
    }

    t->overloadsInitialized = true;
}

void initBuiltinConstructor(ArrayTypePtr t)
{
    CodePtr code = new Code();
    unsigned size = t->size;
    vector<IdentifierPtr> argNames;
    for (unsigned i = 0; i < size; ++i) {
        ostringstream sout;
        sout << "arg" << i;
        IdentifierPtr argName = new Identifier(sout.str());
        argNames.push_back(argName);
        ExprPtr type = new ObjectExpr(t->elementType.ptr());
        ValueArgPtr arg = new ValueArg(argName, type);
        code->formalArgs.push_back(arg.ptr());
    }
    code->returnType = new ObjectExpr(t.ptr());
    code->returnRef = false;

    BlockPtr body = new Block();
    for (unsigned i = 0; i < size; ++i) {
        IndexingPtr left = new Indexing(new Returned());
        ExprPtr indexExpr = new ObjectExpr(sizeTToValueHolder(i).ptr());
        left->args.push_back(indexExpr);
        ExprPtr right = new NameRef(argNames[i]);
        StatementPtr stmt = new InitAssignment(left.ptr(), right);
        body->statements.push_back(stmt);
    }
    code->body = body.ptr();

    ExprPtr returnTypeExpr = new ObjectExpr(t.ptr());
    OverloadPtr overload = new Overload(returnTypeExpr, code, true);
    overload->env = new Env();
    t->overloads.push_back(overload);
}

void initBuiltinConstructor(TupleTypePtr t)
{
    CodePtr code = new Code();
    const vector<TypePtr> &elementTypes = t->elementTypes;
    vector<IdentifierPtr> argNames;
    for (unsigned i = 0; i < elementTypes.size(); ++i) {
        ostringstream sout;
        sout << "arg" << i;
        IdentifierPtr argName = new Identifier(sout.str());
        argNames.push_back(argName);
        ExprPtr type = new ObjectExpr(elementTypes[i].ptr());
        ValueArgPtr arg = new ValueArg(argName, type);
        code->formalArgs.push_back(arg.ptr());
    }
    code->returnType = new ObjectExpr(t.ptr());
    code->returnRef = false;

    BlockPtr body = new Block();
    for (unsigned i = 0; i < elementTypes.size(); ++i) {
        ExprPtr left = new TupleRef(new Returned(), i);
        ExprPtr right = new NameRef(argNames[i]);
        StatementPtr stmt = new InitAssignment(left, right);
        body->statements.push_back(stmt);
    }
    code->body = body.ptr();

    ExprPtr returnTypeExpr = new ObjectExpr(t.ptr());
    OverloadPtr overload = new Overload(returnTypeExpr, code, true);
    overload->env = new Env();
    t->overloads.push_back(overload);
}

void initBuiltinConstructor(RecordTypePtr t)
{
    CodePtr code = new Code();
    const vector<TypePtr> &fieldTypes = recordFieldTypes(t);
    vector<IdentifierPtr> argNames;
    for (unsigned i = 0; i < fieldTypes.size(); ++i) {
        ostringstream sout;
        sout << "arg" << i;
        IdentifierPtr argName = new Identifier(sout.str());
        argNames.push_back(argName);
        ExprPtr type = new ObjectExpr(fieldTypes[i].ptr());
        ValueArgPtr arg = new ValueArg(argName, type);
        code->formalArgs.push_back(arg.ptr());
    }
    code->returnType = new ObjectExpr(t.ptr());
    code->returnRef = false;

    BlockPtr body = new Block();
    for (unsigned i = 0; i < fieldTypes.size(); ++i) {
        CallPtr left = new Call(primNameRef("recordFieldRef"));
        left->args.push_back(new Returned());
        ExprPtr indexExpr = new ObjectExpr(sizeTToValueHolder(i).ptr());
        left->args.push_back(indexExpr);
        ExprPtr right = new NameRef(argNames[i]);
        StatementPtr stmt = new InitAssignment(left.ptr(), right);
        body->statements.push_back(stmt);
    }
    code->body = body.ptr();

    ExprPtr returnTypeExpr = new ObjectExpr(t.ptr());
    OverloadPtr overload = new Overload(returnTypeExpr, code, false);
    overload->env = new Env();
    t->overloads.push_back(overload);
}

void initBuiltinConstructor(RecordPtr x)
{
    assert(!(x->builtinOverloadInitialized));
    x->builtinOverloadInitialized = true;

    assert(x->patternVars.size() > 0);

    ExprPtr recName = new NameRef(x->name);
    recName->location = x->name->location;

    CodePtr code = new Code();
    code->location = x->location;
    code->patternVars = x->patternVars;

    for (unsigned i = 0; i < x->fields.size(); ++i) {
        RecordFieldPtr f = x->fields[i];
        ValueArgPtr arg = new ValueArg(f->name, f->type);
        arg->location = f->location;
        code->formalArgs.push_back(arg.ptr());
    }

    IndexingPtr retType = new Indexing(recName);
    retType->location = x->location;
    for (unsigned i = 0; i < x->patternVars.size(); ++i) {
        ExprPtr typeArg = new NameRef(x->patternVars[i]);
        typeArg->location = x->patternVars[i]->location;
        retType->args.push_back(typeArg);
    }

    CallPtr returnExpr = new Call(retType.ptr());
    returnExpr->location = x->location;
    for (unsigned i = 0; i < x->fields.size(); ++i) {
        ExprPtr callArg = new NameRef(x->fields[i]->name);
        callArg->location = x->fields[i]->location;
        returnExpr->args.push_back(callArg);
    }

    code->body = new Return(returnExpr.ptr());
    code->body->location = returnExpr->location;

    OverloadPtr defaultOverload = new Overload(recName, code, false);
    defaultOverload->location = x->location;
    defaultOverload->env = x->env;
    x->overloads.push_back(defaultOverload);
}
