#ifndef __INVOKETABLES_HPP
#define __INVOKETABLES_HPP

#include "clay.hpp"
#include "matchinvoke.hpp"


namespace clay {


struct InvokeSet;
struct InvokeEntry;

static llvm::SpecificBumpPtrAllocator<InvokeEntry> *invokeEntryAllocator
    = new llvm::SpecificBumpPtrAllocator<InvokeEntry>();
static llvm::SpecificBumpPtrAllocator<InvokeSet> *invokeSetAllocator
    = new llvm::SpecificBumpPtrAllocator<InvokeSet>();

struct InvokeEntry {
    InvokeSet *parent;
    ObjectPtr callable;
    vector<TypePtr> argsKey;
    vector<uint8_t> forwardedRValueFlags;

    CodePtr origCode;
    CodePtr code;
    EnvPtr env;
    EnvPtr interfaceEnv;

    vector<TypePtr> fixedArgTypes;
    vector<IdentifierPtr> fixedArgNames;
    IdentifierPtr varArgName;
    vector<TypePtr> varArgTypes;
    unsigned varArgPosition;

    InlineAttribute isInline;

    ObjectPtr analysis;
    vector<uint8_t> returnIsRef;
    vector<TypePtr> returnTypes;

    llvm::Function *llvmFunc;
    llvm::Function *llvmCWrappers[CC_Count];

    llvm::TrackingVH<llvm::MDNode> debugInfo;

    bool analyzed:1;
    bool analyzing:1;
    bool callByName:1; // if callByName the rest of InvokeEntry is not set
    bool runtimeNop:1;

    InvokeEntry(InvokeSet *parent,
                ObjectPtr callable,
                llvm::ArrayRef<TypePtr> argsKey)
        : parent(parent), callable(callable),
          argsKey(argsKey),
          varArgPosition(0),
          isInline(IGNORE),
          llvmFunc(NULL),
          debugInfo(NULL),
          analyzed(false),
          analyzing(false),
          callByName(false),
          runtimeNop(false)
    {
        for (size_t i = 0; i < CC_Count; ++i)
            llvmCWrappers[i] = NULL;
    }
    void *operator new(size_t num_bytes) {
        return invokeEntryAllocator->Allocate();
    }
    void operator delete(void* invokeEntry) {
        ANodeAllocator->Deallocate(invokeEntry);
    }
    llvm::DISubprogram getDebugInfo() { return llvm::DISubprogram(debugInfo); }
};

struct InvokeSet {
    ObjectPtr callable;
    vector<TypePtr> argsKey;
    OverloadPtr interface;
    vector<OverloadPtr> overloads;

    vector<MatchSuccessPtr> matches;
    map<vector<ValueTempness>, InvokeEntry*> tempnessMap;
    map<vector<ValueTempness>, InvokeEntry*> tempnessMap2;

    unsigned nextOverloadIndex; //:31;

    bool shouldLog:1;
    bool evaluatingPredicate:1;

    CompilerState* cst;

    InvokeSet(ObjectPtr callable,
              llvm::ArrayRef<TypePtr> argsKey,
              OverloadPtr symbolInterface,
              llvm::ArrayRef<OverloadPtr> symbolOverloads,
              CompilerState* cst)
        : callable(callable), argsKey(argsKey),
          interface(symbolInterface),
          overloads(symbolOverloads), nextOverloadIndex(0),
          shouldLog(false),
          evaluatingPredicate(false),
          cst(cst)
    {
        overloads.insert(overloads.end(), 
                         cst->patternOverloads.begin(),
                         cst->patternOverloads.end());
    }
    void *operator new(size_t num_bytes) {
        return invokeSetAllocator->Allocate();
    }
    virtual void dealloc() { ANodeAllocator->Deallocate(this); }
};

typedef vector< pair<OverloadPtr, MatchResultPtr> > MatchFailureVector;

struct MatchFailureError {
    MatchFailureVector failures;
    bool failedInterface:1;
    bool ambiguousMatch:1;

    MatchFailureError() : failedInterface(false), ambiguousMatch(false) {}
};

InvokeSet *lookupInvokeSet(ObjectPtr callable,
                           llvm::ArrayRef<TypePtr> argsKey,
                           CompilerState* cst);
vector<InvokeSet*> lookupInvokeSets(ObjectPtr callable,
                                    CompilerState* cst);
InvokeEntry* lookupInvokeEntry(ObjectPtr callable,
                               llvm::ArrayRef<TypePtr> argsKey,
                               llvm::ArrayRef<ValueTempness> argsTempness,
                               MatchFailureError &failures,
                               CompilerState* cst);

void setFinalOverloadsEnabled(bool enabled, CompilerState* cst); 

}

#endif // __INVOKETABLES_HPP
