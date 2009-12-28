#ifndef __CLAY_INVOKETABLE_HPP
#define __CLAY_INVOKETABLE_HPP

template <typename ARG>
static inline
ValuePtr evaluateArg(ARG arg) {
    return arg->evaluate();
}

template <>
static inline
ValuePtr evaluateArg<ValuePtr>(ValuePtr arg) {
    return arg;
}

template <typename ARG>
int hashArgs(const vector<ARG> &args,
             const vector<bool> &isStaticFlags) {
    int h = 0;
    for (unsigned i = 0; i < isStaticFlags.size(); ++i) {
        if (!isStaticFlags[i]) {
            h += toCOIndex(args[i]->type.raw());
        }
        else {
            ValuePtr v = evaluateArg(args[i]);
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
            if (args[i]->type != (Type *)entry->argsInfo[i].raw())
                return false;
        }
        else {
            ValuePtr v = evaluateArg(args[i]);
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
            entry->argsInfo.push_back(args[i]->type.raw());
        }
        else {
            ValuePtr v = evaluateArg(args[i]);
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

#endif
