#include "clay.hpp"



//
// callableOverloads
//

const vector<OverloadPtr> &callableOverloads(ObjectPtr x)
{
    switch (x->objKind) {
    case TYPE : {
        Type *y = (Type *)x.ptr();
        return y->overloads;
    }
    case RECORD : {
        Record *y = (Record *)x.ptr();
        return y->overloads;
    }
    case PROCEDURE : {
        Procedure *y = (Procedure *)x.ptr();
        if (y->overloads.empty()) {
            ExprPtr target = new ObjectExpr(y);
            OverloadPtr z = new Overload(target, y->code, y->inlined);
            z->env = y->env;
            y->overloads.push_back(z);
        }
        return y->overloads;
    }
    case OVERLOADABLE : {
        Overloadable *y = (Overloadable *)x.ptr();
        return y->overloads;
    }
    default : {
        assert(false);
        const vector<OverloadPtr> *ptr = NULL;
        return *ptr;
    }
    }
}



//
// isStaticFlags
//

static map<FlagsMapKey, FlagsMapEntry> flagsMap;

static FlagsMapEntry &lookupFlagsMapEntry(ObjectPtr callable, unsigned nArgs)
{
    return flagsMap[make_pair(callable, nArgs)];
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

static void initIsStaticFlags(ObjectPtr callable,
                              const vector<OverloadPtr> &overloads)
{
    for (unsigned i = 0; i < overloads.size(); ++i)
        updateIsStaticFlags(callable, overloads[i]);
}

void initIsStaticFlags(ObjectPtr x)
{
    switch (x->objKind) {
    case TYPE : {
        Type *y = (Type *)x.ptr();
        if (!y->overloadsInitialized)
            initTypeOverloads(y);
        if (!y->staticFlagsInitialized) {
            initIsStaticFlags(y, callableOverloads(y));
            y->staticFlagsInitialized = true;
        }
        break;
    }
    case RECORD : {
        Record *y = (Record *)x.ptr();
        if (!y->builtinOverloadInitialized)
            initBuiltinConstructor(y);
        if (!y->staticFlagsInitialized) {
            initIsStaticFlags(y, callableOverloads(y));
            y->staticFlagsInitialized = true;
        }
        break;
    }
    case PROCEDURE : {
        Procedure *y = (Procedure *)x.ptr();
        if (!y->staticFlagsInitialized) {
            initIsStaticFlags(y, callableOverloads(y));
            y->staticFlagsInitialized = true;
        }
        break;
    }
    case OVERLOADABLE : {
        Overloadable *y = (Overloadable *)x.ptr();
        if (!y->staticFlagsInitialized) {
            initIsStaticFlags(y, callableOverloads(y));
            y->staticFlagsInitialized = true;
        }
        break;
    }
    default :
        assert(false);
    }
}

const vector<bool> &lookupIsStaticFlags(ObjectPtr callable, unsigned nArgs)
{
    initIsStaticFlags(callable);
    FlagsMapEntry &entry = lookupFlagsMapEntry(callable, nArgs);
    if (!entry.initialized)
        error("incorrect no. of arguments");
    return entry.isStaticFlags;
}

bool computeArgsKey(const vector<bool> &isStaticFlags,
                    const vector<ExprPtr> &args,
                    EnvPtr env,
                    vector<ObjectPtr> &argsKey,
                    vector<ValueTempness> &argsTempness,
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
            argsTempness.push_back(pv->isTemp ? RVALUE : LVALUE);
        }
        argLocations.push_back(args[i]->location);
    }
    return true;
}



//
// invoke tables
//

static bool invokeTablesInitialized = false;

static vector< vector<InvokeSetPtr> > invokeTable;

static vector< vector<StaticInvokeEntryPtr> > staticInvokeTable;



//
// initInvokeTable
//

static void initInvokeTables() {
    assert(!invokeTablesInitialized);
    invokeTable.resize(16384);
    staticInvokeTable.resize(8192);
    invokeTablesInitialized = true;
}



//
// lookupInvokeSet
//

InvokeSetPtr lookupInvokeSet(ObjectPtr callable,
                             const vector<bool> &isStaticFlags,
                             const vector<ObjectPtr> &argsKey)
{
    if (!invokeTablesInitialized)
        initInvokeTables();
    int h = objectHash(callable) + objectVectorHash(argsKey);
    h &= (invokeTable.size() - 1);
    vector<InvokeSetPtr> &bucket = invokeTable[h];
    for (unsigned i = 0; i < bucket.size(); ++i) {
        InvokeSetPtr invokeSet = bucket[i];
        if (objectEquals(invokeSet->callable, callable) &&
            objectVectorEquals(invokeSet->argsKey, argsKey))
        {
            return invokeSet;
        }
    }
    InvokeSetPtr invokeSet = new InvokeSet(callable, isStaticFlags, argsKey);
    bucket.push_back(invokeSet);
    return invokeSet;
}

StaticInvokeEntryPtr lookupStaticInvoke(ObjectPtr callable,
                                        const vector<ObjectPtr> &args)
{
    if (!invokeTablesInitialized)
        initInvokeTables();
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
