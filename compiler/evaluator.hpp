#ifndef __EVALUATOR_HPP
#define __EVALUATOR_HPP

#include "clay.hpp"

namespace clay {

struct EValue : public Object {
    TypePtr type;
    char *addr;
    bool forwardedRValue;
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
    MultiEValue(const vector<EValuePtr> &values)
        : Object(MULTI_EVALUE), values(values) {}
    size_t size() { return values.size(); }
    void add(EValuePtr x) { values.push_back(x); }
    void add(MultiEValuePtr x) {
        values.insert(values.end(), x->values.begin(), x->values.end());
    }
};

bool staticToType(ObjectPtr x, TypePtr &out);
TypePtr staticToType(MultiStaticPtr x, unsigned index);

void evaluateReturnSpecs(const vector<ReturnSpecPtr> &returnSpecs,
                         ReturnSpecPtr varReturnSpec,
                         EnvPtr env,
                         vector<bool> &isRef,
                         vector<TypePtr> &types);

MultiStaticPtr evaluateExprStatic(ExprPtr expr, EnvPtr env);
ObjectPtr evaluateOneStatic(ExprPtr expr, EnvPtr env);
MultiStaticPtr evaluateMultiStatic(ExprListPtr exprs, EnvPtr env);

TypePtr evaluateType(ExprPtr expr, EnvPtr env);
void evaluateMultiType(ExprListPtr exprs, EnvPtr env, vector<TypePtr> &out);
IdentifierPtr evaluateIdentifier(ExprPtr expr, EnvPtr env);
bool evaluateBool(ExprPtr expr, EnvPtr env);
void evaluatePredicate(const vector<PatternVar> &patternVars,
    ExprPtr expr, EnvPtr env);
void evaluateStaticAssert(Location const& location,
        const ExprPtr& cond, const ExprListPtr& message, EnvPtr env);

ValueHolderPtr intToValueHolder(int x);
ValueHolderPtr sizeTToValueHolder(size_t x);
ValueHolderPtr ptrDiffTToValueHolder(ptrdiff_t x);
ValueHolderPtr boolToValueHolder(bool x);

size_t valueHolderToSizeT(ValueHolderPtr vh);

ObjectPtr makeTupleValue(const vector<ObjectPtr> &elements);
ObjectPtr evalueToStatic(EValuePtr ev);

void evalValueInit(EValuePtr dest);
void evalValueDestroy(EValuePtr dest);
void evalValueCopy(EValuePtr dest, EValuePtr src);
void evalValueMove(EValuePtr dest, EValuePtr src);
void evalValueAssign(EValuePtr dest, EValuePtr src);
void evalValueMoveAssign(EValuePtr dest, EValuePtr src);
bool evalToBoolFlag(EValuePtr a, bool acceptStatics);

int evalMarkStack();
void evalDestroyStack(int marker);
void evalPopStack(int marker);
void evalDestroyAndPopStack(int marker);
EValuePtr evalAllocValue(TypePtr t);

EValuePtr evalOneAsRef(ExprPtr expr, EnvPtr env);

}

#endif // __EVALUATOR_HPP
