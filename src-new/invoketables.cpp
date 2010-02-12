#include "clay.hpp"



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

static FlagsMapEntry & getIsStaticFlags(ObjectPtr callable, unsigned nArgs)
{
    return flagsMap[make_pair(callable, nArgs)];
}

void initIsStaticFlags(ProcedurePtr x)
{
    assert(!x->staticFlagsInitialized);
    const vector<FormalArgPtr> &formalArgs = x->code->formalArgs;
    FlagsMapEntry &entry = getIsStaticFlags(x.ptr(), formalArgs.size());
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

void initIsStaticFlags(OverloadablePtr x)
{
    assert(!x->staticFlagsInitialized);
    for (unsigned i = 0; i < x->overloads.size(); ++i) {
        const vector<FormalArgPtr> &formalArgs =
            x->overloads[i]->code->formalArgs;
        FlagsMapEntry &entry = getIsStaticFlags(x.ptr(), formalArgs.size());
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
    x->staticFlagsInitialized = true;
}

const vector<bool> &lookupIsStaticFlags(ObjectPtr callable, unsigned nArgs)
{
    FlagsMapEntry &entry = getIsStaticFlags(callable, nArgs);
    if (!entry.initialized)
        error("incorrect no. of arguments");
    return entry.isStaticFlags;
}



//
// invoke entries
//

static bool invokeTableInitialized = false;
static vector< vector<InvokeEntryPtr> > invokeTable;

static void initInvokeTable() {
    assert(!invokeTableInitialized);
    invokeTable.resize(16384);
    invokeTableInitialized = true;
}

InvokeEntryPtr lookupInvoke(ObjectPtr callable,
                          const vector<ObjectPtr> &argsMeta)
{
    if (!invokeTableInitialized)
        initInvokeTable();
    int h = objectHash(callable) + objectVectorHash(argsMeta);
    h &= (invokeTable.size() - 1);
    vector<InvokeEntryPtr> &bucket = invokeTable[h];
    for (unsigned i = 0; i < bucket.size(); ++i) {
        InvokeEntryPtr entry = bucket[i];
        if (objectEquals(entry->callable, callable) &&
            objectVectorEquals(entry->argsMeta, argsMeta))
        {
            return entry;
        }
    }
    InvokeEntryPtr entry = new InvokeEntry(callable, argsMeta);
    bucket.push_back(entry);
    return entry;
}
