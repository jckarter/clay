#include "clay.hpp"

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
        PatternPtr y = evaluateIndexingPattern(indexable, x->args, env);
        if (!y)
            y = new PatternCell(NULL, evaluateOneStatic(expr, env));
        return y;
    }

    default : {
        ObjectPtr y = evaluateOneStatic(expr, env);
        return new PatternCell(NULL, y);
    }

    }
}

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

PatternPtr evaluateIndexingPattern(ObjectPtr indexable,
                                   const vector<ExprPtr> &args,
                                   EnvPtr env)
{
    switch (indexable->objKind) {

    case PRIM_OP : {
        PrimOpPtr x = (PrimOp *)indexable.ptr();

        switch (x->primOpCode) {

        case PRIM_Pointer : {
            ensureArity(args, 1);
            PatternPtr p = evaluatePattern(args[0], env);
            return new PointerTypePattern(p);
        }

        case PRIM_CodePointer :
        case PRIM_RefCodePointer :
            // FIXME: code pointer patterns are not yet supported
            return NULL;

        case PRIM_CCodePointer :
        case PRIM_StdCallCodePointer :
        case PRIM_FastCallCodePointer :
            // FIXME: code pointer patterns are not yet supported
            return NULL;

        case PRIM_Array : {
            ensureArity(args, 2);
            PatternPtr elementType = evaluatePattern(args[0], env);
            PatternPtr size = evaluatePattern(args[1], env);
            return new ArrayTypePattern(elementType, size);
        }

        case PRIM_Tuple : {
            vector<PatternPtr> elementTypes;
            for (unsigned i = 0; i < args.size(); ++i)
                elementTypes.push_back(evaluatePattern(args[i], env));
            return new TupleTypePattern(elementTypes);
        }

        case PRIM_Static : {
            ensureArity(args, 1);
            PatternPtr obj = evaluatePattern(args[0], env);
            return new StaticTypePattern(obj);
        }

        }

        break;
    }

    case RECORD : {
        Record *x = (Record *)indexable.ptr();
        ensureArity(args, x->patternVars.size());
        vector<PatternPtr> params;
        for (unsigned i = 0; i < args.size(); ++i)
            params.push_back(evaluatePattern(args[i], env));
        return new RecordTypePattern(x, params);
    }

    }

    return NULL;
}


// PatternPtr evaluateAliasIndexingPattern(GlobalAliasPtr x,
//                                         const vector<ExprPtr> &args,
//                                         EnvPtr env)
// {
//     assert(x->hasParams());
//     MultiPatternPtr params = evaluateMultiPattern(args, env);
//     if (x->varParam.ptr()) {
//         if (params->size() < x->params.size())
//             arityError2(x->params.size(), params->size());
//     }
//     else {
//         ensureArity(params->size(), x->params.size());
//     }
//     EnvPtr bodyEnv = new Env(x->env);
//     for (unsigned i = 0; i < x->params.size(); ++i) {
//         addLocal(bodyEnv, x->params[i], params->values[i]);
//     }
//     if (x->varParam.ptr()) {
//         MultiPatternPtr varParams = new MultiPattern();
//         for (unsigned i = x->params.size(); i < params->size(); ++i)
//             varParams->add(params->values[i]);
//         addLocal(bodyEnv, x->varParam, varParams.ptr());
//     }
//     evalExpr(x->expr, bodyEnv, out);
// }


bool unify(PatternPtr pattern, ObjectPtr obj) {

    switch (pattern->patternKind) {

    case PATTERN_CELL : {
        PatternCell *x = (PatternCell *)pattern.ptr();
        if (!x->obj) {
            x->obj = obj;
            return true;
        }
        return objectEquals(x->obj, obj);
    }

    case POINTER_TYPE_PATTERN : {
        PointerTypePattern *x = (PointerTypePattern *)pattern.ptr();
        if (obj->objKind != TYPE)
            return false;
        TypePtr type = (Type *)obj.ptr();
        if (type->typeKind != POINTER_TYPE)
            return false;
        PointerTypePtr t = (PointerType *)type.ptr();
        return unify(x->pointeeType, t->pointeeType.ptr());
    }

    case CODE_POINTER_TYPE_PATTERN : {
        CodePointerTypePattern *x =
            (CodePointerTypePattern *)pattern.ptr();
        if (obj->objKind != TYPE)
            return false;
        TypePtr type = (Type *)obj.ptr();
        if (type->typeKind != CODE_POINTER_TYPE)
            return false;
        CodePointerTypePtr t = (CodePointerType *)type.ptr();
        if (x->argTypes.size() != t->argTypes.size())
            return false;
        if (x->returnTypes.size() != t->returnTypes.size())
            return false;
        for (unsigned i = 0; i < x->argTypes.size(); ++i) {
            if (!unify(x->argTypes[i], t->argTypes[i].ptr()))
                return false;
        }
        for (unsigned i = 0; i < x->returnTypes.size(); ++i) {
            if (!unify(x->returnTypes[i], t->returnTypes[i].ptr()))
                return false;
        }
        return true;
    }

    case CCODE_POINTER_TYPE_PATTERN : {
        CCodePointerTypePattern *x =
            (CCodePointerTypePattern *)pattern.ptr();
        if (obj->objKind != TYPE)
            return false;
        TypePtr type = (Type *)obj.ptr();
        if (type->typeKind != CCODE_POINTER_TYPE)
            return false;
        CCodePointerTypePtr t = (CCodePointerType *)type.ptr();
        if (x->callingConv != t->callingConv)
            return false;
        if (x->argTypes.size() != t->argTypes.size())
            return false;
        for (unsigned i = 0; i < x->argTypes.size(); ++i) {
            if (!unify(x->argTypes[i], t->argTypes[i].ptr()))
                return false;
        }
        if (x->hasVarArgs != t->hasVarArgs)
            return false;
        if (!t->returnType) {
            if (x->returnType.ptr())
                return false;
        }
        else if (!x->returnType) {
            return false;
        }
        else if (!unify(x->returnType, t->returnType.ptr())) {
            return false;
        }
        return true;
    }

    case ARRAY_TYPE_PATTERN : {
        ArrayTypePattern *x = (ArrayTypePattern *)pattern.ptr();
        if (obj->objKind != TYPE)
            return false;
        TypePtr type = (Type *)obj.ptr();
        if (type->typeKind != ARRAY_TYPE)
            return false;
        ArrayTypePtr t = (ArrayType *)type.ptr();
        if (!unify(x->elementType, t->elementType.ptr()))
            return false;
        if (!unify(x->size, intToValueHolder(t->size).ptr()))
            return false;
        return true;
    }

    case TUPLE_TYPE_PATTERN : {
        TupleTypePattern *x = (TupleTypePattern *)pattern.ptr();
        if (obj->objKind != TYPE)
            return false;
        TypePtr type = (Type *)obj.ptr();
        if (type->typeKind != TUPLE_TYPE)
            return false;
        TupleTypePtr t = (TupleType *)type.ptr();
        if (x->elementTypes.size() != t->elementTypes.size())
            return false;
        for (unsigned i = 0; i < x->elementTypes.size(); ++i) {
            if (!unify(x->elementTypes[i], t->elementTypes[i].ptr()))
                return false;
        }
        return true;
    }

    case RECORD_TYPE_PATTERN : {
        RecordTypePattern *x = (RecordTypePattern *)pattern.ptr();
        if (obj->objKind != TYPE)
            return false;
        TypePtr type = (Type *)obj.ptr();
        if (type->typeKind != RECORD_TYPE)
            return false;
        RecordTypePtr t = (RecordType *)type.ptr();
        if (x->record != t->record)
            return false;
        if (x->params.size() != t->params.size())
            return false;
        for (unsigned i = 0; i < x->params.size(); ++i) {
            if (!unify(x->params[i], t->params[i]))
                return false;
        }
        return true;
    }

    case STATIC_TYPE_PATTERN : {
        StaticTypePattern *x = (StaticTypePattern *)pattern.ptr();
        if (obj->objKind != TYPE)
            return false;
        TypePtr type = (Type *)obj.ptr();
        if (type->typeKind != STATIC_TYPE)
            return false;
        StaticTypePtr t = (StaticType *)type.ptr();
        return unify(x->obj, t->obj);
    }

    default :
        assert(false);
        return false;

    }
}

ObjectPtr derefCell(PatternCellPtr cell) {
    if (!cell->obj) {
        if (cell->name.ptr())
            error(cell->name, "unresolved pattern variable");
        else
            error("unresolved pattern variable");
    }
    return cell->obj;
}

ObjectPtr reducePattern(PatternPtr pattern) {
    if (pattern->patternKind == PATTERN_CELL) {
        PatternCellPtr x = (PatternCell *)pattern.ptr();
        if (x->obj.ptr())
            return x->obj;
    }
    return pattern.ptr();
}

void patternPrint(ostream &out, PatternPtr x)
{
    switch (x->patternKind) {
    case PATTERN_CELL : {
        PatternCell *y = (PatternCell *)x.ptr();
        out << "PatternCell(" << y->name << ", " << y->obj << ")";
        break;
    }
    case POINTER_TYPE_PATTERN : {
        PointerTypePattern *y = (PointerTypePattern *)x.ptr();
        out << "PointerTypePattern(" << y->pointeeType << ")";
        break;
    }
    case CODE_POINTER_TYPE_PATTERN : {
        CodePointerTypePattern *y = (CodePointerTypePattern *)x.ptr();
        out << "CodePointerTypePattern(" << y->argTypes << ", "
            << y->returnIsRef << ", " << y->returnTypes << ")";
        break;
    }
    case CCODE_POINTER_TYPE_PATTERN : {
        CCodePointerTypePattern *y = (CCodePointerTypePattern *)x.ptr();
        out << "CCodePointerTypePattern(" << y->argTypes << ", "
            << y->returnType << ")";
        break;
    }
    case ARRAY_TYPE_PATTERN : {
        ArrayTypePattern *y = (ArrayTypePattern *)x.ptr();
        out << "ArrayTypePattern(" << y->elementType << ", " << y->size << ")";
        break;
    }
    case TUPLE_TYPE_PATTERN : {
        TupleTypePattern *y = (TupleTypePattern *)x.ptr();
        out << "TupleTypePattern(" << y->elementTypes << ")";
        break;
    }
    case RECORD_TYPE_PATTERN : {
        RecordTypePattern *y = (RecordTypePattern *)x.ptr();
        out << "RecordTypePattern(" << y->record->name->str << ", "
            << y->params << ")";
        break;
    }
    case STATIC_TYPE_PATTERN : {
        StaticTypePattern *y = (StaticTypePattern *)x.ptr();
        out << "StaticTypePattern(" << y->obj << ")";
        break;
    }
    default :
        assert(false);
    }
}
