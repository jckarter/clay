#ifndef __CODEGEN_HPP
#define __CODEGEN_HPP

#include "clay.hpp"
#include "types.hpp"

namespace clay {


static const unsigned short DW_LANG_user_CLAY = 0xC1A4;

llvm::PointerType *exceptionReturnType(CompilerState* cst);
llvm::Value *noExceptionReturnValue(CompilerState* cst);

llvm::TargetMachine *initLLVM(llvm::StringRef targetTriple,
    llvm::StringRef targetCPU,
    llvm::StringRef targetFeatures,
    bool softFloat,
    llvm::StringRef name,
    llvm::StringRef flags,
    bool relocPic,
    bool debug,
    unsigned optLevel,
    CompilerState* cst);

bool inlineEnabled(CompilerState* cst);
void setInlineEnabled(bool enabled, CompilerState* cst);
bool exceptionsEnabled(CompilerState* cst);
void setExceptionsEnabled(bool enabled, CompilerState* cst);


void initExternalTarget(string target, CompilerState* cst);


struct CValue : public Object {
    TypePtr type;
    llvm::Value *llValue;
    bool forwardedRValue:1;
    CValue(TypePtr type, llvm::Value *llValue)
        : Object(CVALUE), type(type), llValue(llValue),
          forwardedRValue(false)
    {
        llvmType(type); // force full definition of type
    }
};

struct MultiCValue : public Object {
    vector<CValuePtr> values;
    MultiCValue()
        : Object(MULTI_CVALUE) {}
    MultiCValue(CValuePtr pv)
        : Object(MULTI_CVALUE) {
        values.push_back(pv);
    }
    MultiCValue(llvm::ArrayRef<CValuePtr> values)
        : Object(MULTI_CVALUE), values(values) {}
    size_t size() { return values.size(); }
    void add(CValuePtr x) { values.push_back(x); }
    void add(MultiCValuePtr x) {
        values.insert(values.end(), x->values.begin(), x->values.end());
    }

    void toArgsKey(vector<TypePtr> *types)
    {
        for (vector<CValuePtr>::const_iterator i = values.begin(), end = values.end();
             i != end;
             ++i)
        {
            types->push_back((*i)->type);
        }
    }
};

struct JumpTarget {
    llvm::BasicBlock *block;
    size_t stackMarker;
    int useCount;
    JumpTarget() : block(NULL), stackMarker(0), useCount(0) {}
    JumpTarget(llvm::BasicBlock *block, size_t stackMarker)
        : block(block), stackMarker(stackMarker), useCount(0) {}
};

struct CReturn {
    TypePtr type;
    CValuePtr value;
    bool byRef:1;
    CReturn(bool byRef, TypePtr type, CValuePtr value)
        : type(type), value(value), byRef(byRef) {}
};

struct StackSlot {
    llvm::Type *llType;
    llvm::Value *llValue;
    StackSlot(llvm::Type *llType, llvm::Value *llValue)
        : llType(llType), llValue(llValue) {}
};

enum ValueStackEntryType {
    LOCAL_VALUE,
    FINALLY_STATEMENT,
    ONERROR_STATEMENT
};

struct ValueStackEntry {
    ValueStackEntryType type;
    CValuePtr value;
    EnvPtr statementEnv;
    StatementPtr statement;

    explicit ValueStackEntry(CValuePtr value)
        : type(LOCAL_VALUE), value(value), statementEnv(NULL), statement(NULL) {}
    ValueStackEntry(ValueStackEntryType type,
        EnvPtr statementEnv,
        StatementPtr statement)
        : type(type), value(NULL), statementEnv(statementEnv), statement(statement)
    {
        assert(type != LOCAL_VALUE);
    }
};

struct CodegenContext {
    CompilerState* cst;

    llvm::Function *llvmFunc;
    vector<llvm::TrackingVH<llvm::MDNode> > debugScope;
    llvm::IRBuilder<> *initBuilder;
    llvm::IRBuilder<> *builder;

    vector<ValueStackEntry> valueStack;
    llvm::Value *valueForStatics;

    vector<StackSlot> allocatedSlots;
    vector<StackSlot> discardedSlots;

    vector< vector<CReturn> > returnLists;
    vector<JumpTarget> returnTargets;
    llvm::StringMap<JumpTarget> labels;
    vector<JumpTarget> breaks;
    vector<JumpTarget> continues;
    vector<JumpTarget> exceptionTargets;
    llvm::Value *exceptionValue;
    int inlineDepth; //:31;
    bool checkExceptions:1;

    int callByNameDepth;

    CodegenContext(CompilerState* cst)
        : llvmFunc(NULL),
          initBuilder(NULL),
          builder(NULL),
          valueForStatics(NULL),
          exceptionValue(NULL),
          inlineDepth(0),
          checkExceptions(true),
          callByNameDepth(0),
          cst(cst)
    {
    }

    CodegenContext(CompilerState* cst, llvm::Function *llvmFunc)
        : llvmFunc(llvmFunc),
          initBuilder(NULL),
          builder(NULL),
          valueForStatics(NULL),
          exceptionValue(NULL),
          inlineDepth(0),
          checkExceptions(true),
          callByNameDepth(0),
          cst(cst)
    {
    }

    ~CodegenContext() {
        delete builder;
        delete initBuilder;
    }

    llvm::DILexicalBlock getDebugScope() {
        if (debugScope.empty())
            return llvm::DILexicalBlock(NULL);
        else
            return llvm::DILexicalBlock(debugScope.back());
    }

    void pushDebugScope(llvm::DILexicalBlock scope) {
        debugScope.push_back((llvm::MDNode*)scope);
    }
    void popDebugScope() {
        debugScope.pop_back();
    }
};

struct DebugLocationContext {
    Location loc;
    CodegenContext* ctx;
    DebugLocationContext(Location const &loc, CodegenContext* ctx)
        : loc(loc), ctx(ctx)
    {
        if (loc.ok()) {
            pushLocation(loc);
            if (ctx->cst->llvmDIBuilder != NULL && ctx->inlineDepth == 0) {
                unsigned line, column;
                getDebugLineCol(loc, line, column);
                llvm::DebugLoc debugLoc = llvm::DebugLoc::get(line, column, ctx->getDebugScope());
                ctx->builder->SetCurrentDebugLocation(debugLoc);
            }
        }
    }
    ~DebugLocationContext() {
        if (loc.ok()) {
            popLocation();
        }
    }
private :
    DebugLocationContext(const DebugLocationContext &) {}
    void operator=(const DebugLocationContext &) {}
};


void codegenGVarInstance(GVarInstancePtr x);
void codegenExternalVariable(ExternalVariablePtr x);
void codegenExternalProcedure(ExternalProcedurePtr x, bool codegenBody);

struct InvokeEntry;

InvokeEntry* codegenCallable(ObjectPtr x,
                             llvm::ArrayRef<TypePtr> argsKey,
                             llvm::ArrayRef<ValueTempness> 
                                 argsTempness,
                             CompilerState* cst);
void codegenCodeBody(InvokeEntry* entry);
void codegenCWrapper(InvokeEntry* entry);

void codegenEntryPoints(ModulePtr module, bool importedExternals);
void codegenMain(ModulePtr module);

void codegenBeforeRepl(ModulePtr module);
void codegenAfterRepl(llvm::Function*& ctor,
                      llvm::Function*& dtor, CompilerState* cst);

}

#endif // __CODEGEN_HPP
