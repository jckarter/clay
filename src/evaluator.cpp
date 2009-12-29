#include "clay.hpp"
#include "invokeutil.hpp"



//
// internal declarations
//

IdentifierPtr internIdentifier(IdentifierPtr x);

ValuePtr allocValue(TypePtr t);

void valueInit(ValuePtr dest);
void valueDestroy(ValuePtr dest);
void valueCopy(ValuePtr dest, ValuePtr src);
void valueAssign(ValuePtr dest, ValuePtr src);

void pushTempBlock();
void popTempBlock();
void installTemp(ValuePtr value);

// the following versions of evaluate don't create a new temp block
// but they push the expression's locaiton onto location stack
ValuePtr evaluateNonVoid(ExprPtr expr, EnvPtr env);
ValuePtr evaluate(ExprPtr expr, EnvPtr env);
ValuePtr evaluateNested(ExprPtr expr, EnvPtr env);

// the following versions of evaluate don't create a new temp block
// and they don't push the expression's location onto location stack
ValuePtr evaluateNonVoid2(ExprPtr expr, EnvPtr env);
ValuePtr evaluate2(ExprPtr expr, EnvPtr env);

PatternPtr indexingPattern(ObjectPtr obj, const vector<PatternPtr> &args);

ValuePtr invokeIndexing(ObjectPtr obj, const vector<ValuePtr> &args);
bool invokeToBool(ObjectPtr callable, const vector<ValuePtr> &args);
int invokeToInt(ObjectPtr callable, const vector<ValuePtr> &args);

ValuePtr invokeRecord(RecordPtr x, const vector<ValuePtr> &args);
ValuePtr invokeType(TypePtr x, const vector<ValuePtr> &args);
ValuePtr invokeProcedure(ProcedurePtr x, const vector<ValuePtr> &args);
ValuePtr invokeOverloadable(OverloadablePtr x, const vector<ValuePtr> &args);

struct StatementResult;
struct GotoResult;
struct BreakResult;
struct ContinueResult;
struct ReturnResult;

typedef Ptr<StatementResult> StatementResultPtr;
typedef Ptr<GotoResult> GotoResultPtr;
typedef Ptr<BreakResult> BreakResultPtr;
typedef Ptr<ContinueResult> ContinueResultPtr;
typedef Ptr<ReturnResult> ReturnResultPtr;

enum StatementResultKind {
    GOTO_RESULT,
    BREAK_RESULT,
    CONTINUE_RESULT,
    RETURN_RESULT,
};

struct StatementResult : public Object {
    int resultKind;
    StatementResult(int resultKind)
        : Object(DONT_CARE), resultKind(resultKind) {}
};

struct GotoResult : public StatementResult {
    IdentifierPtr labelName;
    GotoResult(IdentifierPtr labelName)
        : StatementResult(GOTO_RESULT), labelName(labelName) {}
};

struct BreakResult : public StatementResult {
    StatementPtr stmt;
    BreakResult(StatementPtr stmt)
        : StatementResult(BREAK_RESULT), stmt(stmt) {}
};

struct ContinueResult : public StatementResult {
    StatementPtr stmt;
    ContinueResult(StatementPtr stmt)
        : StatementResult(CONTINUE_RESULT), stmt(stmt) {}
};

struct ReturnResult : public StatementResult {
    ValuePtr result;
    ReturnResult(ValuePtr result)
        : StatementResult(RETURN_RESULT), result(result) {}
};

ValuePtr evalCodeBody(CodePtr code, EnvPtr env);

StatementResultPtr evalStatement(StatementPtr stmt, EnvPtr env);

struct LabelInfo {
    EnvPtr env;
    unsigned blockPos;
    LabelInfo() : env(NULL), blockPos((unsigned)-1) {}
    LabelInfo(EnvPtr env, unsigned blockPos)
        : env(env), blockPos(blockPos) {}
};

void evalCollectLabels(const vector<StatementPtr> &statements,
                       unsigned startIndex, map<string, LabelInfo> &labels,
                       EnvPtr env);

EnvPtr evalBinding(BindingPtr x, EnvPtr env, vector<ValuePtr> &blockTemps);

ValuePtr invokeExternal(ExternalProcedurePtr x, const vector<ValuePtr> &args);
llvm::Function *
generateExternalFunc(const vector<TypePtr> &argTypes, TypePtr returnType,
                     const string &name);
llvm::Function *
generateExternalWrapper(llvm::Function *func, const vector<TypePtr> &argTypes,
                        TypePtr returnType, const string &name);

ValuePtr execLLVMFunc(llvm::Function *func, const vector<ValuePtr> &args,
                      TypePtr returnType);

ValuePtr invokePrimOp(PrimOpPtr x, const vector<ValuePtr> &args);

bool numericEquals(ValuePtr a, ValuePtr b);
bool numericLesser(ValuePtr a, ValuePtr b);
ValuePtr numericAdd(ValuePtr a, ValuePtr b);
ValuePtr numericSubtract(ValuePtr a, ValuePtr b);
ValuePtr numericMultiply(ValuePtr a, ValuePtr b);
ValuePtr numericDivide(ValuePtr a, ValuePtr b);
ValuePtr numericNegate(ValuePtr a);

ValuePtr integerRemainder(ValuePtr a, ValuePtr b);
ValuePtr integerShiftLeft(ValuePtr a, ValuePtr b);
ValuePtr integerShiftRight(ValuePtr a, ValuePtr b);
ValuePtr integerBitwiseAnd(ValuePtr a, ValuePtr b);
ValuePtr integerBitwiseOr(ValuePtr a, ValuePtr b);
ValuePtr integerBitwiseXor(ValuePtr a, ValuePtr b);

ValuePtr numericConvert(TypePtr t, ValuePtr a);

ValuePtr pointerToInt(IntegerTypePtr t, void *ptr);
ValuePtr intToPointer(TypePtr pointeeType, ValuePtr a);



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
// voidValue
//

ValuePtr voidValue;

void initVoidValue() {
    voidValue = new Value(voidType, NULL, true);
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
// boolToValue, intToValue, valueToInt, valueToBool,
// coToValue, valueToCO, valueToType, lower
//

ValuePtr boolToValue(bool x) {
    ValuePtr v = allocValue(boolType);
    *((char *)v->buf) = (x ? 1 : 0);
    return v;
}

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
    case VOID_TYPE :
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
    case VOID_TYPE :
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
// valueCopy
//

void valueCopy(ValuePtr dest, ValuePtr src) {
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
    case VOID_TYPE :
        memcpy(dest->buf, src->buf, typeSize(dest->type));
        break;
    case ARRAY_TYPE : {
        ArrayType *t = (ArrayType *)dest->type.raw();
        TypePtr etype = t->elementType;
        int esize = typeSize(etype);
        for (int i = 0; i < t->size; ++i)
            valueCopy(new Value(etype, dest->buf + i*esize, false),
                      new Value(etype, src->buf + i*esize, false));
        break;
    }
    case TUPLE_TYPE : {
        TupleType *t = (TupleType *)dest->type.raw();
        const llvm::StructLayout *layout = tupleTypeLayout(t);
        for (unsigned i = 0; i < t->elementTypes.size(); ++i) {
            TypePtr etype = t->elementTypes[i];
            unsigned offset = layout->getElementOffset(i);
            valueCopy(new Value(etype, dest->buf + offset, false),
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
    valueCopy(dest, src);
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
    case VOID_TYPE :
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
    case VOID_TYPE :
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
    case VOID_TYPE :
        return 0;
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
// valuePrint
//

void valuePrint(ValuePtr a, ostream &out) {
    switch (a->type->typeKind) {
    case BOOL_TYPE :
        out << (valueToBool(a) ? "true" : "false");
        break;
    case INTEGER_TYPE : {
        IntegerType *t = (IntegerType *)a->type.raw();
        if (t->isSigned) {
            switch (t->bits) {
            case 8 :
                out << int(*((char *)a->buf)) << "#i8";
                break;
            case 16 :
                out << *((short *)a->buf) << "#i16";
                break;
            case 32 :
                out << *((long *)a->buf);
                break;
            case 64 :
                out << *((long long *)a->buf) << "#i64";
                break;
            default :
                assert(false);
            }
        }
        else {
            switch (t->bits) {
            case 8 :
                out << int(*((unsigned char *)a->buf)) << "#u8";
                break;
            case 16 :
                out << *((unsigned short *)a->buf) << "#u16";
                break;
            case 32 :
                out << *((unsigned long *)a->buf) << "#u32";
                break;
            case 64 :
                out << *((unsigned long long *)a->buf) << "#u64";
                break;
            default :
                assert(false);
            }
        }
        break;
    }
    case FLOAT_TYPE : {
        FloatType *t = (FloatType *)a->type.raw();
        switch (t->bits) {
        case 32 :
            out << *((float *)a->buf) << "#f32";
            break;
        case 64 :
            out << *((double *)a->buf) << "#f64";
            break;
        default :
            assert(false);
        }
        break;
    }
    case ARRAY_TYPE : {
        ArrayType *t = (ArrayType *)a->type.raw();
        TypePtr etype = t->elementType;
        int esize = typeSize(etype);
        out << "[";
        for (int i = 0; i < t->size; ++i) {
            if (i != 0)
                out << ", ";
            out << new Value(etype, a->buf + i*esize, false);
        }
        out << "]";
        break;
    }
    case TUPLE_TYPE : {
        TupleType *t = (TupleType *)a->type.raw();
        const llvm::StructLayout *layout = tupleTypeLayout(t);
        out << "(";
        for (unsigned i = 0; i < t->elementTypes.size(); ++i) {
            TypePtr etype = t->elementTypes[i];
            unsigned offset = layout->getElementOffset(i);
            if (i != 0)
                out << ", ";
            out << new Value(etype, a->buf + offset, false);
        }
        out << ")";
        break;
    }
    case POINTER_TYPE : {
        PointerType *t = (PointerType *)a->type.raw();
        out << *t << "(" << *((void **)a->buf) << ")";
        break;
    }
    case RECORD_TYPE : {
        RecordType *t = (RecordType *)a->type.raw();
        const vector<TypePtr> &fieldTypes = recordFieldTypes(t);
        const llvm::StructLayout *layout = recordTypeLayout(t);
        out << *t << "(";
        for (unsigned i = 0; i < fieldTypes.size(); ++i) {
            char *p = a->buf + layout->getElementOffset(i);
            if (i != 0)
                out << ", ";
            out << new Value(fieldTypes[i], p, false);
        }
        out << ")";
        break;
    }
    case COMPILER_OBJECT_TYPE :
        out << valueToCO(a);
        break;
    case VOID_TYPE :
        out << "void";
        break;
    default :
        assert(false);
    }
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

ValuePtr evaluateToStatic(ExprPtr expr, EnvPtr env) {
    LocationContext loc(expr->location);
    pushTempBlock();
    ValuePtr v = evaluateNonVoid2(expr, env);
    if (!v->isOwned)
        v = cloneValue(v);
    popTempBlock();
    return v;
}

ObjectPtr evaluateToCO(ExprPtr expr, EnvPtr env) {
    LocationContext loc(expr->location);
    pushTempBlock();
    ValuePtr v = evaluateNonVoid2(expr, env);
    ObjectPtr obj = valueToCO(v);
    popTempBlock();
    return obj;
}

TypePtr evaluateType(ExprPtr expr, EnvPtr env) {
    LocationContext loc(expr->location);
    pushTempBlock();
    TypePtr t = valueToType(evaluateNonVoid2(expr, env));
    popTempBlock();
    return t;
}

TypePtr evaluateNonVoidType(ExprPtr expr, EnvPtr env) {
    LocationContext loc(expr->location);
    pushTempBlock();
    TypePtr t = valueToType(evaluateNonVoid2(expr, env));
    popTempBlock();
    if (t == voidType)
        error("void type not allowed here");
    return t;
}

bool evaluateToBool(ExprPtr expr, EnvPtr env) {
    LocationContext loc(expr->location);
    pushTempBlock();
    bool b = valueToBool(evaluateNonVoid2(expr, env));
    popTempBlock();
    return b;
}

ValuePtr evaluateNonVoid(ExprPtr expr, EnvPtr env) {
    LocationContext loc(expr->location);
    return evaluateNonVoid2(expr, env);
}

ValuePtr evaluate(ExprPtr expr, EnvPtr env) {
    LocationContext loc(expr->location);
    return evaluate2(expr, env);
}

ValuePtr evaluateNested(ExprPtr expr, EnvPtr env) {
    LocationContext loc(expr->location);
    ValuePtr v = evaluateNonVoid2(expr, env);
    if (v->isOwned)
        installTemp(v);
    return v;
}

ValuePtr evaluateNonVoid2(ExprPtr expr, EnvPtr env) {
    ValuePtr v = evaluate2(expr, env);
    if (v->type == voidType)
        error("expecting non void-type expression");
    return v;
}

ValuePtr evaluate2(ExprPtr expr, EnvPtr env) {
    switch (expr->objKind) {

    case BOOL_LITERAL : {
        BoolLiteral *x = (BoolLiteral *)expr.raw();
        return boolToValue(x->value);
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
        return invoke(primName("tupleRef"), args);
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

    case VALUE_EXPR : {
        ValueExpr *x = (ValueExpr *)expr.raw();
        return cloneValue(x->value);
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
        break;
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
        if (cell->name.raw())
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
        RecordPtr r = (Record *)obj.raw();
        ensureArity(args, r->patternVars.size());
        return coToValue(recordType(r, args).raw());
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
    case TYPE :
        return invokeType((Type *)callable.raw(), args);
    case PROCEDURE :
        return invokeProcedure((Procedure *)callable.raw(), args);
    case OVERLOADABLE :
        return invokeOverloadable((Overloadable *)callable.raw(), args);
    case EXTERNAL_PROCEDURE :
        return invokeExternal((ExternalProcedure *)callable.raw(), args);
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
        if (!matchFormalArg(args[i], x->formalArgs[i], env))
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
            valueCopy(new Value(etype, v->buf + i*esize, false), args[i]);
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
            valueCopy(new Value(etype, p, false), args[i]);
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
            valueCopy(new Value(fieldTypes[i], p, false), args[i]);
        }
        return v;
    }
    }
    error("invalid constructor");
    return NULL;
}



//
// invokeProcedure
//

ValuePtr invokeProcedure(ProcedurePtr x, const vector<ValuePtr> &args) {
    InvokeTableEntry *entry = lookupProcedureInvoke(x, args);
    if (entry->env.raw()) {
        EnvPtr env = bindValueArgs(entry->env, args, entry->code);
        return evalCodeBody(entry->code, env);
    }
    MatchInvokeResultPtr result = matchInvoke(x->code, x->env, args);
    switch (result->resultKind) {
    case MATCH_INVOKE_SUCCESS : {
        MatchInvokeSuccess *y = (MatchInvokeSuccess *)result.raw();
        entry->env = y->env;
        entry->code = x->code;
        EnvPtr env = bindValueArgs(y->env, args, x->code);
        return evalCodeBody(x->code, env);
    }
    }
    signalMatchInvokeError(result);
    return NULL;
}



//
// invokeOverloadable
//

ValuePtr invokeOverloadable(OverloadablePtr x, const vector<ValuePtr> &args) {
    InvokeTableEntry *entry = lookupOverloadableInvoke(x, args);
    if (entry->env.raw()) {
        EnvPtr env = bindValueArgs(entry->env, args, entry->code);
        return evalCodeBody(entry->code, env);
    }
    for (unsigned i = 0; i < x->overloads.size(); ++i) {
        OverloadPtr y = x->overloads[i];
        MatchInvokeResultPtr result = matchInvoke(y->code, y->env, args);
        if (result->resultKind == MATCH_INVOKE_SUCCESS) {
            MatchInvokeSuccess *z = (MatchInvokeSuccess *)result.raw();
            entry->env = z->env;
            entry->code = y->code;
            EnvPtr env = bindValueArgs(z->env, args, y->code);
            return evalCodeBody(y->code, env);
        }
    }
    error("no matching overload");
    return NULL;
}



//
// evalCodeBody, evalStatement
//

ValuePtr evalCodeBody(CodePtr code, EnvPtr env) {
    StatementResultPtr result = evalStatement(code->body, env);
    if (!result)
        result = new ReturnResult(voidValue);
    switch (result->resultKind) {
    case GOTO_RESULT : {
        GotoResult *x = (GotoResult *)result.raw();
        LocationContext loc(x->labelName->location);
        fmtError("label not found: %s", x->labelName->str.c_str());
        break;
    }
    case BREAK_RESULT : {
        BreakResult *x = (BreakResult *)result.raw();
        error(x->stmt, "invalid break statement");
        break;
    }
    case CONTINUE_RESULT : {
        ContinueResult *x = (ContinueResult *)result.raw();
        error(x->stmt, "invalid continue statement");
        break;
    }
    case RETURN_RESULT : {
        ReturnResult *x = (ReturnResult *)result.raw();
        return x->result;
    }
    }
    assert(false);
    return NULL;
}

StatementResultPtr evalStatement(StatementPtr stmt, EnvPtr env) {
    LocationContext loc(stmt->location);
    switch (stmt->objKind) {
    case BLOCK : {
        Block *x = (Block *)stmt.raw();
        unsigned i = 0;
        map<string, LabelInfo> labels;
        vector<ValuePtr> blockTemps;
        evalCollectLabels(x->statements, i, labels, env);
        for (; i < x->statements.size(); ++i) {
            StatementPtr y = x->statements[i];
            if (y->objKind == BINDING) {
                env = evalBinding((Binding *)y.raw(), env, blockTemps);
                evalCollectLabels(x->statements, i+1, labels, env);
            }
            else if (y->objKind == LABEL) {
                // ignore
            }
            else {
                StatementResultPtr result = evalStatement(y, env);
                if (!result)
                    continue;
                if (result->resultKind == GOTO_RESULT) {
                    GotoResult *z = (GotoResult *)result.raw();
                    map<string, LabelInfo>::iterator li =
                        labels.find(z->labelName->str);
                    if (li != labels.end()) {
                        env = li->second.env;
                        i = li->second.blockPos;
                        continue;
                    }
                }
                while (!blockTemps.empty())
                    blockTemps.pop_back();
                return result;
            }
        }
        while (!blockTemps.empty())
            blockTemps.pop_back();
        return NULL;
    }

    case LABEL :
    case BINDING :
        error("invalid statement");

    case ASSIGNMENT : {
        Assignment *x = (Assignment *)stmt.raw();
        pushTempBlock();
        ValuePtr right = evaluateNonVoid(x->right, env);
        ValuePtr left = evaluateNonVoid(x->left, env);
        if (left->isOwned)
            error("cannot assign to a temp");
        valueAssign(left, right);
        popTempBlock();
        return NULL;
    }

    case GOTO : {
        Goto *x = (Goto *)stmt.raw();
        return new GotoResult(x->labelName);
    }

    case RETURN : {
        Return *x = (Return *)stmt.raw();
        if (!x->expr)
            return new ReturnResult(voidValue);
        ValuePtr v = evaluateToStatic(x->expr, env);
        return new ReturnResult(v);
    }

    case RETURN_REF : {
        ReturnRef *x = (ReturnRef *)stmt.raw();
        pushTempBlock();
        ValuePtr v = evaluateNonVoid(x->expr, env);
        if (v->isOwned)
            error("cannot return a temporary by reference");
        popTempBlock();
        return new ReturnResult(v);
    }

    case IF : {
        If *x = (If *)stmt.raw();
        bool cond = evaluateToBool(x->condition, env);
        if (cond)
            return evalStatement(x->thenPart, env);
        if (x->elsePart.raw())
            return evalStatement(x->elsePart, env);
        return NULL;
            
    }

    case EXPR_STATEMENT : {
        ExprStatement *x = (ExprStatement *)stmt.raw();
        pushTempBlock();
        evaluate(x->expr, env);
        popTempBlock();
        return NULL;
    }

    case WHILE : {
        While *x = (While *)stmt.raw();
        while (true) {
            bool cond = evaluateToBool(x->condition, env);
            if (!cond) break;
            StatementResultPtr result = evalStatement(x->body, env);
            if (!result)
                continue;
            if (result->resultKind == BREAK_RESULT)
                break;
            if (result->resultKind == CONTINUE_RESULT)
                continue;
            return result;
        }
        return NULL;
    }

    case BREAK : {
        Break *x = (Break *)stmt.raw();
        return new BreakResult(x);
    }

    case CONTINUE : {
        Continue *x = (Continue *)stmt.raw();
        return new ContinueResult(x);
    }

    case FOR : {
        For *x = (For *)stmt.raw();
        if (!x->converted)
            x->converted = convertForStatement(x);
        return evalStatement(x->converted, env);
    }

    }
    assert(false);
    return NULL;
}

void evalCollectLabels(const vector<StatementPtr> &statements,
                       unsigned startIndex, map<string, LabelInfo> &labels,
                       EnvPtr env) {
    for (unsigned i = startIndex; i < statements.size(); ++i) {
        StatementPtr x = statements[i];
        switch (x->objKind) {
        case LABEL : {
            Label *y = (Label *)x.raw();
            labels[y->name->str] = LabelInfo(env, i);
            break;
        }
        case BINDING :
            return;
        }
    }
}

EnvPtr evalBinding(BindingPtr x, EnvPtr env, vector<ValuePtr> &blockTemps) {
    ValuePtr right;
    pushTempBlock();
    switch (x->bindingKind) {
    case VAR :
        right = evaluateNonVoid(x->expr, env);
        if (!right->isOwned)
            right = cloneValue(right);
        blockTemps.push_back(right);
        right = new Value(right->type, right->buf, false);
        break;
    case REF :
        right = evaluateNonVoid(x->expr, env);
        if (right->isOwned) {
            blockTemps.push_back(right);
            right = new Value(right->type, right->buf, false);
        }
        break;
    case STATIC :
        right = evaluateNonVoid(x->expr, env);
        if (!right->isOwned)
            right = cloneValue(right);
        break;
    default :
        assert(false);
    }
    popTempBlock();
    EnvPtr env2 = new Env(env);
    addLocal(env2, x->name, right.raw());
    return env2;
}

StatementPtr convertForStatement(ForPtr x) {
    IdentifierPtr exprVar = new Identifier("%expr");
    IdentifierPtr iterVar = new Identifier("%iter");

    BlockPtr block = new Block();
    block->statements.push_back(new Binding(REF, exprVar, x->expr));

    CallPtr iteratorCall = new Call(coreNameRef("iterator"));
    iteratorCall->args.push_back(new NameRef(exprVar));
    block->statements.push_back(new Binding(VAR, iterVar, iteratorCall.raw()));

    CallPtr hasNextCall = new Call(coreNameRef("hasNext?"));
    hasNextCall->args.push_back(new NameRef(iterVar));
    CallPtr nextCall = new Call(coreNameRef("next"));
    nextCall->args.push_back(new NameRef(iterVar));
    BlockPtr whileBody = new Block();
    whileBody->statements.push_back(new Binding(REF, x->variable, nextCall.raw()));
    whileBody->statements.push_back(x->body);

    block->statements.push_back(new While(hasNextCall.raw(), whileBody.raw()));
    return block.raw();
}



//
// invokeExternal
//

ValuePtr invokeExternal(ExternalProcedurePtr x, const vector<ValuePtr> &args) {
    if (!x->llvmFunc)
        initExternalProcedure(x);
    ensureArity(args, x->args.size());
    for (unsigned i = 0; i < args.size(); ++i) {
        ExternalArgPtr earg = x->args[i];
        if (args[i]->type != x->args[i]->type2)
            fmtError("type mismatch at argument %d", (i+1));
    }
    return execLLVMFunc(x->llvmFunc, args, x->returnType2);
}

void initExternalProcedure(ExternalProcedurePtr x) {
    vector<TypePtr> argTypes;
    for (unsigned i = 0; i < x->args.size(); ++i) {
        TypePtr t = evaluateNonVoidType(x->args[i]->type, x->env);
        argTypes.push_back(t);
        x->args[i]->type2 = t;
    }
    TypePtr returnType = evaluateType(x->returnType, x->env);
    x->returnType2 = returnType;
    llvm::Function *func = generateExternalFunc(argTypes, returnType,
                                                x->name->str);
    x->llvmFunc = generateExternalWrapper(func, argTypes, returnType,
                                          x->name->str);
}

llvm::Function *
generateExternalFunc(const vector<TypePtr> &argTypes, TypePtr returnType,
                     const string &name) {
    vector<const llvm::Type*> llvmArgTypes;
    for (unsigned i = 0; i < argTypes.size(); ++i)
        llvmArgTypes.push_back(llvmType(argTypes[i]));
    const llvm::Type *llvmReturnType = llvmType(returnType);
    llvm::FunctionType *llvmFuncType =
        llvm::FunctionType::get(llvmReturnType, llvmArgTypes, false);
    llvm::Function *func =
        llvm::Function::Create(llvmFuncType, llvm::Function::ExternalLinkage,
                               name.c_str(), llvmModule);
    return func;
}

llvm::Function *
generateExternalWrapper(llvm::Function *func, const vector<TypePtr> &argTypes,
                        TypePtr returnType, const string &name) {
    vector<const llvm::Type*> llvmArgTypes;
    for (unsigned i = 0; i < argTypes.size(); ++i)
        llvmArgTypes.push_back(llvmType(pointerType(argTypes[i])));
    if (returnType != voidType)
        llvmArgTypes.push_back(llvmType(pointerType(returnType)));
    const llvm::Type *llvmVoidType =
        llvm::Type::getVoidTy(llvm::getGlobalContext());
    llvm::FunctionType *llvmFuncType =
        llvm::FunctionType::get(llvmVoidType, llvmArgTypes, false);
    llvm::Function *wrapper =
        llvm::Function::Create(llvmFuncType, llvm::Function::InternalLinkage,
                               name.c_str(), llvmModule);
    llvm::BasicBlock *block =
        llvm::BasicBlock::Create(llvm::getGlobalContext(), "code", wrapper);
    llvm::IRBuilder<> builder(block);
    vector<llvm::Value*> llvmArgs;
    llvm::Function::arg_iterator arg_i = wrapper->arg_begin();
    for (unsigned i = 0; i < argTypes.size(); ++i) {
        llvm::Value *v = builder.CreateLoad(arg_i++);
        llvmArgs.push_back(v);
    }
    llvm::Value *result =
        builder.CreateCall(func, llvmArgs.begin(), llvmArgs.end());
    if (returnType != voidType)
        builder.CreateStore(result, arg_i);
    builder.CreateRetVoid();
    return wrapper;
}



//
// execLLVMFunc
//

ValuePtr execLLVMFunc(llvm::Function *func, const vector<ValuePtr> &args,
                      TypePtr returnType) {
    vector<llvm::GenericValue> gvArgs;
    for (unsigned i = 0; i < args.size(); ++i)
        gvArgs.push_back(llvm::GenericValue(args[i]->buf));
    ValuePtr out;
    if (returnType != voidType) {
        out = allocValue(returnType);
        gvArgs.push_back(llvm::GenericValue(out->buf));
    }
    else {
        out = voidValue;
    }
    llvmEngine->runFunction(func, gvArgs);
    return out;
}



//
// invokePrimOp
//

ValuePtr invokePrimOp(PrimOpPtr x, const vector<ValuePtr> &args) {
    switch (x->primOpCode) {
    case PRIM_TypeP : {
        ensureArity(args, 1);
        ObjectPtr obj = valueToCO(args[0]);
        return boolToValue(obj->objKind == TYPE);
    }
    case PRIM_TypeSize : {
        ensureArity(args, 1);
        TypePtr t = valueToType(args[0]);
        return intToValue(typeSize(t));
    }

    case PRIM_primitiveInit : {
        ensureArity(args, 1);
        ensurePrimitiveType(args[0]->type);
        valueInit(args[0]);
        return voidValue;
    }
    case PRIM_primitiveDestroy : {
        ensureArity(args, 1);
        ensurePrimitiveType(args[0]->type);
        valueDestroy(args[0]);
        return voidValue;
    }
    case PRIM_primitiveCopy : {
        ensureArity(args, 2);
        ensurePrimitiveType(args[0]->type);
        ensureSameType(args[0]->type, args[1]->type);
        valueCopy(args[0], args[1]);
        return voidValue;
    }
    case PRIM_primitiveAssign : {
        ensureArity(args, 2);
        ensurePrimitiveType(args[0]->type);
        ensureSameType(args[0]->type, args[1]->type);
        valueAssign(args[0], args[1]);
        return voidValue;
    }
    case PRIM_primitiveEqualsP : {
        ensureArity(args, 2);
        ensurePrimitiveType(args[0]->type);
        ensureSameType(args[0]->type, args[1]->type);
        return boolToValue(valueEquals(args[0], args[1]));
    }
    case PRIM_primitiveHash : {
        ensureArity(args, 1);
        ensurePrimitiveType(args[0]->type);
        return intToValue(valueHash(args[0]));
    }

    case PRIM_BoolTypeP : {
        ensureArity(args, 1);
        TypePtr t = valueToType(args[0]);
        return boolToValue(t == boolType);
    }
    case PRIM_boolNot : {
        ensureArity(args, 1);
        bool x = valueToBool(args[0]);
        return boolToValue(!x);
    }
    case PRIM_boolTruth : {
        ensureArity(args, 1);
        bool x = valueToBool(args[0]);
        return boolToValue(x);
    }

    case PRIM_IntegerTypeP : {
        ensureArity(args, 1);
        TypePtr t = valueToType(args[0]);
        return boolToValue(t->typeKind == INTEGER_TYPE);
    }
    case PRIM_SignedIntegerTypeP : {
        ensureArity(args, 1);
        TypePtr t = valueToType(args[0]);
        if (t->typeKind != INTEGER_TYPE)
            return boolToValue(false);
        IntegerType *y = (IntegerType *)t.raw();
        return boolToValue(y->isSigned);
    }
    case PRIM_FloatTypeP : {
        ensureArity(args, 1);
        TypePtr t = valueToType(args[0]);
        return boolToValue(t->typeKind == FLOAT_TYPE);
    }
    case PRIM_numericEqualsP : {
        ensureArity(args, 2);
        ensureNumericType(args[0]->type);
        ensureSameType(args[0]->type, args[1]->type);
        return boolToValue(numericEquals(args[0], args[1]));
    }
    case PRIM_numericLesserP : {
        ensureArity(args, 2);
        ensureNumericType(args[0]->type);
        ensureSameType(args[0]->type, args[1]->type);
        return boolToValue(numericLesser(args[0], args[1]));
    }
    case PRIM_numericAdd : {
        ensureArity(args, 2);
        ensureNumericType(args[0]->type);
        ensureSameType(args[0]->type, args[1]->type);
        return numericAdd(args[0], args[1]);
    }
    case PRIM_numericSubtract : {
        ensureArity(args, 2);
        ensureNumericType(args[0]->type);
        ensureSameType(args[0]->type, args[1]->type);
        return numericSubtract(args[0], args[1]);
    }
    case PRIM_numericMultiply : {
        ensureArity(args, 2);
        ensureNumericType(args[0]->type);
        ensureSameType(args[0]->type, args[1]->type);
        return numericMultiply(args[0], args[1]);
    }
    case PRIM_numericDivide : {
        ensureArity(args, 2);
        ensureNumericType(args[0]->type);
        ensureSameType(args[0]->type, args[1]->type);
        return numericDivide(args[0], args[1]);
    }
    case PRIM_numericNegate : {
        ensureArity(args, 1);
        ensureNumericType(args[0]->type);
        return numericNegate(args[0]);
    }

    case PRIM_integerRemainder : {
        ensureArity(args, 2);
        ensureIntegerType(args[0]->type);
        ensureSameType(args[0]->type, args[1]->type);
        return integerRemainder(args[0], args[1]);
    }
    case PRIM_integerShiftLeft : {
        ensureArity(args, 2);
        ensureIntegerType(args[0]->type);
        ensureSameType(args[0]->type, args[1]->type);
        return integerShiftLeft(args[0], args[1]);
    }
    case PRIM_integerShiftRight : {
        ensureArity(args, 2);
        ensureIntegerType(args[0]->type);
        ensureSameType(args[0]->type, args[1]->type);
        return integerShiftRight(args[0], args[1]);
    }
    case PRIM_integerBitwiseAnd : {
        ensureArity(args, 2);
        ensureIntegerType(args[0]->type);
        ensureSameType(args[0]->type, args[1]->type);
        return integerBitwiseAnd(args[0], args[1]);
    }
    case PRIM_integerBitwiseOr : {
        ensureArity(args, 2);
        ensureIntegerType(args[0]->type);
        ensureSameType(args[0]->type, args[1]->type);
        return integerBitwiseOr(args[0], args[1]);
    }
    case PRIM_integerBitwiseXor : {
        ensureArity(args, 2);
        ensureIntegerType(args[0]->type);
        ensureSameType(args[0]->type, args[1]->type);
        return integerBitwiseXor(args[0], args[1]);
    }

    case PRIM_numericConvert : {
        ensureArity(args, 2);
        TypePtr t = valueToType(args[0]);
        ensureNumericType(t);
        ensureNumericType(args[1]->type);
        return numericConvert(t, args[1]);
    }

    case PRIM_VoidTypeP : {
        ensureArity(args, 1);
        TypePtr t = valueToType(args[0]);
        return boolToValue(t == voidType);
    }

    case PRIM_CompilerObjectTypeP : {
        ensureArity(args, 1);
        TypePtr t = valueToType(args[0]);
        return boolToValue(t == compilerObjectType);
    }

    case PRIM_PointerTypeP : {
        ensureArity(args, 1);
        TypePtr t = valueToType(args[0]);
        return boolToValue(t->typeKind == POINTER_TYPE);
    }
    case PRIM_PointerType : {
        ensureArity(args, 1);
        TypePtr t = valueToType(args[0]);
        return coToValue(pointerType(t).raw());
    }
    case PRIM_Pointer : {
        error("Pointer type constructor cannot be invoked");
    }
    case PRIM_PointeeType : {
        ensureArity(args, 1);
        TypePtr t = valueToType(args[0]);
        ensurePointerType(t);
        PointerType *pt = (PointerType *)t.raw();
        return coToValue(pt->pointeeType.raw());
    }

    case PRIM_addressOf : {
        ensureArity(args, 1);
        ValuePtr out = allocValue(pointerType(args[0]->type));
        *((void **)out->buf) = args[0]->buf;
        return out;
    }
    case PRIM_pointerDereference : {
        ensureArity(args, 1);
        ensurePointerType(args[0]->type);
        PointerType *t = (PointerType *)args[0]->type.raw();
        void *ptrValue = *((void **)args[0]->buf);
        return new Value(t->pointeeType, (char *)ptrValue, false);
    }
    case PRIM_pointerToInt : {
        ensureArity(args, 2);
        TypePtr t = valueToType(args[0]);
        ensureIntegerType(t);
        IntegerType *t2 = (IntegerType *)t.raw();
        ensurePointerType(args[1]->type);
        void *ptrValue = *((void **)args[1]->buf);
        return pointerToInt(t2, ptrValue);
    }
    case PRIM_intToPointer : {
        ensureArity(args, 2);
        TypePtr pointeeType = valueToType(args[0]);
        ensureIntegerType(args[1]->type);
        return intToPointer(pointeeType, args[1]);
    }
    case PRIM_pointerCast : {
        ensureArity(args, 2);
        TypePtr pointeeType = valueToType(args[0]);
        ensurePointerType(args[1]->type);
        ValuePtr out = allocValue(pointerType(pointeeType));
        *((void **)out->buf) = *((void **)args[1]->buf);
        return out;
    }
    case PRIM_allocateMemory : {
        ensureArity(args, 2);
        TypePtr t = valueToType(args[0]);
        int n = valueToInt(args[1]);
        void *ptr = malloc(typeSize(t) * n);
        ValuePtr out = allocValue(pointerType(t));
        *((void **)out->buf) = ptr;
        return out;
    }
    case PRIM_freeMemory : {
        ensureArity(args, 1);
        ensurePointerType(args[0]->type);
        void *ptr = *((void **)args[0]->buf);
        free(ptr);
        return voidValue;
    }

    case PRIM_ArrayTypeP : {
        ensureArity(args, 1);
        TypePtr t = valueToType(args[0]);
        return boolToValue(t->typeKind == ARRAY_TYPE);
    }
    case PRIM_ArrayType : {
        ensureArity(args, 2);
        TypePtr t = valueToType(args[0]);
        int n = valueToInt(args[1]);
        return coToValue(arrayType(t, n).raw());
    }
    case PRIM_Array : {
        error("Array type constructor cannot be invoked");
    }
    case PRIM_ArrayElementType : {
        ensureArity(args, 1);
        TypePtr t = valueToType(args[0]);
        ensureArrayType(t);
        ArrayType *at = (ArrayType *)t.raw();
        return coToValue(at->elementType.raw());
    }
    case PRIM_ArraySize : {
        ensureArity(args, 1);
        TypePtr t = valueToType(args[0]);
        ensureArrayType(t);
        ArrayType *at = (ArrayType *)t.raw();
        return intToValue(at->size);
    }
    case PRIM_array : {
        if (args.empty())
            error("atleast one argument required for creating an array");
        TypePtr elementType = args[0]->type;
        int n = (int)args.size();
        ValuePtr v = allocValue(arrayType(elementType, n));
        int esize = typeSize(elementType);
        for (int i = 0; i < n; ++i) {
            if (args[i]->type != elementType)
                fmtError("type mismatch at argument %d", (i+1));
            ValuePtr eref = new Value(elementType, v->buf + i*esize, false);
            valueCopy(eref, args[i]);
        }
        return v;
    }
    case PRIM_arrayRef : {
        ensureArity(args, 2);
        ensureArrayType(args[0]->type);
        int i = valueToInt(args[1]);
        ArrayType *at = (ArrayType *)args[0]->type.raw();
        int esize = typeSize(at->elementType);
        return new Value(at->elementType, args[0]->buf + i*esize, false);
    }

    case PRIM_TupleTypeP : {
        ensureArity(args, 1);
        TypePtr t = valueToType(args[0]);
        return boolToValue(t->typeKind == TUPLE_TYPE);
    }
    case PRIM_TupleType : {
        if (args.size() < 2)
            error("tuple type requires atleast two element types");
        vector<TypePtr> elementTypes;
        for (unsigned i = 0; i < args.size(); ++i)
            elementTypes.push_back(valueToType(args[i]));
        return coToValue(tupleType(elementTypes).raw());
    }
    case PRIM_Tuple : {
        error("Tuple type constructor cannot be invoked");
    }
    case PRIM_TupleSize : {
        ensureArity(args, 1);
        TypePtr t = valueToType(args[0]);
        ensureTupleType(t);
        TupleType *tt = (TupleType *)t.raw();
        return intToValue(tt->elementTypes.size());
    }
    case PRIM_TupleElementType : {
        ensureArity(args, 2);
        TypePtr t = valueToType(args[0]);
        ensureTupleType(t);
        TupleType *tt = (TupleType *)t.raw();
        int i = valueToInt(args[1]);
        if ((i < 0) || (i >= (int)tt->elementTypes.size()))
            error("tuple type index out of range");
        return coToValue(tt->elementTypes[i].raw());
    }
    case PRIM_TupleElementOffset : {
        ensureArity(args, 2);
        TypePtr t = valueToType(args[0]);
        ensureTupleType(t);
        TupleType *tt = (TupleType *)t.raw();
        int i = valueToInt(args[1]);
        if ((i < 0) || (i >= (int)tt->elementTypes.size()))
            error("tuple type index out of range");
        const llvm::StructLayout *layout = tupleTypeLayout(tt);
        return intToValue(layout->getElementOffset(i));
    }
    case PRIM_tuple : {
        if (args.size() < 2)
            error("tuples require atleast two elements");
        vector<TypePtr> elementTypes;
        for (unsigned i = 0; i < args.size(); ++i)
            elementTypes.push_back(args[i]->type);
        TypePtr t = tupleType(elementTypes);
        TupleType *tt = (TupleType *)t.raw();
        ValuePtr v = allocValue(t);
        const llvm::StructLayout *layout = tupleTypeLayout(tt);
        for (unsigned i = 0; i < args.size(); ++i) {
            char *p = v->buf + layout->getElementOffset(i);
            valueCopy(new Value(elementTypes[i], p, false), args[i]);
        }
        return v;
    }
    case PRIM_tupleRef : {
        ensureArity(args, 2);
        ensureTupleType(args[0]->type);
        TupleType *tt = (TupleType *)args[0]->type.raw();
        int i = valueToInt(args[1]);
        if ((i < 0) || (i >= (int)tt->elementTypes.size()))
            error("tuple type index out of range");
        const llvm::StructLayout *layout = tupleTypeLayout(tt);
        char *p = args[0]->buf + layout->getElementOffset(i);
        return new Value(tt->elementTypes[i], p, false);
    }

    case PRIM_RecordTypeP : {
        ensureArity(args, 1);
        TypePtr t = valueToType(args[0]);
        return boolToValue(t->typeKind == RECORD_TYPE);
    }
    case PRIM_RecordType : {
        if (args.size() < 1)
            error("insufficient no. of arguments");
        ObjectPtr obj = valueToCO(args[0]);
        if (obj->objKind != RECORD)
            error("invalid record argument");
        RecordPtr r = (Record *)obj.raw();
        if (r->patternVars.size() != args.size()-1)
            error("incorrect no. of arguments");
        vector<ValuePtr> params;
        for (unsigned i = 1; i < args.size(); ++i)
            params.push_back(args[i]);
        return coToValue(recordType(r, params).raw());
    }
    case PRIM_RecordFieldCount : {
        ensureArity(args, 1);
        TypePtr t = valueToType(args[0]);
        ensureRecordType(t);
        RecordType *rt = (RecordType *)t.raw();
        return intToValue(recordFieldTypes(rt).size());
    }
    case PRIM_RecordFieldType : {
        ensureArity(args, 2);
        TypePtr t = valueToType(args[0]);
        ensureRecordType(t);
        RecordType *rt = (RecordType *)t.raw();
        const vector<TypePtr> &fieldTypes = recordFieldTypes(rt);
        int i = valueToInt(args[1]);
        if ((i < 0) || (i >= (int)fieldTypes.size()))
            error("field index out of range");
        return coToValue(fieldTypes[i].raw());
    }
    case PRIM_RecordFieldOffset : {
        ensureArity(args, 2);
        TypePtr t = valueToType(args[0]);
        ensureRecordType(t);
        RecordType *rt = (RecordType *)t.raw();
        const vector<TypePtr> &fieldTypes = recordFieldTypes(rt);
        int i = valueToInt(args[1]);
        if ((i < 0) || (i >= (int)fieldTypes.size()))
            error("field index out of range");
        const llvm::StructLayout *layout = recordTypeLayout(rt);
        return intToValue(layout->getElementOffset(i));
    }
    case PRIM_RecordFieldIndex : {
        ensureArity(args, 2);
        TypePtr t = valueToType(args[0]);
        ensureRecordType(t);
        RecordType *rt = (RecordType *)t.raw();
        ObjectPtr obj = valueToCO(args[1]);
        if (obj->objKind != IDENTIFIER)
            error("expecting an identifier value");
        Identifier *name = (Identifier *)obj.raw();
        const map<string, int> &fieldIndexMap = recordFieldIndexMap(rt);
        map<string, int>::const_iterator fi = fieldIndexMap.find(name->str);
        if (fi == fieldIndexMap.end())
            error("field not in record");
        return intToValue(fi->second);
    }

    case PRIM_recordFieldRef : {
        ensureArity(args, 2);
        ensureRecordType(args[0]->type);
        RecordType *rt = (RecordType *)args[0]->type.raw();
        int i = valueToInt(args[1]);
        const vector<TypePtr> &fieldTypes = recordFieldTypes(rt);
        if ((i < 0) || (i >= (int)fieldTypes.size()))
            error("field index out of range");
        const llvm::StructLayout *layout = recordTypeLayout(rt);
        char *p = args[0]->buf + layout->getElementOffset(i);
        return new Value(fieldTypes[i], p, false);
    }
    case PRIM_recordFieldRefByName : {
        ensureArity(args, 2);
        ensureRecordType(args[0]->type);
        RecordType *rt = (RecordType *)args[0]->type.raw();
        ObjectPtr obj = valueToCO(args[1]);
        if (obj->objKind != IDENTIFIER)
            error("expecting an identifier value");
        Identifier *name = (Identifier *)obj.raw();
        const map<string, int> &fieldIndexMap = recordFieldIndexMap(rt);
        map<string, int>::const_iterator fi = fieldIndexMap.find(name->str);
        if (fi == fieldIndexMap.end())
            error("field not in record");
        int i = fi->second;
        const vector<TypePtr> &fieldTypes = recordFieldTypes(rt);
        const llvm::StructLayout *layout = recordTypeLayout(rt);
        char *p = args[0]->buf + layout->getElementOffset(i);
        return new Value(fieldTypes[i], p, false);
    }

    case PRIM_recordInit : {
        ensureArity(args, 1);
        ensureRecordType(args[0]->type);
        RecordType *rt = (RecordType *)args[0]->type.raw();
        const vector<TypePtr> &fieldTypes = recordFieldTypes(rt);
        const llvm::StructLayout *layout = recordTypeLayout(rt);
        for (unsigned i = 0; i < fieldTypes.size(); ++i) {
            char *p = args[0]->buf + layout->getElementOffset(i);
            valueInit(new Value(fieldTypes[i], p, false));
        }
        return voidValue;
    }
    case PRIM_recordDestroy : {
        ensureArity(args, 1);
        ensureRecordType(args[0]->type);
        RecordType *rt = (RecordType *)args[0]->type.raw();
        const vector<TypePtr> &fieldTypes = recordFieldTypes(rt);
        const llvm::StructLayout *layout = recordTypeLayout(rt);
        for (unsigned i = 0; i < fieldTypes.size(); ++i) {
            char *p = args[0]->buf + layout->getElementOffset(i);
            valueDestroy(new Value(fieldTypes[i], p, false));
        }
        return voidValue;
    }
    case PRIM_recordCopy : {
        ensureArity(args, 2);
        ensureRecordType(args[0]->type);
        ensureSameType(args[0]->type, args[1]->type);
        RecordType *rt = (RecordType *)args[0]->type.raw();
        const vector<TypePtr> &fieldTypes = recordFieldTypes(rt);
        const llvm::StructLayout *layout = recordTypeLayout(rt);
        for (unsigned i = 0; i < fieldTypes.size(); ++i) {
            unsigned offset = layout->getElementOffset(i);
            TypePtr t = fieldTypes[i];
            valueCopy(new Value(t, args[0]->buf + offset, false),
                      new Value(t, args[1]->buf + offset, false));
        }
        return voidValue;
    }
    case PRIM_recordAssign : {
        ensureArity(args, 2);
        ensureRecordType(args[0]->type);
        ensureSameType(args[0]->type, args[1]->type);
        RecordType *rt = (RecordType *)args[0]->type.raw();
        const vector<TypePtr> &fieldTypes = recordFieldTypes(rt);
        const llvm::StructLayout *layout = recordTypeLayout(rt);
        for (unsigned i = 0; i < fieldTypes.size(); ++i) {
            unsigned offset = layout->getElementOffset(i);
            TypePtr t = fieldTypes[i];
            valueAssign(new Value(t, args[0]->buf + offset, false),
                        new Value(t, args[1]->buf + offset, false));
        }
        return voidValue;
    }
    case PRIM_recordEqualsP : {
        ensureArity(args, 2);
        ensureRecordType(args[0]->type);
        ensureSameType(args[0]->type, args[1]->type);
        RecordType *rt = (RecordType *)args[0]->type.raw();
        const vector<TypePtr> &fieldTypes = recordFieldTypes(rt);
        const llvm::StructLayout *layout = recordTypeLayout(rt);
        for (unsigned i = 0; i < fieldTypes.size(); ++i) {
            unsigned offset = layout->getElementOffset(i);
            TypePtr t = fieldTypes[i];
            if (!valueEquals(new Value(t, args[0]->buf + offset, false),
                             new Value(t, args[1]->buf + offset, false)))
                return boolToValue(false);
        }
        return boolToValue(true);
    }
    case PRIM_recordHash : {
        ensureArity(args, 1);
        ensureRecordType(args[0]->type);
        RecordType *rt = (RecordType *)args[0]->type.raw();
        const vector<TypePtr> &fieldTypes = recordFieldTypes(rt);
        const llvm::StructLayout *layout = recordTypeLayout(rt);
        int h = 0;
        for (unsigned i = 0; i < fieldTypes.size(); ++i) {
            char *p = args[0]->buf + layout->getElementOffset(i);
            h += valueHash(new Value(fieldTypes[i], p, false));
        }
        return intToValue(h);
    }
    }
    error("invalid prim op");
    return NULL;
}



//
// numeric primitives
//

#define NUMERIC_BINARY_OP(operation, a, b) \
    switch (a->type->typeKind) { \
    case INTEGER_TYPE : { \
        IntegerType *t = (IntegerType *)a->type.raw(); \
        if (t->isSigned) { \
            switch (t->bits) { \
            case 8 : return operation<char>(a, b); \
            case 16 : return operation<short>(a, b); \
            case 32 : return operation<long>(a, b); \
            case 64 : return operation<long long>(a, b); \
            default : \
                assert (false); \
            } \
        } \
        else { \
            switch (t->bits) { \
            case 8 : return operation<unsigned char>(a, b); \
            case 16 : return operation<unsigned short>(a, b); \
            case 32 : return operation<unsigned long>(a, b); \
            case 64 : return operation<unsigned long long>(a, b); \
            default : \
                assert (false); \
            } \
        } \
        break; \
    } \
    case FLOAT_TYPE : { \
        FloatType *t = (FloatType *)a->type.raw(); \
        switch (t->bits) { \
        case 32 : return operation<float>(a, b); \
        case 64 : return operation<double>(a, b); \
        default : \
            assert(false); \
        } \
        break; \
    } \
    default : \
        assert(false); \
    }
    

template <typename T>
bool _numericEquals(ValuePtr a, ValuePtr b) {
    return *((T *)a->buf) == *((T *)b->buf);
}

bool numericEquals(ValuePtr a, ValuePtr b) {
    assert(a->type == b->type);
    NUMERIC_BINARY_OP(_numericEquals, a, b);
    return false;
}

template <typename T>
bool _numericLesser(ValuePtr a, ValuePtr b) {
    return *((T *)a->buf) < *((T *)b->buf);
}

bool numericLesser(ValuePtr a, ValuePtr b) {
    assert(a->type == b->type);
    NUMERIC_BINARY_OP(_numericLesser, a, b);
    return false;
}

template <typename T>
ValuePtr _numericAdd(ValuePtr a, ValuePtr b) {
    ValuePtr v = allocValue(a->type);
    *((T *)v->buf) = *((T *)a->buf) + *((T *)b->buf);
    return v;
}

ValuePtr numericAdd(ValuePtr a, ValuePtr b) {
    assert(a->type == b->type);
    NUMERIC_BINARY_OP(_numericAdd, a, b);
    return NULL;
}

template <typename T>
ValuePtr _numericSubtract(ValuePtr a, ValuePtr b) {
    ValuePtr v = allocValue(a->type);
    *((T *)v->buf) = *((T *)a->buf) - *((T *)b->buf);
    return v;
}

ValuePtr numericSubtract(ValuePtr a, ValuePtr b) {
    assert(a->type == b->type);
    NUMERIC_BINARY_OP(_numericSubtract, a, b);
    return NULL;
}

template <typename T>
ValuePtr _numericMultiply(ValuePtr a, ValuePtr b) {
    ValuePtr v = allocValue(a->type);
    *((T *)v->buf) = *((T *)a->buf) * *((T *)b->buf);
    return v;
}

ValuePtr numericMultiply(ValuePtr a, ValuePtr b) {
    assert(a->type == b->type);
    NUMERIC_BINARY_OP(_numericMultiply, a, b);
    return NULL;
}

template <typename T>
ValuePtr _numericDivide(ValuePtr a, ValuePtr b) {
    ValuePtr v = allocValue(a->type);
    *((T *)v->buf) = *((T *)a->buf) / *((T *)b->buf);
    return v;
}

ValuePtr numericDivide(ValuePtr a, ValuePtr b) {
    assert(a->type == b->type);
    NUMERIC_BINARY_OP(_numericDivide, a, b);
    return NULL;
}

template <typename T>
ValuePtr _numericNegate(ValuePtr a) {
    ValuePtr v = allocValue(a->type);
    *((T *)v->buf) = - *((T *)a->buf);
    return v;
}

ValuePtr numericNegate(ValuePtr a) {
    switch (a->type->typeKind) {
    case INTEGER_TYPE : {
        IntegerType *t = (IntegerType *)a->type.raw();
        if (t->isSigned) {
            switch (t->bits) {
            case 8 : return _numericNegate<char>(a);
            case 16 : return _numericNegate<short>(a);
            case 32 : return _numericNegate<long>(a);
            case 64 : return _numericNegate<long long>(a);
            default :
                assert (false);
            }
        }
        else {
            switch (t->bits) {
            case 8 : return _numericNegate<unsigned char>(a);
            case 16 : return _numericNegate<unsigned short>(a);
            case 32 : return _numericNegate<unsigned long>(a);
            case 64 : return _numericNegate<unsigned long long>(a);
            default :
                assert (false);
            }
        }
        break;
    }
    case FLOAT_TYPE : {
        FloatType *t = (FloatType *)a->type.raw();
        switch (t->bits) {
        case 32 : return _numericNegate<float>(a);
        case 64 : return _numericNegate<double>(a);
        default :
            assert(false);
        }
        break;
    }
    default :
        assert(false);
    }
}

#define INTEGER_BINARY_OP(operation, a, b) \
    switch (a->type->typeKind) { \
    case INTEGER_TYPE : { \
        IntegerType *t = (IntegerType *)a->type.raw(); \
        if (t->isSigned) { \
            switch (t->bits) { \
            case 8 : return operation<char>(a, b); \
            case 16 : return operation<short>(a, b); \
            case 32 : return operation<long>(a, b); \
            case 64 : return operation<long long>(a, b); \
            default : \
                assert (false); \
            } \
        } \
        else { \
            switch (t->bits) { \
            case 8 : return operation<unsigned char>(a, b); \
            case 16 : return operation<unsigned short>(a, b); \
            case 32 : return operation<unsigned long>(a, b); \
            case 64 : return operation<unsigned long long>(a, b); \
            default : \
                assert (false); \
            } \
        } \
        break; \
    } \
    default : \
        assert(false); \
    }

template <typename T>
ValuePtr _integerRemainder(ValuePtr a, ValuePtr b) {
    ValuePtr v = allocValue(a->type);
    *((T *)v->buf) = *((T *)a->buf) % *((T *)b->buf);
    return v;
}

ValuePtr integerRemainder(ValuePtr a, ValuePtr b) {
    assert(a->type == b->type);
    INTEGER_BINARY_OP(_integerRemainder, a, b);
    return NULL;
}

template <typename T>
ValuePtr _integerShiftLeft(ValuePtr a, ValuePtr b) {
    ValuePtr v = allocValue(a->type);
    *((T *)v->buf) = *((T *)a->buf) << *((T *)b->buf);
    return v;
}

ValuePtr integerShiftLeft(ValuePtr a, ValuePtr b) {
    assert(a->type == b->type);
    INTEGER_BINARY_OP(_integerShiftLeft, a, b);
    return NULL;
}

template <typename T>
ValuePtr _integerShiftRight(ValuePtr a, ValuePtr b) {
    ValuePtr v = allocValue(a->type);
    *((T *)v->buf) = *((T *)a->buf) >> *((T *)b->buf);
    return v;
}

ValuePtr integerShiftRight(ValuePtr a, ValuePtr b) {
    assert(a->type == b->type);
    INTEGER_BINARY_OP(_integerShiftRight, a, b);
    return NULL;
}

template <typename T>
ValuePtr _integerBitwiseAnd(ValuePtr a, ValuePtr b) {
    ValuePtr v = allocValue(a->type);
    *((T *)v->buf) = *((T *)a->buf) & *((T *)b->buf);
    return v;
}

ValuePtr integerBitwiseAnd(ValuePtr a, ValuePtr b) {
    assert(a->type == b->type);
    INTEGER_BINARY_OP(_integerBitwiseAnd, a, b);
    return NULL;
}

template <typename T>
ValuePtr _integerBitwiseOr(ValuePtr a, ValuePtr b) {
    ValuePtr v = allocValue(a->type);
    *((T *)v->buf) = *((T *)a->buf) | *((T *)b->buf);
    return v;
}

ValuePtr integerBitwiseOr(ValuePtr a, ValuePtr b) {
    assert(a->type == b->type);
    INTEGER_BINARY_OP(_integerBitwiseOr, a, b);
    return NULL;
}

template <typename T>
ValuePtr _integerBitwiseXor(ValuePtr a, ValuePtr b) {
    ValuePtr v = allocValue(a->type);
    *((T *)v->buf) = *((T *)a->buf) ^ *((T *)b->buf);
    return v;
}

ValuePtr integerBitwiseXor(ValuePtr a, ValuePtr b) {
    assert(a->type == b->type);
    INTEGER_BINARY_OP(_integerBitwiseXor, a, b);
    return NULL;
}

template <typename DEST, typename SRC>
void _numericConvert1(void *dest, void *src) {
    *((DEST *)dest) = (DEST)(*((SRC *)src));
}

template <typename DEST>
void _numericConvert2(void *dest, ValuePtr a) {
    switch (a->type->typeKind) {
    case INTEGER_TYPE : {
        IntegerType *t = (IntegerType *)a->type.raw();
        if (t->isSigned) {
            switch (t->bits) {
            case 8 :
                _numericConvert1<DEST, char>(dest, a->buf);
                break;
            case 16 :
                _numericConvert1<DEST, short>(dest, a->buf);
                break;
            case 32 :
                _numericConvert1<DEST, long>(dest, a->buf);
                break;
            case 64 :
                _numericConvert1<DEST, long long>(dest, a->buf);
                break;
            default :
                assert(false);
            }
        }
        else {
            switch (t->bits) {
            case 8 :
                _numericConvert1<DEST, unsigned char>(dest, a->buf);
                break;
            case 16 :
                _numericConvert1<DEST, unsigned short>(dest, a->buf);
                break;
            case 32 :
                _numericConvert1<DEST, unsigned long>(dest, a->buf);
                break;
            case 64 :
                _numericConvert1<DEST, unsigned long long>(dest, a->buf);
                break;
            default :
                assert(false);
            }
        }
        break;
    }
    case FLOAT_TYPE : {
        FloatType *t = (FloatType *)a->type.raw();
        switch (t->bits) {
        case 32 :
            _numericConvert1<DEST, float>(dest, a->buf);
            break;
        case 64 :
            _numericConvert1<DEST, double>(dest, a->buf);
            break;
        default :
            assert(false);
        }
        break;
    }
    default :
        assert(false);
    }
}

ValuePtr numericConvert(TypePtr t, ValuePtr a) {
    ValuePtr v = allocValue(t);
    switch (t->typeKind) {
    case INTEGER_TYPE : {
        IntegerType *t2 = (IntegerType *)t.raw();
        if (t2->isSigned) {
            switch (t2->bits) {
            case 8 :
                _numericConvert2<char>(v->buf, a);
                break;
            case 16 :
                _numericConvert2<short>(v->buf, a);
                break;
            case 32 :
                _numericConvert2<long>(v->buf, a);
                break;
            case 64 :
                _numericConvert2<long long>(v->buf, a);
                break;
            default :
                assert(false);
            }
        }
        else {
            switch (t2->bits) {
            case 8 :
                _numericConvert2<unsigned char>(v->buf, a);
                break;
            case 16 :
                _numericConvert2<unsigned short>(v->buf, a);
                break;
            case 32 :
                _numericConvert2<unsigned long>(v->buf, a);
                break;
            case 64 :
                _numericConvert2<unsigned long long>(v->buf, a);
                break;
            default :
                assert(false);
            }
        }
        break;
    }
    case FLOAT_TYPE : {
        FloatType *t2 = (FloatType *)t.raw();
        switch (t2->bits) {
        case 32 :
            _numericConvert2<float>(v->buf, a);
            break;
        case 64 :
            _numericConvert2<double>(v->buf, a);
            break;
        default :
            assert(false);
        }
        break;
    }
    default :
        assert(false);
    }
    return v;
}



//
// pointer primitives
//

template <typename T>
void _pointerToInt(void *dest, void *ptr) {
    *((T *)dest) = (T)((long)ptr);
}

ValuePtr pointerToInt(IntegerTypePtr t, void *ptr) {
    ValuePtr v = allocValue(t.raw());
    switch (t->bits) {
    case 8 :
        _pointerToInt<char>(v->buf, ptr);
        break;
    case 16 :
        _pointerToInt<short>(v->buf, ptr);
        break;
    case 32 :
        _pointerToInt<long>(v->buf, ptr);
        break;
    case 64 :
        _pointerToInt<long long>(v->buf, ptr);
        break;
    default :
        assert(false);
    }
    return v;
}

template <typename T>
void *_intToPointer(void *x) {
    T y = *((T *)x);
    return (void *)y;
}

ValuePtr intToPointer(TypePtr pointeeType, ValuePtr a) {
    assert(a->type->typeKind == INTEGER_TYPE);
    IntegerType *t = (IntegerType *)a->type.raw();
    ValuePtr v = allocValue(pointerType(pointeeType));
    void *ptrValue = NULL;
    switch (t->bits) {
    case 8 :
        ptrValue = _intToPointer<char>(a->buf);
        break;
    case 16 :
        ptrValue = _intToPointer<short>(a->buf);
        break;
    case 32 :
        ptrValue = _intToPointer<long>(a->buf);
        break;
    case 64 :
        ptrValue = _intToPointer<long long>(a->buf);
        break;
    default :
        assert(false);
    }
    *((void **)v->buf) = ptrValue;
    return v;
}
