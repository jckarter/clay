#ifndef __CLAY_INVOKETABLE_HPP
#define __CLAY_INVOKETABLE_HPP



//
// argumentValue, argumentType, argumentRef
//

template <typename ARG>
static inline
ValuePtr argumentValue(ARG arg) {
    return arg->evaluate();
}

template <>
static inline
ValuePtr argumentValue<ValuePtr>(ValuePtr arg) {
    return arg;
}

template <typename ARG>
static inline
TypePtr argumentType(ARG arg) {
    return arg->type;
}

template <typename ARG>
static inline
ARG argumentRef(ARG arg) {
    return arg->asRef(); // dummy
}

template <>
static inline
ValuePtr argumentRef<ValuePtr>(ValuePtr arg) {
    if (arg->isOwned)
        return new Value(arg->type, arg->buf, false);
    return arg;
}

template <>
static inline
AnalysisPtr argumentRef<AnalysisPtr>(AnalysisPtr arg) {
    return new Analysis(arg->type, false, false);
}



//
// invoke table procs
//

template <typename ARG>
int hashArgs(const vector<ARG> &args,
             const vector<bool> &isStaticFlags) {
    int h = 0;
    for (unsigned i = 0; i < isStaticFlags.size(); ++i) {
        if (!isStaticFlags[i]) {
            h += toCOIndex(argumentType(args[i]).raw());
        }
        else {
            ValuePtr v = argumentValue(args[i]);
            h += valueHash(v);
        }
    }
    return h;
}

template <typename ARG>
bool matchArgs(const vector<ARG> &args,
               const vector<bool> &isStaticFlags,
               InvokeTableEntry *entry) {
    for (unsigned i = 0; i < isStaticFlags.size(); ++i) {
        if (!isStaticFlags[i]) {
            Type *t = (Type *)entry->argsInfo[i].raw();
            if (argumentType(args[i]) != t)
                return false;
        }
        else {
            ValuePtr v = argumentValue(args[i]);
            if (!valueEquals(v, (Value *)entry->argsInfo[i].raw()))
                return false;
        }
    }
    return true;
}

template <typename ARG>
void initArgsInfo(InvokeTableEntry *entry,
                  const vector<ARG> &args,
                  const vector<bool> &isStaticFlags) {
    for (unsigned i = 0; i < isStaticFlags.size(); ++i) {
        if (!isStaticFlags[i]) {
            entry->argsInfo.push_back(argumentType(args[i]).raw());
        }
        else {
            ValuePtr v = argumentValue(args[i]);
            entry->argsInfo.push_back(cloneValue(v).raw());
        }
    }
}

template <typename ARG>
InvokeTableEntry *findMatchingEntry(const vector<ARG> &args,
                                    InvokeTable *table) {
    const vector<bool> &isStaticFlags = table->isStaticFlags;
    int h = hashArgs(args, isStaticFlags);
    h &= (table->data.size() - 1);
    vector<InvokeTableEntryPtr> &entries = table->data[h];
    for (unsigned i = 0; i < entries.size(); ++i) {
        InvokeTableEntry *entry = entries[i].raw();
        if (matchArgs(args, isStaticFlags, entry))
            return entry;
    }
    InvokeTableEntryPtr entry = new InvokeTableEntry();
    entries.push_back(entry);
    initArgsInfo(entry.raw(), args, isStaticFlags);
    return entry.raw();
}

static inline
InvokeTablePtr emptyInvokeTable(const vector<FormalArgPtr> &formalArgs) {
    InvokeTablePtr table = new InvokeTable();
    for (unsigned i = 0; i < formalArgs.size(); ++i) {
        bool isStatic = formalArgs[i]->objKind == STATIC_ARG;
        table->isStaticFlags.push_back(isStatic);
    }
    table->data.resize(64);
    return table;
}



//
// lookupProcedureInvoke
//
                       
static inline
void initProcedureInvokeTable(ProcedurePtr x) {
    const vector<FormalArgPtr> &formalArgs = x->code->formalArgs;
    x->invokeTable = emptyInvokeTable(formalArgs);
}

template <typename ARG>
InvokeTableEntry *lookupProcedureInvoke(ProcedurePtr x,
                                        const vector<ARG> &args) {
    InvokeTable *table = x->invokeTable.raw();
    if (!table) {
        initProcedureInvokeTable(x);
        table = x->invokeTable.raw();
    }
    ensureArity(args, table->isStaticFlags.size());
    return findMatchingEntry(args, table);
}



//
// lookupOverloadableInvoke
//

static inline
void initOverloadableInvokeTables(OverloadablePtr x) {
    for (unsigned i = 0; i < x->overloads.size(); ++i) {
        const vector<FormalArgPtr> &formalArgs =
            x->overloads[i]->code->formalArgs;
        unsigned nArgs = formalArgs.size();
        if (x->invokeTables.size() <= nArgs)
            x->invokeTables.resize(nArgs+1);
        if (!x->invokeTables[nArgs]) {
            x->invokeTables[nArgs] = emptyInvokeTable(formalArgs);
        }
        else {
            InvokeTablePtr table = x->invokeTables[nArgs];
            for (unsigned j = 0; j < formalArgs.size(); ++j) {
                if (!table->isStaticFlags[j]) {
                    if (formalArgs[j]->objKind == STATIC_ARG)
                        error(formalArgs[j], "expecting non static argument");
                }
                else {
                    if (formalArgs[j]->objKind != STATIC_ARG)
                        error(formalArgs[j], "expecting static argument");
                }
            }
        }
    }
}

template <typename ARG>
InvokeTableEntry *lookupOverloadableInvoke(OverloadablePtr x,
                                           const vector<ARG> &args) {
    if (x->invokeTables.empty())
        initOverloadableInvokeTables(x);
    if (x->invokeTables.size() <= args.size())
        error("no matching overload");
    InvokeTable *table = x->invokeTables[args.size()].raw();
    if (!table)
        error("no matching overload");
    return findMatchingEntry(args, table);
}



//
// MatchInvokeResult
//

enum MatchInvokeResultKind {
    MATCH_INVOKE_SUCCESS,
    MATCH_INVOKE_ARG_COUNT_ERROR,
    MATCH_INVOKE_ARG_MISMATCH,
    MATCH_INVOKE_PREDICATE_FAILURE,
};

struct MatchInvokeResult : public Object {
    int resultKind;
    MatchInvokeResult(int resultKind)
        : Object(DONT_CARE), resultKind(resultKind) {}
};

struct MatchInvokeSuccess : public MatchInvokeResult {
    EnvPtr env;
    MatchInvokeSuccess(EnvPtr env)
        : MatchInvokeResult(MATCH_INVOKE_SUCCESS), env(env) {}
};

struct MatchInvokeArgCountError : public MatchInvokeResult {
    MatchInvokeArgCountError()
        : MatchInvokeResult(MATCH_INVOKE_ARG_COUNT_ERROR) {}
};

struct MatchInvokeArgMismatch : public MatchInvokeResult {
    int pos;
    MatchInvokeArgMismatch(int pos)
        : MatchInvokeResult(MATCH_INVOKE_ARG_MISMATCH), pos(pos) {}
};

struct MatchInvokePredicateFailure : public MatchInvokeResult {
    MatchInvokePredicateFailure()
        : MatchInvokeResult(MATCH_INVOKE_PREDICATE_FAILURE) {}
};

static inline
void signalMatchInvokeError(MatchInvokeResultPtr result) {
    switch (result->resultKind) {
    case MATCH_INVOKE_SUCCESS :
        assert(false);
        break;
    case MATCH_INVOKE_ARG_COUNT_ERROR :
        error("incorrect no. of arguments");
        break;
    case MATCH_INVOKE_ARG_MISMATCH : {
        MatchInvokeArgMismatch *x = (MatchInvokeArgMismatch *)result.raw();
        fmtError("mismatch at argument %d", (x->pos + 1));
        break;
    }
    case MATCH_INVOKE_PREDICATE_FAILURE :
        error("predicate failure");
        break;
    }
}



//
// matchInvoke, matchFormalArg
//

template <typename ARG>
MatchInvokeResultPtr
matchInvoke(CodePtr code, EnvPtr env, const vector<ARG> &args) {
    if (args.size() != code->formalArgs.size())
        return new MatchInvokeArgCountError();
    EnvPtr patternEnv = new Env(env);
    vector<PatternCellPtr> cells;
    for (unsigned i = 0; i < code->patternVars.size(); ++i) {
        cells.push_back(new PatternCell(code->patternVars[i], NULL));
        addLocal(patternEnv, code->patternVars[i], cells[i].raw());
    }
    for (unsigned i = 0; i < args.size(); ++i) {
        if (!matchFormalArg(args[i], code->formalArgs[i], patternEnv))
            return new MatchInvokeArgMismatch(i);
    }
    EnvPtr env2 = new Env(env);
    for (unsigned i = 0; i < cells.size(); ++i) {
        ValuePtr v = derefCell(cells[i]);
        addLocal(env2, code->patternVars[i], v.raw());
    }
    if (code->predicate.raw()) {
        bool result = evaluateToBool(code->predicate, env2);
        if (!result)
            return new MatchInvokePredicateFailure();
    }
    return new MatchInvokeSuccess(env2);
}

template <typename ARG>
bool matchFormalArg(ARG arg, FormalArgPtr farg, EnvPtr env) {
    switch (farg->objKind) {
    case VALUE_ARG : {
        ValueArg *x = (ValueArg *)farg.raw();
        if (!x->type)
            return true;
        PatternPtr pattern = evaluatePattern(x->type, env);
        return unifyType(pattern, argumentType(arg));
    }
    case STATIC_ARG : {
        StaticArg *x = (StaticArg *)farg.raw();
        PatternPtr pattern = evaluatePattern(x->pattern, env);
        return unify(pattern, argumentValue(arg));
    }
    }
    assert(false);
    return false;
}



//
// bindValueArgs
//

template <typename ARG>
EnvPtr bindValueArgs(EnvPtr env, const vector<ARG> &args, CodePtr code) {
    EnvPtr env2 = new Env(env);
    for (unsigned i = 0; i < args.size(); ++i) {
        FormalArgPtr farg = code->formalArgs[i];
        if (farg->objKind == VALUE_ARG) {
            ValueArg *x = (ValueArg *)farg.raw();
            addLocal(env2, x->name, argumentRef(args[i]).raw());
        }
    }
    return env2;
}


#endif
