#ifndef __EVALUATOR_HPP
#define __EVALUATOR_HPP

#include "clay.hpp"

namespace clay {

struct EValue : public Object {
    TypePtr type;
    char *addr;
    bool forwardedRValue:1;
    EValue(TypePtr type, char *addr)
        : Object(EVALUE), type(type), addr(addr),
          forwardedRValue(false) {}
    EValue(TypePtr type, char *addr, bool forwardedRValue)
        : Object(EVALUE), type(type), addr(addr),
          forwardedRValue(forwardedRValue) {}

    template<typename T>
    T &as() { return *(T*)addr; }

    template<typename T>
    T const &as() const { return *(T const *)addr; }
};

struct MultiEValue : public Object {
    vector<EValuePtr> values;
    MultiEValue()
        : Object(MULTI_EVALUE) {}
    MultiEValue(EValuePtr pv)
        : Object(MULTI_EVALUE) {
        values.push_back(pv);
    }
    MultiEValue(llvm::ArrayRef<EValuePtr> values)
        : Object(MULTI_EVALUE), values(values) {}
    size_t size() { return values.size(); }
    void add(EValuePtr x) { values.push_back(x); }
    void add(MultiEValuePtr x) {
        values.insert(values.end(), x->values.begin(), x->values.end());
    }
};

bool staticToType(ObjectPtr x, TypePtr &out);
TypePtr staticToType(MultiStaticPtr x, size_t index);

void evaluateReturnSpecs(llvm::ArrayRef<ReturnSpecPtr> returnSpecs,
                         ReturnSpecPtr varReturnSpec,
                         EnvPtr env,
                         vector<uint8_t> &isRef,
                         vector<TypePtr> &types,
                         CompilerStatePtr cst);

MultiStaticPtr evaluateExprStatic(ExprPtr expr, EnvPtr env, CompilerStatePtr cst);
ObjectPtr evaluateOneStatic(ExprPtr expr, EnvPtr env, CompilerStatePtr cst);
MultiStaticPtr evaluateMultiStatic(ExprListPtr exprs, EnvPtr env, CompilerStatePtr cst);

TypePtr evaluateType(ExprPtr expr, EnvPtr env, CompilerStatePtr cst);
void evaluateMultiType(ExprListPtr exprs, EnvPtr env, vector<TypePtr> &out,
                       CompilerStatePtr cst);
IdentifierPtr evaluateIdentifier(ExprPtr expr, EnvPtr env);
bool evaluateBool(ExprPtr expr, EnvPtr env, CompilerStatePtr cst);
void evaluatePredicate(llvm::ArrayRef<PatternVar> patternVars,
    ExprPtr expr, EnvPtr env, CompilerStatePtr cst);
void evaluateStaticAssert(Location const& location,
                          const ExprPtr& cond, const ExprListPtr& message, EnvPtr env,
                          CompilerStatePtr cst);

ValueHolderPtr intToValueHolder(int x, CompilerStatePtr cst);
ValueHolderPtr sizeTToValueHolder(size_t x, CompilerStatePtr cst);
ValueHolderPtr ptrDiffTToValueHolder(ptrdiff_t x);
ValueHolderPtr boolToValueHolder(bool x, CompilerStatePtr cst);

size_t valueHolderToSizeT(ValueHolderPtr vh);

ObjectPtr makeTupleValue(llvm::ArrayRef<ObjectPtr> elements,
                         CompilerStatePtr cst);
ObjectPtr evalueToStatic(EValuePtr ev);

void evalValueInit(EValuePtr dest);
void evalValueDestroy(EValuePtr dest);
void evalValueCopy(EValuePtr dest, EValuePtr src);
void evalValueMove(EValuePtr dest, EValuePtr src);
void evalValueAssign(EValuePtr dest, EValuePtr src);
void evalValueMoveAssign(EValuePtr dest, EValuePtr src);
bool evalToBoolFlag(EValuePtr a, bool acceptStatics);

unsigned evalMarkStack();
void evalDestroyStack(unsigned marker);
void evalPopStack(unsigned marker);
void evalDestroyAndPopStack(unsigned marker);
EValuePtr evalAllocValue(TypePtr t);

EValuePtr evalOneAsRef(ExprPtr expr, EnvPtr env, CompilerStatePtr cst);

}

#endif // __EVALUATOR_HPP
