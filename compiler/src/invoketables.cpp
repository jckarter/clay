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

typedef pair<ObjectPtr, unsigned> FlagsMapKey;

struct FlagsMapEntry {
    bool initialized;
    vector<bool> isStaticFlags;
    FlagsMapEntry()
        : initialized(false) {}
};

static map<FlagsMapKey, FlagsMapEntry> flagsMap;

static FlagsMapEntry &lookupFlagsMapEntry(ObjectPtr callable, unsigned nArgs)
{
    return flagsMap[make_pair(callable, nArgs)];
}

static void initCallable(ObjectPtr x)
{
    switch (x->objKind) {
    case TYPE : {
        Type *y = (Type *)x.ptr();
        if (!y->overloadsInitialized)
            initTypeOverloads(y);
        break;
    }
    case RECORD : {
        Record *y = (Record *)x.ptr();
        if (!y->builtinOverloadInitialized)
            initBuiltinConstructor(y);
        break;
    }
    case PROCEDURE :
    case OVERLOADABLE :
        break;
    default :
        assert(false);
    }
}

static bool isStaticArg(FormalArgPtr farg) {
    return false;
}

static void computeIsStaticFlags(ObjectPtr x,
                                 unsigned nArgs,
                                 vector<bool> &isStaticFlags)
{
    initCallable(x);
    const vector<OverloadPtr> &overloads = callableOverloads(x);
    for (unsigned i = 0; i < overloads.size(); ++i) {
        CodePtr y = overloads[i]->code;
        const vector<FormalArgPtr> &fargs = y->formalArgs;
        if (y->hasVarArgs && (fargs.size() <= nArgs)) {
            for (unsigned j = 0; j < fargs.size(); ++j)
                isStaticFlags.push_back(isStaticArg(fargs[j]));
            for (unsigned j = fargs.size(); j < nArgs; ++j)
                isStaticFlags.push_back(false);
            return;
        }
        else if (fargs.size() == nArgs) {
            for (unsigned j = 0; j < fargs.size(); ++j)
                isStaticFlags.push_back(isStaticArg(fargs[j]));
            return;
        }
    }

    // when no overload matches, don't report error here
    // instead, assume all args are non-static.
    // errors reported at a later point have more context info
    // that could be displayed to the user.
    for (unsigned i = 0; i < nArgs; ++i)
        isStaticFlags.push_back(false);
}

const vector<bool> &lookupIsStaticFlags(ObjectPtr callable, unsigned nArgs)
{
    FlagsMapEntry &entry = lookupFlagsMapEntry(callable, nArgs);
    if (!entry.initialized) {
        computeIsStaticFlags(callable, nArgs, entry.isStaticFlags);
        entry.initialized = true;
    }
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


static void initInvokeTables() {
    assert(!invokeTablesInitialized);
    invokeTable.resize(16384);
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
