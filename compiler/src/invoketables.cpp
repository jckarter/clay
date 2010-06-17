#include "clay.hpp"



//
// callableOverloads
//

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
        break;
    default :
        assert(false);
    }
}

const vector<OverloadPtr> &callableOverloads(ObjectPtr x)
{
    initCallable(x);
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
                             const vector<TypePtr> &argsKey)
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
    const vector<OverloadPtr> &overloads = callableOverloads(callable);
    InvokeSetPtr invokeSet = new InvokeSet(callable, argsKey, overloads);
    bucket.push_back(invokeSet);
    return invokeSet;
}



//
// lookupInvokeEntry
//

static InvokeEntryPtr findNextMatchingEntry(InvokeSetPtr invokeSet,
                                            unsigned entryIndex)
{
    if (entryIndex < invokeSet->entries.size())
        return invokeSet->entries[entryIndex];
    assert(entryIndex == invokeSet->entries.size());
    unsigned nextOverloadIndex = invokeSet->nextOverloadIndex;
    InvokeEntryPtr entry = findMatchingInvoke(invokeSet->overloads,
                                              nextOverloadIndex,
                                              invokeSet->callable,
                                              invokeSet->argsKey);
    if (!entry)
        return NULL;
    invokeSet->entries.push_back(entry);
    invokeSet->nextOverloadIndex = nextOverloadIndex;
    return entry;
}

static bool tempnessMatches(ValueTempness a, ValueTempness b)
{
    switch (a) {

    case TEMPNESS_LVALUE :
        return ((b == TEMPNESS_DONTCARE) ||
                (b == TEMPNESS_LVALUE));

    case TEMPNESS_RVALUE :
        return ((b == TEMPNESS_DONTCARE) ||
                (b == TEMPNESS_RVALUE));

    default :
        assert(false);
        return false;
    }
}

static bool tempnessMatches(CodePtr code,
                            const vector<ValueTempness> &tempness)
{
    if (code->formalVarArg.ptr())
        assert(code->formalArgs.size() <= tempness.size());
    else
        assert(code->formalArgs.size() == tempness.size());

    for (unsigned i = 0; i < code->formalArgs.size(); ++i) {
        FormalArgPtr arg = code->formalArgs[i];
        if (!tempnessMatches(tempness[i], arg->tempness))
            return false;
    }
    if (code->formalVarArg.ptr()) {
        for (unsigned i = code->formalArgs.size(); i < tempness.size(); ++i) {
            if (!tempnessMatches(tempness[i], code->formalVarArg->tempness))
                return false;
        }
    }
    return true;
}

InvokeEntryPtr lookupInvokeEntry(ObjectPtr callable,
                                 const vector<TypePtr> &argsKey,
                                 const vector<ValueTempness> &argsTempness)
{
    InvokeSetPtr invokeSet = lookupInvokeSet(callable, argsKey);
    unsigned i = 0;
    InvokeEntryPtr entry;
    while ((entry = findNextMatchingEntry(invokeSet, i)).ptr()) {
        if (tempnessMatches(entry->code, argsTempness))
            break;
        ++i;
    }
    return entry;
}
