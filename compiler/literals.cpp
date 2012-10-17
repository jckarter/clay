#include "clay.hpp"
#include "types.hpp"

namespace clay {

static bool ishex(char *ptr) {
    return
        ((ptr[0] == '+' || ptr[0] == '-')
            && ptr[1] == '0'
            && (ptr[2] == 'x' || ptr[2] == 'X'))
        || (ptr[0] == '0' && (ptr[1] == 'x' || ptr[1] == 'X'));
}

static int bitcount(unsigned long long bits)
{
    int k = 0;
    while (bits != 0) {
        k += 1;
        bits = bits >> 1;
    }
    return k - 1;
}

static int hexdigit(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    assert(false);
    return 0;
}

static double floatFromParts(bool negp, int exponent, unsigned long long mantissa)
{
    union { double f; unsigned long long bits; } x;
    x.bits = ((unsigned long long)negp << 63)
        | ((unsigned long long)(exponent & 0x7FF) << 52)
        | (mantissa & 0xFFFFFFFFFFFFFULL);
    return x.f;
}

template<typename F>
static F parseHexFloat(char *number, char **end) {
    bool negp = false;
    bool negExponentp = false;
    unsigned long long mantissa = 0;
    int mantissaBits = 0;
    int usedMantissaBits = 0;
    size_t exponentI = 0;
    int point = 0;
    int pointp = false;

    for (size_t i = 0; number[i] != '\0'; ++i) {
        char c = number[i];

        if (c == '-') {
            assert(i == 0); // "junk before negative sign in hex float literal"
            negp = true;
        } else if (c == '+') {
            assert(i == 0); // "junk before positive sign in hex float literal"
            negp = false;
        } else if (c == '.') {
            assert(!pointp); // "multiple decimal points in hex float literal"
            pointp = true;
        } else if (c == 'p' || c == 'P') {
            assert(exponentI == 0); // "multiple P markers in hex float literal"
            exponentI = i;
            break;
        } else if (c == 'x') {
            assert(mantissa == 0); // "junk before hex designator in hex float literal"
            continue;
        } else {
            if (pointp)
                point += 4;
            if (c == '0' && mantissa == 0)
                continue;
            mantissaBits += 4;
            if (c != '0')
                usedMantissaBits = mantissaBits;
            if (mantissaBits <= 64)
                mantissa = (mantissa << 4) | hexdigit(c);
        }
    }

    assert(number[exponentI] == 'p' || number[exponentI] == 'P'); // "no exponent in hex float literal"

    long exponent = strtol(number + exponentI + 1, end, 10);
    assert(**end == '\0'); // "junk after exponent in hex float literal"
    double value = 0.0;

    if (mantissa == 0) {
        value = floatFromParts(negp, 0, 0);
    } else {
        int mantissaLog = bitcount(mantissa);
        int mantissaExponent = mantissaLog
            + (mantissaBits > 64 ? mantissaBits - 64 : 0)
            - point
            + exponent
            + 1023;

        mantissa = mantissa << (60 - mantissaLog);
        int mantissaShift = 8 + std::max(0, 1-mantissaExponent);

        if (mantissaShift > 61) {
            mantissaExponent = 0;
            mantissa = 0;
        } else if (mantissaExponent >= 2047) {
            mantissaExponent = 2047;
            mantissa = 0;
        } else {
            unsigned long long roundBit  = mantissa & (1ULL << (mantissaShift-1));
            unsigned long long roundMask = mantissa & ((1ULL << (mantissaShift-1)) - 1);
            unsigned long long evenBit   = mantissa & (1ULL << mantissaShift);

            mantissa = mantissa >> mantissaShift;

            if (roundBit != 0 && (roundMask != 0 || usedMantissaBits > 60 || evenBit != 0))
                mantissa += 1;

            if (mantissaExponent < 0)
                mantissaExponent = 0;
        }

        value = floatFromParts(negp, mantissaExponent, mantissa);
    }

    return value;
}

static double clay_strtod(char *ptr, char **end) {
    if (ishex(ptr))
        return parseHexFloat<double>(ptr, end);
    else
        return strtod(ptr, end);
}

static long double clay_strtold(char *ptr, char **end) {
    // XXX parse hex long doubles
    return strtold(ptr, end);
}

static bool typeSuffix(llvm::StringRef suffix,
                       TypePtr defaultType,
                       llvm::StringRef testSuffix,
                       TypePtr testDefaultType)
{
    return suffix == testSuffix || (suffix.empty() && defaultType == testDefaultType);
}

static bool imagTypeSuffix(llvm::StringRef suffix,
                              TypePtr defaultType,
                              llvm::StringRef testSuffix,
                              TypePtr testDefaultType)
{
    return suffix == testSuffix || (suffix == "j" && defaultType == testDefaultType);
}

ValueHolderPtr parseIntLiteral(ModulePtr module, IntLiteral *x)
{
    TypePtr defaultType = module->attrDefaultIntegerType.ptr();
    if (defaultType == NULL)
        defaultType = int32Type;

    char *ptr = const_cast<char *>(x->value.c_str());
    char *end = ptr;
    int base = ishex(ptr) ? 16 : 10;
    ValueHolderPtr vh;
    if (typeSuffix(x->suffix, defaultType, "ss", int8Type)) {
        long y = strtol(ptr, &end, base);
        if (*end != 0)
            error("invalid int8 literal");
        if ((errno == ERANGE) || (y < SCHAR_MIN) || (y > SCHAR_MAX))
            error("int8 literal out of range");
        vh = new ValueHolder(int8Type);
        *((char *)vh->buf) = (char)y;
    }
    else if (typeSuffix(x->suffix, defaultType, "s", int16Type)) {
        long y = strtol(ptr, &end, base);
        if (*end != 0)
            error("invalid int16 literal");
        if ((errno == ERANGE) || (y < SHRT_MIN) || (y > SHRT_MAX))
            error("int16 literal out of range");
        vh = new ValueHolder(int16Type);
        *((short *)vh->buf) = (short)y;
    }
    else if (typeSuffix(x->suffix, defaultType, "i", int32Type)) {
        long y = strtol(ptr, &end, base);
        if (*end != 0)
            error("invalid int32 literal");
        if (errno == ERANGE || (y < INT_MIN) || (y > INT_MAX))
            error("int32 literal out of range");
        vh = new ValueHolder(int32Type);
        *((int *)vh->buf) = (int)y;
    }
    else if (typeSuffix(x->suffix, defaultType, "l", int64Type)) {
        long long y = strtoll(ptr, &end, base);
        if (*end != 0)
            error("invalid int64 literal");
        if (errno == ERANGE)
            error("int64 literal out of range");
        vh = new ValueHolder(int64Type);
        *((long long *)vh->buf) = y;
    }
    else if (typeSuffix(x->suffix, defaultType, "ll", int128Type)) {
        long long y = strtoll(ptr, &end, base);
        if (*end != 0)
            error("invalid int128 literal");
        if (errno == ERANGE)
            error("int128 literal out of range");
        vh = new ValueHolder(int128Type);
        *((clay_int128 *)vh->buf) = y;
    }
    else if (typeSuffix(x->suffix, defaultType, "uss", uint8Type)) {
        unsigned long y = strtoul(ptr, &end, base);
        if (*end != 0)
            error("invalid uint8 literal");
        if ((errno == ERANGE) || (y > UCHAR_MAX))
            error("uint8 literal out of range");
        vh = new ValueHolder(uint8Type);
        *((unsigned char *)vh->buf) = (unsigned char)y;
    }
    else if (typeSuffix(x->suffix, defaultType, "us", uint16Type)) {
        unsigned long y = strtoul(ptr, &end, base);
        if (*end != 0)
            error("invalid uint16 literal");
        if ((errno == ERANGE) || (y > USHRT_MAX))
            error("uint16 literal out of range");
        vh = new ValueHolder(uint16Type);
        *((unsigned short *)vh->buf) = (unsigned short)y;
    }
    else if (typeSuffix(x->suffix, defaultType, "u", uint32Type)) {
        unsigned long y = strtoul(ptr, &end, base);
        if (*end != 0)
            error("invalid uint32 literal");
        if (errno == ERANGE)
            error("uint32 literal out of range");
        vh = new ValueHolder(uint32Type);
        *((unsigned int *)vh->buf) = (unsigned int)y;
    }
    else if (typeSuffix(x->suffix, defaultType, "ul", uint64Type)) {
        unsigned long long y = strtoull(ptr, &end, base);
        if (*end != 0)
            error("invalid uint64 literal");
        if (errno == ERANGE)
            error("uint64 literal out of range");
        vh = new ValueHolder(uint64Type);
        *((unsigned long long *)vh->buf) = y;
    }
    else if (typeSuffix(x->suffix, defaultType, "ull", uint128Type)) {
        unsigned long long y = strtoull(ptr, &end, base);
        if (*end != 0)
            error("invalid uint128 literal");
        if (errno == ERANGE)
            error("uint128 literal out of range");
        vh = new ValueHolder(uint128Type);
        *((clay_uint128 *)vh->buf) = y;
    }
    else if (x->suffix == "f") {
        float y = (float)clay_strtod(ptr, &end);
        if (*end != 0)
            error("invalid float32 literal");
        if (errno == ERANGE)
            error("float32 literal out of range");
        vh = new ValueHolder(float32Type);
        *((float *)vh->buf) = y;
    }
    else if (x->suffix == "ff") {
        double y = clay_strtod(ptr, &end);
        if (*end != 0)
            error("invalid float64 literal");
        if (errno == ERANGE)
            error("float64 literal out of range");
        vh = new ValueHolder(float64Type);
        *((double *)vh->buf) = y;
    }
    else if (x->suffix == "fl") {
        long double y = clay_strtold(ptr, &end);
        if (*end != 0)
            error("invalid float80 literal");
        if (errno == ERANGE)
            error("float80 literal out of range");
        vh = new ValueHolder(float80Type);
        *((long double *)vh->buf) = y;
    }
    else if (x->suffix == "fj") {
        float y = (float)clay_strtod(ptr, &end);
        if (*end != 0)
            error("invalid imag32 literal");
        if (errno == ERANGE)
            error("imag32 literal out of range");
        vh = new ValueHolder(imag32Type);
        *((float *)vh->buf) = y;
    }
    else if (x->suffix == "j" || x->suffix == "ffj") {
        double y = clay_strtod(ptr, &end);
        if (*end != 0)
            error("invalid imag64 literal");
        if (errno == ERANGE)
            error("imag64 literal out of range");
        vh = new ValueHolder(imag64Type);
        *((double *)vh->buf) = y;
    }
    else if (x->suffix == "lj" || x->suffix == "flj") {
        long double y = clay_strtold(ptr, &end);
        if (*end != 0)
            error("invalid imag80 literal");
        if (errno == ERANGE)
            error("imag80 literal out of range");
        vh = new ValueHolder(imag80Type);
        *((long double *)vh->buf) = y;
    }
    else {
        error("invalid literal suffix: " + x->suffix);
    }
    return vh;
}

ValueHolderPtr parseFloatLiteral(ModulePtr module, FloatLiteral *x)
{
    TypePtr defaultType = module->attrDefaultFloatType.ptr();
    if (defaultType == NULL)
        defaultType = float64Type;

    char *ptr = const_cast<char *>(x->value.c_str());
    char *end = ptr;
    ValueHolderPtr vh;
    if (typeSuffix(x->suffix, defaultType, "f", float32Type)) {
        float y = (float)clay_strtod(ptr, &end);
        if (*end != 0)
            error("invalid float32 literal");
        if (errno == ERANGE)
            error("float32 literal out of range");
        vh = new ValueHolder(float32Type);
        *((float *)vh->buf) = y;
    }
    else if (typeSuffix(x->suffix, defaultType, "ff", float64Type)) {
        double y = clay_strtod(ptr, &end);
        if (*end != 0)
            error("invalid float64 literal");
        if (errno == ERANGE)
            error("float64 literal out of range");
        vh = new ValueHolder(float64Type);
        *((double *)vh->buf) = y;
    }
    else if (x->suffix == "fl" || x->suffix == "l"
        || (x->suffix.empty() && defaultType == float80Type))
    {
        long double y = clay_strtold(ptr, &end);
        if (*end != 0)
            error("invalid float80 literal");
        if (errno == ERANGE)
            error("float80 literal out of range");
        vh = new ValueHolder(float80Type);
        *((long double *)vh->buf) = y;
    }
    else if (imagTypeSuffix(x->suffix, defaultType, "fj", float32Type)) {
        float y = (float)clay_strtod(ptr, &end);
        if (*end != 0)
            error("invalid imag32 literal");
        if (errno == ERANGE)
            error("imag32 literal out of range");
        vh = new ValueHolder(imag32Type);
        *((float *)vh->buf) = y;
    }
    else if (imagTypeSuffix(x->suffix, defaultType, "ffj", float64Type)) {
        double y = clay_strtod(ptr, &end);
        if (*end != 0)
            error("invalid imag64 literal");
        if (errno == ERANGE)
            error("imag64 literal out of range");
        vh = new ValueHolder(imag64Type);
        *((double *)vh->buf) = y;
    }
    else if (x->suffix == "lj" || x->suffix == "flj"
        || (x->suffix == "j" && defaultType == float80Type))
    {
        long double y = clay_strtold(ptr, &end);
        if (*end != 0)
            error("invalid imag80 literal");
        if (errno == ERANGE)
            error("imag80 literal out of range");
        vh = new ValueHolder(imag80Type);
        *((long double *)vh->buf) = y;
    }
    else {
        error("invalid float literal suffix: " + x->suffix);
    }
    return vh;
}

}
