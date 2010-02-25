#include "clay.hpp"

vector<OverloadPtr> typeOverloads;

void addTypeOverload(OverloadPtr x)
{
    typeOverloads.insert(typeOverloads.begin(), x);
}

void initializeTypeOverloads(TypePtr t)
{
    assert(!t->overloadsInitialized);

    for (unsigned i = 0; i < typeOverloads.size(); ++i) {
        OverloadPtr x = typeOverloads[i];
        EnvPtr env = new Env(x->env);
        const vector<IdentifierPtr> &pvars = x->code->patternVars;
        for (unsigned i = 0; i < pvars.size(); ++i) {
            PatternCellPtr cell = new PatternCell(pvars[i], NULL);
            addLocal(env, pvars[i], cell.ptr());
        }
        PatternPtr pattern = evaluatePattern(x->target, env);
        if (unify(pattern, t.ptr()))
            t->overloads.insert(t->overloads.begin(), x);
    }

    t->overloadsInitialized = true;
}
