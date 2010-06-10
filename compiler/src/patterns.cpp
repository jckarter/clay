#include "clay.hpp"



//
// isPatternHead
//

static bool isPatternHead(ObjectPtr x)
{
    switch (x->objKind) {
    case PRIM_OP : {
        PrimOpPtr y = (PrimOp *)x.ptr();
        switch (y->primOpCode) {
        case PRIM_Pointer :
        case PRIM_Array :
        case PRIM_Tuple :
        case PRIM_Static :
            return true;
        default :
            return false;
        }
        assert(false);
    }
    case RECORD :
        return true;
    default :
        return false;
    }
    assert(false);
}



//
// evaluatePattern
//

PatternPtr evaluatePattern(ExprPtr expr, EnvPtr env)
{
    LocationContext loc(expr->location);

    switch (expr->exprKind) {

    case NAME_REF : {
        NameRef *x = (NameRef *)expr.ptr();
        ObjectPtr y = lookupEnv(env, x->name);
        return evaluateStaticObjectPattern(y);
    }

    case INDEXING : {
        Indexing *x = (Indexing *)expr.ptr();
        ObjectPtr indexable = evaluateOneStatic(x->expr, env);
        if (isPatternHead(indexable)) {
            vector<PatternPtr> params;
            for (unsigned i = 0; i < x->args.size(); ++i)
                params.push_back(evaluatePattern(x->args[i], env));
            return new PatternTerm(indexable, params);
        }
        return new PatternCell(NULL, evaluateOneStatic(expr, env));
    }

    default : {
        ObjectPtr y = evaluateOneStatic(expr, env);
        return new PatternCell(NULL, y);
    }

    }
}



//
// evaluateStaticObjectPattern
//

PatternPtr evaluateStaticObjectPattern(ObjectPtr x)
{
    switch (x->objKind) {
    case PATTERN : {
        PatternPtr y = (Pattern *)x.ptr();
        assert(y->patternKind == PATTERN_CELL);
        return y;
    }
    case GLOBAL_ALIAS : {
        GlobalAlias *y = (GlobalAlias *)x.ptr();
        if (y->hasParams()) {
            return new PatternCell(NULL, x);
        }
        return evaluatePattern(y->expr, y->env);
    }
    default :
        return new PatternCell(NULL, x);
    }
}



//
// unify, unifyList
//

bool unify(PatternPtr pattern, ObjectPtr obj)
{
    switch (pattern->patternKind) {

    case PATTERN_CELL : {
        PatternCell *x = (PatternCell *)pattern.ptr();
        if (!x->obj) {
            x->obj = obj;
            return true;
        }
        return objectEquals(x->obj, obj);
    }

    case PATTERN_TERM : {
        PatternTerm *x = (PatternTerm *)pattern.ptr();
        return unifyTerm(x, obj);
    }

    default :
        assert(false);
        return false;

    }
}

bool unifyList(const vector<PatternPtr> patterns,
               const vector<ObjectPtr> objs)
{
    if (patterns.size() != objs.size())
        return false;
    for (unsigned i = 0; i < patterns.size(); ++i) {
        if (!unify(patterns[i], objs[i]))
            return false;
    }
    return true;
}



//
// unifyTerm
//

bool unifyTerm(PatternTermPtr term, ObjectPtr obj)
{
    switch (term->head->objKind) {

    case PRIM_OP : {
        PrimOpPtr x = (PrimOp *)term->head.ptr();
        switch (x->primOpCode) {
        case PRIM_Pointer : {
            if (obj->objKind != TYPE)
                return false;
            Type *t = (Type *)obj.ptr();
            if (t->typeKind != POINTER_TYPE)
                return false;
            PointerType *pt = (PointerType *)t;
            vector<ObjectPtr> objs;
            objs.push_back(pt->pointeeType.ptr());
            return unifyList(term->params, objs);
        }
        case PRIM_Array : {
            if (obj->objKind != TYPE)
                return false;
            Type *t = (Type *)obj.ptr();
            if (t->typeKind != ARRAY_TYPE)
                return false;
            ArrayType *at = (ArrayType *)t;
            vector<ObjectPtr> objs;
            objs.push_back(at->elementType.ptr());
            objs.push_back(intToValueHolder(at->size).ptr());
            return unifyList(term->params, objs);
        }
        case PRIM_Tuple : {
            if (obj->objKind != TYPE)
                return false;
            Type *t = (Type *)obj.ptr();
            if (t->typeKind != TUPLE_TYPE)
                return false;
            TupleType *tt = (TupleType *)t;
            vector<ObjectPtr> objs;
            for (unsigned i = 0; i < tt->elementTypes.size(); ++i)
                objs.push_back(tt->elementTypes[i].ptr());
            return unifyList(term->params, objs);
        }
        case PRIM_Static : {
            if (obj->objKind != TYPE)
                return false;
            Type *t = (Type *)obj.ptr();
            if (t->typeKind != STATIC_TYPE)
                return false;
            StaticType *st = (StaticType *)t;
            vector<ObjectPtr> objs;
            objs.push_back(st->obj);
            return unifyList(term->params, objs);
        }
        default :
            return false;
        }
    }

    case RECORD : {
        if (obj->objKind != TYPE)
            return false;
        Type *t = (Type *)obj.ptr();
        if (t->typeKind != RECORD_TYPE)
            return false;
        RecordType *rt = (RecordType *)t;
        if (term->head != rt->record.ptr())
            return false;
        return unifyList(term->params, rt->params);
    }

    default :
        return false;

    }
}



//
// derefCell, reducePattern
//

ObjectPtr derefCell(PatternCellPtr cell)
{
    if (!cell->obj) {
        if (cell->name.ptr())
            error(cell->name, "unresolved pattern variable");
        else
            error("unresolved pattern variable");
    }
    return cell->obj;
}

ObjectPtr reducePattern(PatternPtr pattern)
{
    if (pattern->patternKind == PATTERN_CELL) {
        PatternCellPtr x = (PatternCell *)pattern.ptr();
        if (x->obj.ptr())
            return x->obj;
    }
    return pattern.ptr();
}



//
// patternPrint
//

void patternPrint(ostream &out, PatternPtr x)
{
    switch (x->patternKind) {
    case PATTERN_CELL : {
        PatternCell *y = (PatternCell *)x.ptr();
        out << "PatternCell(" << y->name << ", " << y->obj << ")";
        break;
    }
    case PATTERN_TERM : {
        PatternTerm *y = (PatternTerm *)x.ptr();
        out << "PatternTerm(" << y->head << ", " << y->params << ")";
        break;
    }
    default :
        assert(false);
    }
}
