#include "clay.hpp"
#include "invokeutil2.hpp"



//
// ArgList
//

ArgList::ArgList(const vector<ExprPtr> &exprs, EnvPtr env)
    : Object(DONT_CARE), exprs(exprs), env(env), allStatic(true),
      recursionError(false)
{
    for (unsigned i = 0; i < exprs.size(); ++i) {
        PValuePtr pv = partialEval(exprs[i], env);
        if (!pv)
            recursionError = true;
        else if (!pv->isStatic)
            allStatic = false;
        this->_pvalues.push_back(pv);
    }
    this->_values.resize(exprs.size());
}

TypePtr ArgList::type(int i)
{
    return this->_pvalues[i]->type;
}

bool ArgList::isTemp(int i )
{
    return this->_pvalues[i]->isTemp;
}

PValuePtr ArgList::pvalue(int i)
{
    return this->_pvalues[i];
}

ValuePtr ArgList::value(int i)
{
    if (!this->_values[i])
        this->_values[i] = evaluateNonVoid(this->exprs[i], this->env);
    return this->_values[i];
}

TypePtr ArgList::typeValue(int i)
{
    return valueToType(this->value(i));
}

void ArgList::ensureArity(int n)
{
    if ((int)this->exprs.size() != n)
        error("incorrect no. of arguments");
}

void ArgList::ensureArity2(int n, bool hasVarArgs)
{
    if (!hasVarArgs) 
        ensureArity(n);
    else if ((int)this->exprs.size() < n)
        error("incorrect no of arguments");
}

bool ArgList::unifyFormalArg(int i, FormalArgPtr farg, EnvPtr fenv)
{
    switch (farg->objKind) {
    case VALUE_ARG : {
        ValueArg *x = (ValueArg *)farg.ptr();
        if (!x->type)
            return true;
        PatternPtr pattern = evaluatePattern(x->type, fenv);
        return unifyType(pattern, type(i));
    }
    case STATIC_ARG : {
        StaticArg *x = (StaticArg *)farg.ptr();
        PatternPtr pattern = evaluatePattern(x->pattern, fenv);
        return unify(pattern, this->value(i));
    }
    default :
        assert(false);
        return false;
    }
}

bool
ArgList::unifyFormalArgs(const vector<FormalArgPtr> &fargs, EnvPtr fenv)
{
    if (this->size() != fargs.size())
        return false;
    for (unsigned i = 0; i < fargs.size(); ++i) {
        if (!this->unifyFormalArg(i, fargs[i], fenv))
            return false;
    }
    return true;
}

void 
ArgList::ensureUnifyFormalArgs(const vector<FormalArgPtr> &fargs,
                               EnvPtr fenv)
{
    ensureArity(fargs.size());
    for (unsigned i = 0; i < fargs.size(); ++i) {
        if (!this->unifyFormalArg(i, fargs[i], fenv))
            error(this->exprs[i], "non-matching argument");
    }
}

ArgListPtr ArgList::removeStaticArgs(const vector<FormalArgPtr> &fargs)
{
    assert(fargs.size() == this->size());
    ensureArity(fargs.size());

    ArgListPtr args = new ArgList(vector<ExprPtr>(), this->env);
    args->allStatic = this->allStatic;
    args->recursionError = this->recursionError;

    for (unsigned i = 0; i < fargs.size(); ++i) {
        if (fargs[i]->objKind != STATIC_ARG) {
            args->exprs.push_back(this->exprs[i]);
            args->_pvalues.push_back(this->_pvalues[i]);
            args->_values.push_back(this->_values[i]);
        }
    }

    return args;
}

bool
ArgList::unifyRecordField(int i, RecordFieldPtr field, EnvPtr env)
{
    PatternPtr pattern = evaluatePattern(field->type, env);
    return unifyType(pattern, type(i));
}

bool
ArgList::unifyRecordFields(const vector<RecordFieldPtr> &fields, EnvPtr env)
{
    if (this->size() != fields.size())
        return false;
    for (unsigned i = 0; i < fields.size(); ++i) {
        if (!this->unifyRecordField(i, fields[i], env))
            return false;
    }
    return true;
}

void
ArgList::ensureUnifyRecordFields(const vector<RecordFieldPtr> &fields,
                                 EnvPtr env)
{
    ensureArity(fields.size());
    for (unsigned i = 0; i < fields.size(); ++i) {
        if (!this->unifyRecordField(i, fields[i], env))
            error(this->exprs[i], "non-matching argument");
    }
}



//
// invoke table procs
//

int hashArgs(ArgListPtr args, const vector<bool> &isStaticFlags)
{
    int h = 0;
    for (unsigned i = 0; i < isStaticFlags.size(); ++i) {
        if (!isStaticFlags[i])
            h += toCOIndex(args->type(i).ptr());
        else
            h += valueHash(args->value(i));
    }
    return h;
}

bool matchingArgs(ArgListPtr args, InvokeTableEntryPtr entry)
{
    const vector<bool> &isStaticFlags = entry->table->isStaticFlags;
    const vector<ObjectPtr> &argsInfo = entry->argsInfo;
    for (unsigned i = 0; i < isStaticFlags.size(); ++i) {
        if (!isStaticFlags[i]) {
            TypePtr t = (Type *)argsInfo[i].ptr();
            if (args->type(i) != t)
                return false;
        }
        else {
            ValuePtr v = args->value(i);
            if (!valueEquals(v, (Value *)argsInfo[i].ptr()))
                return false;
        }
    }
    return true;
}

void initArgsInfo(InvokeTableEntryPtr entry, ArgListPtr args)
{
    const vector<bool> &isStaticFlags = entry->table->isStaticFlags;
    vector<ObjectPtr> &argsInfo = entry->argsInfo;
    for (unsigned i = 0; i < isStaticFlags.size(); ++i) {
        if (!isStaticFlags[i])
            argsInfo.push_back(args->type(i).ptr());
        else
            argsInfo.push_back(cloneValue(args->value(i)).ptr());
    }
}

InvokeTableEntryPtr findMatchingEntry(InvokeTablePtr table, ArgListPtr args)
{
    int h = hashArgs(args, table->isStaticFlags);
    h &= (table->data.size() - 1);
    vector<InvokeTableEntryPtr> &entries = table->data[h];
    for (unsigned i = 0; i < entries.size(); ++i) {
        if (matchingArgs(args, entries[i]))
            return entries[i];
    }
    InvokeTableEntryPtr entry = new InvokeTableEntry();
    entry->table = table;
    entries.push_back(entry);
    initArgsInfo(entry, args);
    return entry;
}

InvokeTablePtr newInvokeTable(const vector<FormalArgPtr> &formalArgs)
{
    InvokeTablePtr table = new InvokeTable();
    for (unsigned i = 0; i < formalArgs.size(); ++i) {
        bool isStatic = formalArgs[i]->objKind == STATIC_ARG;
        table->isStaticFlags.push_back(isStatic);
    }
    table->data.resize(64);
    return table;
}



//
// procedure invoke table
//

void initProcedureInvokeTable(ProcedurePtr x)
{
    x->invokeTable = newInvokeTable(x->code->formalArgs);
}

InvokeTableEntryPtr
lookupProcedureInvoke(ProcedurePtr x, ArgListPtr args)
{
    InvokeTablePtr table = x->invokeTable;
    if (!table) {
        initProcedureInvokeTable(x);
        table = x->invokeTable;
    }
    args->ensureArity(x->code->formalArgs.size());
    return findMatchingEntry(table, args);
}

void
initProcedureEnv(ProcedurePtr x,
                 ArgListPtr args,
                 InvokeTableEntryPtr entry)
{
    CodePtr code = x->code;
    vector<PatternCellPtr> cells;
    EnvPtr patternEnv = initPatternVars(x->env, code->patternVars, cells);
    args->ensureUnifyFormalArgs(code->formalArgs, patternEnv);
    EnvPtr env = bindPatternVars(x->env, code->patternVars, cells);
    if (code->predicate.ptr()) {
        bool result = evaluateToBool(code->predicate, env);
        if (!result)
            error(code->predicate, "procedure predicate failed");
    }
    entry->env = env;
    entry->code = code;
}



//
// overloadable invoke table
//

void initOverloadableInvokeTables(OverloadablePtr x)
{
    for (unsigned i = x->overloads.size(); i > 0; --i) {
        const vector<FormalArgPtr> &formalArgs =
            x->overloads[i-1]->code->formalArgs;
        unsigned nArgs = formalArgs.size();
        if (x->invokeTables.size() <= nArgs)
            x->invokeTables.resize(nArgs + 1);
        if (!x->invokeTables[nArgs]) {
            x->invokeTables[nArgs] = newInvokeTable(formalArgs);
        }
        else {
            InvokeTablePtr table = x->invokeTables[nArgs];
            for (unsigned j = 0; j < formalArgs.size(); ++j) {
                if (table->isStaticFlags[j]) {
                    if (formalArgs[j]->objKind != STATIC_ARG)
                        error(formalArgs[j], "expecting static argument");
                }
                else {
                    if (formalArgs[j]->objKind == STATIC_ARG)
                        error(formalArgs[j], "expecting non static argument");
                }
            }
        }
    }
}

InvokeTableEntryPtr
lookupOverloadableInvoke(OverloadablePtr x, ArgListPtr args)
{
    if (x->invokeTables.empty())
        initOverloadableInvokeTables(x);
    if (x->invokeTables.size() <= args->size())
        error("no matching overload");
    InvokeTablePtr table = x->invokeTables[args->size()];
    if (!table)
        error("no matching overload");
    return findMatchingEntry(table, args);
}

void
initOverloadableEnv(OverloadablePtr x,
                    ArgListPtr args,
                    InvokeTableEntryPtr entry)
{
    for (unsigned i = 0; i < x->overloads.size(); ++i) {
        OverloadPtr y = x->overloads[i];
        CodePtr code = y->code;
        vector<PatternCellPtr> cells;
        EnvPtr patternEnv = initPatternVars(y->env, code->patternVars, cells);
        if (!args->unifyFormalArgs(code->formalArgs, patternEnv))
            continue;
        EnvPtr env = bindPatternVars(y->env, code->patternVars, cells);
        if (code->predicate.ptr()) {
            bool result = evaluateToBool(code->predicate, env);
            if (!result)
                continue;
        }
        entry->env = env;
        entry->code = code;
        return;
    }
    error("no matching overload");
}
