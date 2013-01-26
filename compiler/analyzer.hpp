#ifndef __ANALYZER_HPP
#define __ANALYZER_HPP

#include "clay.hpp"

#include "invoketables.hpp"
#include "types.hpp"

namespace clay {


void disableAnalysisCaching(CompilerStatePtr cst);
void enableAnalysisCaching(CompilerStatePtr cst);

struct AnalysisCachingDisabler {
    CompilerStatePtr cst;
    AnalysisCachingDisabler(CompilerStatePtr cst) :cst(cst) {
        disableAnalysisCaching(cst); 
    }
    ~AnalysisCachingDisabler() {
        enableAnalysisCaching(cst);
    }
};


void initializeStaticForClones(StaticForPtr x, size_t count, CompilerStatePtr cst);
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


PVData safeAnalyzeOne(ExprPtr expr, EnvPtr env, CompilerStatePtr cst);
MultiPValuePtr safeAnalyzeMulti(ExprListPtr exprs, EnvPtr env, 
                                size_t wantCount, CompilerStatePtr cst);
MultiPValuePtr safeAnalyzeExpr(ExprPtr expr, EnvPtr env, CompilerStatePtr cst);
MultiPValuePtr safeAnalyzeIndexingExpr(ExprPtr indexable,
                                       ExprListPtr args,
                                       EnvPtr env,
                                       CompilerStatePtr cst);
MultiPValuePtr safeAnalyzeMultiArgs(ExprListPtr exprs,
                                    EnvPtr env,
                                    vector<unsigned> &dispatchIndices);
InvokeEntry* safeAnalyzeCallable(ObjectPtr x,
                                 llvm::ArrayRef<TypePtr> argsKey,
                                 llvm::ArrayRef<ValueTempness> argsTempness,
                                 CompilerStatePtr cst);
MultiPValuePtr safeAnalyzeCallByName(InvokeEntry* entry,
                                     ExprPtr callable,
                                     ExprListPtr args,
                                     EnvPtr env,
                                     CompilerStatePtr cst);
MultiPValuePtr safeAnalyzeGVarInstance(GVarInstancePtr x);

MultiPValuePtr analyzeMulti(ExprListPtr exprs, EnvPtr env, 
                            size_t wantCount, CompilerStatePtr cst);
PVData analyzeOne(ExprPtr expr, EnvPtr env, CompilerStatePtr cst);

MultiPValuePtr analyzeMultiArgs(ExprListPtr exprs,
                                EnvPtr env,
                                vector<unsigned> &dispatchIndices);
PVData analyzeOneArg(ExprPtr x,
                     EnvPtr env,
                     unsigned startIndex,
                     vector<unsigned> &dispatchIndices,
                     CompilerStatePtr cst);
MultiPValuePtr analyzeArgExpr(ExprPtr x,
                              EnvPtr env,
                              unsigned startIndex,
                              vector<unsigned> &dispatchIndices,
                              CompilerStatePtr cst);

MultiPValuePtr analyzeExpr(ExprPtr expr, EnvPtr env, CompilerStatePtr cst);
MultiPValuePtr analyzeStaticObject(ObjectPtr x, CompilerStatePtr cst);

GVarInstancePtr lookupGVarInstance(GlobalVariablePtr x,
                                   llvm::ArrayRef<ObjectPtr> params);
GVarInstancePtr defaultGVarInstance(GlobalVariablePtr x);
GVarInstancePtr analyzeGVarIndexing(GlobalVariablePtr x,
                                    ExprListPtr args,
                                    EnvPtr env,
                                    CompilerStatePtr cst);
MultiPValuePtr analyzeGVarInstance(GVarInstancePtr x);

PVData analyzeExternalVariable(ExternalVariablePtr x, CompilerStatePtr cst);
void analyzeExternalProcedure(ExternalProcedurePtr x);
void verifyAttributes(ExternalProcedurePtr x, CompilerStatePtr cst);
void verifyAttributes(ExternalVariablePtr x, CompilerStatePtr cst);
void verifyAttributes(ModulePtr x);
MultiPValuePtr analyzeIndexingExpr(ExprPtr indexable,
                                   ExprListPtr args,
                                   EnvPtr env,
                                   CompilerStatePtr cst);
bool unwrapByRef(TypePtr &t);
TypePtr constructType(ObjectPtr constructor, MultiStaticPtr args);
PVData analyzeTypeConstructor(ObjectPtr obj, MultiStaticPtr args);
MultiPValuePtr analyzeAliasIndexing(GlobalAliasPtr x,
                                    ExprListPtr args,
                                    EnvPtr env,
                                    CompilerStatePtr cst);
void computeArgsKey(MultiPValuePtr args,
                    vector<TypePtr> &argsKey,
                    vector<ValueTempness> &argsTempness);
MultiPValuePtr analyzeReturn(llvm::ArrayRef<uint8_t> returnIsRef,
                             llvm::ArrayRef<TypePtr> returnTypes);
MultiPValuePtr analyzeCallExpr(ExprPtr callable,
                               ExprListPtr args,
                               EnvPtr env,
                               CompilerStatePtr cst);
PVData analyzeDispatchIndex(PVData const &pv, unsigned tag, CompilerStatePtr cst);
MultiPValuePtr analyzeDispatch(ObjectPtr obj,
                               MultiPValuePtr args,
                               llvm::ArrayRef<unsigned> dispatchIndices,
                               CompilerStatePtr cst);
MultiPValuePtr analyzeCallValue(PVData const &callable,
                                MultiPValuePtr args,
                                CompilerStatePtr cst);
MultiPValuePtr analyzeCallPointer(PVData const &x,
                                  MultiPValuePtr args);
bool analyzeIsDefined(ObjectPtr x,
                      llvm::ArrayRef<TypePtr> argsKey,
                      llvm::ArrayRef<ValueTempness> argsTempness,
                      CompilerStatePtr cst);
InvokeEntry* analyzeCallable(ObjectPtr x,
                             llvm::ArrayRef<TypePtr> argsKey,
                             llvm::ArrayRef<ValueTempness> argsTempness,
                             CompilerStatePtr cst);

MultiPValuePtr analyzeCallByName(InvokeEntry* entry,
                                 ExprPtr callable,
                                 ExprListPtr args,
                                 EnvPtr env,
                                 CompilerStatePtr cst);

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
