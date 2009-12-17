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
// interned identifiers
//

static map<string, IdentifierPtr> identTable;

IdentifierPtr internIdentifier(IdentifierPtr x) {
    map<string, IdentifierPtr>::iterator i = identTable.find(x->str);
    if (i != identTable.end())
        return i->second;
    IdentifierPtr y = new Identifier(x->str);
    identTable[x->str] = y;
    return y;
}



//
// compiler object table
//

static vector<ObjectPtr> coTable;

int toCOIndex(ObjectPtr obj) {
    switch (obj->objKind) {
    case IDENTIFIER :
        obj = internIdentifier((Identifier *)obj.raw()).raw();
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
// intToValue, valueToInt, valueToBool,
// coToValue, valueToCO, valueToType, lower
//

ValuePtr intToValue(int x) {
    ValuePtr v = allocValue(int32Type);
    *((int *)v->buf) = x;
    return v;
}

int valueToInt(ValuePtr v) {
    if (v->type != int32Type)
        error("expecting value of int32 type");
    return *((int *)v->buf);
}

bool valueToBool(ValuePtr v) {
    if (v->type != boolType)
        error("expecting value of bool type");
    return *((char *)v->buf) != 0;
}

ValuePtr coToValue(ObjectPtr x) {
    ValuePtr v = allocValue(compilerObjectType);
    *((int *)v->buf) = toCOIndex(x);
    return v;
}

ObjectPtr valueToCO(ValuePtr v) {
    if (v->type != compilerObjectType)
        error("expecting compiler object type");
    int x = *((int *)v->buf);
    return fromCOIndex(x);
}

TypePtr valueToType(ValuePtr v) {
    if (v->type != compilerObjectType)
        error("expecting a type");
    ObjectPtr obj = valueToCO(v);
    if (obj->objKind != TYPE)
        error("expecting a type");
    return (Type *)obj.raw();
}

ObjectPtr lower(ValuePtr v) {
    if (v->type != compilerObjectType)
        return v.raw();
    int x = *((int *)v->buf);
    return fromCOIndex(x);
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

static const llvm::StructLayout *recordTypeLayout(RecordType *t) {
    if (t->layout == NULL) {
        const llvm::StructType *st =
            llvm::cast<llvm::StructType>(llvmType(t));
        t->layout = llvmTargetData->getStructLayout(st);
    }
    return t->layout;
}



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
        args.push_back(a);
        return invokeToInt(coreName("hash"), args);
    }
    default :
        assert(false);
        return 0;
    }
}



//
// access names from other modules
//

ObjectPtr coreName(const string &name) {
    return lookupPublic(loadedModule("_core"), new Identifier(name));
}

ObjectPtr primName(const string &name) {
    return lookupPublic(loadedModule("__primitives__"), new Identifier(name));
}

ExprPtr moduleNameRef(const string &module, const string &name) {
    ExprPtr nameRef = new NameRef(new Identifier(name));
    return new SCExpr(loadedModule(module)->env, nameRef);
}

ExprPtr coreNameRef(const string &name) {
    return moduleNameRef("_core", name);
}

ExprPtr primNameRef(const string &name) {
    return moduleNameRef("__primitives__", name);
}



//
// temp blocks
//

static vector<vector<ValuePtr> > tempBlocks;

void pushTempBlock() {
    vector<ValuePtr> block;
    tempBlocks.push_back(block);
}

void popTempBlock() {
    assert(tempBlocks.size() > 0);
    vector<ValuePtr> &block = tempBlocks.back();
    while (block.size() > 0)
        block.pop_back();
    tempBlocks.pop_back();
}

void installTemp(ValuePtr value) {
    assert(tempBlocks.size() > 0);
    assert(value->isOwned);
    tempBlocks.back().push_back(value);
}



//
// evaluate
//

ValuePtr evaluateNested(ExprPtr expr, EnvPtr env) {
    LocationContext loc(expr->location);
    ValuePtr v = evaluate(expr, env);
    if (v->isOwned)
        installTemp(v);
    return v;
}

ValuePtr evaluateToStatic(ExprPtr expr, EnvPtr env) {
    LocationContext loc(expr->location);
    pushTempBlock();
    ValuePtr v = evaluate(expr, env);
    if (!v->isOwned)
        v = cloneValue(v);
    popTempBlock();
    return v;
}

ObjectPtr evaluateToCO(ExprPtr expr, EnvPtr env) {
    LocationContext loc(expr->location);
    pushTempBlock();
    ValuePtr v = evaluate(expr, env);
    ObjectPtr obj = valueToCO(v);
    popTempBlock();
    return obj;
}

TypePtr evaluateToType(ExprPtr expr, EnvPtr env) {
    LocationContext loc(expr->location);
    pushTempBlock();
    TypePtr t = valueToType(evaluate(expr, env));
    popTempBlock();
    return t;
}

bool evaluateToBool(ExprPtr expr, EnvPtr env) {
    LocationContext loc(expr->location);
    pushTempBlock();
    bool b = valueToBool(evaluate(expr, env));
    popTempBlock();
    return b;
}

ValuePtr evaluate(ExprPtr expr, EnvPtr env) {
    switch (expr->objKind) {

    case BOOL_LITERAL : {
        BoolLiteral *x = (BoolLiteral *)expr.raw();
        ValuePtr v = allocValue(boolType);
        *((char *)v->buf) = (x->value ? 1 : 0);
        return v;
    }

    case INT_LITERAL : {
        IntLiteral *x = (IntLiteral *)expr.raw();
        char *ptr = (char *)x->value.c_str();
        char *end = ptr;
        if (x->suffix == "i8") {
            long y = strtol(ptr, &end, 0);
            if (*end != 0)
                error("invalid int8 literal");
            if ((errno == ERANGE) || (y < SCHAR_MIN) || (y > SCHAR_MAX))
                error("int8 literal out of range");
            ValuePtr v = allocValue(int8Type);
            *((char *)v->buf) = (char)y;
            return v;
        }
        else if (x->suffix == "i16") {
            long y = strtol(ptr, &end, 0);
            if (*end != 0)
                error("invalid int16 literal");
            if ((errno == ERANGE) || (y < SHRT_MIN) || (y > SHRT_MAX))
                error("int16 literal out of range");
            ValuePtr v = allocValue(int16Type);
            *((short *)v->buf) = (short)y;
            return v;
        }
        else if ((x->suffix == "i32") || x->suffix.empty()) {
            long y = strtol(ptr, &end, 0);
            if (*end != 0)
                error("invalid int32 literal");
            if (errno == ERANGE)
                error("int32 literal out of range");
            ValuePtr v = allocValue(int32Type);
            *((int *)v->buf) = (int)y;
            return v;
        }
        else if (x->suffix == "i64") {
            long long y = strtoll(ptr, &end, 0);
            if (*end != 0)
                error("invalid int64 literal");
            if (errno == ERANGE)
                error("int64 literal out of range");
            ValuePtr v = allocValue(int64Type);
            *((long long *)v->buf) = y;
            return v;
        }
        else if (x->suffix == "u8") {
            unsigned long y = strtoul(ptr, &end, 0);
            if (*end != 0)
                error("invalid uint8 literal");
            if ((errno == ERANGE) || (y > UCHAR_MAX))
                error("uint8 literal out of range");
            ValuePtr v = allocValue(uint8Type);
            *((unsigned char *)v->buf) = (unsigned char)y;
            return v;
        }
        else if (x->suffix == "u16") {
            unsigned long y = strtoul(ptr, &end, 0);
            if (*end != 0)
                error("invalid uint16 literal");
            if ((errno == ERANGE) || (y > USHRT_MAX))
                error("uint16 literal out of range");
            ValuePtr v = allocValue(uint16Type);
            *((unsigned short *)v->buf) = (unsigned short)y;
            return v;
        }
        else if (x->suffix == "u32") {
            unsigned long y = strtoul(ptr, &end, 0);
            if (*end != 0)
                error("invalid uint32 literal");
            if (errno == ERANGE)
                error("uint32 literal out of range");
            ValuePtr v = allocValue(uint32Type);
            *((unsigned int *)v->buf) = (unsigned int)y;
            return v;
        }
        else if (x->suffix == "u64") {
            unsigned long long y = strtoull(ptr, &end, 0);
            if (*end != 0)
                error("invalid uint64 literal");
            if (errno == ERANGE)
                error("uint64 literal out of range");
            ValuePtr v = allocValue(uint64Type);
            *((unsigned long long *)v->buf) = y;
            return v;
        }
        else if (x->suffix == "f32") {
            float y = strtof(ptr, &end);
            if (*end != 0)
                error("invalid float32 literal");
            if (errno == ERANGE)
                error("float32 literal out of range");
            ValuePtr v = allocValue(float32Type);
            *((float *)v->buf) = y;
            return v;
        }
        else if (x->suffix == "f64") {
            double y = strtod(ptr, &end);
            if (*end != 0)
                error("invalid float64 literal");
            if (errno == ERANGE)
                error("float64 literal out of range");
            ValuePtr v = allocValue(float64Type);
            *((double *)v->buf) = y;
            return v;
        }
        error("invalid literal suffix: " + x->suffix);
        return NULL;
    }

    case FLOAT_LITERAL : {
        FloatLiteral *x = (FloatLiteral *)expr.raw();
        char *ptr = (char *)x->value.c_str();
        char *end = ptr;
        if (x->suffix == "f32") {
            float y = strtof(ptr, &end);
            if (*end != 0)
                error("invalid float32 literal");
            if (errno == ERANGE)
                error("float32 literal out of range");
            ValuePtr v = allocValue(float32Type);
            *((float *)v->buf) = y;
            return v;
        }
        else if ((x->suffix == "f64") || x->suffix.empty()) {
            double y = strtod(ptr, &end);
            if (*end != 0)
                error("invalid float64 literal");
            if (errno == ERANGE)
                error("float64 literal out of range");
            ValuePtr v = allocValue(float64Type);
            *((double *)v->buf) = y;
            return v;
        }
        error("invalid float literal suffix: " + x->suffix);
        return NULL;
    }

    case CHAR_LITERAL : {
        CharLiteral *x = (CharLiteral *)expr.raw();
        if (!x->converted)
            x->converted = convertCharLiteral(x->value);
        return evaluate(x->converted, env);
    }

    case STRING_LITERAL : {
        StringLiteral *x = (StringLiteral *)expr.raw();
        if (!x->converted)
            x->converted = convertStringLiteral(x->value);
        return evaluate(x->converted, env);
    }

    case NAME_REF : {
        NameRef *x = (NameRef *)expr.raw();
        ObjectPtr y = lookupEnv(env, x->name);
        if (y->objKind == VALUE) {
            ValuePtr z = (Value *)y.raw();
            if (z->isOwned) {
                // static values
                z = cloneValue(z);
            }
            return z;
        }
        return coToValue(y);
    }

    case TUPLE : {
        Tuple *x = (Tuple *)expr.raw();
        if (!x->converted)
            x->converted = convertTuple(x);
        return evaluate(x->converted, env);
    }

    case ARRAY : {
        Array *x = (Array *)expr.raw();
        if (!x->converted)
            x->converted = convertArray(x);
        return evaluate(x->converted, env);
    }

    case INDEXING : {
        Indexing *x = (Indexing *)expr.raw();
        ValuePtr thing = evaluateNested(x->expr, env);
        vector<ValuePtr> args;
        for (unsigned i = 0; i < x->args.size(); ++i)
            args.push_back(evaluateNested(x->args[i], env));
        return invokeIndexing(lower(thing), args);
    }

    case CALL : {
        Call *x = (Call *)expr.raw();
        ValuePtr thing = evaluateNested(x->expr, env);
        vector<ValuePtr> args;
        for (unsigned i = 0; i < x->args.size(); ++i)
            args.push_back(evaluateNested(x->args[i], env));
        return invoke(lower(thing), args);
    }

    case FIELD_REF : {
        FieldRef *x = (FieldRef *)expr.raw();
        ValuePtr thing = evaluateNested(x->expr, env);
        ValuePtr name = coToValue(x->name.raw());
        vector<ValuePtr> args;
        args.push_back(thing);
        args.push_back(name);
        return invoke(primName("recordFieldRefByName"), args);
    }

    case TUPLE_REF : {
        TupleRef *x = (TupleRef *)expr.raw();
        ValuePtr thing = evaluateNested(x->expr, env);
        vector<ValuePtr> args;
        args.push_back(thing);
        args.push_back(intToValue(x->index));
        return invoke(primName("tupleFieldRef"), args);
    }

    case UNARY_OP : {
        UnaryOp *x = (UnaryOp *)expr.raw();
        if (!x->converted)
            x->converted = convertUnaryOp(x);
        return evaluate(x->converted, env);
    }

    case BINARY_OP : {
        BinaryOp *x = (BinaryOp *)expr.raw();
        if (!x->converted)
            x->converted = convertBinaryOp(x);
        return evaluate(x->converted, env);
    }

    case AND : {
        And *x = (And *)expr.raw();
        ValuePtr v1 = evaluate(x->expr1, env);
        vector<ValuePtr> args;
        args.push_back(v1);
        if (!valueToBool(invoke(primName("boolTruth"), args)))
            return v1;
        return evaluate(x->expr2, env);
    }

    case OR : {
        Or *x = (Or *)expr.raw();
        ValuePtr v1 = evaluate(x->expr1, env);
        vector<ValuePtr> args;
        args.push_back(v1);
        if (valueToBool(invoke(primName("boolTruth"), args)))
            return v1;
        return evaluate(x->expr2, env);
    }

    case SC_EXPR : {
        SCExpr *x = (SCExpr *)expr.raw();
        return evaluate(x->expr, x->env);
    }

    default :
        assert(false);
        return NULL;
    }
}

ExprPtr convertCharLiteral(char c) {
    ExprPtr nameRef = moduleNameRef("_char", "Char");
    CallPtr call = new Call(nameRef);
    ostringstream out;
    out << (int)c;
    call->args.push_back(new IntLiteral(out.str(), "i8"));
    return call.raw();
}

ExprPtr convertStringLiteral(const string &s) {
    ArrayPtr charArray = new Array();
    for (unsigned i = 0; i < s.size(); ++i)
        charArray->args.push_back(convertCharLiteral(s[i]));
    ExprPtr nameRef = moduleNameRef("_string", "string");
    CallPtr call = new Call(nameRef);
    call->args.push_back(charArray.raw());
    return call.raw();
}

ExprPtr convertTuple(TuplePtr x) {
    if (x->args.size() == 1)
        return x->args[0];
    return new Call(primNameRef("tuple"), x->args);
}

ExprPtr convertArray(ArrayPtr x) {
    return new Call(primNameRef("array"), x->args);
}

ExprPtr convertUnaryOp(UnaryOpPtr x) {
    ExprPtr callable;
    switch (x->op) {
    case DEREFERENCE :
        callable = primNameRef("pointerDereference");
        break;
    case ADDRESS_OF :
        callable = primNameRef("addressOf");
        break;
    case PLUS :
        callable = coreNameRef("plus");
        break;
    case MINUS :
        callable = coreNameRef("minus");
        break;
    case NOT :
        callable = primNameRef("boolNot");
        break;
    default :
        assert(false);
    }
    CallPtr call = new Call(callable);
    call->args.push_back(x->expr);
    return call.raw();
}

ExprPtr convertBinaryOp(BinaryOpPtr x) {
    ExprPtr callable;
    switch (x->op) {
    case ADD :
        callable = coreNameRef("add");
        break;
    case SUBTRACT :
        callable = coreNameRef("subtract");
        break;
    case MULTIPLY :
        callable = coreNameRef("multiply");
        break;
    case DIVIDE :
        callable = coreNameRef("divide");
        break;
    case REMAINDER :
        callable = coreNameRef("remainder");
        break;
    case EQUALS :
        callable = coreNameRef("equals?");
        break;
    case NOT_EQUALS :
        callable = coreNameRef("notEquals?");
        break;
    case LESSER :
        callable = coreNameRef("lesser?");
        break;
    case LESSER_EQUALS :
        callable = coreNameRef("lesserEquals?");
        break;
    case GREATER :
        callable = coreNameRef("greater?");
        break;
    case GREATER_EQUALS :
        callable = coreNameRef("greaterEquals?");
        break;
    default :
        assert(false);
    }
    CallPtr call = new Call(callable);
    call->args.push_back(x->expr1);
    call->args.push_back(x->expr2);
    return call.raw();
}



//
// evaluatePattern
//

PatternPtr evaluatePattern(ExprPtr expr, EnvPtr env) {
    switch (expr->objKind) {
    case NAME_REF : {
        NameRef *x = (NameRef *)expr.raw();
        ObjectPtr y = lookupEnv(env, x->name);
        if (y->objKind == PATTERN) {
            PatternPtr z = (Pattern *)y.raw();
            assert(z->patternKind == PATTERN_CELL);
            return z;
        }
    }
    case INDEXING : {
        Indexing *x = (Indexing *)expr.raw();
        ValuePtr thing = evaluateToStatic(x->expr, env);
        vector<PatternPtr> args;
        for (unsigned i = 0; i < x->args.size(); ++i)
            args.push_back(evaluatePattern(x->args[i], env));
        return indexingPattern(lower(thing), args);
    }
    }
    ValuePtr v = evaluateToStatic(expr, env);
    return new PatternCell(NULL, v);
}

PatternPtr indexingPattern(ObjectPtr obj, const vector<PatternPtr> &args) {
    switch(obj->objKind) {
    case RECORD : {
        Record *x = (Record *)obj.raw();
        ensureArity(args, x->patternVars.size());
        return new RecordTypePattern(x, args);
    }
    case PRIM_OP : {
        PrimOp *x = (PrimOp *)obj.raw();
        switch (x->primOpCode) {
        case PRIM_Pointer :
            ensureArity(args, 1);
            return new PointerTypePattern(args[0]);
        case PRIM_Array :
            ensureArity(args, 2);
            return new ArrayTypePattern(args[0], args[1]);
        case PRIM_Tuple :
            if (args.size() < 2)
                error("tuples require atleast elements");
            return new TupleTypePattern(args);
        }
    }
    }
    error("invalid indexing pattern");
    return NULL;
}



//
// unify, unifyType
//

bool unify(PatternPtr pattern, ValuePtr value) {
    if (pattern->patternKind == PATTERN_CELL) {
        PatternCell *x = (PatternCell *)pattern.raw();
        if (!x->value) {
            x->value = value;
            return true;
        }
        return valueEquals(x->value, value);
    }
    if (value->type != compilerObjectType)
        return false;
    ObjectPtr x = valueToCO(value);
    if (x->objKind != TYPE)
        return false;
    TypePtr y = (Type *)x.raw();
    return unifyType(pattern, y);
}

bool unifyType(PatternPtr pattern, TypePtr type) {
    switch (pattern->patternKind) {
    case PATTERN_CELL : {
        return unify(pattern, coToValue(type.raw()));
    }
    case ARRAY_TYPE_PATTERN : {
        ArrayTypePattern *x = (ArrayTypePattern *)pattern.raw();
        if (type->typeKind != ARRAY_TYPE)
            return false;
        ArrayType *y = (ArrayType *)type.raw();
        if (!unifyType(x->elementType, y->elementType))
            return false;
        if (!unify(x->size, intToValue(y->size)))
            return false;
        return true;
    }
    case TUPLE_TYPE_PATTERN : {
        TupleTypePattern *x = (TupleTypePattern *)pattern.raw();
        if (type->typeKind != TUPLE_TYPE)
            return false;
        TupleType *y = (TupleType *)type.raw();
        if (x->elementTypes.size() != y->elementTypes.size())
            return false;
        for (unsigned i = 0; i < x->elementTypes.size(); ++i)
            if (!unifyType(x->elementTypes[i], y->elementTypes[i]))
                return false;
        return true;
    }
    case POINTER_TYPE_PATTERN : {
        PointerTypePattern *x = (PointerTypePattern *)pattern.raw();
        if (type->typeKind != POINTER_TYPE)
            return false;
        PointerType *y = (PointerType *)type.raw();
        return unifyType(x->pointeeType, y->pointeeType);
    }
    case RECORD_TYPE_PATTERN : {
        RecordTypePattern *x = (RecordTypePattern *)pattern.raw();
        if (type->typeKind != RECORD_TYPE)
            return false;
        RecordType *y = (RecordType *)type.raw();
        if (x->record != y->record)
            return false;
        if (x->params.size() != y->params.size())
            return false;
        for (unsigned i = 0; i < x->params.size(); ++i)
            if (!unify(x->params[i], y->params[i]))
                return false;
        return true;
    }
    default :
        assert(false);
        return false;
    }
}



//
// derefCell
//

ValuePtr derefCell(PatternCellPtr cell) {
    if (!cell->value) {
        if (cell->name)
            error(cell->name, "unresolved pattern variable");
        else
            error("unresolved pattern variable");
    }
    ValuePtr v = cell->value;
    if (!v->isOwned)
        v = cloneValue(v);
    return v;
}



//
// invokeIndexing
//

ValuePtr invokeIndexing(ObjectPtr obj, const vector<ValuePtr> &args) {
    switch (obj->objKind) {
    case RECORD : {
        vector<ValuePtr> args2;
        args2.push_back(coToValue(obj));
        for (unsigned i = 0; i < args.size(); ++i)
            args2.push_back(args[i]);
        return invoke(primName("RecordType"), args2);
    }
    case PRIM_OP : {
        PrimOp *x = (PrimOp *)obj.raw();
        switch (x->primOpCode) {
        case PRIM_Pointer :
            return invoke(primName("PointerType"), args);
        case PRIM_Array :
            return invoke(primName("ArrayType"), args);
        case PRIM_Tuple :
            return invoke(primName("TupleType"), args);
        }
    }
    }
    error("invalid indexing operation");
    return NULL;
}



//
// invokeToInt, invokeToBool
//

int invokeToInt(ObjectPtr callable, const vector<ValuePtr> &args) {
    return valueToInt(invoke(callable, args));
}

bool invokeToBool(ObjectPtr callable, const vector<ValuePtr> &args) {
    return valueToBool(invoke(callable, args));
}



//
// invoke
//

ValuePtr invoke(ObjectPtr callable, const vector<ValuePtr> &args) {
    switch (callable->objKind) {
    case RECORD :
        return invokeRecord((Record *)callable.raw(), args);
    case PROCEDURE :
        return invokeProcedure((Procedure *)callable.raw(), args);
    case OVERLOADABLE :
        return invokeOverloadable((Overloadable *)callable.raw(), args);
    case EXTERNAL_PROCEDURE :
        return invokeExternal((ExternalProcedure *)callable.raw(), args);
    case TYPE :
        return invokeType((Type *)callable.raw(), args);
    case PRIM_OP :
        return invokePrimOp((PrimOp *)callable.raw(), args);
    }
    error("invalid operation");
    return NULL;
}



//
// invokeRecord
//

ValuePtr invokeRecord(RecordPtr x, const vector<ValuePtr> &args) {
    ensureArity(args, x->formalArgs.size());
    EnvPtr env = new Env(x->env);
    vector<PatternCellPtr> cells;
    for (unsigned i = 0; i < x->patternVars.size(); ++i) {
        cells.push_back(new PatternCell(x->patternVars[i], NULL));
        addLocal(env, x->patternVars[i], cells[i].raw());
    }
    vector<ValuePtr> nonStaticArgs;
    for (unsigned i = 0; i < args.size(); ++i) {
        if (!matchArg(args[i], x->formalArgs[i], env))
            fmtError("mismatch at argument %d", i+1);
        if (x->formalArgs[i]->objKind == VALUE_ARG)
            nonStaticArgs.push_back(args[i]);
    }
    vector<ValuePtr> cellValues;
    for (unsigned i = 0; i < cells.size(); ++i)
        cellValues.push_back(derefCell(cells[i]));
    TypePtr t = recordType(x, cellValues);
    return invokeType(t, nonStaticArgs);
}

bool matchArg(ValuePtr arg, FormalArgPtr farg, EnvPtr env) {
    switch (farg->objKind) {
    case VALUE_ARG : {
        ValueArg *x = (ValueArg *)farg.raw();
        if (!x->type)
            return true;
        PatternPtr pattern = evaluatePattern(x->type, env);
        return unifyType(pattern, arg->type);
    }
    case STATIC_ARG : {
        StaticArg *x = (StaticArg *)farg.raw();
        PatternPtr pattern = evaluatePattern(x->pattern, env);
        return unify(pattern, arg);
    }
    }
    assert(false);
    return false;
}



//
// invokeType
//
// invoke constructors for array types, tuple types, and record types.
// invoke default constructor, copy constructor for all types
//

ValuePtr invokeType(TypePtr x, const vector<ValuePtr> &args) {
    if (args.size() == 0) {
        ValuePtr v = allocValue(x);
        valueInit(v);
        return v;
    }
    if ((args.size() == 1) && (args[0]->type == x))
        return cloneValue(args[0]);
    switch (x->typeKind) {
    case ARRAY_TYPE : {
        ArrayType *y = (ArrayType *)x.raw();
        ensureArity(args, y->size);
        TypePtr etype = y->elementType;
        int esize = typeSize(etype);
        ValuePtr v = allocValue(y);
        for (int i = 0; i < y->size; ++i) {
            if (args[i]->type != etype)
                fmtError("type mismatch at argument %d", i+1);
            valueInitCopy(new Value(etype, v->buf + i*esize, false), args[i]);
        }
        return v;
    }
    case TUPLE_TYPE : {
        TupleType *y = (TupleType *)x.raw();
        ensureArity(args, y->elementTypes.size());
        const llvm::StructLayout *layout = tupleTypeLayout(y);
        ValuePtr v = allocValue(y);
        for (unsigned i = 0; i < y->elementTypes.size(); ++i) {
            char *p = v->buf + layout->getElementOffset(i);
            TypePtr etype = y->elementTypes[i];
            if (args[i]->type != etype)
                fmtError("type mismatch at argument %d", i+1);
            valueInitCopy(new Value(etype, p, false), args[i]);
        }
        return v;
    }
    case RECORD_TYPE : {
        RecordType *y = (RecordType *)x.raw();
        const vector<TypePtr> &fieldTypes = recordFieldTypes(y);
        ensureArity(args, fieldTypes.size());
        const llvm::StructLayout *layout = recordTypeLayout(y);
        ValuePtr v = allocValue(y);
        for (unsigned i = 0; i < fieldTypes.size(); ++i) {
            char *p = v->buf + layout->getElementOffset(i);
            if (args[i]->type != fieldTypes[i])
                fmtError("type mismatch at argument %d", i+1);
            valueInitCopy(new Value(fieldTypes[i], p, false), args[i]);
        }
        return v;
    }
    }
    error("invalid constructor");
    return NULL;
}



//
// invokeProcedure, invokeOverloadable
// invoke procedures and overloadables
//

ValuePtr invokeProcedure(ProcedurePtr x, const vector<ValuePtr> &args) {
    MatchInvokeResultPtr result = matchInvokeCode(x->code, x->env, args);
    switch (result->resultKind) {
    case MATCH_INVOKE_SUCCESS : {
        MatchInvokeSuccess *y = (MatchInvokeSuccess *)result.raw();
        EnvPtr env = bindValueArgs(y->env, args, x->code);
        return evalCodeBody(x->code, env);
    }
    }
    signalMatchInvokeError(result);
    return NULL;
}

ValuePtr invokeOverloadable(OverloadablePtr x, const vector<ValuePtr> &args) {
    for (unsigned i = 0; i < x->overloads.size(); ++i) {
        OverloadPtr y = x->overloads[i];
        MatchInvokeResultPtr result = matchInvokeCode(y->code, y->env, args);
        if (result->resultKind == MATCH_INVOKE_SUCCESS) {
            MatchInvokeSuccess *z = (MatchInvokeSuccess *)result.raw();
            EnvPtr env = bindValueArgs(z->env, args, y->code);
            return evalCodeBody(y->code, env);
        }
    }
    error("no matching overload");
    return NULL;
}



//
// matchInvokeCode
//

void signalMatchInvokeError(MatchInvokeResultPtr result) {
    switch (result->resultKind) {
    case MATCH_INVOKE_SUCCESS :
        assert(false);
        break;
    case MATCH_INVOKE_ARG_COUNT_ERROR :
        error("incorrect no. of arguments");
        break;
    case MATCH_INVOKE_ARG_MISMATCH : {
        MatchInvokeArgMismatch *x = (MatchInvokeArgMismatch *)result.raw();
        fmtError("mismatch at argument %d", (x->pos + 1));
        break;
    }
    case MATCH_INVOKE_PREDICATE_FAILURE :
        error("predicate failure");
        break;
    }
}

MatchInvokeResultPtr
matchInvokeCode(CodePtr code, EnvPtr env, const vector<ValuePtr> &args) {
    if (args.size() != code->formalArgs.size())
        return new MatchInvokeArgCountError();
    EnvPtr patternEnv = new Env(env);
    vector<PatternCellPtr> cells;
    for (unsigned i = 0; i < code->patternVars.size(); ++i) {
        cells.push_back(new PatternCell(code->patternVars[i], NULL));
        addLocal(patternEnv, code->patternVars[i], cells[i].raw());
    }
    for (unsigned i = 0; i < args.size(); ++i) {
        if (!matchArg(args[i], code->formalArgs[i], patternEnv))
            return new MatchInvokeArgMismatch(i);
    }
    EnvPtr env2 = new Env(env);
    for (unsigned i = 0; i < cells.size(); ++i) {
        ValuePtr v = derefCell(cells[i]);
        addLocal(env2, code->patternVars[i], v.raw());
    }
    if (code->predicate) {
        bool result = evaluateToBool(code->predicate, env2);
        if (!result)
            return new MatchInvokePredicateFailure();
    }
    return new MatchInvokeSuccess(env2);
}

EnvPtr bindValueArgs(EnvPtr env, const vector<ValuePtr> &args, CodePtr code) {
    EnvPtr env2 = new Env(env);
    for (unsigned i = 0; i < args.size(); ++i) {
        FormalArgPtr farg = code->formalArgs[i];
        if (farg->objKind == VALUE_ARG) {
            ValueArg *x = (ValueArg *)farg.raw();
            ValuePtr v = args[i];
            if (v->isOwned)
                v = new Value(v->type, v->buf, false);
            addLocal(env2, x->name, v.raw());
        }
    }
    return env2;
}



//
// evalCodeBody
//

// ValuePtr evalCodeBody(CodePtr code, EnvPtr env) {
// }
