#ifndef __ANALYZER_HPP
#define __ANALYZER_HPP

#include "clay.hpp"

#include "invoketables.hpp"
#include "types.hpp"

namespace clay {


void disableAnalysisCaching();
void enableAnalysisCaching();

struct AnalysisCachingDisabler {
    AnalysisCachingDisabler() { disableAnalysisCaching(); }
    ~AnalysisCachingDisabler() { enableAnalysisCaching(); }
};


void initializeStaticForClones(StaticForPtr x, size_t count);
bool returnKindToByRef(ReturnKind returnKind, PVData const &pv);

ObjectPtr unwrapStaticType(TypePtr t);

bool staticToBool(ObjectPtr x, bool &out, TypePtr &type);
bool staticToBool(MultiStaticPtr x, unsigned index);
bool staticToCallingConv(ObjectPtr x, CallingConv &out);
CallingConv staticToCallingConv(MultiStaticPtr x, unsigned index);

static inline PVData staticPValue(ObjectPtr x)
{
    TypePtr t = staticType(x);
    return PVData(t, true);
}

enum BoolKind {
    BOOL_EXPR,
    BOOL_STATIC_TRUE,
    BOOL_STATIC_FALSE
};

BoolKind typeBoolKind(TypePtr type);


PVData safeAnalyzeOne(ExprPtr expr, EnvPtr env);
MultiPValuePtr safeAnalyzeMulti(ExprListPtr exprs, EnvPtr env, size_t wantCount);
MultiPValuePtr safeAnalyzeExpr(ExprPtr expr, EnvPtr env);
MultiPValuePtr safeAnalyzeIndexingExpr(ExprPtr indexable,
                                       ExprListPtr args,
                                       EnvPtr env);
MultiPValuePtr safeAnalyzeMultiArgs(ExprListPtr exprs,
                                    EnvPtr env,
                                    vector<unsigned> &dispatchIndices);
InvokeEntry* safeAnalyzeCallable(ObjectPtr x,
                                 llvm::ArrayRef<TypePtr> argsKey,
                                 llvm::ArrayRef<ValueTempness> argsTempness);
MultiPValuePtr safeAnalyzeCallByName(InvokeEntry* entry,
                                     ExprPtr callable,
                                     ExprListPtr args,
                                     EnvPtr env);
MultiPValuePtr safeAnalyzeGVarInstance(GVarInstancePtr x);

MultiPValuePtr analyzeMulti(ExprListPtr exprs, EnvPtr env, size_t wantCount);
PVData analyzeOne(ExprPtr expr, EnvPtr env);

MultiPValuePtr analyzeMultiArgs(ExprListPtr exprs,
                                EnvPtr env,
                                vector<unsigned> &dispatchIndices);
PVData analyzeOneArg(ExprPtr x,
                        EnvPtr env,
                        unsigned startIndex,
                        vector<unsigned> &dispatchIndices);
MultiPValuePtr analyzeArgExpr(ExprPtr x,
                              EnvPtr env,
                              unsigned startIndex,
                              vector<unsigned> &dispatchIndices);

MultiPValuePtr analyzeExpr(ExprPtr expr, EnvPtr env);
MultiPValuePtr analyzeStaticObject(ObjectPtr x);

GVarInstancePtr lookupGVarInstance(GlobalVariablePtr x,
                                   llvm::ArrayRef<ObjectPtr> params);
GVarInstancePtr defaultGVarInstance(GlobalVariablePtr x);
GVarInstancePtr analyzeGVarIndexing(GlobalVariablePtr x,
                                    ExprListPtr args,
                                    EnvPtr env);
MultiPValuePtr analyzeGVarInstance(GVarInstancePtr x);

PVData analyzeExternalVariable(ExternalVariablePtr x);
void analyzeExternalProcedure(ExternalProcedurePtr x);
void verifyAttributes(ExternalProcedurePtr x);
void verifyAttributes(ExternalVariablePtr x);
void verifyAttributes(ModulePtr x);
MultiPValuePtr analyzeIndexingExpr(ExprPtr indexable,
                                   ExprListPtr args,
                                   EnvPtr env);
bool unwrapByRef(TypePtr &t);
TypePtr constructType(ObjectPtr constructor, MultiStaticPtr args);
PVData analyzeTypeConstructor(ObjectPtr obj, MultiStaticPtr args);
MultiPValuePtr analyzeAliasIndexing(GlobalAliasPtr x,
                                    ExprListPtr args,
                                    EnvPtr env);
void computeArgsKey(MultiPValuePtr args,
                    vector<TypePtr> &argsKey,
                    vector<ValueTempness> &argsTempness);
MultiPValuePtr analyzeReturn(llvm::ArrayRef<uint8_t> returnIsRef,
                             llvm::ArrayRef<TypePtr> returnTypes);
MultiPValuePtr analyzeCallExpr(ExprPtr callable,
                               ExprListPtr args,
                               EnvPtr env);
PVData analyzeDispatchIndex(PVData const &pv, unsigned tag);
MultiPValuePtr analyzeDispatch(ObjectPtr obj,
                               MultiPValuePtr args,
                               llvm::ArrayRef<unsigned> dispatchIndices);
MultiPValuePtr analyzeCallValue(PVData const &callable,
                                MultiPValuePtr args);
MultiPValuePtr analyzeCallPointer(PVData const &x,
                                  MultiPValuePtr args);
bool analyzeIsDefined(ObjectPtr x,
                      llvm::ArrayRef<TypePtr> argsKey,
                      llvm::ArrayRef<ValueTempness> argsTempness);
InvokeEntry* analyzeCallable(ObjectPtr x,
                             llvm::ArrayRef<TypePtr> argsKey,
                             llvm::ArrayRef<ValueTempness> argsTempness);

MultiPValuePtr analyzeCallByName(InvokeEntry* entry,
                                 ExprPtr callable,
                                 ExprListPtr args,
                                 EnvPtr env);

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
