#ifndef __ANALYZER_HPP
#define __ANALYZER_HPP

#include "clay.hpp"

#include "invoketables.hpp"
#include "types.hpp"

namespace clay {


void disableAnalysisCaching(CompilerState* cst);
void enableAnalysisCaching(CompilerState* cst);

struct AnalysisCachingDisabler {
    CompilerState* cst;
    AnalysisCachingDisabler(CompilerState* cst) :cst(cst) {
        disableAnalysisCaching(cst); 
    }
    ~AnalysisCachingDisabler() {
        enableAnalysisCaching(cst);
    }
};


void initializeStaticForClones(StaticForPtr x, size_t count, CompilerState* cst);
bool returnKindToByRef(ReturnKind returnKind, PVData const &pv);

MultiPValuePtr analyzePrimOp(PrimOpPtr x, MultiPValuePtr args);

ObjectPtr unwrapStaticType(TypePtr t);

bool staticToBool(ObjectPtr x, bool &out, TypePtr &type);
bool staticToBool(MultiStaticPtr x, unsigned index);
bool staticToCallingConv(ObjectPtr x, CallingConv &out);
CallingConv staticToCallingConv(MultiStaticPtr x, unsigned index);


enum BoolKind {
    BOOL_EXPR,
    BOOL_STATIC_TRUE,
    BOOL_STATIC_FALSE
};

BoolKind typeBoolKind(TypePtr type);


PVData safeAnalyzeOne(ExprPtr expr, EnvPtr env, CompilerState* cst);
MultiPValuePtr safeAnalyzeMulti(ExprListPtr exprs, EnvPtr env, 
                                size_t wantCount, CompilerState* cst);
MultiPValuePtr safeAnalyzeExpr(ExprPtr expr, EnvPtr env, CompilerState* cst);
MultiPValuePtr safeAnalyzeIndexingExpr(ExprPtr indexable,
                                       ExprListPtr args,
                                       EnvPtr env,
                                       CompilerState* cst);
MultiPValuePtr safeAnalyzeMultiArgs(ExprListPtr exprs,
                                    EnvPtr env,
                                    vector<unsigned> &dispatchIndices);
InvokeEntry* safeAnalyzeCallable(ObjectPtr x,
                                 llvm::ArrayRef<TypePtr> argsKey,
                                 llvm::ArrayRef<ValueTempness> argsTempness,
                                 CompilerState* cst);
MultiPValuePtr safeAnalyzeCallByName(InvokeEntry* entry,
                                     ExprPtr callable,
                                     ExprListPtr args,
                                     EnvPtr env,
                                     CompilerState* cst);
MultiPValuePtr safeAnalyzeGVarInstance(GVarInstancePtr x);

MultiPValuePtr analyzeMulti(ExprListPtr exprs, EnvPtr env, 
                            size_t wantCount, CompilerState* cst);
PVData analyzeOne(ExprPtr expr, EnvPtr env, CompilerState* cst);

MultiPValuePtr analyzeMultiArgs(ExprListPtr exprs,
                                EnvPtr env,
                                vector<unsigned> &dispatchIndices);
PVData analyzeOneArg(ExprPtr x,
                     EnvPtr env,
                     unsigned startIndex,
                     vector<unsigned> &dispatchIndices,
                     CompilerState* cst);
MultiPValuePtr analyzeArgExpr(ExprPtr x,
                              EnvPtr env,
                              unsigned startIndex,
                              vector<unsigned> &dispatchIndices,
                              CompilerState* cst);

MultiPValuePtr analyzeExpr(ExprPtr expr, EnvPtr env, CompilerState* cst);
MultiPValuePtr analyzeStaticObject(ObjectPtr x, CompilerState* cst);

GVarInstancePtr lookupGVarInstance(GlobalVariablePtr x,
                                   llvm::ArrayRef<ObjectPtr> params);
GVarInstancePtr defaultGVarInstance(GlobalVariablePtr x);
GVarInstancePtr analyzeGVarIndexing(GlobalVariablePtr x,
                                    ExprListPtr args,
                                    EnvPtr env,
                                    CompilerState* cst);
MultiPValuePtr analyzeGVarInstance(GVarInstancePtr x);

PVData analyzeExternalVariable(ExternalVariablePtr x, CompilerState* cst);
void analyzeExternalProcedure(ExternalProcedurePtr x);
void verifyAttributes(ExternalProcedurePtr x, CompilerState* cst);
void verifyAttributes(ExternalVariablePtr x, CompilerState* cst);
void verifyAttributes(ModulePtr x);
MultiPValuePtr analyzeIndexingExpr(ExprPtr indexable,
                                   ExprListPtr args,
                                   EnvPtr env,
                                   CompilerState* cst);
bool unwrapByRef(TypePtr &t);
TypePtr constructType(ObjectPtr constructor, MultiStaticPtr args);
PVData analyzeTypeConstructor(ObjectPtr obj, MultiStaticPtr args);
MultiPValuePtr analyzeAliasIndexing(GlobalAliasPtr x,
                                    ExprListPtr args,
                                    EnvPtr env,
                                    CompilerState* cst);
void computeArgsKey(MultiPValuePtr args,
                    vector<TypePtr> &argsKey,
                    vector<ValueTempness> &argsTempness);
MultiPValuePtr analyzeReturn(llvm::ArrayRef<uint8_t> returnIsRef,
                             llvm::ArrayRef<TypePtr> returnTypes);
MultiPValuePtr analyzeCallExpr(ExprPtr callable,
                               ExprListPtr args,
                               EnvPtr env,
                               CompilerState* cst);
PVData analyzeDispatchIndex(PVData const &pv, unsigned tag, CompilerState* cst);
MultiPValuePtr analyzeDispatch(ObjectPtr obj,
                               MultiPValuePtr args,
                               llvm::ArrayRef<unsigned> dispatchIndices,
                               CompilerState* cst);
MultiPValuePtr analyzeCallValue(PVData const &callable,
                                MultiPValuePtr args,
                                CompilerState* cst);
MultiPValuePtr analyzeCallPointer(PVData const &x,
                                  MultiPValuePtr args);
bool analyzeIsDefined(ObjectPtr x,
                      llvm::ArrayRef<TypePtr> argsKey,
                      llvm::ArrayRef<ValueTempness> argsTempness,
                      CompilerState* cst);
InvokeEntry* analyzeCallable(ObjectPtr x,
                             llvm::ArrayRef<TypePtr> argsKey,
                             llvm::ArrayRef<ValueTempness> argsTempness,
                             CompilerState* cst);

MultiPValuePtr analyzeCallByName(InvokeEntry* entry,
                                 ExprPtr callable,
                                 ExprListPtr args,
                                 EnvPtr env,
                                 CompilerState* cst);

void analyzeCodeBody(InvokeEntry* entry);

struct AnalysisContext {
    vector<uint8_t> returnIsRef;
    vector<TypePtr> returnTypes;
    bool hasRecursivePropagation:1;
    bool returnInitialized:1;
    AnalysisContext()
        : hasRecursivePropagation(false),
          returnInitialized(false) {}
};

enum StatementAnalysis {
    SA_FALLTHROUGH,
    SA_RECURSIVE,
    SA_TERMINATED
};

ExprPtr implicitUnpackExpr(size_t wantCount, ExprListPtr exprs);

}

#endif // __ANALYZER_HPP
