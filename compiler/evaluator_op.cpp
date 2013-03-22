#include "evaluator.hpp"
#include "types.hpp"
#include "error.hpp"
#include "objects.hpp"
#include "loader.hpp"
#include "env.hpp"
#include "analyzer.hpp"

#include "evaluator_op.hpp"



namespace clay {


static llvm::StringMap<const void*> staticStringTableConstants;



static void setSizeTEValue(EValuePtr v, size_t x) {
    switch (typeSize(cSizeTType)) {
    case 4 : *(size32_t *)v->addr = size32_t(x); break;
    case 8 : *(size64_t *)v->addr = size64_t(x); break;
    default : assert(false);
    }
}

/* not used yet
static void setPtrDiffTEValue(EValuePtr v, ptrdiff_t x) {
    switch (typeSize(cPtrDiffTType)) {
    case 4 : *(ptrdiff32_t *)v->addr = ptrdiff32_t(x); break;
    case 8 : *(ptrdiff64_t *)v->addr = ptrdiff64_t(x); break;
    default : assert(false);
    }
}
*/



//
// valueToStatic, valueToStaticSizeTOrInt
// valueToType, valueToNumericType, valueToIntegerType,
// valueToPointerLikeType, valueToTupleType, valueToRecordType,
// valueToVariantType, valueToEnumType, valueToIdentifier
//

static ObjectPtr valueToStatic(EValuePtr ev)
{
    if (ev->type->typeKind != STATIC_TYPE)
        return NULL;
    StaticType *st = (StaticType *)ev->type.ptr();
    return st->obj;
}

static ObjectPtr valueToStatic(MultiEValuePtr args, unsigned index)
{
    ObjectPtr obj = valueToStatic(args->values[index]);
    if (!obj)
        argumentError(index, "expecting a static value");
    return obj;
}

size_t valueHolderToSizeT(ValueHolderPtr vh)
{
    switch (typeSize(cSizeTType)) {
    case 4 : return vh->as<size32_t>();
    case 8 : return (size_t)vh->as<size64_t>();
    default : llvm_unreachable("unexpected pointer size");
    }
}

static size_t valueToStaticSizeTOrInt(MultiEValuePtr args, unsigned index)
{
    ObjectPtr obj = valueToStatic(args->values[index]);
    if (!obj || (obj->objKind != VALUE_HOLDER))
        argumentError(index, "expecting a static SizeT or Int value");
    ValueHolder *vh = (ValueHolder *)obj.ptr();
    if (vh->type == cSizeTType) {
        return valueHolderToSizeT(vh);
    }
    else if (vh->type == cIntType) {
        return size_t(vh->as<int>());
    }
    else {
        argumentError(index, "expecting a static SizeT or Int value");
        return 0;
    }
}

static TypePtr valueToType(MultiEValuePtr args, unsigned index)
{
    ObjectPtr obj = valueToStatic(args->values[index]);
    if (!obj || (obj->objKind != TYPE))
        argumentError(index, "expecting a type");
    return (Type *)obj.ptr();
}

static TypePtr valueToNumericType(MultiEValuePtr args, unsigned index)
{
    TypePtr t = valueToType(args, index);
    switch (t->typeKind) {
    case INTEGER_TYPE :
    case FLOAT_TYPE :
        return t;
    default :
        argumentTypeError(index, "numeric type", t);
        return NULL;
    }
}

static IntegerTypePtr valueToIntegerType(MultiEValuePtr args, unsigned index)
{
    TypePtr t = valueToType(args, index);
    if (t->typeKind != INTEGER_TYPE)
        argumentTypeError(index, "integer type", t);
    return (IntegerType *)t.ptr();
}

static TypePtr valueToPointerLikeType(MultiEValuePtr args, unsigned index)
{
    TypePtr t = valueToType(args, index);
    if (!isPointerOrCodePointerType(t))
        argumentTypeError(index, "pointer or code-pointer type", t);
    return t;
}

static TupleTypePtr valueToTupleType(MultiEValuePtr args, unsigned index)
{
    TypePtr t = valueToType(args, index);
    if (t->typeKind != TUPLE_TYPE)
        argumentTypeError(index, "tuple type", t);
    return (TupleType *)t.ptr();
}

static UnionTypePtr valueToUnionType(MultiEValuePtr args, unsigned index)
{
    TypePtr t = valueToType(args, index);
    if (t->typeKind != UNION_TYPE)
        argumentTypeError(index, "union type", t);
    return (UnionType *)t.ptr();
}

static RecordTypePtr valueToRecordType(MultiEValuePtr args, unsigned index)
{
    TypePtr t = valueToType(args, index);
    if (t->typeKind != RECORD_TYPE)
        argumentTypeError(index, "record type", t);
    return (RecordType *)t.ptr();
}

static VariantTypePtr valueToVariantType(MultiEValuePtr args, unsigned index)
{
    TypePtr t = valueToType(args, index);
    if (t->typeKind != VARIANT_TYPE)
        argumentTypeError(index, "variant type", t);
    return (VariantType *)t.ptr();
}

static EnumTypePtr valueToEnumType(MultiEValuePtr args, unsigned index)
{
    TypePtr t = valueToType(args, index);
    if (t->typeKind != ENUM_TYPE)
        argumentTypeError(index, "enum type", t);
    initializeEnumType((EnumType*)t.ptr());
    return (EnumType *)t.ptr();
}

static IdentifierPtr valueToIdentifier(MultiEValuePtr args, unsigned index)
{
    ObjectPtr obj = valueToStatic(args->values[index]);
    if (!obj || (obj->objKind != IDENTIFIER))
        argumentError(index, "expecting identifier value");
    return (Identifier *)obj.ptr();
}



//
// numericValue, integerValue, pointerValue, pointerLikeValue,
// arrayValue, tupleValue, recordValue, variantValue, enumValue
//

static EValuePtr numericValue(MultiEValuePtr args, unsigned index,
                              TypePtr &type)
{
    EValuePtr ev = args->values[index];
    if (type.ptr()) {
        if (ev->type != type)
            argumentTypeError(index, type, ev->type);
    }
    else {
        switch (ev->type->typeKind) {
        case INTEGER_TYPE :
        case FLOAT_TYPE :
            break;
        default :
            argumentTypeError(index, "numeric type", ev->type);
        }
        type = ev->type;
    }
    return ev;
}

static EValuePtr floatValue(MultiEValuePtr args, unsigned index,
                            FloatTypePtr &type)
{
    EValuePtr ev = args->values[index];
    if (type.ptr()) {
        if (ev->type.ptr() != type.ptr())
            argumentTypeError(index, type.ptr(), ev->type);
    }
    else {
        switch (ev->type->typeKind) {
        case FLOAT_TYPE :
            break;
        default :
            argumentTypeError(index, "float type", ev->type);
        }
        type = (FloatType*)ev->type.ptr();
    }
    return ev;
}

static EValuePtr integerOrPointerLikeValue(MultiEValuePtr args, unsigned index,
                                           TypePtr &type)
{
    EValuePtr ev = args->values[index];
    if (type.ptr()) {
        if (ev->type != type)
            argumentTypeError(index, type, ev->type);
    }
    else {
        switch (ev->type->typeKind) {
        case INTEGER_TYPE :
        case POINTER_TYPE :
        case CODE_POINTER_TYPE :
        case CCODE_POINTER_TYPE :
            break;
        default :
            argumentTypeError(index, "integer, pointer, or code pointer type", ev->type);
        }
        type = ev->type;
    }
    return ev;
}

static EValuePtr integerValue(MultiEValuePtr args, unsigned index,
                              IntegerTypePtr &type)
{
    EValuePtr ev = args->values[index];
    if (type.ptr()) {
        if (ev->type != type.ptr())
            argumentTypeError(index, type.ptr(), ev->type);
    }
    else {
        if (ev->type->typeKind != INTEGER_TYPE)
            argumentTypeError(index, "integer type", ev->type);
        type = (IntegerType *)ev->type.ptr();
    }
    return ev;
}

static EValuePtr pointerValue(MultiEValuePtr args, unsigned index,
                              PointerTypePtr &type)
{
    EValuePtr ev = args->values[index];
    if (type.ptr()) {
        if (ev->type != (Type *)type.ptr())
            argumentTypeError(index, type.ptr(), ev->type);
    }
    else {
        if (ev->type->typeKind != POINTER_TYPE)
            argumentTypeError(index, "pointer type", ev->type);
        type = (PointerType *)ev->type.ptr();
    }
    return ev;
}

static EValuePtr pointerLikeValue(MultiEValuePtr args, unsigned index,
                                  TypePtr &type)
{
    EValuePtr ev = args->values[index];
    if (type.ptr()) {
        if (ev->type != type)
            argumentTypeError(index, type, ev->type);
    }
    else {
        if (!isPointerOrCodePointerType(ev->type))
            argumentTypeError(index,
                              "pointer or code-pointer type",
                              ev->type);
        type = ev->type;
    }
    return ev;
}

static EValuePtr arrayValue(MultiEValuePtr args, unsigned index,
                            ArrayTypePtr &type)
{
    EValuePtr ev = args->values[index];
    if (type.ptr()) {
        if (ev->type != (Type *)type.ptr())
            argumentTypeError(index, type.ptr(), ev->type);
    }
    else {
        if (ev->type->typeKind != ARRAY_TYPE)
            argumentTypeError(index, "array type", ev->type);
        type = (ArrayType *)ev->type.ptr();
    }
    return ev;
}

static EValuePtr tupleValue(MultiEValuePtr args, unsigned index,
                            TupleTypePtr &type)
{
    EValuePtr ev = args->values[index];
    if (type.ptr()) {
        if (ev->type != (Type *)type.ptr())
            argumentTypeError(index, type.ptr(), ev->type);
    }
    else {
        if (ev->type->typeKind != TUPLE_TYPE)
            argumentTypeError(index, "tuple type", ev->type);
        type = (TupleType *)ev->type.ptr();
    }
    return ev;
}

static EValuePtr recordValue(MultiEValuePtr args, unsigned index,
                            RecordTypePtr &type)
{
    EValuePtr ev = args->values[index];
    if (type.ptr()) {
        if (ev->type != (Type *)type.ptr())
            argumentTypeError(index, type.ptr(), ev->type);
    }
    else {
        if (ev->type->typeKind != RECORD_TYPE)
            argumentTypeError(index, "record type", ev->type);
        type = (RecordType *)ev->type.ptr();
    }
    return ev;
}

static EValuePtr enumValue(MultiEValuePtr args, unsigned index,
                           EnumTypePtr &type)
{
    EValuePtr ev = args->values[index];
    if (type.ptr()) {
        if (ev->type != (Type *)type.ptr())
            argumentTypeError(index, type.ptr(), ev->type);
    }
    else {
        if (ev->type->typeKind != ENUM_TYPE)
            argumentTypeError(index, "enum type", ev->type);
        type = (EnumType *)ev->type.ptr();
    }
    return ev;
}



//
// binaryNumericOp
//

template <template<typename> class T>
static void binaryNumericOp(EValuePtr a, EValuePtr b, EValuePtr out)
{
    assert(a->type == b->type);
    switch (a->type->typeKind) {

    case INTEGER_TYPE : {
        IntegerType *t = (IntegerType *)a->type.ptr();
        if (t->isSigned) {
            switch (t->bits) {
            case 8 :  T<char>().eval(a, b, out); break;
            case 16 : T<short>().eval(a, b, out); break;
            case 32 : T<int>().eval(a, b, out); break;
            case 64 : T<long long>().eval(a, b, out); break;
            case 128 : T<clay_int128>().eval(a, b, out); break;
            default : assert(false);
            }
        }
        else {
            switch (t->bits) {
            case 8 :  T<unsigned char>().eval(a, b, out); break;
            case 16 : T<unsigned short>().eval(a, b, out); break;
            case 32 : T<unsigned int>().eval(a, b, out); break;
            case 64 : T<unsigned long long>().eval(a, b, out); break;
            case 128 : T<clay_uint128>().eval(a, b, out); break;
            default : assert(false);
            }
        }
        break;
    }

    case FLOAT_TYPE : {
        FloatType *t = (FloatType *)a->type.ptr();
        switch (t->bits) {
        case 32 : T<float>().eval(a, b, out); break;
        case 64 : T<double>().eval(a, b, out); break;
        case 80 : T<long double>().eval(a, b, out); break;
        default : assert(false);
        }
        break;
    }

    case POINTER_TYPE :
    case CODE_POINTER_TYPE :
    case CCODE_POINTER_TYPE : {
        switch (typeSize(a->type)) {
        case 2 : T<unsigned short>().eval(a, b, out); break;
        case 4 : T<size32_t>().eval(a, b, out); break;
        case 8 : T<size64_t>().eval(a, b, out); break;
        default : error("unsupported pointer size");
        }
        break;
    }

    default :
        assert(false);
    }
}

template<typename T>
static void overflowError(const char *op, T a, T b) {
    string buf;
    llvm::raw_string_ostream sout(buf);
    sout << "integer overflow: " << a << " " << op << " " << b;
    error(sout.str());
}

template<typename T>
static void invalidShiftError(const char *op, T a, T b) {
    string buf;
    llvm::raw_string_ostream sout(buf);
    sout << "invalid shift: " << a << " " << op << " " << b;
    error(sout.str());
}

template<typename T>
static void overflowError(const char *op, T a) {
    string buf;
    llvm::raw_string_ostream sout(buf);
    sout << "integer overflow: " << op << a;
    error(sout.str());
}

static void divideByZeroError() {
    error("integer division by zero");
}

template <typename T>
class BinaryOpHelper {
public :
    void eval(EValuePtr a, EValuePtr b, EValuePtr out) {
        perform(a->as<T>(), b->as<T>(), out->addr);
    }
    virtual void perform(T &a, T &b, void *out) = 0;
};

#define BINARY_OP(name, type, expr) \
    template <typename T>                               \
    class name : public BinaryOpHelper<T> {             \
    public :                                            \
        virtual void perform(T &a, T &b, void *out) {   \
            *((type *)out) = (expr);                    \
        }                                               \
    }

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wtautological-compare"

BINARY_OP(Op_orderedEqualsP, bool, a == b);
BINARY_OP(Op_orderedLesserP, bool, a < b);
BINARY_OP(Op_orderedLesserEqualsP, bool, a <= b);
BINARY_OP(Op_orderedGreaterP, bool, a > b);
BINARY_OP(Op_orderedGreaterEqualsP, bool, a >= b);
BINARY_OP(Op_orderedNotEqualsP, bool, a != b && a == a && b == b);
BINARY_OP(Op_orderedP, bool, a == a && b == b);
BINARY_OP(Op_unorderedEqualsP, bool, a == b || a != a || b != b);
BINARY_OP(Op_unorderedLesserP, bool, !(a >= b));
BINARY_OP(Op_unorderedLesserEqualsP, bool, !(a > b));
BINARY_OP(Op_unorderedGreaterP, bool, !(a <= b));
BINARY_OP(Op_unorderedGreaterEqualsP, bool, !(a < b));
BINARY_OP(Op_unorderedNotEqualsP, bool, a != b);
BINARY_OP(Op_unorderedP, bool, a != a || b != b);
BINARY_OP(Op_numericAdd, T, a + b);
BINARY_OP(Op_numericSubtract, T, a - b);
BINARY_OP(Op_numericMultiply, T, a * b);
BINARY_OP(Op_floatDivide, T, a / b);
BINARY_OP(Op_integerQuotient, T, a / b);
BINARY_OP(Op_integerRemainder, T, a % b);
BINARY_OP(Op_integerShiftLeft, T, a << b);
BINARY_OP(Op_integerShiftRight, T, a >> b);
BINARY_OP(Op_integerBitwiseAnd, T, a & b);
BINARY_OP(Op_integerBitwiseOr, T, a | b);
BINARY_OP(Op_integerBitwiseXor, T, a ^ b);

#pragma clang diagnostic pop

#undef BINARY_OP


//
// unaryNumericOp
//

template <template<typename> class T>
static void unaryNumericOp(EValuePtr a, EValuePtr out)
{
    switch (a->type->typeKind) {

    case INTEGER_TYPE : {
        IntegerType *t = (IntegerType *)a->type.ptr();
        if (t->isSigned) {
            switch (t->bits) {
            case 8 :  T<char>().eval(a, out); break;
            case 16 : T<short>().eval(a, out); break;
            case 32 : T<int>().eval(a, out); break;
            case 64 : T<long long>().eval(a, out); break;
            case 128 : T<clay_int128>().eval(a, out); break;
            default : assert(false);
            }
        }
        else {
            switch (t->bits) {
            case 8 :  T<unsigned char>().eval(a, out); break;
            case 16 : T<unsigned short>().eval(a, out); break;
            case 32 : T<unsigned int>().eval(a, out); break;
            case 64 : T<unsigned long long>().eval(a, out); break;
            case 128 : T<clay_uint128>().eval(a, out); break;
            default : assert(false);
            }
        }
        break;
    }

    case FLOAT_TYPE : {
        FloatType *t = (FloatType *)a->type.ptr();
        switch (t->bits) {
        case 32 : T<float>().eval(a, out); break;
        case 64 : T<double>().eval(a, out); break;
        case 80 : T<long double>().eval(a, out); break;
        default : assert(false);
        }
        break;
    }

    default :
        assert(false);
    }
}

template <typename T>
class UnaryOpHelper {
public :
    void eval(EValuePtr a, EValuePtr out) {
        perform(a->as<T>(), out->addr);
    }
    virtual void perform(T &a, void *out) = 0;
};

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4146)
#endif

#define UNARY_OP(name, type, expr) \
    template <typename T>                       \
    class name : public UnaryOpHelper<T> {      \
    public :                                    \
        virtual void perform(T &a, void *out) { \
            *((type *)out) = (expr);            \
        }                                       \
    };

UNARY_OP(Op_numericNegate, T, -a)
UNARY_OP(Op_integerBitwiseNot, T, ~a)

#undef UNARY_OP

#ifdef _MSC_VER
#pragma warning(pop)
#endif


//
// binaryIntegerOp
//

template <template<typename> class T>
static void binaryIntegerOp(EValuePtr a, EValuePtr b, EValuePtr out)
{
    assert(a->type == b->type);
    assert(a->type->typeKind == INTEGER_TYPE);
    IntegerType *t = (IntegerType *)a->type.ptr();
    if (t->isSigned) {
        switch (t->bits) {
        case 8 :  T<char>().eval(a, b, out); break;
        case 16 : T<short>().eval(a, b, out); break;
        case 32 : T<int>().eval(a, b, out); break;
        case 64 : T<long long>().eval(a, b, out); break;
        case 128 : T<clay_int128>().eval(a, b, out); break;
        default : assert(false);
        }
    }
    else {
        switch (t->bits) {
        case 8 :  T<unsigned char>().eval(a, b, out); break;
        case 16 : T<unsigned short>().eval(a, b, out); break;
        case 32 : T<unsigned int>().eval(a, b, out); break;
        case 64 : T<unsigned long long>().eval(a, b, out); break;
        case 128 : T<clay_uint128>().eval(a, b, out); break;
        default : assert(false);
        }
    }
}

#if defined(__clang__)
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wtautological-compare"
#elif defined(__GNUC__)
#  if __GNUC__ < 4 || __GNUC__ == 4 && __GNUC_MINOR__ < 6
#    warning "--- The following warnings are expected ---"
#  else
#    pragma GCC diagnostic push
#    pragma GCC diagnostic ignored "-Wtype-limits"
#  endif
#endif

template <typename T>
class Op_integerAddChecked : public BinaryOpHelper<T> {
public :
    virtual void perform(T &a, T &b, void *out) {
        if ((b < T(0) && a < std::numeric_limits<T>::min() - b)
            || (b > T(0) && a > std::numeric_limits<T>::max() - b))
                overflowError("+", a, b);
        else
            *((T *)out) = a + b;
    }
};

template <typename T>
class Op_integerSubtractChecked : public BinaryOpHelper<T> {
public :
    virtual void perform(T &a, T &b, void *out) {
        if ((b < T(0) && a > std::numeric_limits<T>::max() + b)
            || (b > T(0) && a < std::numeric_limits<T>::min() + b))
                overflowError("-", a, b);
        else
            *((T *)out) = a - b;
    }
};

template <typename T>
class Op_integerShiftLeftChecked : public BinaryOpHelper<T> {
    virtual void perform(T &a, T &b, void *out) {
        if (b < 0)
            invalidShiftError("bitshl", a, b);

        if (b == 0) {
            *((T *)out) = a;
            return;
        }

        if (std::numeric_limits<T>::min() != 0) {
            // signed
            T testa = a < 0 ? ~a : a;
            if (b > T(sizeof(T)*8 - 1) || (testa >> (sizeof(T)*8 - (size_t)b - 1)) != 0)
                overflowError("bitshl", a, b);
        } else {
            // unsigned
            if (b > T(sizeof(T)*8) || (a >> (sizeof(T)*8 - (size_t)b)) != 0)
                overflowError("bitshl", a, b);
        }
        *((T *)out) = a << b;
    }
};

#if defined(__clang__)
#  pragma clang diagnostic pop
#elif defined(__GNUC__)
#  if __GNUC__ > 4 || __GNUC__ == 4 && __GNUC_MINOR__ >= 6
#    pragma GCC diagnostic pop
#  endif
#endif

template <typename T>
class Op_integerRemainderChecked : public BinaryOpHelper<T> {
    virtual void perform(T &a, T &b, void *out) {
        if (b == 0)
            divideByZeroError();
        *((T *)out) = a % b;
    }
};

template<typename T> struct next_larger_type;
template<> struct next_larger_type<char> { typedef short type; };
template<> struct next_larger_type<short> { typedef int type; };
template<> struct next_larger_type<int> { typedef ptrdiff64_t type; };
template<> struct next_larger_type<ptrdiff64_t> { typedef clay_int128 type; };
template<> struct next_larger_type<clay_int128> { typedef clay_int128 type; };
template<> struct next_larger_type<unsigned char> { typedef unsigned short type; };
template<> struct next_larger_type<unsigned short> { typedef unsigned type; };
template<> struct next_larger_type<unsigned> { typedef size64_t type; };
template<> struct next_larger_type<size64_t> { typedef clay_uint128 type; };
template<> struct next_larger_type<clay_uint128> { typedef clay_uint128 type; };

template <typename T>
class Op_integerMultiplyChecked : public BinaryOpHelper<T> {
public :
    typedef typename next_larger_type<T>::type BigT;
    virtual void perform(T &a, T &b, void *out) {
        // XXX this won't work for 128-bit types or 64-bit types without
        // compiler int128 support
        BigT bigout = BigT(a) * BigT(b);
        if (bigout < std::numeric_limits<T>::min() || std::numeric_limits<T>::max() < bigout)
            overflowError("*", a, b);
        *((T *)out) = a * b;
    }
};

template <typename T>
class Op_integerQuotientChecked : public BinaryOpHelper<T> {
public :
    virtual void perform(T &a, T &b, void *out) {
        if (std::numeric_limits<T>::min() != 0
            && a == std::numeric_limits<T>::min()
            && b == (-1))
            overflowError("/", a, b);
        if (b == 0)
            divideByZeroError();
        *((T *)out) = a / b;
    }
};


//
// unaryIntegerOp
//

template <template<typename> class T>
static void unaryIntegerOp(EValuePtr a, EValuePtr out)
{
    assert(a->type->typeKind == INTEGER_TYPE);
    IntegerType *t = (IntegerType *)a->type.ptr();
    if (t->isSigned) {
        switch (t->bits) {
        case 8 :  T<char>().eval(a, out); break;
        case 16 : T<short>().eval(a, out); break;
        case 32 : T<int>().eval(a, out); break;
        case 64 : T<long long>().eval(a, out); break;
        case 128 : T<clay_int128>().eval(a, out); break;
        default : assert(false);
        }
    }
    else {
        switch (t->bits) {
        case 8 :  T<unsigned char>().eval(a, out); break;
        case 16 : T<unsigned short>().eval(a, out); break;
        case 32 : T<unsigned int>().eval(a, out); break;
        case 64 : T<unsigned long long>().eval(a, out); break;
        case 128 : T<clay_uint128>().eval(a, out); break;
        default : assert(false);
        }
    }
}


#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4146)
#endif

template <typename T>
class Op_integerNegateChecked : public UnaryOpHelper<T> {
public :
    virtual void perform(T &a, void *out) {
        if (std::numeric_limits<T>::min() != 0
            && a == std::numeric_limits<T>::min())
            overflowError("-", a);
        *((T *)out) = -a;
    }
};

#ifdef _MSC_VER
#pragma warning(pop)
#endif


//
// op_numericConvert
//

template <typename DEST, typename SRC, bool CHECK>
struct op_numericConvert3;

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4244)
#endif
template <typename DEST, typename SRC>
struct op_numericConvert3<DEST, SRC, false> {
    static void perform(EValuePtr dest, EValuePtr src)
    {
        SRC value = src->as<SRC>();
        dest->as<DEST>() = DEST(value);
    }
};

template <typename DEST, typename SRC>
struct op_numericConvert3<DEST, SRC, true> {
    static void perform(EValuePtr dest, EValuePtr src)
    {
        SRC value = src->as<SRC>();
        // ???
        dest->as<DEST>() = DEST(value);
    }
};
#ifdef _MSC_VER
#pragma warning(pop)
#endif

template <typename D, bool CHECK>
static void op_numericConvert2(EValuePtr dest, EValuePtr src)
{
    switch (src->type->typeKind) {
    case INTEGER_TYPE : {
        IntegerType *t = (IntegerType *)src->type.ptr();
        if (t->isSigned) {
            switch (t->bits) {
            case 8 : op_numericConvert3<D,char,CHECK>::perform(dest, src); break;
            case 16 : op_numericConvert3<D,short,CHECK>::perform(dest, src); break;
            case 32 : op_numericConvert3<D,int,CHECK>::perform(dest, src); break;
            case 64 : op_numericConvert3<D,long long,CHECK>::perform(dest, src); break;
            case 128 : op_numericConvert3<D,clay_int128,CHECK>::perform(dest, src); break;
            default : assert(false);
            }
        }
        else {
            switch (t->bits) {
            case 8 : op_numericConvert3<D,unsigned char,CHECK>::perform(dest, src); break;
            case 16 : op_numericConvert3<D,unsigned short,CHECK>::perform(dest, src); break;
            case 32 : op_numericConvert3<D,unsigned int,CHECK>::perform(dest, src); break;
            case 64 : op_numericConvert3<D,unsigned long long,CHECK>::perform(dest, src); break;
            case 128 : op_numericConvert3<D,clay_uint128,CHECK>::perform(dest, src); break;
            default : assert(false);
            }
        }
        break;
    }
    case FLOAT_TYPE : {
        FloatType *t = (FloatType *)src->type.ptr();
        switch (t->bits) {
        case 32 : op_numericConvert3<D,float,CHECK>::perform(dest, src); break;
        case 64 : op_numericConvert3<D,double,CHECK>::perform(dest, src); break;
        case 80 : op_numericConvert3<D,long double,CHECK>::perform(dest, src); break;
        default : assert(false);
        }
        break;
    }

    default :
        assert(false);
    }
}

template <bool CHECK>
static void op_numericConvert(EValuePtr dest, EValuePtr src)
{
    switch (dest->type->typeKind) {
    case INTEGER_TYPE : {
        IntegerType *t = (IntegerType *)dest->type.ptr();
        if (t->isSigned) {
            switch (t->bits) {
            case 8 : op_numericConvert2<char,CHECK>(dest, src); break;
            case 16 : op_numericConvert2<short,CHECK>(dest, src); break;
            case 32 : op_numericConvert2<int,CHECK>(dest, src); break;
            case 64 : op_numericConvert2<long long,CHECK>(dest, src); break;
            case 128 : op_numericConvert2<clay_int128,CHECK>(dest, src); break;
            default : assert(false);
            }
        }
        else {
            switch (t->bits) {
            case 8 : op_numericConvert2<unsigned char,CHECK>(dest, src); break;
            case 16 : op_numericConvert2<unsigned short,CHECK>(dest, src); break;
            case 32 : op_numericConvert2<unsigned int,CHECK>(dest, src); break;
            case 64 : op_numericConvert2<unsigned long long,CHECK>(dest, src); break;
            case 128 : op_numericConvert2<clay_uint128,CHECK>(dest, src); break;
            default : assert(false);
            }
        }
        break;
    }
    case FLOAT_TYPE : {
        FloatType *t = (FloatType *)dest->type.ptr();
        switch (t->bits) {
        case 32 : op_numericConvert2<float,false>(dest, src); break;
        case 64 : op_numericConvert2<double,false>(dest, src); break;
        case 80 : op_numericConvert2<long double,false>(dest, src); break;
        default : assert(false);
        }
        break;
    }

    default :
        assert(false);
    }
}



//
// op_intToPtrInt
//

static ptrdiff_t op_intToPtrInt(EValuePtr a)
{
    assert(a->type->typeKind == INTEGER_TYPE);
    IntegerType *t = (IntegerType *)a->type.ptr();
    if (t->isSigned) {
        switch (t->bits) {
        case 8 : return ptrdiff_t(a->as<char>());
        case 16 : return ptrdiff_t(a->as<short>());
        case 32 : return ptrdiff_t(a->as<int>());
        case 64 : return ptrdiff_t(a->as<long long>());
        case 128 : return ptrdiff_t(a->as<clay_int128>());
        default : assert(false);
        }
    }
    else {
        switch (t->bits) {
        case 8 : return ptrdiff_t(a->as<unsigned char>());
        case 16 : return ptrdiff_t(a->as<unsigned short>());
        case 32 : return ptrdiff_t(a->as<unsigned int>());
        case 64 : return ptrdiff_t(a->as<unsigned long long>());
        case 128 : return ptrdiff_t(a->as<clay_uint128>());
        default : assert(false);
        }
    }
    return 0;
}


//
// op_intToSizeT
//

static size_t op_intToSizeT(EValuePtr a)
{
    assert(a->type->typeKind == INTEGER_TYPE);
    IntegerType *t = (IntegerType *)a->type.ptr();
    if (t->isSigned) {
        switch (t->bits) {
        case 8 : return size_t(a->as<char>());
        case 16 : return size_t(a->as<short>());
        case 32 : return size_t(a->as<int>());
        case 64 : return size_t(a->as<long long>());
        case 128 : return size_t(a->as<clay_int128>());
        default : assert(false);
        }
    }
    else {
        switch (t->bits) {
        case 8 : return size_t(a->as<unsigned char>());
        case 16 : return size_t(a->as<unsigned short>());
        case 32 : return size_t(a->as<unsigned int>());
        case 64 : return size_t(a->as<unsigned long long>());
        case 128 : return size_t(a->as<clay_uint128>());
        default : assert(false);
        }
    }
    return 0;
}




//
// op_pointerToInt
//

static void op_pointerToInt(EValuePtr dest, EValuePtr ev)
{
    assert(dest->type->typeKind == INTEGER_TYPE);
    IntegerType *t = (IntegerType *)dest->type.ptr();
    if (t->isSigned) {
        switch (t->bits) {
        case 8 : dest->as<char>() = ev->as<char>(); break;
        case 16 : dest->as<short>() = ev->as<short>(); break;
        case 32 : dest->as<int>() = ev->as<int>(); break;
        case 64 : dest->as<long long>() = ev->as<long long>(); break;
        case 128 : dest->as<clay_int128>() = ev->as<clay_int128>(); break;
        default : assert(false);
        }
    }
    else {
        switch (t->bits) {
        case 8 : dest->as<unsigned char>() = ev->as<unsigned char>(); break;
        case 16 : dest->as<unsigned short>() = ev->as<unsigned short>(); break;
        case 32 : dest->as<unsigned int>() = ev->as<unsigned int>(); break;
        case 64 : dest->as<unsigned long long>() = ev->as<unsigned long long>(); break;
        case 128 : dest->as<clay_uint128>() = ev->as<clay_uint128>(); break;
        default : assert(false);
        }
    }
}



//
// evalPrimOp
//
static const void *evalStringTableConstant(llvm::StringRef s)
{
    llvm::StringMap<const void*>::const_iterator oldConstant = staticStringTableConstants.find(s);
    if (oldConstant != staticStringTableConstants.end())
        return oldConstant->second;
    size_t bits = typeSize(cSizeTType);
    size_t size = s.size();
    void *buf = malloc(size + bits + 1);
    switch (bits) {
    case 4:
        *(size32_t*)buf = (size32_t)size;
        break;
    case 8:
        *(size64_t*)buf = size;
        break;
    default:
        error("unsupported pointer width");
    }
    memcpy((void*)((char*)buf + bits), (const void*)s.begin(), s.size());
    ((char*)buf)[s.size()] = 0;
    staticStringTableConstants[s] = (const void*)buf;
    return buf;
}

void evalPrimOp(PrimOpPtr x, MultiEValuePtr args, MultiEValuePtr out)
{
    switch (x->primOpCode) {

    case PRIM_TypeP : {
        ensureArity(args, 1);
        ObjectPtr obj = valueToStatic(args->values[0]);
        bool isType = (obj.ptr() && (obj->objKind == TYPE));
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type == boolType);
        out0->as<bool>() = isType;
        break;
    }

    case PRIM_TypeSize : {
        ensureArity(args, 1);
        TypePtr t = valueToType(args, 0);
        ValueHolderPtr vh = sizeTToValueHolder(typeSize(t));
        evalStaticObject(vh.ptr(), out);
        break;
    }

    case PRIM_TypeAlignment : {
        ensureArity(args, 1);
        TypePtr t = valueToType(args, 0);
        ValueHolderPtr vh = sizeTToValueHolder(typeAlignment(t));
        evalStaticObject(vh.ptr(), out);
        break;
    }

    case PRIM_OperatorP : {
        ensureArity(args, 1);
        ObjectPtr obj = valueToStatic(args->values[0]);
        bool isOperator = false;
        if (!!obj.ptr() && (obj->objKind==PROCEDURE || obj->objKind==GLOBAL_ALIAS)) {
            TopLevelItem *item = (TopLevelItem *)obj.ptr();
            isOperator = item->name->isOperator;
        }
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type == boolType);
        out0->as<bool>() = isOperator;
        break;
    }

    case PRIM_SymbolP : {
        ensureArity(args, 1);
        ObjectPtr obj = valueToStatic(args->values[0]);
        bool isSymbol = false;
        if (obj.ptr() != NULL) {
            switch (obj->objKind) {
            case TYPE :
            case RECORD_DECL :
            case VARIANT_DECL :
            case PROCEDURE :
            case INTRINSIC :
            case GLOBAL_ALIAS:
                isSymbol = true;
                break;
            default :
                break;
            }
        }
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type == boolType);
        out0->as<bool>() = isSymbol;
        break;
    }

    case PRIM_StaticCallDefinedP : {
        if (args->size() < 1)
            arityError2(1, args->size());
        ObjectPtr callable = valueToStatic(args->values[0]);
        if (callable == NULL) {
            argumentError(0, "static callable expected");
        }
        switch (callable->objKind) {
        case TYPE :
        case RECORD_DECL :
        case VARIANT_DECL :
        case PROCEDURE :
        case INTRINSIC :
        case GLOBAL_ALIAS :
            break;
        default :
            argumentError(0, "invalid callable");
        }
        vector<TypePtr> argsKey;
        vector<ValueTempness> argsTempness;
        for (unsigned i = 1; i < args->size(); ++i) {
            TypePtr t = valueToType(args, i);
            argsKey.push_back(t);
            argsTempness.push_back(TEMPNESS_LVALUE);
        }
        CompileContextPusher pusher(callable, argsKey);
        bool isDefined = analyzeIsDefined(callable, argsKey, argsTempness);
        ValueHolderPtr vh = boolToValueHolder(isDefined);
        evalStaticObject(vh.ptr(), out);
        break;
    }

    case PRIM_StaticCallOutputTypes :
        break;

    case PRIM_StaticMonoP : {
        ensureArity(args, 1);

        bool isMono;

        ObjectPtr callable = valueToStatic(args->values[0]);
        if (callable != NULL && callable->objKind == PROCEDURE) {
            Procedure *p = (Procedure*)callable.ptr();
            isMono = p->mono.monoState == Procedure_MonoOverload;
        } else
            isMono = false;


        ValueHolderPtr vh = boolToValueHolder(isMono);
        evalStaticObject(vh.ptr(), out);
        break;
    }

    case PRIM_StaticMonoInputTypes :
        break;

    case PRIM_bitcopy : {
        ensureArity(args, 2);
        EValuePtr ev0 = args->values[0];
        EValuePtr ev1 = args->values[1];
        if (!isPrimitiveType(ev0->type))
            argumentTypeError(0, "primitive type", ev0->type);
        if (ev0->type != ev1->type)
            argumentTypeError(1, ev0->type, ev1->type);
        memcpy(ev0->addr, ev1->addr, typeSize(ev0->type));
        assert(out->size() == 0);
        break;
    }

    case PRIM_boolNot : {
        ensureArity(args, 1);
        EValuePtr ev = args->values[0];
        if (ev->type != boolType)
            argumentTypeError(0, boolType, ev->type);
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type == boolType);
        out0->as<bool>() = !ev->as<bool>();
        break;
    }

    case PRIM_integerEqualsP :
    case PRIM_integerLesserP : {
        ensureArity(args, 2);
        TypePtr t;
        EValuePtr ev0 = integerOrPointerLikeValue(args, 0, t);
        EValuePtr ev1 = integerOrPointerLikeValue(args, 1, t);
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type == boolType);
        switch (x->primOpCode) {
        case PRIM_integerEqualsP:
            binaryNumericOp<Op_orderedEqualsP>(ev0, ev1, out0);
            break;
        case PRIM_integerLesserP:
            binaryNumericOp<Op_orderedLesserP>(ev0, ev1, out0);
            break;
        default:
            assert(false);
        }
        break;
    }

    case PRIM_floatOrderedEqualsP :
    case PRIM_floatOrderedLesserP :
    case PRIM_floatOrderedLesserEqualsP :
    case PRIM_floatOrderedGreaterP :
    case PRIM_floatOrderedGreaterEqualsP :
    case PRIM_floatOrderedNotEqualsP :
    case PRIM_floatOrderedP :
    case PRIM_floatUnorderedEqualsP :
    case PRIM_floatUnorderedLesserP :
    case PRIM_floatUnorderedLesserEqualsP :
    case PRIM_floatUnorderedGreaterP :
    case PRIM_floatUnorderedGreaterEqualsP :
    case PRIM_floatUnorderedNotEqualsP :
    case PRIM_floatUnorderedP : {
        ensureArity(args, 2);
        FloatTypePtr t;
        EValuePtr ev0 = floatValue(args, 0, t);
        EValuePtr ev1 = floatValue(args, 1, t);
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type == boolType);
        switch (x->primOpCode) {
        case PRIM_floatOrderedEqualsP :
            binaryNumericOp<Op_orderedEqualsP>(ev0, ev1, out0);
            break;
        case PRIM_floatOrderedLesserP :
            binaryNumericOp<Op_orderedLesserP>(ev0, ev1, out0);
            break;
        case PRIM_floatOrderedLesserEqualsP :
            binaryNumericOp<Op_orderedLesserEqualsP>(ev0, ev1, out0);
            break;
        case PRIM_floatOrderedGreaterP :
            binaryNumericOp<Op_orderedGreaterP>(ev0, ev1, out0);
            break;
        case PRIM_floatOrderedGreaterEqualsP :
            binaryNumericOp<Op_orderedGreaterEqualsP>(ev0, ev1, out0);
            break;
        case PRIM_floatOrderedNotEqualsP :
            binaryNumericOp<Op_orderedNotEqualsP>(ev0, ev1, out0);
            break;
        case PRIM_floatOrderedP :
            binaryNumericOp<Op_orderedP>(ev0, ev1, out0);
            break;
        case PRIM_floatUnorderedEqualsP :
            binaryNumericOp<Op_unorderedEqualsP>(ev0, ev1, out0);
            break;
        case PRIM_floatUnorderedLesserP :
            binaryNumericOp<Op_unorderedLesserP>(ev0, ev1, out0);
            break;
        case PRIM_floatUnorderedLesserEqualsP :
            binaryNumericOp<Op_unorderedLesserEqualsP>(ev0, ev1, out0);
            break;
        case PRIM_floatUnorderedGreaterP :
            binaryNumericOp<Op_unorderedGreaterP>(ev0, ev1, out0);
            break;
        case PRIM_floatUnorderedGreaterEqualsP :
            binaryNumericOp<Op_unorderedGreaterEqualsP>(ev0, ev1, out0);
            break;
        case PRIM_floatUnorderedNotEqualsP :
            binaryNumericOp<Op_unorderedNotEqualsP>(ev0, ev1, out0);
            break;
        case PRIM_floatUnorderedP :
            binaryNumericOp<Op_unorderedP>(ev0, ev1, out0);
            break;
        default:
            assert(false);
        }
        break;
    }

    case PRIM_numericAdd : {
        ensureArity(args, 2);
        TypePtr t;
        EValuePtr ev0 = numericValue(args, 0, t);
        EValuePtr ev1 = numericValue(args, 1, t);
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type == t);
        binaryNumericOp<Op_numericAdd>(ev0, ev1, out0);
        break;
    }

    case PRIM_numericSubtract : {
        ensureArity(args, 2);
        TypePtr t;
        EValuePtr ev0 = numericValue(args, 0, t);
        EValuePtr ev1 = numericValue(args, 1, t);
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type == t);
        binaryNumericOp<Op_numericSubtract>(ev0, ev1, out0);
        break;
    }

    case PRIM_numericMultiply : {
        ensureArity(args, 2);
        TypePtr t;
        EValuePtr ev0 = numericValue(args, 0, t);
        EValuePtr ev1 = numericValue(args, 1, t);
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type == t);
        binaryNumericOp<Op_numericMultiply>(ev0, ev1, out0);
        break;
    }

    case PRIM_floatDivide : {
        ensureArity(args, 2);
        FloatTypePtr t;
        EValuePtr ev0 = floatValue(args, 0, t);
        EValuePtr ev1 = floatValue(args, 1, t);
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type == t.ptr());
        binaryNumericOp<Op_floatDivide>(ev0, ev1, out0);
        break;
    }

    case PRIM_numericNegate : {
        ensureArity(args, 1);
        TypePtr t;
        EValuePtr ev = numericValue(args, 0, t);
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type == t);
        unaryNumericOp<Op_numericNegate>(ev, out0);
        break;
    }

    case PRIM_integerQuotient : {
        ensureArity(args, 2);
        IntegerTypePtr t;
        EValuePtr ev0 = integerValue(args, 0, t);
        EValuePtr ev1 = integerValue(args, 1, t);
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type.ptr() == t.ptr());
        binaryIntegerOp<Op_integerQuotient>(ev0, ev1, out0);
        break;
    }

    case PRIM_integerRemainder : {
        ensureArity(args, 2);
        IntegerTypePtr t;
        EValuePtr ev0 = integerValue(args, 0, t);
        EValuePtr ev1 = integerValue(args, 1, t);
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type.ptr() == t.ptr());
        binaryIntegerOp<Op_integerRemainder>(ev0, ev1, out0);
        break;
    }

    case PRIM_integerAddChecked : {
        ensureArity(args, 2);
        IntegerTypePtr t;
        EValuePtr ev0 = integerValue(args, 0, t);
        EValuePtr ev1 = integerValue(args, 1, t);
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type.ptr() == t.ptr());
        binaryIntegerOp<Op_integerAddChecked>(ev0, ev1, out0);
        break;
    }

    case PRIM_integerSubtractChecked : {
        ensureArity(args, 2);
        IntegerTypePtr t;
        EValuePtr ev0 = integerValue(args, 0, t);
        EValuePtr ev1 = integerValue(args, 1, t);
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type.ptr() == t.ptr());
        binaryIntegerOp<Op_integerSubtractChecked>(ev0, ev1, out0);
        break;
    }

    case PRIM_integerMultiplyChecked : {
        ensureArity(args, 2);
        IntegerTypePtr t;
        EValuePtr ev0 = integerValue(args, 0, t);
        EValuePtr ev1 = integerValue(args, 1, t);
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type.ptr() == t.ptr());
        binaryIntegerOp<Op_integerMultiplyChecked>(ev0, ev1, out0);
        break;
    }

    case PRIM_integerQuotientChecked : {
        ensureArity(args, 2);
        IntegerTypePtr t;
        EValuePtr ev0 = integerValue(args, 0, t);
        EValuePtr ev1 = integerValue(args, 1, t);
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type.ptr() == t.ptr());
        binaryIntegerOp<Op_integerQuotientChecked>(ev0, ev1, out0);
        break;
    }

    case PRIM_integerRemainderChecked : {
        ensureArity(args, 2);
        IntegerTypePtr t;
        EValuePtr ev0 = integerValue(args, 0, t);
        EValuePtr ev1 = integerValue(args, 1, t);
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type.ptr() == t.ptr());
        binaryIntegerOp<Op_integerRemainderChecked>(ev0, ev1, out0);
        break;
    }

    case PRIM_integerNegateChecked : {
        ensureArity(args, 1);
        IntegerTypePtr t;
        EValuePtr ev = integerValue(args, 0, t);
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type == t.ptr());
        unaryIntegerOp<Op_integerNegateChecked>(ev, out0);
        break;
    }

    case PRIM_integerShiftLeft : {
        ensureArity(args, 2);
        IntegerTypePtr t;
        EValuePtr ev0 = integerValue(args, 0, t);
        EValuePtr ev1 = integerValue(args, 1, t);
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type == t.ptr());
        binaryIntegerOp<Op_integerShiftLeft>(ev0, ev1, out0);
        break;
    }

    case PRIM_integerShiftRight : {
        ensureArity(args, 2);
        IntegerTypePtr t;
        EValuePtr ev0 = integerValue(args, 0, t);
        EValuePtr ev1 = integerValue(args, 1, t);
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type == t.ptr());
        binaryIntegerOp<Op_integerShiftRight>(ev0, ev1, out0);
        break;
    }

    case PRIM_integerBitwiseAnd : {
        ensureArity(args, 2);
        IntegerTypePtr t;
        EValuePtr ev0 = integerValue(args, 0, t);
        EValuePtr ev1 = integerValue(args, 1, t);
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type == t.ptr());
        binaryIntegerOp<Op_integerBitwiseAnd>(ev0, ev1, out0);
        break;
    }

    case PRIM_integerBitwiseOr : {
        ensureArity(args, 2);
        IntegerTypePtr t;
        EValuePtr ev0 = integerValue(args, 0, t);
        EValuePtr ev1 = integerValue(args, 1, t);
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type == t.ptr());
        binaryIntegerOp<Op_integerBitwiseOr>(ev0, ev1, out0);
        break;
    }

    case PRIM_integerBitwiseXor : {
        ensureArity(args, 2);
        IntegerTypePtr t;
        EValuePtr ev0 = integerValue(args, 0, t);
        EValuePtr ev1 = integerValue(args, 1, t);
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type == t.ptr());
        binaryIntegerOp<Op_integerBitwiseXor>(ev0, ev1, out0);
        break;
    }

    case PRIM_integerBitwiseNot : {
        ensureArity(args, 1);
        IntegerTypePtr t;
        EValuePtr ev = integerValue(args, 0, t);
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type == t.ptr());
        unaryIntegerOp<Op_integerBitwiseNot>(ev, out0);
        break;
    }

    case PRIM_numericConvert : {
        ensureArity(args, 2);
        TypePtr dest = valueToNumericType(args, 0);
        TypePtr src;
        EValuePtr ev = numericValue(args, 1, src);
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type == dest);
        op_numericConvert<false>(out0, ev);
        break;
    }

    case PRIM_integerConvertChecked : {
        ensureArity(args, 2);
        TypePtr dest = valueToIntegerType(args, 0).ptr();
        TypePtr src;
        EValuePtr ev = numericValue(args, 1, src);
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type == dest);
        op_numericConvert<true>(out0, ev);
        break;
    }

    case PRIM_integerShiftLeftChecked : {
        ensureArity(args, 2);
        IntegerTypePtr t;
        EValuePtr ev0 = integerValue(args, 0, t);
        EValuePtr ev1 = integerValue(args, 1, t);
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type == t.ptr());
        binaryIntegerOp<Op_integerShiftLeftChecked>(ev0, ev1, out0);
        break;
    }

    case PRIM_Pointer :
        error("Pointer type constructor cannot be called");

    case PRIM_addressOf : {
        ensureArity(args, 1);
        EValuePtr ev = args->values[0];
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type == pointerType(ev->type));
        out0->as<void *>() = (void *)ev->addr;
        break;
    }

    case PRIM_pointerDereference : {
        ensureArity(args, 1);
        PointerTypePtr t;
        EValuePtr ev = pointerValue(args, 0, t);
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type == t.ptr());
        out0->as<void*>() = ev->as<void*>();
        break;
    }

    case PRIM_pointerOffset : {
        ensureArity(args, 2);
        PointerTypePtr t;
        EValuePtr v0 = pointerValue(args, 0, t);
        IntegerTypePtr offsetT;
        EValuePtr v1 = integerValue(args, 1, offsetT);
        ptrdiff_t offset = op_intToPtrInt(v1);
        char *ptr = v0->as<char *>();
        ptr += offset*(ptrdiff_t)typeSize(t->pointeeType);
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type == t.ptr());
        out0->as<void*>() = (void*)ptr;
        break;
    }

    case PRIM_pointerToInt : {
        ensureArity(args, 2);
        IntegerTypePtr dest = valueToIntegerType(args, 0);
        TypePtr pt;
        EValuePtr ev = pointerLikeValue(args, 1, pt);
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type == dest.ptr());
        op_pointerToInt(out0, ev);
        break;
    }

    case PRIM_intToPointer : {
        ensureArity(args, 2);
        TypePtr dest = valueToPointerLikeType(args, 0);
        IntegerTypePtr t;
        EValuePtr ev = integerValue(args, 1, t);
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type == dest);
        ptrdiff_t ptrInt = op_intToPtrInt(ev);
        out0->as<void *>() = (void *)ptrInt;
        break;
    }

    case PRIM_nullPointer : {
        ensureArity(args, 1);
        TypePtr dest = valueToPointerLikeType(args, 0);
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type == dest);
        out0->as<void *>() = 0;
        break;
    }

    case PRIM_CodePointer :
        error("CodePointer type constructor cannot be called");

    case PRIM_makeCodePointer : {
        error("code pointers cannot be taken at compile time");
        break;
    }

    case PRIM_ExternalCodePointer :
        error("ExternalCodePointer type constructor cannot be called");

    case PRIM_makeExternalCodePointer : {
        error("code pointers cannot be created at compile time");
        break;
    }

    case PRIM_callExternalCodePointer : {
        error("invoking a code pointer not yet supported in evaluator");
        break;
    }

    case PRIM_bitcast : {
        ensureArity(args, 2);
        TypePtr dest = valueToType(args, 0);
        EValuePtr ev = args->values[1];
        if (typeSize(dest) > typeSize(ev->type))
            error("destination type for bitcast is larger than source type");
        if (typeAlignment(dest) > typeAlignment(ev->type))
            error("destination type for bitcast has stricter alignment than source type");
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type == pointerType(dest));
        out0->as<void*>() = (void*)ev->addr;
        break;
    }

    case PRIM_Array :
        error("Array type constructor cannot be called");

    case PRIM_Vec :
        error("Vec type constructor cannot be called");

    case PRIM_arrayRef : {
        ensureArity(args, 2);
        ArrayTypePtr at;
        EValuePtr earray = arrayValue(args, 0, at);
        IntegerTypePtr indexType;
        EValuePtr iv = integerValue(args, 1, indexType);
        ptrdiff_t i = op_intToPtrInt(iv);
        char *ptr = earray->addr + i*(ptrdiff_t)typeSize(at->elementType);
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type == pointerType(at->elementType));
        out0->as<void*>() = (void *)ptr;
        break;
    }

    case PRIM_arrayElements : {
        ensureArity(args, 1);
        ArrayTypePtr at;
        EValuePtr earray = arrayValue(args, 0, at);
        assert(out->size() == (size_t)at->size);
        for (size_t i = 0; i < (size_t)at->size; ++i) {
            EValuePtr outi = out->values[i];
            assert(outi->type == pointerType(at->elementType));
            char *ptr = earray->addr + i*typeSize(at->elementType);
            outi->as<void*>() = (void *)ptr;
        }
        break;
    }

    case PRIM_Tuple :
        error("Tuple type constructor cannot be called");

    case PRIM_TupleElementCount : {
        ensureArity(args, 1);
        TupleTypePtr t = valueToTupleType(args, 0);
        ValueHolderPtr vh = sizeTToValueHolder(t->elementTypes.size());
        evalStaticObject(vh.ptr(), out);
        break;
    }

    case PRIM_tupleRef : {
        ensureArity(args, 2);
        TupleTypePtr tt;
        EValuePtr etuple = tupleValue(args, 0, tt);
        size_t i = valueToStaticSizeTOrInt(args, 1);
        if (i >= tt->elementTypes.size())
            argumentIndexRangeError(1, "tuple element index",
                                    i, tt->elementTypes.size());
        const llvm::StructLayout *layout = tupleTypeLayout(tt.ptr());
        char *ptr = etuple->addr + layout->getElementOffset(unsigned(i));
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type == pointerType(tt->elementTypes[i]));
        out0->as<void*>() = (void *)ptr;
        break;
    }

    case PRIM_tupleElements : {
        ensureArity(args, 1);
        TupleTypePtr tt;
        EValuePtr etuple = tupleValue(args, 0, tt);
        assert(out->size() == tt->elementTypes.size());
        const llvm::StructLayout *layout = tupleTypeLayout(tt.ptr());
        for (unsigned i = 0; i < tt->elementTypes.size(); ++i) {
            EValuePtr outi = out->values[i];
            assert(outi->type == pointerType(tt->elementTypes[i]));
            char *ptr = etuple->addr + layout->getElementOffset(i);
            outi->as<void*>() = (void *)ptr;
        }
        break;
    }

    case PRIM_Union :
        error("Union type constructor cannot be called");

    case PRIM_UnionMemberCount : {
        ensureArity(args, 1);
        UnionTypePtr t = valueToUnionType(args, 0);
        ValueHolderPtr vh = sizeTToValueHolder(t->memberTypes.size());
        evalStaticObject(vh.ptr(), out);
        break;
    }

    case PRIM_RecordP : {
        ensureArity(args, 1);
        bool isRecordType = false;
        ObjectPtr obj = valueToStatic(args->values[0]);
        if (obj.ptr() && (obj->objKind == TYPE)) {
            Type *t = (Type *)obj.ptr();
            if (t->typeKind == RECORD_TYPE)
                isRecordType = true;
        }
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type == boolType);
        out0->as<bool>() = isRecordType;
        break;
    }

    case PRIM_RecordFieldCount : {
        ensureArity(args, 1);
        RecordTypePtr rt = valueToRecordType(args, 0);
        ValueHolderPtr vh = sizeTToValueHolder(rt->fieldCount());
        evalStaticObject(vh.ptr(), out);
        break;
    }

    case PRIM_RecordFieldName : {
        ensureArity(args, 2);
        RecordTypePtr rt = valueToRecordType(args, 0);
        size_t i = valueToStaticSizeTOrInt(args, 1);
        llvm::ArrayRef<IdentifierPtr> fieldNames = recordFieldNames(rt);
        if (i >= fieldNames.size())
            argumentIndexRangeError(1, "record field index",
                                    i, fieldNames.size());
        evalStaticObject(fieldNames[i].ptr(), out);
        break;
    }

    case PRIM_RecordWithFieldP : {
        ensureArity(args, 2);
        bool result = false;
        ObjectPtr obj = valueToStatic(args->values[0]);
        IdentifierPtr fname = valueToIdentifier(args, 1);
        if (obj.ptr() && (obj->objKind == TYPE)) {
            Type *t = (Type *)obj.ptr();
            if (t->typeKind == RECORD_TYPE) {
                RecordType *rt = (RecordType *)t;
                const llvm::StringMap<size_t> &fieldIndexMap =
                    recordFieldIndexMap(rt);
                result = (fieldIndexMap.find(fname->str)
                          != fieldIndexMap.end());
            }
        }
        ValueHolderPtr vh = boolToValueHolder(result);
        evalStaticObject(vh.ptr(), out);
        break;
    }

    case PRIM_recordFieldRef : {
        ensureArity(args, 2);
        RecordTypePtr rt;
        EValuePtr erec = recordValue(args, 0, rt);
        unsigned i = unsigned(valueToStaticSizeTOrInt(args, 1));
        llvm::ArrayRef<TypePtr> fieldTypes = recordFieldTypes(rt);
        if (i >= rt->fieldCount())
            argumentIndexRangeError(1, "record field index",
                                    i, rt->fieldCount());
        const llvm::StructLayout *layout = recordTypeLayout(rt.ptr());

        if (rt->hasVarField && i >= rt->varFieldPosition) {
            if (i == rt->varFieldPosition) {
                assert(out->size() == rt->varFieldSize());
                for (unsigned j = 0; j < rt->varFieldSize(); ++j) {
                    char *ptr = erec->addr + layout->getElementOffset(i+j);
                    EValuePtr out0 = out->values[j];
                    assert(out0->type == pointerType(fieldTypes[i+j]));
                    out0->as<void *>() = (void *)ptr;
                }
            } else {
                unsigned k = i+unsigned(rt->varFieldSize()-1);
                char *ptr = erec->addr + layout->getElementOffset(k);
                assert(out->size() == 1);
                EValuePtr out0 = out->values[0];
                assert(out0->type == pointerType(fieldTypes[k]));
                out0->as<void *>() = (void *)ptr;
            }
        } else {
            char *ptr = erec->addr + layout->getElementOffset(i);
            assert(out->size() == 1);
            EValuePtr out0 = out->values[0];
            assert(out0->type == pointerType(fieldTypes[i]));
            out0->as<void *>() = (void *)ptr;
        }
        break;
    }

    case PRIM_recordFieldRefByName : {
        ensureArity(args, 2);
        RecordTypePtr rt;
        EValuePtr erec = recordValue(args, 0, rt);
        IdentifierPtr fname = valueToIdentifier(args, 1);
        const llvm::StringMap<size_t> &fieldIndexMap = recordFieldIndexMap(rt);
        llvm::StringMap<size_t>::const_iterator fi =
            fieldIndexMap.find(fname->str);
        if (fi == fieldIndexMap.end()) {
            string buf;
            llvm::raw_string_ostream sout(buf);
            sout << "field not found: " << fname->str;
            argumentError(1, sout.str());
        }
        unsigned i = unsigned(fi->second);
        llvm::ArrayRef<TypePtr> fieldTypes = recordFieldTypes(rt);
        const llvm::StructLayout *layout = recordTypeLayout(rt.ptr());

        if (rt->hasVarField && i >= rt->varFieldPosition) {
            if (i == rt->varFieldPosition) {
                assert(out->size() == rt->varFieldSize());
                for (unsigned j = 0; j < rt->varFieldSize(); ++j) {
                    char *ptr = erec->addr + layout->getElementOffset(i+j);
                    EValuePtr out0 = out->values[j];
                    assert(out0->type == pointerType(fieldTypes[i+j]));
                    out0->as<void *>() = (void *)ptr;
                }
            } else {
                unsigned k = i+unsigned(rt->varFieldSize())-1;
                char *ptr = erec->addr + layout->getElementOffset(k);
                assert(out->size() == 1);
                EValuePtr out0 = out->values[0];
                assert(out0->type == pointerType(fieldTypes[k]));
                out0->as<void *>() = (void *)ptr;
            }
        } else {
            char *ptr = erec->addr + layout->getElementOffset(i);
            assert(out->size() == 1);
            EValuePtr out0 = out->values[0];
            assert(out0->type == pointerType(fieldTypes[i]));
            out0->as<void *>() = (void *)ptr;
        }
        break;
    }

    case PRIM_recordFields : {
        ensureArity(args, 1);
        RecordTypePtr rt;
        EValuePtr erec = recordValue(args, 0, rt);
        llvm::ArrayRef<TypePtr> fieldTypes = recordFieldTypes(rt);
        assert(out->size() == fieldTypes.size());
        const llvm::StructLayout *layout = recordTypeLayout(rt.ptr());
        for (unsigned i = 0; i < fieldTypes.size(); ++i) {
            EValuePtr outi = out->values[i];
            assert(outi->type == pointerType(fieldTypes[i]));
            char *ptr = erec->addr + layout->getElementOffset(i);
            outi->as<void *>() = (void *)ptr;
        }
        break;
    }

    case PRIM_recordVariadicField : {
        ensureArity(args, 1);
        RecordTypePtr rt;
        EValuePtr erec = recordValue(args, 0, rt);
        llvm::ArrayRef<TypePtr> fieldTypes = recordFieldTypes(rt);
        if (!rt->hasVarField)
            argumentError(0, "expecting a record with a variadic field");
        assert(out->size() == fieldTypes.size());
        const llvm::StructLayout *layout = recordTypeLayout(rt.ptr());
        assert(out->size() == rt->varFieldSize());
        for (unsigned i = 0; i < rt->varFieldSize(); ++i) {
            unsigned k = unsigned(rt->varFieldPosition) + i;
            EValuePtr outi = out->values[i];
            assert(outi->type == pointerType(fieldTypes[k]));
            char *ptr = erec->addr + layout->getElementOffset(k);
            outi->as<void *>() = (void *)ptr;
        }
        break;
    }

    case PRIM_VariantP : {
        ensureArity(args, 1);
        bool isVariantType = false;
        ObjectPtr obj = valueToStatic(args->values[0]);
        if (obj.ptr() && (obj->objKind == TYPE)) {
            Type *t = (Type *)obj.ptr();
            if (t->typeKind == VARIANT_TYPE)
                isVariantType = true;
        }
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type == boolType);
        out0->as<bool>() = isVariantType;
        break;
    }

    case PRIM_VariantMemberIndex : {
        ensureArity(args, 2);
        VariantTypePtr vt = valueToVariantType(args, 0);
        TypePtr mt = valueToType(args, 1);
        llvm::ArrayRef<TypePtr> memberTypes = variantMemberTypes(vt);
        size_t index = (size_t)-1;
        for (size_t i = 0; i < memberTypes.size(); ++i) {
            if (memberTypes[i] == mt) {
                index = i;
                break;
            }
        }
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type == cSizeTType);
        setSizeTEValue(out0, index);
        break;
    }

    case PRIM_VariantMemberCount : {
        ensureArity(args, 1);
        VariantTypePtr vt = valueToVariantType(args, 0);
        llvm::ArrayRef<TypePtr> memberTypes = variantMemberTypes(vt);
        size_t size = memberTypes.size();
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type == cSizeTType);
        setSizeTEValue(out0, size);
        break;
    }

    case PRIM_VariantMembers :
        break;

    case PRIM_BaseType :
        break;

    case PRIM_Static :
        error("Static type constructor cannot be called");

    case PRIM_staticIntegers :
        break;

    case PRIM_integers : {
        ensureArity(args, 1);
        ObjectPtr obj = valueToStatic(args, 0);
        if (obj->objKind != VALUE_HOLDER)
            argumentError(0, "expecting a static SizeT or Int value");
        ValueHolder *vh = (ValueHolder *)obj.ptr();
        if (vh->type == cIntType) {
            int count = vh->as<int>();
            if (count < 0)
                argumentError(0, "negative values are not allowed");
            assert(out->size() == (size_t)count);
            for (int i = 0; i < count; ++i) {
                ValueHolderPtr vhi = intToValueHolder(i);
                EValuePtr outi = out->values[size_t(i)];
                assert(outi->type == cIntType);
                outi->as<int>() = i;
            }
        }
        else if (vh->type == cSizeTType) {
            size_t count = valueHolderToSizeT(vh);
            assert(out->size() == count);
            for (size_t i = 0; i < count; ++i) {
                ValueHolderPtr vhi = sizeTToValueHolder(i);
                EValuePtr outi = out->values[i];
                assert(outi->type == cSizeTType);
                outi->as<size_t>() = i;
            }
        }
        else {
            argumentError(0, "expecting a static SizeT or Int value");
        }
        break;
    }

    case PRIM_staticFieldRef : {
        ensureArity(args, 2);
        ObjectPtr moduleObj = valueToStatic(args, 0);
        if (moduleObj->objKind != MODULE)
            argumentError(0, "expecting a module");
        Module *module = (Module *)moduleObj.ptr();
        IdentifierPtr ident = valueToIdentifier(args, 1);
        ObjectPtr obj = safeLookupPublic(module, ident);
        evalStaticObject(obj, out);
        break;
    }

    case PRIM_EnumP : {
        ensureArity(args, 1);
        bool isEnumType = false;
        ObjectPtr obj = valueToStatic(args->values[0]);
        if (obj.ptr() && (obj->objKind == TYPE)) {
            Type *t = (Type *)obj.ptr();
            if (t->typeKind == ENUM_TYPE)
                isEnumType = true;
        }
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type == boolType);
        out0->as<bool>() = isEnumType;
        break;
    }

    case PRIM_EnumMemberCount : {
        ensureArity(args, 1);
        EnumTypePtr et = valueToEnumType(args, 0);
        size_t n = et->enumeration->members.size();
        ValueHolderPtr vh = sizeTToValueHolder(n);
        evalStaticObject(vh.ptr(), out);
        break;
    }

    case PRIM_EnumMemberName :
        break;

    case PRIM_enumToInt : {
        ensureArity(args, 1);
        EnumTypePtr et;
        EValuePtr ev = enumValue(args, 0, et);
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type == cIntType);
        out0->as<int>() = ev->as<int>();
        break;
    }

    case PRIM_intToEnum : {
        ensureArity(args, 2);
        EnumTypePtr et = valueToEnumType(args, 0);
        IntegerTypePtr it = (IntegerType *)cIntType.ptr();
        EValuePtr ev = integerValue(args, 1, it);
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type == et.ptr());
        out0->as<int>() = ev->as<int>();
        break;
    }

    case PRIM_StringLiteralP : {
        ensureArity(args, 1);
        bool result = false;
        EValuePtr ev0 = args->values[0];
        if (ev0->type->typeKind == STATIC_TYPE) {
            StaticType *st = (StaticType *)ev0->type.ptr();
            result = (st->obj->objKind == IDENTIFIER);
        }
        ValueHolderPtr vh = boolToValueHolder(result);
        evalStaticObject(vh.ptr(), out);
        break;
    }

    case PRIM_stringLiteralByteIndex : {
        ensureArity(args, 2);
        IdentifierPtr ident = valueToIdentifier(args, 0);
        size_t n = valueToStaticSizeTOrInt(args, 1);
        if (n >= ident->str.size())
            argumentError(1, "string literal index out of bounds");

        assert(out->size() == 1);
        EValuePtr outi = out->values[0];
        assert(outi->type == cIntType);
        outi->as<int>() = ident->str[unsigned(n)];
        break;
    }

    case PRIM_stringLiteralBytes : {
        ensureArity(args, 1);
        IdentifierPtr ident = valueToIdentifier(args, 0);
        assert(out->size() == ident->str.size());
        for (unsigned i = 0, sz = unsigned(ident->str.size()); i < sz; ++i) {
            EValuePtr outi = out->values[i];
            assert(outi->type == cIntType);
            outi->as<int>() = ident->str[i];
        }
        break;
    }

    case PRIM_stringLiteralByteSize : {
        ensureArity(args, 1);
        IdentifierPtr ident = valueToIdentifier(args, 0);
        ValueHolderPtr vh = sizeTToValueHolder(ident->str.size());
        evalStaticObject(vh.ptr(), out);
        break;
    }

    case PRIM_stringLiteralByteSlice :
    case PRIM_stringLiteralConcat :
    case PRIM_stringLiteralFromBytes :
        break;

    case PRIM_stringTableConstant : {
        ensureArity(args, 1);
        IdentifierPtr ident = valueToIdentifier(args, 0);
        const void *value = evalStringTableConstant(ident->str);

        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type == pointerType(cSizeTType));
        out0->as<const void*>() = value;
        break;
    }

    case PRIM_StaticName :
    case PRIM_MainModule :
    case PRIM_StaticModule :
    case PRIM_ModuleName :
    case PRIM_ModuleMemberNames :
        break;

    case PRIM_FlagP : {
        ensureArity(args, 1);
        IdentifierPtr ident = valueToIdentifier(args, 0);
        llvm::StringMap<string>::const_iterator flag = globalFlags.find(ident->str);
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type == boolType);
        out0->as<bool>() = flag != globalFlags.end();
        break;
    }

    case PRIM_Flag :
        break;

    case PRIM_atomicFence:
    case PRIM_atomicRMW:
    case PRIM_atomicLoad:
    case PRIM_atomicStore:
    case PRIM_atomicCompareExchange:
        error("atomic operations not supported in evaluator");
        break;

    case PRIM_activeException:
        error("exceptions not supported in evaluator");
        break;

    case PRIM_memcpy :
    case PRIM_memmove : {
        ensureArity(args, 3);
        PointerTypePtr topt;
        PointerTypePtr frompt;
        EValuePtr tov = pointerValue(args, 0, topt);
        EValuePtr fromv = pointerValue(args, 1, frompt);
        IntegerTypePtr it;
        EValuePtr countv = integerValue(args, 2, it);

        void *to = tov->as<void*>();
        void *from = fromv->as<void*>();
        size_t size = op_intToSizeT(countv);

        if (x->primOpCode == PRIM_memcpy)
            memcpy(to, from, size);
        else
            memmove(to, from, size);

        break;
    }

    case PRIM_countValues : {
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type == cIntType);
        out0->as<int>() = int(args->size());
        break;
    }

    case PRIM_nthValue : {
        if (args->size() < 1)
            arityError2(1, args->size());
        assert(out->size() == 1);
        EValuePtr out0 = out->values[0];

        size_t i = valueToStaticSizeTOrInt(args, 0);
        if (i+1 >= args->size())
            argumentError(0, "nthValue argument out of bounds");

        evalValueForward(out0, args->values[i+1]);
        break;
    }

    case PRIM_withoutNthValue : {
        if (args->size() < 1)
            arityError2(1, args->size());
        assert(out->size() == args->size() - 2);

        size_t i = valueToStaticSizeTOrInt(args, 0);
        if (i+1 >= args->size())
            argumentError(0, "withoutNthValue argument out of bounds");

        for (size_t argi = 1, outi = 0; argi < args->size(); ++argi) {
            if (argi == i+1)
                continue;
            assert(outi < out->size());
            evalValueForward(out->values[outi], args->values[argi]);
            ++outi;
        }
        break;
    }

    case PRIM_takeValues : {
        if (args->size() < 1)
            arityError2(1, args->size());

        size_t i = valueToStaticSizeTOrInt(args, 0);
        if (i+1 >= args->size())
            i = args->size() - 1;

        assert(out->size() == i);
        for (size_t argi = 1, outi = 0; argi < i+1; ++argi, ++outi) {
            assert(outi < out->size());
            evalValueForward(out->values[outi], args->values[argi]);
        }
        break;
    }

    case PRIM_dropValues : {
        if (args->size() < 1)
            arityError2(1, args->size());

        size_t i = valueToStaticSizeTOrInt(args, 0);
        if (i+1 >= args->size())
            i = args->size() - 1;

        assert(out->size() == args->size() - i - 1);
        for (size_t argi = i+1, outi = 0; argi < args->size(); ++argi, ++outi) {
            assert(outi < out->size());
            evalValueForward(out->values[outi], args->values[argi]);
        }
        break;
    }

    case PRIM_LambdaRecordP : {
        ensureArity(args, 1);

        bool isLambda;

        ObjectPtr callable = valueToStatic(args->values[0]);
        if (callable != NULL && callable->objKind == TYPE) {
            Type *t = (Type*)callable.ptr();
            if (t->typeKind == RECORD_TYPE) {
                RecordType *r = (RecordType*)t;
                isLambda = r->record->lambda != NULL;
            } else
                isLambda = false;
        } else {
            isLambda = false;
        }

        ValueHolderPtr vh = boolToValueHolder(isLambda);
        evalStaticObject(vh.ptr(), out);
        break;
    }

    case PRIM_LambdaSymbolP : {
        ensureArity(args, 1);

        bool isLambda;

        ObjectPtr callable = valueToStatic(args->values[0]);
        if (callable != NULL && callable->objKind == PROCEDURE) {
            Procedure *p = (Procedure*)callable.ptr();
            isLambda = p->lambda != NULL;
        } else
            isLambda = false;

        ValueHolderPtr vh = boolToValueHolder(isLambda);
        evalStaticObject(vh.ptr(), out);
        break;
    }

    case PRIM_LambdaMonoP : {
        ensureArity(args, 1);

        bool isMono;

        ObjectPtr callable = valueToStatic(args->values[0]);
        if (callable != NULL && callable->objKind == TYPE) {
            Type *t = (Type*)callable.ptr();
            if (t->typeKind == RECORD_TYPE) {
                RecordType *r = (RecordType*)t;
                isMono = r->record->lambda->mono.monoState == Procedure_MonoOverload;
            } else
                isMono = false;
        } else {
            isMono = false;
        }

        ValueHolderPtr vh = boolToValueHolder(isMono);
        evalStaticObject(vh.ptr(), out);
        break;
    }

    case PRIM_LambdaMonoInputTypes :
        break;

    case PRIM_GetOverload :
        break;

    case PRIM_usuallyEquals : {
        ensureArity(args, 2);
        EValuePtr ev0 = args->values[0];
        if (!isPrimitiveType(ev0->type))
            argumentTypeError(0, "primitive type", ev0->type);

        assert(out->values.size() == 1);
        EValuePtr out0 = out->values[0];
        assert(out0->type == ev0->type);
        memcpy(out0->addr, ev0->addr, typeSize(ev0->type));
        break;
    }

    default :
        assert(false);

    }
}



}
