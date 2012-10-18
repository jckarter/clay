#ifndef __INVOKETABLES_HPP
#define __INVOKETABLES_HPP

#include "clay.hpp"
#include "matchinvoke.hpp"


namespace clay {


struct InvokeSet;


struct InvokeEntry {
    InvokeSet *parent;
    ObjectPtr callable;
    vector<TypePtr> argsKey;
    vector<bool> forwardedRValueFlags;

    bool analyzed;
    bool analyzing;

    CodePtr origCode;
    CodePtr code;
    EnvPtr env;
    EnvPtr interfaceEnv;

    vector<TypePtr> fixedArgTypes;
    vector<IdentifierPtr> fixedArgNames;
    IdentifierPtr varArgName;
    vector<TypePtr> varArgTypes;
    unsigned varArgPosition;

    bool callByName; // if callByName the rest of InvokeEntry is not set
    InlineAttribute isInline;

    ObjectPtr analysis;
    vector<bool> returnIsRef;
    vector<TypePtr> returnTypes;

    llvm::Function *llvmFunc;
    llvm::Function *llvmCWrappers[CC_Count];

    llvm::TrackingVH<llvm::MDNode> debugInfo;

    InvokeEntry(InvokeSet *parent,
                ObjectPtr callable,
                const vector<TypePtr> &argsKey)
        : parent(parent), callable(callable),
          argsKey(argsKey), analyzed(false),
          analyzing(false), varArgPosition(0),
          callByName(false),
          isInline(IGNORE), llvmFunc(NULL), debugInfo(NULL)
    {
        for (size_t i = 0; i < CC_Count; ++i)
            llvmCWrappers[i] = NULL;
    }

    llvm::DISubprogram getDebugInfo() { return llvm::DISubprogram(debugInfo); }
};

extern vector<OverloadPtr> patternOverloads;

struct InvokeSet {
    ObjectPtr callable;
    vector<TypePtr> argsKey;
    OverloadPtr interface;
    vector<OverloadPtr> overloads;

    vector<MatchSuccessPtr> matches;
    unsigned nextOverloadIndex;

    bool shouldLog;

    map<vector<ValueTempness>, InvokeEntry*> tempnessMap;
    map<vector<ValueTempness>, InvokeEntry*> tempnessMap2;

    InvokeSet(ObjectPtr callable,
              const vector<TypePtr> &argsKey,
              OverloadPtr symbolInterface,
              const vector<OverloadPtr> &symbolOverloads)
        : callable(callable), argsKey(argsKey),
          interface(symbolInterface),
          overloads(symbolOverloads), nextOverloadIndex(0),
          shouldLog(false)
    {
        overloads.insert(overloads.end(), patternOverloads.begin(), patternOverloads.end());
    }
};

typedef vector< pair<OverloadPtr, MatchResultPtr> > MatchFailureVector;

struct MatchFailureError {
    MatchFailureVector failures;
    bool failedInterface;

    MatchFailureError() : failedInterface(false) {}
};

InvokeSet *lookupInvokeSet(ObjectPtr callable,
                           const vector<TypePtr> &argsKey);
InvokeEntry* lookupInvokeEntry(ObjectPtr callable,
                               const vector<TypePtr> &argsKey,
                               const vector<ValueTempness> &argsTempness,
                               MatchFailureError &failures);


}

#endif // __INVOKETABLES_HPP
