#include "clay.hpp"

PatternPtr evaluatePattern(ExprPtr expr, EnvPtr env)
{
    LocationContext loc(expr->location);

    switch (expr->objKind) {

    case NAME_REF : {
        NameRef *x = (NameRef *)expr.ptr();
        ObjectPtr y = lookupEnv(env, x->name);
        if (y->objKind == PATTERN) {
            PatternPtr z = (Pattern *)y.ptr();
            assert(z->patternKind == PATTERN_CELL);
            return z;
        }
        return new PatternCell(NULL, y);
    }

    case INDEXING : {
        Indexing *x = (Indexing *)expr.ptr();
        ObjectPtr indexable = evaluateStatic(x->expr, env);
        PatternPtr y = evaluateIndexingPattern(indexable, x->args, env);
        if (!y)
            y = new PatternCell(NULL, evaluateStatic(expr, env));
        return y;
    }

    default : {
        ObjectPtr y = evaluateStatic(expr, env);
        return new PatternCell(NULL, y);
    }

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

        case PRIM_CodePointer : {
            if (args.size() < 1)
                error("code pointer type requires return type");
            vector<PatternPtr> argTypes;
            for (unsigned i = 0; i+1 < args.size(); ++i)
                argTypes.push_back(evaluatePattern(args[i], env));
            PatternPtr returnType = evaluatePattern(args.back(), env);
            return new CodePointerTypePattern(argTypes, returnType, true);
        }

        case PRIM_RefCodePointer : {
            if (args.size() < 1)
                error("code pointer type requires return type");
            vector<PatternPtr> argTypes;
            for (unsigned i = 0; i+1 < args.size(); ++i)
                argTypes.push_back(evaluatePattern(args[i], env));
            PatternPtr returnType = evaluatePattern(args.back(), env);
            return new CodePointerTypePattern(argTypes, returnType, false);
        }

        case PRIM_CCodePointer : {
            if (args.size() < 1)
                error("c-code pointer type requries return type");
            vector<PatternPtr> argTypes;
            for (unsigned i = 0; i+1 < args.size(); ++i)
                argTypes.push_back(evaluatePattern(args[i], env));
            PatternPtr returnType = evaluatePattern(args.back(), env);
            return new CCodePointerTypePattern(argTypes, returnType);
        }

        case PRIM_Array : {
            ensureArity(args, 2);
            PatternPtr elementType = evaluatePattern(args[0], env);
            PatternPtr size = evaluatePattern(args[1], env);
            return new ArrayTypePattern(elementType, size);
        }

        case PRIM_Tuple : {
            if (args.size() < 2)
                error("tuples require atleast 2 elements");
            vector<PatternPtr> elementTypes;
            for (unsigned i = 0; i < args.size(); ++i)
                elementTypes.push_back(evaluatePattern(args[i], env));
            return new TupleTypePattern(elementTypes);
        }

        case PRIM_StaticObject : {
            ensureArity(args, 1);
            PatternPtr obj = evaluatePattern(args[0], env);
            return new StaticObjectTypePattern(obj);
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
        if (!t->returnType) {
            if (!unify(x->returnType, voidType.ptr()))
                return false;
        }
        else {
            if (x->returnIsTemp != t->returnIsTemp)
                return false;
            if (!unify(x->returnType, t->returnType.ptr()))
                return false;
        }
        if (x->argTypes.size() != t->argTypes.size())
            return false;
        for (unsigned i = 0; i < x->argTypes.size(); ++i) {
            if (!unify(x->argTypes[i], t->argTypes[i].ptr()))
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
        if (!t->returnType) {
            if (!unify(x->returnType, voidType.ptr()))
                return false;
        }
        else if (!unify(x->returnType, t->returnType.ptr())) {
            return false;
        }
        if (x->argTypes.size() != t->argTypes.size())
            return false;
        for (unsigned i = 0; i < x->argTypes.size(); ++i) {
            if (!unify(x->argTypes[i], t->argTypes[i].ptr()))
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

    case STATIC_OBJECT_TYPE_PATTERN : {
        StaticObjectTypePattern *x = (StaticObjectTypePattern *)pattern.ptr();
        if (obj->objKind != TYPE)
            return false;
        TypePtr type = (Type *)obj.ptr();
        if (type->typeKind != STATIC_OBJECT_TYPE)
            return false;
        StaticObjectTypePtr t = (StaticObjectType *)type.ptr();
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

void patternPrint(PatternPtr x, ostream &out)
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
            << y->returnType << ", " << y->returnIsTemp << ")";
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
    case STATIC_OBJECT_TYPE_PATTERN : {
        StaticObjectTypePattern *y = (StaticObjectTypePattern *)x.ptr();
        out << "StaticObjectTypePattern(" << y->obj << ")";
        break;
    }
    default :
        assert(false);
    }
}
