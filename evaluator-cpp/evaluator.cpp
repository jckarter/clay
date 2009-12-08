#include "clay.hpp"



//
// Value destructor
//

Value::~Value() {
    if (isOwned) {
        ValuePtr ref = new Value(type, buf, false);
        valueDestroy(ref);
        free(buf);
    }
}



//
// compiler object table
//

static vector<ObjectPtr> coTable;

int toCOIndex(ObjectPtr obj) {
    switch (obj->objKind) {
    case RECORD :
    case PROCEDURE :
    case OVERLOADABLE :
    case EXTERNAL_PROCEDURE :
    case PRIM_OP :
    case TYPE :
        if (obj->coIndex == -1) {
            obj->coIndex = (int)coTable.size();
            coTable.push_back(obj);
        }
        return obj->coIndex;
    default :
        error("invalid compiler object");
        return -1;
    }
}

ObjectPtr fromCOIndex(int i) {
    assert(i >= 0);
    assert(i < (int)coTable.size());
    return coTable[i];
}



//
// allocValue
//

ValuePtr allocValue(TypePtr t) {
    return new Value(t, (char *)malloc(typeSize(t)), true);
}



//
// layouts
//

static const llvm::StructLayout *tupleTypeLayout(TupleType *t) {
    if (t->layout == NULL) {
        const llvm::StructType *st =
            llvm::cast<llvm::StructType>(llvmType(t));
        t->layout = llvmTargetData->getStructLayout(st);
    }
    return t->layout;
}

// static const llvm::StructLayout *recordTypeLayout(RecordType *t) {
//     if (t->layout == NULL) {
//         const llvm::StructType *st =
//             llvm::cast<llvm::StructType>(llvmType(t));
//         t->layout = llvmTargetData->getStructLayout(st);
//     }
//     return t->layout;
// }



//
// valueInit
//

void valueInit(ValuePtr dest) {
    switch (dest->type->typeKind) {
    case BOOL_TYPE :
    case INTEGER_TYPE :
    case FLOAT_TYPE :
    case POINTER_TYPE :
    case COMPILER_OBJECT_TYPE :
        break;
    case ARRAY_TYPE : {
        ArrayType *t = (ArrayType *)dest->type.raw();
        TypePtr etype = t->elementType;
        int esize = typeSize(etype);
        for (int i = 0; i < t->size; ++i)
            valueInit(new Value(etype, dest->buf + i*esize, false));
        break;
    }
    case TUPLE_TYPE : {
        TupleType *t = (TupleType *)dest->type.raw();
        const llvm::StructLayout *layout = tupleTypeLayout(t);
        for (unsigned i = 0; i < t->elementTypes.size(); ++i) {
            char *p = dest->buf + layout->getElementOffset(i);
            valueInit(new Value(t->elementTypes[i], p, false));
        }
        break;
    }
    case RECORD_TYPE : {
        vector<ValuePtr> args;
        args.push_back(dest);
        invoke(coreName("init"), args);
        break;
    }
    default :
        assert(false);
    }
}



//
// valueDestroy
//

void valueDestroy(ValuePtr dest) {
    switch (dest->type->typeKind) {
    case BOOL_TYPE :
    case INTEGER_TYPE :
    case FLOAT_TYPE :
    case POINTER_TYPE :
    case COMPILER_OBJECT_TYPE :
        break;
    case ARRAY_TYPE : {
        ArrayType *t = (ArrayType *)dest->type.raw();
        TypePtr etype = t->elementType;
        int esize = typeSize(etype);
        for (int i = 0; i < t->size; ++i)
            valueDestroy(new Value(etype, dest->buf + i*esize, false));
        break;
    }
    case TUPLE_TYPE : {
        TupleType *t = (TupleType *)dest->type.raw();
        const llvm::StructLayout *layout = tupleTypeLayout(t);
        for (unsigned i = 0; i < t->elementTypes.size(); ++i) {
            char *p = dest->buf + layout->getElementOffset(i);
            valueDestroy(new Value(t->elementTypes[i], p, false));
        }
        break;
    }
    case RECORD_TYPE : {
        vector<ValuePtr> args;
        args.push_back(dest);
        invoke(coreName("destroy"), args);
        break;
    }
    default :
        assert(false);
    }
}



//
// valueInitCopy
//

void valueInitCopy(ValuePtr dest, ValuePtr src) {
    if (dest->type != src->type) {
        vector<ValuePtr> args;
        args.push_back(dest);
        args.push_back(src);
        invoke(coreName("copy"), args);
        return;
    }
    switch (dest->type->typeKind) {
    case BOOL_TYPE :
    case INTEGER_TYPE :
    case FLOAT_TYPE :
    case POINTER_TYPE :
    case COMPILER_OBJECT_TYPE :
        memcpy(dest->buf, src->buf, typeSize(dest->type));
        break;
    case ARRAY_TYPE : {
        ArrayType *t = (ArrayType *)dest->type.raw();
        TypePtr etype = t->elementType;
        int esize = typeSize(etype);
        for (int i = 0; i < t->size; ++i)
            valueInitCopy(new Value(etype, dest->buf + i*esize, false),
                          new Value(etype, src->buf + i*esize, false));
        break;
    }
    case TUPLE_TYPE : {
        TupleType *t = (TupleType *)dest->type.raw();
        const llvm::StructLayout *layout = tupleTypeLayout(t);
        for (unsigned i = 0; i < t->elementTypes.size(); ++i) {
            TypePtr etype = t->elementTypes[i];
            unsigned offset = layout->getElementOffset(i);
            valueInitCopy(new Value(etype, dest->buf + offset, false),
                          new Value(etype, src->buf + offset, false));
        }
        break;
    }
    case RECORD_TYPE : {
        vector<ValuePtr> args;
        args.push_back(dest);
        args.push_back(src);
        invoke(coreName("copy"), args);
        break;
    }
    default :
        assert(false);
    }
}



//
// cloneValue
//

ValuePtr cloneValue(ValuePtr src) {
    ValuePtr dest = allocValue(src->type);
    valueInitCopy(dest, src);
    return dest;
}



//
// valueAssign
//

void valueAssign(ValuePtr dest, ValuePtr src) {
    if (dest->type != src->type) {
        vector<ValuePtr> args;
        args.push_back(dest);
        args.push_back(src);
        invoke(coreName("assign"), args);
        return;
    }
    switch (dest->type->typeKind) {
    case BOOL_TYPE :
    case INTEGER_TYPE :
    case FLOAT_TYPE :
    case POINTER_TYPE :
    case COMPILER_OBJECT_TYPE :
        memcpy(dest->buf, src->buf, typeSize(dest->type));
        break;
    case ARRAY_TYPE : {
        ArrayType *t = (ArrayType *)dest->type.raw();
        TypePtr etype = t->elementType;
        int esize = typeSize(etype);
        for (int i = 0; i < t->size; ++i)
            valueAssign(new Value(etype, dest->buf + i*esize, false),
                        new Value(etype, src->buf + i*esize, false));
        break;
    }
    case TUPLE_TYPE : {
        TupleType *t = (TupleType *)dest->type.raw();
        const llvm::StructLayout *layout = tupleTypeLayout(t);
        for (unsigned i = 0; i < t->elementTypes.size(); ++i) {
            TypePtr etype = t->elementTypes[i];
            unsigned offset = layout->getElementOffset(i);
            valueAssign(new Value(etype, dest->buf + offset, false),
                        new Value(etype, src->buf + offset, false));
        }
        break;
    }
    case RECORD_TYPE : {
        vector<ValuePtr> args;
        args.push_back(dest);
        args.push_back(src);
        invoke(coreName("assign"), args);
        break;
    }
    default :
        assert(false);
    }
}



//
// valueEquals
//

bool valueEquals(ValuePtr a, ValuePtr b) {
    if (a->type != b->type) {
        vector<ValuePtr> args;
        args.push_back(a);
        args.push_back(b);
        return invokeToBool(coreName("equals?"), args);
    }
    switch (a->type->typeKind) {
    case BOOL_TYPE :
    case INTEGER_TYPE :
    case FLOAT_TYPE :
    case POINTER_TYPE :
    case COMPILER_OBJECT_TYPE :
        return memcmp(a->buf, b->buf, typeSize(a->type)) == 0;
    case ARRAY_TYPE : {
        ArrayType *t = (ArrayType *)a->type.raw();
        TypePtr etype = t->elementType;
        int esize = typeSize(etype);
        for (int i = 0; i < t->size; ++i) {
            if (!valueEquals(new Value(etype, a->buf + i*esize, false),
                             new Value(etype, b->buf + i*esize, false)))
                return false;
        }
        return true;
    }
    case TUPLE_TYPE : {
        TupleType *t = (TupleType *)a->type.raw();
        const llvm::StructLayout *layout = tupleTypeLayout(t);
        for (unsigned i = 0; i < t->elementTypes.size(); ++i) {
            TypePtr etype = t->elementTypes[i];
            unsigned offset = layout->getElementOffset(i);
            if (!valueEquals(new Value(etype, a->buf + offset, false),
                             new Value(etype, b->buf + offset, false)))
                return false;
        }
        return true;
    }
    case RECORD_TYPE : {
        vector<ValuePtr> args;
        args.push_back(a);
        args.push_back(b);
        return invokeToBool(coreName("equals?"), args);
    }
    default :
        assert(false);
        return false;
    }
}



//
// valueHash
//

int valueHash(ValuePtr a) {
    switch (a->type->typeKind) {
    case BOOL_TYPE :
        return *((unsigned char *)a->buf);
    case INTEGER_TYPE : {
        IntegerType *t = (IntegerType *)a->type.raw();
        switch (t->bits) {
        case 8 :
            return *((unsigned char *)a->buf);
        case 16 :
            return *((unsigned short *)a->buf);
        case 32 :
            return *((int *)a->buf);
        case 64 :
            return (int)(*((long long *)a->buf));
        default :
            assert(false);
            return 0;
        }
    }
    case FLOAT_TYPE : {
        FloatType *t = (FloatType *)a->type.raw();
        switch (t->bits) {
        case 32 :
            return (int)(*((float *)a->buf));
        case 64 :
            return (int)(*((double *)a->buf));
        default :
            assert(false);
            return 0;
        }
    }
    case POINTER_TYPE :
        return (int)(*((void **)a->buf));
    case COMPILER_OBJECT_TYPE :
        return *((int *)a->buf);
    case ARRAY_TYPE : {
        ArrayType *t = (ArrayType *)a->type.raw();
        TypePtr etype = t->elementType;
        int esize = typeSize(etype);
        int h = 0;
        for (int i = 0; i < t->size; ++i)
            h += valueHash(new Value(etype, a->buf + i*esize, false));
        return h;
    }
    case TUPLE_TYPE : {
        TupleType *t = (TupleType *)a->type.raw();
        const llvm::StructLayout *layout = tupleTypeLayout(t);
        int h = 0;
        for (unsigned i = 0; i < t->elementTypes.size(); ++i) {
            TypePtr etype = t->elementTypes[i];
            unsigned offset = layout->getElementOffset(i);
            h += valueHash(new Value(etype, a->buf + offset, false));
        }
        return h;
    }
    case RECORD_TYPE : {
        vector<ValuePtr> args;
        return invokeToInt(coreName("hash"), args);
    }
    default :
        assert(false);
        return 0;
    }
}
