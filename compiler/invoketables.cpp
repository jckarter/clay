#include "clay.hpp"
#include "loader.hpp"
#include "error.hpp"
#include "invoketables.hpp"
#include "constructors.hpp"


#pragma clang diagnostic ignored "-Wcovered-switch-default"


namespace clay {

static llvm::SpecificBumpPtrAllocator<InvokeEntry> *invokeEntryAllocator
    = new llvm::SpecificBumpPtrAllocator<InvokeEntry>();
static llvm::SpecificBumpPtrAllocator<InvokeSet> *invokeSetAllocator
    = new llvm::SpecificBumpPtrAllocator<InvokeSet>();


static llvm::ArrayRef<OverloadPtr> callableOverloads(ObjectPtr x);



//
// callableOverloads
//

static void initCallable(ObjectPtr x)
{
    switch (x->objKind) {
    case TYPE :
        break;
    case RECORD_DECL : {
        RecordDecl *y = (RecordDecl *)x.ptr();
        if (!y->builtinOverloadInitialized)
            initBuiltinConstructor(y);
        break;
    }
    case VARIANT_DECL :
    case PROCEDURE :
    case GLOBAL_ALIAS :
        break;
    case PRIM_OP : {
        assert(isOverloadablePrimOp(x));
        break;
    }
    default :
        assert(false);
    }
}

const OverloadPtr callableInterface(ObjectPtr x)
{
    switch (x->objKind) {
    case PROCEDURE : {
        Procedure *y = (Procedure *)x.ptr();
        return y->interface;
    }
    default : {
        return NULL;
    }
    }
}

static llvm::ArrayRef<OverloadPtr> callableOverloads(ObjectPtr x)
{
    initCallable(x);
    switch (x->objKind) {
    case TYPE : {
        Type *y = (Type *)x.ptr();
        return y->overloads;
    }
    case RECORD_DECL : {
        RecordDecl *y = (RecordDecl *)x.ptr();
        return y->overloads;
    }
    case VARIANT_DECL : {
        VariantDecl *y = (VariantDecl *)x.ptr();
        return y->overloads;
    }
    case PROCEDURE : {
        Procedure *y = (Procedure *)x.ptr();
        return y->overloads;
    }
    case PRIM_OP : {
        assert(isOverloadablePrimOp(x));
        PrimOp *y = (PrimOp *)x.ptr();
        return primOpOverloads(y);
    }
    case GLOBAL_ALIAS : {
        GlobalAlias *a = (GlobalAlias *)x.ptr();
        assert(a->hasParams());
        return a->overloads;
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
static const size_t INVOKE_TABLE_SIZE = 16384;

static vector<llvm::SmallVector<InvokeSet*, 2> > invokeTable;


static void initInvokeTables() {
    assert(!invokeTablesInitialized);
    invokeTable.resize(INVOKE_TABLE_SIZE);
    invokeTablesInitialized = true;
}



//
// lookupInvokeSet
//

static bool shouldLogCallable(ObjectPtr callable)
{
    if (logMatchSymbols.empty())
        return false;

    ModulePtr m = staticModule(callable);
    if (!m)
        return false;

    string name;
    llvm::raw_string_ostream sout(name);
    printStaticName(sout, callable);
    sout.flush();

    pair<string,string> specificKey = make_pair(m->moduleName, name);
    pair<string,string> moduleGlobKey = make_pair(m->moduleName, string("*"));
    pair<string,string> anyModuleKey = make_pair(string("*"), name);
    return logMatchSymbols.find(specificKey) != logMatchSymbols.end()
        || logMatchSymbols.find(moduleGlobKey) != logMatchSymbols.end()
        || logMatchSymbols.find(anyModuleKey) != logMatchSymbols.end();
}

InvokeSet* lookupInvokeSet(ObjectPtr callable,
                             llvm::ArrayRef<TypePtr> argsKey)
{
    if (!invokeTablesInitialized)
        initInvokeTables();
    int h = objectHash(callable) + objectVectorHash(argsKey);
    h &= (invokeTable.size() - 1);
    llvm::SmallVector<InvokeSet*, 2> &bucket = invokeTable[h];
    for (unsigned i = 0; i < bucket.size(); ++i) {
        InvokeSet* invokeSet = bucket[i];
        if (objectEquals(invokeSet->callable, callable) &&
            objectVectorEquals(invokeSet->argsKey, argsKey))
        {
            return invokeSet;
        }
    }
    OverloadPtr interface = callableInterface(callable);
    llvm::ArrayRef<OverloadPtr> overloads = callableOverloads(callable);
    InvokeSet* invokeSet = invokeSetAllocator->Allocate();
    new ((void*)invokeSet) InvokeSet(callable, argsKey, interface, overloads);
    invokeSet->shouldLog = shouldLogCallable(callable);

    bucket.push_back(invokeSet);
    return invokeSet;
}



//
// lookupInvokeEntry
//

static
MatchSuccessPtr findMatchingInvoke(llvm::ArrayRef<OverloadPtr> overloads,
                                   unsigned &overloadIndex,
                                   ObjectPtr callable,
                                   llvm::ArrayRef<TypePtr> argsKey,
                                   MatchFailureError &failures)
{
    while (overloadIndex < overloads.size()) {
        OverloadPtr x = overloads[overloadIndex++];
        MatchResultPtr result = matchInvoke(x, callable, argsKey);
        failures.failures.push_back(make_pair(x, result));
        if (result->matchCode == MATCH_SUCCESS) {
            MatchSuccess *y = (MatchSuccess *)result.ptr();
            return y;
        }
    }
    return NULL;
}

static MatchSuccessPtr getMatch(InvokeSet* invokeSet,
                                unsigned entryIndex,
                                MatchFailureError &failures)
{
    if (entryIndex < invokeSet->matches.size())
        return invokeSet->matches[entryIndex];
    assert(entryIndex == invokeSet->matches.size());

    unsigned nextOverloadIndex = invokeSet->nextOverloadIndex;
    MatchSuccessPtr match = findMatchingInvoke(invokeSet->overloads,
                                               nextOverloadIndex,
                                               invokeSet->callable,
                                               invokeSet->argsKey,
                                               failures);
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
                          llvm::ArrayRef<ValueTempness> argsTempness,
                          bool callByName,
                          vector<ValueTempness> &tempnessKey,
                          vector<uint8_t> &forwardedRValueFlags)
{
    llvm::ArrayRef<FormalArgPtr> fargs = code->formalArgs;
    
    if (code->hasVarArg)
        assert(fargs.size()-1 <= argsTempness.size());
    else
        assert(fargs.size() == argsTempness.size());

    tempnessKey.clear();
    forwardedRValueFlags.clear();
    
    unsigned varArgSize = argsTempness.size()-fargs.size()+1;
    for (unsigned i = 0, j = 0; i < fargs.size(); ++i) {
        if (callByName && (fargs[i]->tempness == TEMPNESS_FORWARD)) {
                error(fargs[i], "forwarded arguments are not allowed "
                      "in call-by-name procedures");
        }
        if (fargs[i]->varArg) {
            for (; j < varArgSize; ++j) {
                if (!tempnessMatches(argsTempness[i+j], fargs[i]->tempness))
                    return false;
                tempnessKey.push_back(
                    tempnessKeyItem(fargs[i]->tempness,
                                    argsTempness[i+j]));
                bool forwardedRValue =
                    (fargs[i]->tempness == TEMPNESS_FORWARD) &&
                    (argsTempness[i+j] == TEMPNESS_RVALUE);
                forwardedRValueFlags.push_back(forwardedRValue);
            }
            --j;
        } else {
            if (!tempnessMatches(argsTempness[i+j], fargs[i]->tempness))
                return false;
            tempnessKey.push_back(
                tempnessKeyItem(fargs[i]->tempness,
                                argsTempness[i+j]));
            bool forwardedRValue = 
                (fargs[i]->tempness == TEMPNESS_FORWARD) &&
                (argsTempness[i+j] == TEMPNESS_RVALUE);
            forwardedRValueFlags.push_back(forwardedRValue);
        }
    }
    return true;
}

static InvokeEntry* newInvokeEntry(InvokeSet* parent,
                                   MatchSuccessPtr match,
                                   MatchSuccessPtr interfaceMatch)
{
    InvokeEntry* entry = invokeEntryAllocator->Allocate();
    new ((void*)entry) InvokeEntry(parent, match->callable, match->argsKey);
    entry->origCode = match->code;
    entry->code = clone(match->code);
    entry->env = match->env;
    if (interfaceMatch != NULL)
        entry->interfaceEnv = interfaceMatch->env;
    entry->fixedArgNames = match->fixedArgNames;
    entry->fixedArgTypes = match->fixedArgTypes;
    entry->varArgName = match->varArgName;
    entry->varArgTypes = match->varArgTypes;
    entry->varArgPosition = match->varArgPosition;
    entry->callByName = match->callByName;
    entry->isInline = match->isInline;

    return entry;
}

InvokeEntry* lookupInvokeEntry(ObjectPtr callable,
                               llvm::ArrayRef<TypePtr> argsKey,
                               llvm::ArrayRef<ValueTempness> argsTempness,
                               MatchFailureError &failures)
{
    InvokeSet* invokeSet = lookupInvokeSet(callable, argsKey);

    map<vector<ValueTempness>,InvokeEntry*>::iterator iter =
        invokeSet->tempnessMap.find(argsTempness);
    if (iter != invokeSet->tempnessMap.end())
        return iter->second;
    
    MatchResultPtr interfaceResult;
    if (invokeSet->interface != NULL) {
        interfaceResult = matchInvoke(invokeSet->interface,
                                                     invokeSet->callable,
                                                     invokeSet->argsKey);
        if (interfaceResult->matchCode != MATCH_SUCCESS) {
            failures.failedInterface = true;
            failures.failures.push_back(make_pair(invokeSet->interface, interfaceResult));
            return NULL;
        }
    }

    MatchSuccessPtr match;
    vector<ValueTempness> tempnessKey;
    vector<uint8_t> forwardedRValueFlags;
    
    unsigned i = 0;
    while ((match = getMatch(invokeSet,i,failures)).ptr() != NULL) {
        if (matchTempness(match->code,
                          argsTempness,
                          match->callByName,
                          tempnessKey,
                          forwardedRValueFlags))
        {
            break;
        }
        ++i;
    }
    if (!match)
        return NULL;

    iter = invokeSet->tempnessMap2.find(tempnessKey);
    if (iter != invokeSet->tempnessMap2.end()) {
        invokeSet->tempnessMap[argsTempness] = iter->second;
        return iter->second;
    }

    InvokeEntry* entry = newInvokeEntry(invokeSet, match,
        (MatchSuccess*)interfaceResult.ptr());
    entry->forwardedRValueFlags = forwardedRValueFlags;
    
    invokeSet->tempnessMap2[tempnessKey] = entry;
    invokeSet->tempnessMap[argsTempness] = entry;

    return entry;
}

}
