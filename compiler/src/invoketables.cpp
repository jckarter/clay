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

static
MatchSuccessPtr findMatchingInvoke(const vector<OverloadPtr> &overloads,
                                   unsigned &overloadIndex,
                                   ObjectPtr callable,
                                   const vector<TypePtr> &argsKey)
{
    while (overloadIndex < overloads.size()) {
        OverloadPtr x = overloads[overloadIndex++];
        MatchResultPtr result = matchInvoke(x, callable, argsKey);
        if (result->matchCode == MATCH_SUCCESS) {
            MatchSuccess *y = (MatchSuccess *)result.ptr();
            return y;
        }
    }
    return NULL;
}

static MatchSuccessPtr getMatch(InvokeSetPtr invokeSet,
                                unsigned entryIndex)
{
    if (entryIndex < invokeSet->matches.size())
        return invokeSet->matches[entryIndex];
    assert(entryIndex == invokeSet->matches.size());
    unsigned nextOverloadIndex = invokeSet->nextOverloadIndex;
    MatchSuccessPtr match = findMatchingInvoke(invokeSet->overloads,
                                               nextOverloadIndex,
                                               invokeSet->callable,
                                               invokeSet->argsKey);
    if (!match)
        return NULL;
    invokeSet->matches.push_back(match);
    invokeSet->nextOverloadIndex = nextOverloadIndex;
    return match;
}

static bool tempnessMatches(ValueTempness tempness,
                            ValueTempness formalTempness)
{
    switch (tempness) {

    case TEMPNESS_LVALUE :
        return ((formalTempness == TEMPNESS_DONTCARE) ||
                (formalTempness == TEMPNESS_LVALUE) ||
                (formalTempness == TEMPNESS_FORWARD));

    case TEMPNESS_RVALUE :
        return ((formalTempness == TEMPNESS_DONTCARE) ||
                (formalTempness == TEMPNESS_RVALUE) ||
                (formalTempness == TEMPNESS_FORWARD));

    default :
        assert(false);
        return false;
    }
}

static ValueTempness tempnessKeyItem(ValueTempness formalTempness,
                                     ValueTempness tempness)
{
    switch (formalTempness) {
    case TEMPNESS_LVALUE :
    case TEMPNESS_RVALUE :
    case TEMPNESS_DONTCARE : 
        return formalTempness;
    case TEMPNESS_FORWARD :
        return tempness;
    default :
        assert(false);
        return formalTempness;
    }
}

static bool matchTempness(CodePtr code,
                          const vector<ValueTempness> &argsTempness,
                          vector<ValueTempness> &tempnessKey)
{
    const vector<FormalArgPtr> &fargs = code->formalArgs;
    FormalArgPtr fvarArg = code->formalVarArg;

    if (fvarArg.ptr())
        assert(fargs.size() <= argsTempness.size());
    else
        assert(fargs.size() == argsTempness.size());

    for (unsigned i = 0; i < fargs.size(); ++i) {
        if (!tempnessMatches(argsTempness[i], fargs[i]->tempness))
            return false;
    }
    if (fvarArg.ptr()) {
        for (unsigned i = fargs.size(); i < argsTempness.size(); ++i) {
            if (!tempnessMatches(argsTempness[i], fvarArg->tempness))
                return false;
        }
    }

    tempnessKey.clear();
    for (unsigned i = 0; i < fargs.size(); ++i) {
        tempnessKey.push_back(
            tempnessKeyItem(fargs[i]->tempness,
                            argsTempness[i]));
    }
    if (fvarArg.ptr()) {
        for (unsigned i = fargs.size(); i < argsTempness.size(); ++i) {
            tempnessKey.push_back(
                tempnessKeyItem(fvarArg->tempness,
                                argsTempness[i]));
        }
    }
    return true;
}

static InvokeEntryPtr newInvokeEntry(MatchSuccessPtr x)
{
    InvokeEntryPtr entry = new InvokeEntry(x->callable, x->argsKey);
    entry->code = clone(x->code);
    entry->env = x->env;
    entry->fixedArgNames = x->fixedArgNames;
    entry->fixedArgTypes = x->fixedArgTypes;
    entry->varArgName = x->varArgName;
    entry->varArgTypes = x->varArgTypes;
    entry->inlined = x->inlined;
    return entry;
}

InvokeEntryPtr lookupInvokeEntry(ObjectPtr callable,
                                 const vector<TypePtr> &argsKey,
                                 const vector<ValueTempness> &argsTempness)
{
    InvokeSetPtr invokeSet = lookupInvokeSet(callable, argsKey);

    map<vector<ValueTempness>,InvokeEntryPtr>::iterator iter =
        invokeSet->tempnessMap.find(argsTempness);
    if (iter != invokeSet->tempnessMap.end())
        return iter->second;

    MatchSuccessPtr match;
    vector<ValueTempness> tempnessKey;
    unsigned i = 0;
    while ((match = getMatch(invokeSet,i)).ptr() != NULL) {
        if (matchTempness(match->code, argsTempness, tempnessKey))
            break;
        ++i;
    }
    if (!match)
        return NULL;

    iter = invokeSet->tempnessMap2.find(tempnessKey);
    if (iter != invokeSet->tempnessMap2.end()) {
        invokeSet->tempnessMap[argsTempness] = iter->second;
        return iter->second;
    }

    InvokeEntryPtr entry = newInvokeEntry(match);

    invokeSet->tempnessMap2[tempnessKey] = entry;
    invokeSet->tempnessMap[argsTempness] = entry;

    return entry;
}
