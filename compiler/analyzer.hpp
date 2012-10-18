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


void initializeStaticForClones(StaticForPtr x, unsigned count);
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


PVData safeAnalyzeOne(ExprPtr expr, EnvPtr env);
MultiPValuePtr safeAnalyzeMulti(ExprListPtr exprs, EnvPtr env, unsigned wantCount);
MultiPValuePtr safeAnalyzeExpr(ExprPtr expr, EnvPtr env);
MultiPValuePtr safeAnalyzeIndexingExpr(ExprPtr indexable,
                                       ExprListPtr args,
                                       EnvPtr env);
MultiPValuePtr safeAnalyzeMultiArgs(ExprListPtr exprs,
                                    EnvPtr env,
                                    vector<unsigned> &dispatchIndices);
InvokeEntry* safeAnalyzeCallable(ObjectPtr x,
                                 const vector<TypePtr> &argsKey,
                                 const vector<ValueTempness> &argsTempness);
MultiPValuePtr safeAnalyzeCallByName(InvokeEntry* entry,
                                     ExprPtr callable,
                                     ExprListPtr args,
                                     EnvPtr env);
MultiPValuePtr safeAnalyzeGVarInstance(GVarInstancePtr x);

MultiPValuePtr analyzeMulti(ExprListPtr exprs, EnvPtr env, unsigned wantCount);
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
                                   const vector<ObjectPtr> &params);
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
MultiPValuePtr analyzeReturn(const vector<bool> &returnIsRef,
                             const vector<TypePtr> &returnTypes);
MultiPValuePtr analyzeCallExpr(ExprPtr callable,
                               ExprListPtr args,
                               EnvPtr env);
PVData analyzeDispatchIndex(PVData const &pv, int tag);
MultiPValuePtr analyzeDispatch(ObjectPtr obj,
                               MultiPValuePtr args,
                               const vector<unsigned> &dispatchIndices);
MultiPValuePtr analyzeCallValue(PVData const &callable,
                                MultiPValuePtr args);
MultiPValuePtr analyzeCallPointer(PVData const &x,
                                  MultiPValuePtr args);
bool analyzeIsDefined(ObjectPtr x,
                      const vector<TypePtr> &argsKey,
                      const vector<ValueTempness> &argsTempness);
InvokeEntry* analyzeCallable(ObjectPtr x,
                             const vector<TypePtr> &argsKey,
                             const vector<ValueTempness> &argsTempness);

MultiPValuePtr analyzeCallByName(InvokeEntry* entry,
                                 ExprPtr callable,
                                 ExprListPtr args,
                                 EnvPtr env);

void analyzeCodeBody(InvokeEntry* entry);

struct AnalysisContext {
    bool hasRecursivePropagation;
    bool returnInitialized;
    vector<bool> returnIsRef;
    vector<TypePtr> returnTypes;
    AnalysisContext()
        : hasRecursivePropagation(false),
          returnInitialized(false) {}
};

enum StatementAnalysis {
    SA_FALLTHROUGH,
    SA_RECURSIVE,
    SA_TERMINATED
};

ExprPtr implicitUnpackExpr(unsigned wantCount, ExprListPtr exprs);

}

#endif // __ANALYZER_HPP
