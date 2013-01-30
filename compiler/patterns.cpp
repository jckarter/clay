#include "clay.hpp"
#include "codegen.hpp"
#include "evaluator.hpp"
#include "loader.hpp"
#include "operators.hpp"
#include "patterns.hpp"
#include "analyzer.hpp"
#include "env.hpp"
#include "objects.hpp"


#pragma clang diagnostic ignored "-Wcovered-switch-default"


namespace clay {


//
// derefDeep
//

static ObjectPtr computeStruct(ObjectPtr head, MultiStaticPtr params, 
                               CompilerState* cst)
{
    if (!head) {
        return makeTupleValue(params->values, cst);
    }
    else {
        TypePtr t = constructType(head, params);
        return t.ptr();
    }
}

ObjectPtr derefDeep(PatternPtr x, CompilerState* cst)
{
    switch (x->kind) {
    case PATTERN_CELL : {
        PatternCell *y = (PatternCell *)x.ptr();
        if (y->obj.ptr() && (y->obj->objKind == PATTERN)) {
            Pattern *z = (Pattern *)y->obj.ptr();
            return derefDeep(z, cst);
        }
        return y->obj;
    }
    case PATTERN_STRUCT : {
        PatternStruct *y = (PatternStruct *)x.ptr();
        MultiStaticPtr params = derefDeep(y->params, cst);
        if (!params)
            return NULL;
        return computeStruct(y->head, params, cst);
    }
    default :
        assert(false);
        return NULL;
    }
}

MultiStaticPtr derefDeep(MultiPatternPtr x, CompilerState* cst)
{
    switch (x->kind) {
    case MULTI_PATTERN_CELL : {
        MultiPatternCell *y = (MultiPatternCell *)x.ptr();
        if (!y->data)
            return NULL;
        return derefDeep(y->data, cst);
    }
    case MULTI_PATTERN_LIST : {
        MultiPatternList *y = (MultiPatternList *)x.ptr();
        MultiStaticPtr out = new MultiStatic();
        for (size_t i = 0; i < y->items.size(); ++i) {
            ObjectPtr z = derefDeep(y->items[i], cst);
            if (!z)
                return NULL;
            out->add(z);
        }
        if (y->tail.ptr()) {
            MultiStaticPtr tail = derefDeep(y->tail, cst);
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

static PatternPtr objectToPattern(ObjectPtr obj, CompilerState* cst)
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
    case VALUE_HOLDER : {
        ValueHolder *x = (ValueHolder *)obj.ptr();
        if (x->type->typeKind == TUPLE_TYPE) {
            TupleType *tt = (TupleType *)x->type.ptr();
            const llvm::StructLayout *layout = tupleTypeLayout(tt);
            MultiPatternListPtr params = new MultiPatternList();
            for (unsigned i = 0; i < tt->elementTypes.size(); ++i) {
                char *addr = x->buf + layout->getElementOffset(i);
                EValuePtr evElement = new EValue(tt->elementTypes[i], addr);
                PatternPtr y = objectToPattern(evalueToStatic(evElement), cst);
                params->items.push_back(y);
            }
            return new PatternStruct(NULL, params.ptr());
        }
        return new PatternCell(obj);
    }
    case TYPE : {
        Type *t = (Type *)obj.ptr();
        switch (t->typeKind) {
        case POINTER_TYPE : {
            PointerType *pt = (PointerType *)t;
            ObjectPtr head = primitive_Pointer(cst);
            MultiPatternListPtr params = new MultiPatternList();
            params->items.push_back(objectToPattern(pt->pointeeType.ptr(), cst));
            return new PatternStruct(head, params.ptr());
        }
        case CODE_POINTER_TYPE : {
            CodePointerType *cpt = (CodePointerType *)t;
            ObjectPtr head = primitive_CodePointer(cst);
            MultiPatternListPtr argTypes = new MultiPatternList();
            for (size_t i = 0; i < cpt->argTypes.size(); ++i) {
                PatternPtr x = objectToPattern(cpt->argTypes[i].ptr(), cst);
                argTypes->items.push_back(x);
            }
            MultiPatternListPtr returnTypes = new MultiPatternList();
            for (size_t i = 0; i < cpt->returnTypes.size(); ++i) {
                TypePtr t = cpt->returnTypes[i];
                if (cpt->returnIsRef[i]) {
                    ObjectPtr obj = primitive_ByRef(cst);
                    assert(obj->objKind == RECORD_DECL);
                    t = recordType((RecordDecl *)obj.ptr(),
                                   vector<ObjectPtr>(1, t.ptr()));
                }
                PatternPtr x = objectToPattern(t.ptr(), cst);
                returnTypes->items.push_back(x);
            }
            MultiPatternListPtr params = new MultiPatternList();
            PatternPtr a = new PatternStruct(NULL, argTypes.ptr());
            PatternPtr b = new PatternStruct(NULL, returnTypes.ptr());
            params->items.push_back(a);
            params->items.push_back(b);
            return new PatternStruct(head, params.ptr());
        }
        case CCODE_POINTER_TYPE : {
            CCodePointerType *cpt = (CCodePointerType *)t;
            ObjectPtr head = primitive_ExternalCodePointer(cst);;
            ObjectPtr ccParam;

            switch (cpt->callingConv) {
            case CC_DEFAULT :
                ccParam = primitive_AttributeCCall(cst);
                break;
            case CC_STDCALL :
                ccParam = primitive_AttributeStdCall(cst);
                break;
            case CC_FASTCALL :
                ccParam = primitive_AttributeFastCall(cst);
                break;
            case CC_THISCALL :
                ccParam = primitive_AttributeThisCall(cst);
                break;
            case CC_LLVM :
                ccParam = primitive_AttributeLLVMCall(cst);
                break;
            default:
                assert(false);
            }
            assert(ccParam != NULL);

            ValueHolderPtr varArgParam = boolToValueHolder(cpt->hasVarArgs, cst);

            MultiPatternListPtr argTypes = new MultiPatternList();
            for (size_t i = 0; i < cpt->argTypes.size(); ++i) {
                PatternPtr x = objectToPattern(cpt->argTypes[i].ptr(), cst);
                argTypes->items.push_back(x);
            }
            MultiPatternListPtr returnTypes = new MultiPatternList();
            if (cpt->returnType.ptr()) {
                PatternPtr x = objectToPattern(cpt->returnType.ptr(), cst);
                returnTypes->items.push_back(x);
            }
            MultiPatternListPtr params = new MultiPatternList();
            PatternPtr argTypesParam = new PatternStruct(NULL, argTypes.ptr());
            PatternPtr retTypesParam = new PatternStruct(NULL, returnTypes.ptr());
            params->items.push_back(objectToPattern(ccParam, cst));
            params->items.push_back(objectToPattern(varArgParam.ptr(), cst));
            params->items.push_back(argTypesParam);
            params->items.push_back(retTypesParam);
            return new PatternStruct(head, params.ptr());
        }
        case ARRAY_TYPE : {
            ArrayType *at = (ArrayType *)t;
            ObjectPtr head = primitive_Array(cst);
            MultiPatternListPtr params = new MultiPatternList();
            params->items.push_back(objectToPattern(at->elementType.ptr(), cst));
            ValueHolderPtr vh = intToValueHolder((int)at->size, cst);
            params->items.push_back(objectToPattern(vh.ptr(), cst));
            return new PatternStruct(head, params.ptr());
        }
        case VEC_TYPE : {
            VecType *vt = (VecType *)t;
            ObjectPtr head = primitive_Vec(cst);
            MultiPatternListPtr params = new MultiPatternList();
            params->items.push_back(objectToPattern(vt->elementType.ptr(), cst));
            ValueHolderPtr vh = intToValueHolder((int)vt->size, cst);
            params->items.push_back(objectToPattern(vh.ptr(), cst));
            return new PatternStruct(head, params.ptr());
        }
        case TUPLE_TYPE : {
            TupleType *tt = (TupleType *)t;
            ObjectPtr head = primitive_Tuple(cst);
            MultiPatternListPtr params = new MultiPatternList();
            for (size_t i = 0; i < tt->elementTypes.size(); ++i) {
                PatternPtr x = objectToPattern(tt->elementTypes[i].ptr(), cst);
                params->items.push_back(x);
            }
            return new PatternStruct(head, params.ptr());
        }
        case UNION_TYPE : {
            UnionType *ut = (UnionType *)t;
            ObjectPtr head = primitive_Union(cst);
            MultiPatternListPtr params = new MultiPatternList();
            for (size_t i = 0; i < ut->memberTypes.size(); ++i) {
                PatternPtr x = objectToPattern(ut->memberTypes[i].ptr(), cst);
                params->items.push_back(x);
            }
            return new PatternStruct(head, params.ptr());
        }
        case STATIC_TYPE : {
            StaticType *st = (StaticType *)t;
            ObjectPtr head = primitive_Static(cst);
            MultiPatternListPtr params = new MultiPatternList();
            params->items.push_back(objectToPattern(st->obj, cst));
            return new PatternStruct(head, params.ptr());
        }
        case RECORD_TYPE : {
            RecordType *rt = (RecordType *)t;
            ObjectPtr head = rt->record.ptr();
            MultiPatternListPtr params = new MultiPatternList();
            for (size_t i = 0; i < rt->params.size(); ++i) {
                PatternPtr x = objectToPattern(rt->params[i], cst);
                params->items.push_back(x);
            }
            return new PatternStruct(head, params.ptr());
        }
        case VARIANT_TYPE : {
            VariantType *vt = (VariantType *)t;
            ObjectPtr head = vt->variant.ptr();
            MultiPatternListPtr params = new MultiPatternList();
            for (size_t i = 0; i < vt->params.size(); ++i) {
                PatternPtr x = objectToPattern(vt->params[i], cst);
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



//
// unify
//

bool unifyObjObj(ObjectPtr a, ObjectPtr b, CompilerState* cst)
{
    if (a->objKind == PATTERN) {
        PatternPtr a2 = (Pattern *)a.ptr();
        return unifyPatternObj(a2, b, cst);
    }
    else if (a->objKind == MULTI_PATTERN) {
        error("incorrect usage of multi-valued pattern "
              "in single-valued context");
        return false;
    }
    else if (b->objKind == PATTERN) {
        PatternPtr b2 = (Pattern *)b.ptr();
        return unifyObjPattern(a, b2, cst);
    }
    else if (b->objKind == MULTI_PATTERN) {
        error("incorrect usage of multi-valued pattern "
              "in single-valued context");
        return false;
    }
    return objectEquals(a, b);
}

bool unifyObjPattern(ObjectPtr a, PatternPtr b, CompilerState* cst)
{
    assert(a.ptr() && b.ptr());
    if (a->objKind == PATTERN) {
        PatternPtr a2 = (Pattern *)a.ptr();
        return unify(a2, b, cst);
    }
    else if (a->objKind == MULTI_PATTERN) {
        error("incorrect usage of multi-valued pattern "
              "in single-valued context");
        return false;
    }
    else if (b->kind == PATTERN_CELL) {
        PatternCell *b2 = (PatternCell *)b.ptr();
        if (!b2->obj) {
            b2->obj = a;
            return true;
        }
        return unifyObjObj(b2->obj, a, cst);
    }
    else {
        assert(b->kind == PATTERN_STRUCT);
        PatternPtr a2 = objectToPattern(a, cst);
        PatternStruct *b2 = (PatternStruct *)b.ptr();
        if (a2->kind == PATTERN_STRUCT) {
            PatternStruct *a3 = (PatternStruct *)a2.ptr();
            if (a3->head == b2->head)
                return unifyMulti(a3->params, b2->params, cst);
        }
        return false;
    }
}

bool unifyPatternObj(PatternPtr a, ObjectPtr b, CompilerState* cst)
{
    return unifyObjPattern(b, a, cst);
}

bool unify(PatternPtr a, PatternPtr b, CompilerState* cst)
{
    assert(a.ptr() && b.ptr());
    if (a->kind == PATTERN_CELL) {
        PatternCell *a2 = (PatternCell *)a.ptr();
        if (!a2->obj) {
            a2->obj = b.ptr();
            return true;
        }
        return unifyObjPattern(a2->obj, b, cst);
    }
    else if (b->kind == PATTERN_CELL) {
        PatternCell *b2 = (PatternCell *)b.ptr();
        if (!b2->obj) {
            b2->obj = a.ptr();
            return true;
        }
        return unifyPatternObj(a, b2->obj, cst);
    }
    else {
        assert(a->kind == PATTERN_STRUCT);
        assert(b->kind == PATTERN_STRUCT);
        PatternStruct *a2 = (PatternStruct *)a.ptr();
        PatternStruct *b2 = (PatternStruct *)b.ptr();
        if (a2->head == b2->head) {
            return unifyMulti(a2->params, b2->params, cst);
        }
        return false;
    }
}

bool unifyMulti(MultiPatternPtr a, MultiStaticPtr b, CompilerState* cst)
{
    assert(a.ptr() && b.ptr());
    MultiPatternListPtr b2 = new MultiPatternList();
    for (size_t i = 0; i < b->size(); ++i)
        b2->items.push_back(objectToPattern(b->values[i], cst));
    return unifyMulti(a, b2.ptr(), cst);
}

bool unifyMulti(MultiPatternPtr a, MultiPatternPtr b, CompilerState* cst)
{
    assert(a.ptr() && b.ptr());
    switch (a->kind) {
    case MULTI_PATTERN_CELL : {
        MultiPatternCell *a2 = (MultiPatternCell *)a.ptr();
        if (!a2->data) {
            a2->data = b;
            return true;
        }
        return unifyMulti(a2->data, b, cst);
    }
    case MULTI_PATTERN_LIST : {
        MultiPatternList *a2 = (MultiPatternList *)a.ptr();
        return unifyMulti(a2, 0, b, cst);
    }
    default :
        assert(false);
        return false;
    }
}

static MultiPatternListPtr subList(MultiPatternListPtr x, unsigned index)
{
    MultiPatternListPtr subList = new MultiPatternList();
    for (size_t i = index; i < x->items.size(); ++i)
        subList->items.push_back(x->items[i]);
    subList->tail = x->tail;
    return subList;
}

bool unifyMulti(MultiPatternListPtr a, unsigned indexA,
                MultiPatternPtr b,
                CompilerState* cst)
{
    assert(a.ptr() && b.ptr());
    switch (b->kind) {
    case MULTI_PATTERN_CELL : {
        MultiPatternCell *b2 = (MultiPatternCell *)b.ptr();
        if (!b2->data) {
            b2->data = subList(a, indexA).ptr();
            return true;
        }
        return unifyMulti(a, indexA, b2->data, cst);
    }
    case MULTI_PATTERN_LIST : {
        MultiPatternList *b2 = (MultiPatternList *)b.ptr();
        return unifyMulti(a, indexA, b2, 0, cst);
    }
    default :
        assert(false);
        return false;
    }
}

bool unifyMulti(MultiPatternPtr a,
                MultiPatternListPtr b, unsigned indexB,
                CompilerState* cst)
{
    assert(a.ptr() && b.ptr());
    return unifyMulti(b, indexB, a, cst);
}

bool unifyMulti(MultiPatternListPtr a, unsigned indexA,
                MultiPatternListPtr b, unsigned indexB,
                CompilerState* cst)
{
    assert(a.ptr() && b.ptr());
    while ((indexA < a->items.size()) &&
           (indexB < b->items.size()))
    {
        if (!unify(a->items[indexA++], b->items[indexB++], cst))
            return false;
    }
    if (indexA < a->items.size()) {
        assert(indexB == b->items.size());
        if (!b->tail)
            return false;
        return unifyMulti(a, indexA, b->tail, cst);
    }
    else if (a->tail.ptr()) {
        return unifyMulti(a->tail, b, indexB, cst);
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

        case PRIM_CodePointer :
        case PRIM_ExternalCodePointer :

        case PRIM_Array :
        case PRIM_Vec :
        case PRIM_Tuple :
        case PRIM_Union :
        case PRIM_Static :
            return true;
        default :
            return false;
        }
        assert(false);
    }
    case RECORD_DECL :
    case VARIANT_DECL :
        return true;
    default :
        return false;
    }
    assert(false);
}



//
// evaluateOnePattern
//

template<typename XValuePtr>
static PatternPtr xvalueToPatternCell(XValuePtr xv)
{
    ObjectPtr stat = unwrapStaticType(xv->type);
    if (stat == NULL) {
        error("non-static value used in pattern context");
        return NULL;
    }
    else
        return new PatternCell(stat);
}

static PatternPtr namedToPattern(ObjectPtr x, CompilerState* cst)
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
        return evaluateOnePattern(y->expr, y->env, cst);
    }
    case RECORD_DECL : {
        RecordDecl *y = (RecordDecl *)x.ptr();
        ObjectPtr z;
        if (y->params.empty() && !y->varParam)
            z = recordType(y, vector<ObjectPtr>()).ptr();
        else
            z = y;
        return new PatternCell(z);
    }
    case VARIANT_DECL : {
        VariantDecl *y = (VariantDecl *)x.ptr();
        ObjectPtr z;
        if (y->params.empty() && !y->varParam)
            z = variantType(y, vector<ObjectPtr>()).ptr();
        else
            z = y;
        return new PatternCell(z);
    }
    case ENUM_MEMBER : {
        EnumMember *member = (EnumMember *)x.ptr();
        ValueHolderPtr vh = new ValueHolder(member->type);
        vh->as<int>() = member->index;
        return new PatternCell(vh.ptr());
    }
    case PVALUE : {
        PValue *pv = (PValue *)x.ptr();
        return xvalueToPatternCell(&pv->data);
    }
    case CVALUE : {
        CValue *cv = (CValue *)x.ptr();
        return xvalueToPatternCell(cv);
    }
    case EVALUE : {
        EValue *ev = (EValue *)x.ptr();
        return xvalueToPatternCell(ev);
    }

    default :
        return new PatternCell(x);
    }
}

PatternPtr evaluateOnePattern(ExprPtr expr, EnvPtr env, 
                              CompilerState* cst)
{
    LocationContext loc(expr->location);

    switch (expr->exprKind) {

    case NAME_REF : {
        NameRef *x = (NameRef *)expr.ptr();
        ObjectPtr y = safeLookupEnv(env, x->name);
        return namedToPattern(y, cst);
    }

    case INDEXING : {
        Indexing *x = (Indexing *)expr.ptr();
        ObjectPtr indexable = evaluateOneStatic(x->expr, env, cst);
        if (isPatternHead(indexable)) {
            MultiPatternPtr params = evaluateMultiPattern(x->args, env, cst);
            return new PatternStruct(indexable, params);
        }
        if (indexable->objKind == GLOBAL_ALIAS) {
            GlobalAlias *y = (GlobalAlias *)indexable.ptr();
            MultiPatternPtr params = evaluateMultiPattern(x->args, env, cst);
            return evaluateAliasPattern(y, params, cst);
        }
        return new PatternCell(evaluateOneStatic(expr, env, cst));
    }

    case TUPLE : {
        Tuple *x = (Tuple *)expr.ptr();
        MultiPatternPtr params = evaluateMultiPattern(x->args, env, cst);
        return new PatternStruct(NULL, params);
    }

    default : {
        ObjectPtr y = evaluateOneStatic(expr, env, cst);
        return new PatternCell(y);
    }

    }
}



//
// evaluateAliasPattern
//

PatternPtr evaluateAliasPattern(GlobalAliasPtr x, MultiPatternPtr params,
                                CompilerState* cst)
{
    MultiPatternListPtr args = new MultiPatternList();
    EnvPtr env = new Env(x->env);
    for (size_t i = 0; i < x->params.size(); ++i) {
        PatternCellPtr cell = new PatternCell(NULL);
        args->items.push_back(cell.ptr());
        addLocal(env, x->params[i], cell.ptr());
    }
    if (x->varParam.ptr()) {
        MultiPatternCellPtr multiCell = new MultiPatternCell(NULL);
        args->tail = multiCell.ptr();
        addLocal(env, x->varParam, multiCell.ptr());
    }
    PatternPtr out = evaluateOnePattern(x->expr, env, cst);
    if (!unifyMulti(args.ptr(), params, x->module->cst))
        error("non-matching alias");
    return out;
}



//
// evaluateMultiPattern
//

static MultiPatternPtr checkMultiPatternNameRef(ExprPtr expr, EnvPtr env)
{
    if (expr->exprKind != NAME_REF)
        return NULL;
    NameRef *x = (NameRef *)expr.ptr();
    ObjectPtr obj = safeLookupEnv(env, x->name);
    if (obj->objKind == PATTERN) {
        error(expr, "single-valued pattern incorrectly used in multi-valued context");
    }
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
            for (size_t i = 0; i < y->items.size(); ++i)
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

MultiPatternPtr evaluateMultiPattern(ExprListPtr exprs, EnvPtr env,
                                     CompilerState* cst)
{
    MultiPatternListPtr out = new MultiPatternList();
    MultiPatternListPtr cur = out;
    for (size_t i = 0; i < exprs->size(); ++i) {
        assert(!cur || !cur->tail);
        ExprPtr x = exprs->exprs[i];
        if (x->exprKind == UNPACK) {
            Unpack *y = (Unpack *)x.ptr();
            MultiPatternPtr mp = checkMultiPatternNameRef(y->expr, env);
            if (mp.ptr()) {
                if (!appendPattern(cur, mp))
                    error(x, "expressions cannot occur after "
                          "multi-pattern variable");
            }
            else {
                MultiStaticPtr z = evaluateExprStatic(y->expr, env, cst);
                if (!cur && (z->size() > 0))
                    error(x, "expressions cannot occur after "
                          "multi-pattern variable");
                for (size_t j = 0; j < z->size(); ++j) {
                    PatternPtr p = new PatternCell(z->values[i]);
                    cur->items.push_back(p);
                }
            }
        }
        else if (x->exprKind == PAREN) {
            Paren *y = (Paren *)x.ptr();
            MultiPatternPtr mp = evaluateMultiPattern(y->args, env, cst);
            if (!appendPattern(cur, mp)) {
                error(x, "expressions cannot occur after "
                      "multi-pattern variable");
            }
        }
        else {
            if (!cur)
                error(x, "expressions cannot occur after "
                      "multi-pattern variable");
            PatternPtr y = evaluateOnePattern(x, env, cst);
            cur->items.push_back(y);
        }
    }
    return out.ptr();
}

}
