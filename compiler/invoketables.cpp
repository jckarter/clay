#include "clay.hpp"
#include "loader.hpp"
#include "error.hpp"
#include "invoketables.hpp"
#include "constructors.hpp"
#include "clone.hpp"
#include "objects.hpp"


#pragma clang diagnostic ignored "-Wcovered-switch-default"


namespace clay {

static bool _finalOverloadsEnabled = false;

void setFinalOverloadsEnabled(bool enabled)
{
    _finalOverloadsEnabled = enabled;
}


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
    unsigned h = objectHash(callable) + objectVectorHash(argsKey);
    h &= unsigned(invokeTable.size() - 1);
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
    InvokeSet* invokeSet = new InvokeSet(callable, argsKey, interface, overloads);
    invokeSet->shouldLog = shouldLogCallable(callable);

    bucket.push_back(invokeSet);
    return invokeSet;
}

vector<InvokeSet*> lookupInvokeSets(ObjectPtr callable) {
    assert(invokeTablesInitialized);
    vector<InvokeSet*> r;
    for (unsigned i = 0; i < invokeTable.size(); ++i) {
        for (unsigned j = 0; j < invokeTable[i].size(); ++j) {
            InvokeSet* set = invokeTable[i][j];
            if (objectEquals(set->callable, callable)) {
                r.push_back(set);
            }
        }
    }
    return r;
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

static bool tempnessMatches(bool rvalue,
                            ValueTempness formalTempness)
{
    if (!rvalue) {
        return ((formalTempness == TEMPNESS_DONTCARE) ||
                (formalTempness == TEMPNESS_LVALUE) ||
                (formalTempness == TEMPNESS_FORWARD));

    } else {
        return ((formalTempness == TEMPNESS_DONTCARE) ||
                (formalTempness == TEMPNESS_RVALUE) ||
                (formalTempness == TEMPNESS_FORWARD));

    }
}

static ValueTempness tempnessKeyItem(ValueTempness formalTempness,
                                     bool rvalue)
{
    switch (formalTempness) {
    case TEMPNESS_LVALUE :
    case TEMPNESS_RVALUE :
    case TEMPNESS_DONTCARE : 
        return formalTempness;
    case TEMPNESS_FORWARD :
        return rvalue ? TEMPNESS_RVALUE : TEMPNESS_LVALUE;
    default :
        assert(false);
        return formalTempness;
    }
}

static bool matchTempness(CodePtr code,
                          const vector<bool>& argsRValues,
                          bool callByName,
                          vector<ValueTempness> &tempnessKey,
                          vector<uint8_t> &forwardedRValueFlags)
{
    llvm::ArrayRef<FormalArgPtr> fargs = code->formalArgs;
    
    if (code->hasVarArg)
        assert(fargs.size()-1 <= argsRValues.size());
    else
        assert(fargs.size() == argsRValues.size());

    assert(tempnessKey.empty());
    assert(forwardedRValueFlags.empty());
    
    size_t varArgSize = argsRValues.size()-fargs.size()+1;
    for (size_t i = 0, j = 0; i < fargs.size(); ++i) {
        if (callByName && (fargs[i]->tempness == TEMPNESS_FORWARD)) {
                error(fargs[i], "forwarded arguments are not allowed "
                      "in call-by-name procedures");
        }
        if (fargs[i]->varArg) {
            for (; j < varArgSize; ++j) {
                if (!tempnessMatches(argsRValues[i+j], fargs[i]->tempness))
                    return false;
                tempnessKey.push_back(
                    tempnessKeyItem(fargs[i]->tempness,
                            argsRValues[i+j]));
                bool forwardedRValue =
                    (fargs[i]->tempness == TEMPNESS_FORWARD) &&
                    argsRValues[i+j];
                forwardedRValueFlags.push_back(forwardedRValue);
            }
            --j;
        } else {
            if (!tempnessMatches(argsRValues[i+j], fargs[i]->tempness))
                return false;
            tempnessKey.push_back(
                tempnessKeyItem(fargs[i]->tempness,
                        argsRValues[i+j]));
            bool forwardedRValue = 
                (fargs[i]->tempness == TEMPNESS_FORWARD) &&
                argsRValues[i+j];
            forwardedRValueFlags.push_back(forwardedRValue);
        }
    }
    return true;
}

static InvokeEntry* newInvokeEntry(InvokeSet* parent,
                                   MatchSuccessPtr match,
                                   MatchSuccessPtr interfaceMatch)
{
    InvokeEntry* entry = new InvokeEntry(parent, match->callable, match->argsKey);
    entry->origCode = match->overload->code;
    entry->code = clone(match->overload->code);
    entry->env = match->env;
    if (interfaceMatch != NULL)
        entry->interfaceEnv = interfaceMatch->env;
    entry->fixedArgNames = match->fixedArgNames;
    entry->fixedArgTypes = match->fixedArgTypes;
    entry->varArgName = match->varArgName;
    entry->varArgTypes = match->varArgTypes;
    entry->varArgPosition = match->varArgPosition;
    entry->callByName = match->overload->callByName;
    entry->isInline = match->overload->isInline;

    return entry;
}

namespace {
    class FinallyClearEvaluatingPredicate {
    private:
        InvokeSet* invokeSet;
    public:
        FinallyClearEvaluatingPredicate(InvokeSet* invokeSet): invokeSet(invokeSet) {}
        ~FinallyClearEvaluatingPredicate() {
            assert(invokeSet->evaluatingPredicate);
            invokeSet->evaluatingPredicate = false;
        }
    };
}

InvokeEntry* lookupInvokeEntry(ObjectPtr callable,
                               llvm::ArrayRef<TypePtr> argsKey,
                               const vector<bool>& argsRValues,
                               MatchFailureError &failures)
{
    InvokeSet* invokeSet = lookupInvokeSet(callable, argsKey);

    if (invokeSet->evaluatingPredicate) {
        // matchInvoke calls the same lookupInvokeEntry
        error("predicate evaluation loop");
    }

    invokeSet->evaluatingPredicate = true;
    FinallyClearEvaluatingPredicate FinallyClearEvaluatingPredicate(invokeSet);

    map<vector<bool>, InvokeEntry*>::iterator iter1 =
        invokeSet->tempnessMap.find(argsRValues);
    if (iter1 != invokeSet->tempnessMap.end())
        return iter1->second;
    
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
        tempnessKey.clear();
        forwardedRValueFlags.clear();
        if (matchTempness(match->overload->code,
                          argsRValues,
                          match->overload->callByName,
                          tempnessKey,
                          forwardedRValueFlags))
        {
            break;
        }
        ++i;
    }
    if (!match)
        return NULL;

    map<vector<ValueTempness>, InvokeEntry*>::iterator iter2 =
            invokeSet->tempnessMap2.find(tempnessKey);
    if (iter2 != invokeSet->tempnessMap2.end()) {
        invokeSet->tempnessMap[argsRValues] = iter2->second;
        return iter2->second;
    }

    InvokeEntry* entry = newInvokeEntry(invokeSet, match,
        (MatchSuccess*)interfaceResult.ptr());
    entry->forwardedRValueFlags = forwardedRValueFlags;
    
    invokeSet->tempnessMap2[tempnessKey] = entry;
    invokeSet->tempnessMap[argsRValues] = entry;

    if (_finalOverloadsEnabled) {
        MatchSuccessPtr match2;
        vector<ValueTempness> tempnessKey2;
        vector<uint8_t> forwardedRValueFlags2;
        unsigned j = invokeSet->nextOverloadIndex;
        while ((match2 = findMatchingInvoke(callableOverloads(callable),
                                           j,
                                           callable,
                                           argsKey,
                                           failures)).ptr() != NULL) {
            if (matchTempness(match2->overload->code,
                              argsRValues,
                              match2->overload->callByName,
                              tempnessKey2,
                              forwardedRValueFlags2))
            {
                break;
            }
            ++j;
        }

        if (match2 != NULL && !match2->overload->isDefault) {
            failures.ambiguousMatch = true;
            return NULL;    
        }
    }

    return entry;
}

}
