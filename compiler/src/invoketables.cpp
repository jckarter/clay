#include "clay.hpp"



//
// isStaticFlags
//

static map<FlagsMapKey, FlagsMapEntry> flagsMap;

FlagsMapEntry &lookupFlagsMapEntry(ObjectPtr callable, unsigned nArgs)
{
    return flagsMap[make_pair(callable, nArgs)];
}

void initIsStaticFlags(ProcedurePtr x)
{
    assert(!x->staticFlagsInitialized);
    const vector<FormalArgPtr> &formalArgs = x->code->formalArgs;
    FlagsMapEntry &entry = lookupFlagsMapEntry(x.ptr(), formalArgs.size());
    assert(!entry.initialized);
    for (unsigned i = 0; i < formalArgs.size(); ++i) {
        switch (formalArgs[i]->objKind) {
        case VALUE_ARG :
            entry.isStaticFlags.push_back(false);
            break;
        case STATIC_ARG :
            entry.isStaticFlags.push_back(true);
            break;
        default :
            assert(false);
        }
    }
    entry.initialized = true;
    x->staticFlagsInitialized = true;
}

void updateIsStaticFlags(ObjectPtr callable, OverloadPtr overload)
{
    const vector<FormalArgPtr> &formalArgs = overload->code->formalArgs;
    FlagsMapEntry &entry = lookupFlagsMapEntry(callable, formalArgs.size());
    if (!entry.initialized) {
        for (unsigned j = 0; j < formalArgs.size(); ++j) {
            switch (formalArgs[j]->objKind) {
            case VALUE_ARG :
                entry.isStaticFlags.push_back(false);
                break;
            case STATIC_ARG :
                entry.isStaticFlags.push_back(true);
                break;
            default :
                assert(false);
            }
        }
        entry.initialized = true;
    }
    else {
        for (unsigned j = 0; j < formalArgs.size(); ++j) {
            switch(formalArgs[j]->objKind) {
            case VALUE_ARG :
                if (entry.isStaticFlags[j])
                    error(formalArgs[j], "expecting static argument");
                break;
            case STATIC_ARG :
                if (!entry.isStaticFlags[j])
                    error(formalArgs[j], "expecting non-static argument");
                break;
            default :
                assert(false);
            }
        }
    }
}

void initIsStaticFlags(ObjectPtr callable,
                       const vector<OverloadPtr> &overloads)
{
    for (unsigned i = 0; i < overloads.size(); ++i)
        updateIsStaticFlags(callable, overloads[i]);
}

void initIsStaticFlags(OverloadablePtr x)
{
    assert(!x->staticFlagsInitialized);
    initIsStaticFlags(x.ptr(), x->overloads);
    x->staticFlagsInitialized = true;
}

void initIsStaticFlags(TypePtr x)
{
    assert(!x->staticFlagsInitialized);
    initBuiltinIsStaticFlags(x);
    initIsStaticFlags(x.ptr(), x->overloads);
    x->staticFlagsInitialized = true;
}

void initIsStaticFlags(RecordPtr x)
{
    assert(!x->staticFlagsInitialized);
    initBuiltinIsStaticFlags(x);
    initIsStaticFlags(x.ptr(), x->overloads);
    x->staticFlagsInitialized = true;
}

const vector<bool> &lookupIsStaticFlags(ObjectPtr callable, unsigned nArgs)
{
    switch (callable->objKind) {
    case PROCEDURE : {
        Procedure *x = (Procedure *)callable.ptr();
        if (!x->staticFlagsInitialized)
            initIsStaticFlags(x);
        break;
    }
    case OVERLOADABLE : {
        Overloadable *x = (Overloadable *)callable.ptr();
        if (!x->staticFlagsInitialized)
            initIsStaticFlags(x);
        break;
    }
    case TYPE : {
        Type *x = (Type *)callable.ptr();
        if (!x->overloadsInitialized)
            initTypeOverloads(x);
        if (!x->staticFlagsInitialized)
            initIsStaticFlags(x);
        break;
    }
    case RECORD : {
        Record *x = (Record *)callable.ptr();
        if (!x->builtinOverloadInitialized)
            initBuiltinConstructor(x);
        if (!x->staticFlagsInitialized)
            initIsStaticFlags(x);
        break;
    }
    default :
        assert(false);
    }

    FlagsMapEntry &entry = lookupFlagsMapEntry(callable, nArgs);
    if (!entry.initialized)
        error("incorrect no. of arguments");
    return entry.isStaticFlags;
}

bool computeArgsKey(const vector<bool> &isStaticFlags,
                    const vector<ExprPtr> &args,
                    EnvPtr env,
                    vector<ObjectPtr> &argsKey,
                    vector<LocationPtr> &argLocations)
{
    for (unsigned i = 0; i < args.size(); ++i) {
        if (isStaticFlags[i]) {
            ObjectPtr v = evaluateStatic(args[i], env);
            argsKey.push_back(v);
        }
        else {
            PValuePtr pv = analyzeValue(args[i], env);
            if (!pv)
                return false;
            argsKey.push_back(pv->type.ptr());
        }
        argLocations.push_back(args[i]->location);
    }
    return true;
}



//
// invoke tables
//

static bool invokeTableInitialized = false;
static vector< vector<InvokeEntryPtr> > invokeTable;
static vector< vector<StaticInvokeEntryPtr> > staticInvokeTable;

static void initInvokeTable() {
    assert(!invokeTableInitialized);
    invokeTable.resize(16384);
    staticInvokeTable.resize(8192);
    invokeTableInitialized = true;
}

InvokeEntryPtr lookupInvoke(ObjectPtr callable,
                            const vector<bool> &isStaticFlags,
                            const vector<ObjectPtr> &argsKey)
{
    if (!invokeTableInitialized)
        initInvokeTable();
    int h = objectHash(callable) + objectVectorHash(argsKey);
    h &= (invokeTable.size() - 1);
    vector<InvokeEntryPtr> &bucket = invokeTable[h];
    for (unsigned i = 0; i < bucket.size(); ++i) {
        InvokeEntryPtr entry = bucket[i];
        if (objectEquals(entry->callable, callable) &&
            objectVectorEquals(entry->argsKey, argsKey))
        {
            return entry;
        }
    }
    InvokeEntryPtr entry = new InvokeEntry(callable, isStaticFlags, argsKey);
    bucket.push_back(entry);
    return entry;
}

StaticInvokeEntryPtr lookupStaticInvoke(ObjectPtr callable,
                                        const vector<ObjectPtr> &args)
{
    if (!invokeTableInitialized)
        initInvokeTable();
    int h = objectHash(callable) + objectVectorHash(args);
    h &= (staticInvokeTable.size() - 1);
    vector<StaticInvokeEntryPtr> &bucket = staticInvokeTable[h];
    for (unsigned i = 0; i < bucket.size(); ++i) {
        StaticInvokeEntryPtr entry = bucket[i];
        if (objectEquals(entry->callable, callable) &&
            objectVectorEquals(entry->args, args))
        {
            return entry;
        }
    }
    StaticInvokeEntryPtr entry = new StaticInvokeEntry(callable, args);
    bucket.push_back(entry);
    return entry;
}
