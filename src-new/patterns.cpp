#include "clay.hpp"

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

    case FUNCTION_POINTER_TYPE_PATTERN : {
        FunctionPointerTypePattern *x =
            (FunctionPointerTypePattern *)pattern.ptr();
        if (obj->objKind != TYPE)
            return false;
        TypePtr type = (Type *)obj.ptr();
        if (type->typeKind != FUNCTION_POINTER_TYPE)
            return false;
        FunctionPointerTypePtr t = (FunctionPointerType *)type.ptr();
        if (!unify(x->returnType, t->returnType.ptr()))
            return false;
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
