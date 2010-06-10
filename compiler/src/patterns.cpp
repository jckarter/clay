#include "clay.hpp"



//
// derefDeep
//

static ObjectPtr computeStruct(ObjectPtr head, MultiStaticPtr params)
{
    TypePtr t = constructType(head, params);
    return t.ptr();
}

ObjectPtr derefDeep(PatternPtr x)
{
    switch (x->kind) {
    case PATTERN_CELL : {
        PatternCell *y = (PatternCell *)x.ptr();
        if (y->obj.ptr() && (y->obj->objKind == PATTERN)) {
            Pattern *z = (Pattern *)y->obj.ptr();
            return derefDeep(z);
        }
        return y->obj;
    }
    case PATTERN_STRUCT : {
        PatternStruct *y = (PatternStruct *)x.ptr();
        MultiStaticPtr params = derefDeep(y->params);
        if (!params)
            return NULL;
        return computeStruct(y->head, params);
    }
    default :
        assert(false);
        return NULL;
    }
}

MultiStaticPtr derefDeep(MultiPatternPtr x)
{
    switch (x->kind) {
    case MULTI_PATTERN_CELL : {
        MultiPatternCell *y = (MultiPatternCell *)x.ptr();
        if (!y->data)
            return NULL;
        return derefDeep(y->data);
    }
    case MULTI_PATTERN_LIST : {
        MultiPatternList *y = (MultiPatternList *)x.ptr();
        MultiStaticPtr out = new MultiStatic();
        for (unsigned i = 0; i < y->items.size(); ++i) {
            ObjectPtr z = derefDeep(y->items[i]);
            if (!z)
                return NULL;
            out->add(z);
        }
        if (y->tail.ptr()) {
            MultiStaticPtr tail = derefDeep(y->tail);
            if (!tail)
                return NULL;
            out->add(tail);
        }
        return out;
    }
    default :
        assert(false);
        return NULL;
    }
}



//
// objectToPattern, objectToPatternStruct
//

static ObjectPtr primOpPointer() {
    static ObjectPtr obj;
    if (!obj)
        obj = primName("Pointer");
    return obj;
}

static ObjectPtr primOpArray() {
    static ObjectPtr obj;
    if (!obj)
        obj = primName("Array");
    return obj;
}

static ObjectPtr primOpTuple() {
    static ObjectPtr obj;
    if (!obj)
        obj = primName("Tuple");
    return obj;
}

static ObjectPtr primOpStatic() {
    static ObjectPtr obj;
    if (!obj)
        obj = primName("Static");
    return obj;
}

static PatternPtr objectToPattern(ObjectPtr obj)
{
    switch (obj->objKind) {
    case PATTERN : {
        Pattern *x = (Pattern *)obj.ptr();
        return x;
    }
    case MULTI_PATTERN : {
        error("incorrect usage of multi-valued pattern "
              "in single valued context");
        return NULL;
    }
    case TYPE : {
        Type *t = (Type *)obj.ptr();
        switch (t->typeKind) {
        case POINTER_TYPE : {
            PointerType *pt = (PointerType *)t;
            ObjectPtr head = primOpPointer();
            MultiPatternListPtr params = new MultiPatternList();
            params->items.push_back(objectToPattern(pt->pointeeType.ptr()));
            return new PatternStruct(head, params.ptr());
        }
        case ARRAY_TYPE : {
            ArrayType *at = (ArrayType *)t;
            ObjectPtr head = primOpArray();
            MultiPatternListPtr params = new MultiPatternList();
            params->items.push_back(objectToPattern(at->elementType.ptr()));
            ValueHolderPtr vh = intToValueHolder(at->size);
            params->items.push_back(objectToPattern(vh.ptr()));
            return new PatternStruct(head, params.ptr());
        }
        case TUPLE_TYPE : {
            TupleType *tt = (TupleType *)t;
            ObjectPtr head = primOpTuple();
            MultiPatternListPtr params = new MultiPatternList();
            for (unsigned i = 0; i < tt->elementTypes.size(); ++i) {
                PatternPtr x = objectToPattern(tt->elementTypes[i].ptr());
                params->items.push_back(x);
            }
            return new PatternStruct(head, params.ptr());
        }
        case STATIC_TYPE : {
            StaticType *st = (StaticType *)t;
            ObjectPtr head = primOpStatic();
            MultiPatternListPtr params = new MultiPatternList();
            params->items.push_back(objectToPattern(st->obj));
            return new PatternStruct(head, params.ptr());
        }
        case RECORD_TYPE : {
            RecordType *rt = (RecordType *)t;
            ObjectPtr head = rt->record.ptr();
            MultiPatternListPtr params = new MultiPatternList();
            for (unsigned i = 0; i < rt->params.size(); ++i) {
                PatternPtr x = objectToPattern(rt->params[i]);
                params->items.push_back(x);
            }
            return new PatternStruct(head, params.ptr());
        }
        default :
            return new PatternCell(obj);
        }
    }
    default :
        return new PatternCell(obj);
    }
}

static PatternStructPtr objectToPatternStruct(ObjectPtr obj)
{
    PatternPtr x = objectToPattern(obj);
    if (x->kind != PATTERN_STRUCT)
        return NULL;
    return (PatternStruct *)x.ptr();
}



//
// unify
//

bool unifyObjObj(ObjectPtr a, ObjectPtr b)
{
    if (a->objKind == PATTERN) {
        PatternPtr a2 = (Pattern *)a.ptr();
        return unifyPatternObj(a2, b);
    }
    else if (a->objKind == MULTI_PATTERN) {
        error("incorrect usage of multi-valued pattern "
              "in single valued context");
        return false;
    }
    else if (b->objKind == PATTERN) {
        PatternPtr b2 = (Pattern *)b.ptr();
        return unifyObjPattern(a, b2);
    }
    else if (b->objKind == MULTI_PATTERN) {
        error("incorrect usage of multi-valued pattern "
              "in single valued context");
        return false;
    }
    return objectEquals(a, b);
}

bool unifyObjPattern(ObjectPtr a, PatternPtr b)
{
    assert(a.ptr() && b.ptr());
    if (a->objKind == PATTERN) {
        PatternPtr a2 = (Pattern *)a.ptr();
        return unify(a2, b);
    }
    else if (a->objKind == MULTI_PATTERN) {
        error("incorrect usage of multi-valued pattern "
              "in single valued context");
        return false;
    }
    else if (b->kind == PATTERN_CELL) {
        PatternCell *b2 = (PatternCell *)b.ptr();
        if (!b2->obj) {
            b2->obj = a;
            return true;
        }
        return unifyObjObj(b2->obj, a);
    }
    else {
        assert(b->kind == PATTERN_STRUCT);
        PatternStruct *b2 = (PatternStruct *)b.ptr();
        PatternStructPtr a2 = objectToPatternStruct(a);
        if (!a2)
            return false;
        if (a2->head != b2->head)
            return false;
        return unifyMulti(a2->params, b2->params);
    }
}

bool unifyPatternObj(PatternPtr a, ObjectPtr b)
{
    return unifyObjPattern(b, a);
}

bool unify(PatternPtr a, PatternPtr b)
{
    assert(a.ptr() && b.ptr());
    if (a->kind == PATTERN_CELL) {
        PatternCell *a2 = (PatternCell *)a.ptr();
        if (!a2->obj) {
            a2->obj = b.ptr();
            return true;
        }
        return unifyObjPattern(a2->obj, b);
    }
    else if (b->kind == PATTERN_CELL) {
        PatternCell *b2 = (PatternCell *)b.ptr();
        if (!b2->obj) {
            b2->obj = a.ptr();
            return true;
        }
        return unifyPatternObj(a, b2->obj);
    }
    else {
        assert(a->kind == PATTERN_STRUCT);
        assert(b->kind == PATTERN_STRUCT);
        PatternStruct *a2 = (PatternStruct *)a.ptr();
        PatternStruct *b2 = (PatternStruct *)b.ptr();
        if (a2->head != b2->head)
            return false;
        return unifyMulti(a2->params, b2->params);
    }
}

bool unifyMulti(MultiPatternPtr a, MultiStaticPtr b)
{
    assert(a.ptr() && b.ptr());
    MultiPatternListPtr b2 = new MultiPatternList();
    for (unsigned i = 0; i < b->size(); ++i)
        b2->items.push_back(objectToPattern(b->values[i]));
    return unifyMulti(a, b2.ptr());
}

bool unifyMulti(MultiPatternPtr a, MultiPatternPtr b)
{
    assert(a.ptr() && b.ptr());
    switch (a->kind) {
    case MULTI_PATTERN_CELL : {
        MultiPatternCell *a2 = (MultiPatternCell *)a.ptr();
        if (!a2->data) {
            a2->data = b;
            return true;
        }
        return unifyMulti(a2->data, b);
    }
    case MULTI_PATTERN_LIST : {
        MultiPatternList *a2 = (MultiPatternList *)a.ptr();
        return unifyMulti(a2, 0, b);
    }
    default :
        assert(false);
        return false;
    }
}

static MultiPatternListPtr subList(MultiPatternListPtr x, unsigned index)
{
    MultiPatternListPtr subList = new MultiPatternList();
    for (unsigned i = index; i < x->items.size(); ++i)
        subList->items.push_back(x->items[i]);
    subList->tail = x->tail;
    return subList;
}

bool unifyMulti(MultiPatternListPtr a, unsigned indexA,
                MultiPatternPtr b)
{
    assert(a.ptr() && b.ptr());
    switch (b->kind) {
    case MULTI_PATTERN_CELL : {
        MultiPatternCell *b2 = (MultiPatternCell *)b.ptr();
        if (!b2->data) {
            b2->data = subList(a, indexA).ptr();
            return true;
        }
        return unifyMulti(a, indexA, b2->data);
    }
    case MULTI_PATTERN_LIST : {
        MultiPatternList *b2 = (MultiPatternList *)b.ptr();
        return unifyMulti(a, indexA, b2, 0);
    }
    default :
        assert(false);
        return false;
    }
}

bool unifyMulti(MultiPatternPtr a,
                MultiPatternListPtr b, unsigned indexB)
{
    assert(a.ptr() && b.ptr());
    return unifyMulti(b, indexB, a);
}

bool unifyMulti(MultiPatternListPtr a, unsigned indexA,
                MultiPatternListPtr b, unsigned indexB)
{
    assert(a.ptr() && b.ptr());
    while ((indexA < a->items.size()) &&
           (indexB < b->items.size()))
    {
        if (!unify(a->items[indexA++], b->items[indexB++]))
            return false;
    }
    if (indexA < a->items.size()) {
        assert(indexB == b->items.size());
        if (!b->tail)
            return false;
        return unifyMulti(a, indexA, b->tail);
    }
    else if (a->tail.ptr()) {
        return unifyMulti(a->tail, b, indexB);
    }
    return unifyEmpty(b, indexB);
}

bool unifyEmpty(MultiPatternListPtr x, unsigned index)
{
    if (index < x->items.size())
        return false;
    if (x->tail.ptr())
        return unifyEmpty(x->tail);
    return true;
}

bool unifyEmpty(MultiPatternPtr x)
{
    switch (x->kind) {
    case MULTI_PATTERN_CELL : {
        MultiPatternCell *y = (MultiPatternCell *)x.ptr();
        if (!y->data) {
            y->data = new MultiPatternList();
            return true;
        }
        return unifyEmpty(y->data);
    }
    case MULTI_PATTERN_LIST : {
        MultiPatternList *y = (MultiPatternList *)x.ptr();
        return unifyEmpty(y, 0);
    }
    default :
        assert(false);
        return false;
    }
}



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
// evaluateOnePattern
//

static PatternPtr namedToPattern(ObjectPtr x)
{
    switch (x->objKind) {
    case PATTERN : {
        PatternPtr y = (Pattern *)x.ptr();
        assert(y->kind == PATTERN_CELL);
        return y;
    }
    case MULTI_PATTERN : {
        error("incorrect usage of multi-valued pattern "
              "in single valued context");
        return NULL;
    }
    case GLOBAL_ALIAS : {
        GlobalAlias *y = (GlobalAlias *)x.ptr();
        if (y->hasParams()) {
            return new PatternCell(x);
        }
        return evaluateOnePattern(y->expr, y->env);
    }
    default :
        return new PatternCell(x);
    }
}


PatternPtr evaluateOnePattern(ExprPtr expr, EnvPtr env)
{
    LocationContext loc(expr->location);

    switch (expr->exprKind) {

    case NAME_REF : {
        NameRef *x = (NameRef *)expr.ptr();
        ObjectPtr y = lookupEnv(env, x->name);
        return namedToPattern(y);
    }

    case INDEXING : {
        Indexing *x = (Indexing *)expr.ptr();
        ObjectPtr indexable = evaluateOneStatic(x->expr, env);
        if (isPatternHead(indexable)) {
            MultiPatternPtr params = evaluateMultiPattern(x->args, env);
            return new PatternStruct(indexable, params);
        }
        return new PatternCell(evaluateOneStatic(expr, env));
    }

    default : {
        ObjectPtr y = evaluateOneStatic(expr, env);
        return new PatternCell(y);
    }

    }
}



//
// evaluateMultiPattern
//

static MultiPatternPtr checkMultiPatternNameRef(ExprPtr expr, EnvPtr env)
{
    if (expr->exprKind != NAME_REF)
        return NULL;
    NameRef *x = (NameRef *)expr.ptr();
    ObjectPtr obj = lookupEnv(env, x->name);
    if (obj->objKind != MULTI_PATTERN)
        return NULL;
    MultiPattern *mp = (MultiPattern *)obj.ptr();
    return mp;
}

static bool appendPattern(MultiPatternListPtr &cur, MultiPatternPtr x)
{
    assert(!cur || !cur->tail);
    assert(x.ptr());
    switch (x->kind) {
    case MULTI_PATTERN_CELL : {
        if (!cur)
            return false;
        MultiPatternCell *y = (MultiPatternCell *)x.ptr();
        if (y->data.ptr())
            return appendPattern(cur, y->data);
        cur->tail = y;
        cur = NULL;
        return true;
    }
    case MULTI_PATTERN_LIST : {
        MultiPatternList *y = (MultiPatternList *)x.ptr();
        if (!y->items.empty()) {
            if (!cur)
                return false;
            for (unsigned i = 0; i < y->items.size(); ++i)
                cur->items.push_back(y->items[i]);
        }
        if (y->tail.ptr())
            return appendPattern(cur, y->tail);
        return true;
    }
    default :
        assert(false);
        return false;
    }
}

MultiPatternPtr evaluateMultiPattern(const vector<ExprPtr> &exprs,
                                     EnvPtr env)
{
    MultiPatternListPtr out = new MultiPatternList();
    MultiPatternListPtr cur = out;
    for (unsigned i = 0; i < exprs.size(); ++i) {
        assert(!cur || !cur->tail);
        ExprPtr x = exprs[i];
        if (x->exprKind == UNPACK) {
            Unpack *y = (Unpack *)x.ptr();
            MultiPatternPtr mp = checkMultiPatternNameRef(y->expr, env);
            if (mp.ptr()) {
                if (!appendPattern(cur, mp))
                    error(x, "expressions cannot occur after "
                          "multi-pattern variable");
            }
            else {
                MultiStaticPtr z = evaluateExprStatic(y->expr, env);
                if (!cur && (z->size() > 0))
                    error(x, "expressions cannot occur after "
                          "multi-pattern variable");
                for (unsigned j = 0; j < z->size(); ++j) {
                    PatternPtr p = new PatternCell(z->values[i]);
                    cur->items.push_back(p);
                }
            }
        }
        else {
            if (!cur)
                error(x, "expressions cannot occur after "
                      "multi-pattern variable");
            PatternPtr y = evaluateOnePattern(x, env);
            cur->items.push_back(y);
        }
    }
    return out.ptr();
}



//
// patternPrint
//

void patternPrint(ostream &out, PatternPtr x)
{
    switch (x->kind) {
    case PATTERN_CELL : {
        PatternCell *y = (PatternCell *)x.ptr();
        out << "PatternCell(" << y->obj << ")";
        break;
    }
    case PATTERN_STRUCT : {
        PatternStruct *y = (PatternStruct *)x.ptr();
        out << "PatternStruct(" << y->head << ", " << y->params << ")";
        break;
    }
    default :
        assert(false);
    }
}

void patternPrint(ostream &out, MultiPatternPtr x)
{
    switch (x->kind) {
    case MULTI_PATTERN_CELL : {
        MultiPatternCell *y = (MultiPatternCell *)x.ptr();
        out << "MultiPatternCell(" << y->data << ")";
        break;
    }
    case MULTI_PATTERN_LIST : {
        MultiPatternList *y = (MultiPatternList *)x.ptr();
        out << "MultiPatternList(" << y->items << ", " << y->tail << ")";
        break;
    }
    default :
        assert(false);
    }
}
