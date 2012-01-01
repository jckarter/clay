#include <cstdio>
#include <cstdlib>
#include <cstdint>
#ifdef _MSC_VER
# define PRIx64 "I64x"
#else
# include <cinttypes>
#endif

struct Struct1 {
    uint64_t a;
};

struct Struct2 {
    uint32_t a;
};

struct Struct3 {
    uint32_t a;
    uint32_t b;
};

struct Struct4 {
    uint32_t a;
    uint32_t b;
    uint32_t c;
};

struct Struct5 {
    uint32_t a;
    uint32_t b;
    uint64_t c;
};

struct Struct6 {
    uint32_t a;
    uint64_t b;
};

struct Struct7 {
    uint32_t a;
    uint64_t b;
    uint32_t c;
};

struct Struct8 {
    uint64_t a;
    uint64_t b;
    uint64_t c;
};

struct Struct9 {
    uint32_t a;
    double b;
};

struct Struct10 {
    uint32_t a;
    float b;
};

struct Struct11 {
    double a;
    uint32_t b;
};

struct Struct12 {
    float a;
    uint32_t b;
};

struct Struct13 {
    float a;
};

struct Struct14 {
    float a;
    float b;
};

struct Struct15 {
    float a;
    float b;
    float c;
};

struct Struct16 {
    float a;
    float b;
    float c;
    float d;
    float e;
};

struct Struct17 {
    double a;
};

struct Struct18 {
    double a;
    double b;
};

struct Struct19 {
    double a;
    double b;
    double c;
};

struct Struct20 {
    double a;
    double b;
    double c;
    double d;
};

struct Struct21 {
    double a;
    double b;
    double c;
    double d;
    double e;
};

union Union22 {
    uint32_t a;
    float b;
};

union Union23 {
    uint32_t a;
    double b;
};

struct Struct26 {
    uint8_t a;
};

struct Struct27 {
    uint16_t a;
};

struct Struct28 {
    uint8_t a;
    uint8_t b;
};

struct Struct29 {
    uint16_t a;
    uint16_t b;
};

struct Struct30 {
    uint8_t a;
    uint8_t b;
    uint8_t c;
};

//
// arguments
//

extern "C" {

void c_scalar(uint32_t x, bool y, float z, double w) {
    printf("%x %x %a %a\n", x, y, z, w);
    fflush(stdout);
}

void c_1(struct Struct1 x) {
    printf("%" PRIx64 "\n", x.a);
    fflush(stdout);
}

void c_2(struct Struct2 x) {
    printf("%x\n", x.a);
    fflush(stdout);
}

void c_3(struct Struct3 x) {
    printf("%x %x\n", x.a, x.b);
    fflush(stdout);
}

void c_4(struct Struct4 x) {
    printf("%x %x %x\n", x.a, x.b, x.c);
    fflush(stdout);
}

void c_5(struct Struct5 x) {
    printf("%x %x %" PRIx64 "\n", x.a, x.b, x.c);
    fflush(stdout);
}

void c_6(struct Struct6 x) {
    printf("%x %" PRIx64 "\n", x.a, x.b);
    fflush(stdout);
}

void c_7(struct Struct7 x) {
    printf("%x %" PRIx64 " %x\n", x.a, x.b, x.c);
    fflush(stdout);
}

void c_8(struct Struct8 x) {
    printf("%" PRIx64 " %" PRIx64 " %" PRIx64 "\n", x.a, x.b, x.c);
    fflush(stdout);
}

void c_9(struct Struct9 x) {
    printf("%x %a\n", x.a, x.b);
    fflush(stdout);
}

void c_10(struct Struct10 x) {
    printf("%x %a\n", x.a, x.b);
    fflush(stdout);
}

void c_11(struct Struct11 x) {
    printf("%a %x\n", x.a, x.b);
    fflush(stdout);
}

void c_12(struct Struct12 x) {
    printf("%a %x\n", x.a, x.b);
    fflush(stdout);
}

void c_13(struct Struct13 x) {
    printf("%a\n", x.a);
    fflush(stdout);
}

void c_14(struct Struct14 x) {
    printf("%a %a\n", x.a, x.b);
    fflush(stdout);
}

void c_15(struct Struct15 x) {
    printf("%a %a %a\n", x.a, x.b, x.c);
    fflush(stdout);
}

void c_16(struct Struct16 x) {
    printf("%a %a %a %a %a\n", x.a, x.b, x.c, x.d, x.e);
    fflush(stdout);
}

void c_17(struct Struct17 x) {
    printf("%a\n", x.a);
    fflush(stdout);
}

void c_18(struct Struct18 x) {
    printf("%a %a\n", x.a, x.b);
    fflush(stdout);
}

void c_19(struct Struct19 x) {
    printf("%a %a %a\n", x.a, x.b, x.c);
    fflush(stdout);
}

void c_20(struct Struct20 x) {
    printf("%a %a %a %a\n", x.a, x.b, x.c, x.d);
    fflush(stdout);
}

void c_21(struct Struct21 x) {
    printf("%a %a %a %a %a\n", x.a, x.b, x.c, x.d, x.e);
    fflush(stdout);
}

void c_22(union Union22 x, int tag) {
    switch (tag) {
    case 0:
        printf("0: %x\n", x.a);
        break;
    case 1:
        printf("1: %a\n", x.b);
        break;
    }
    fflush(stdout);
}

void c_23(union Union23 x, int tag) {
    switch (tag) {
    case 0:
        printf("0: %x\n", x.a);
        break;
    case 1:
        printf("1: %a\n", x.b);
        break;
    }
    fflush(stdout);
}

void c_26(struct Struct26 x) {
    printf("%x\n", x.a);
    fflush(stdout);
}

void c_27(struct Struct27 x) {
    printf("%x\n", x.a);
    fflush(stdout);
}

void c_28(struct Struct28 x) {
    printf("%x %x\n", x.a, x.b);
    fflush(stdout);
}

void c_29(struct Struct29 x) {
    printf("%x %x\n", x.a, x.b);
    fflush(stdout);
}

void c_30(struct Struct30 x) {
    printf("%x %x %x\n", x.a, x.b, x.c);
    fflush(stdout);
}

//
// return
//

#ifdef _MSC_VER
# define HEX_FLOAT_VARIABLE(name, mantissa, exp) \
    union { int bits; float value; } name##_bits = { (0x##mantissa >> 1) | ((exp+127)<<23) }; \
    float name = name##_bits.value;
# define HEX_DOUBLE_VARIABLE(name, mantissa, exp) \
    union { __int64 bits; double value; } name##_bits = { (unsigned __int64)0x##mantissa | ((unsigned __int64)(exp+1023)<<52) }; \
    double name = name##_bits.value;
#else
# define HEX_FLOAT_VARIABLE(name, mantissa, exp) float name = 0x1.mantissa##p##exp##f
# define HEX_DOUBLE_VARIABLE(name, mantissa, exp) double name = 0x1.mantissa##p##exp
#endif

uint32_t c_return_int(void) {
    return 0xC1A4C1A4U;
}

bool c_return_bool(void) {
    return true;
}

float c_return_float(void) {
    HEX_FLOAT_VARIABLE(r, C1A4C0, 123);
    return r;
}

double c_return_double(void) {
    HEX_DOUBLE_VARIABLE(r, C1A4C1A4C1A4C, 123);
    return r;
}

struct Struct1 c_return_1(void) {
    struct Struct1 r = { 0xC1A4C1A4C1A4C1A4ULL };
    return r;
}

struct Struct2 c_return_2(void) {
    struct Struct2 r = { 0xC1A4C1A4 };
    return r;
}

struct Struct3 c_return_3(void) {
    struct Struct3 r = { 0xC1A4C1A4, 0x12345678 };
    return r;
}

struct Struct4 c_return_4(void) {
    struct Struct4 r = { 0xC1A4C1A4, 0x12345678, 0xABCDABCD };
    return r;
}

struct Struct5 c_return_5(void) {
    struct Struct5 r = { 0xC1A4C1A4, 0x12345678, 0xABCDABCDABCDABCDULL };
    return r;
}

struct Struct6 c_return_6(void) {
    struct Struct6 r = { 0xC1A4C1A4, 0xABCDABCDABCDABCDULL };
    return r;
}

struct Struct7 c_return_7(void) {
    struct Struct7 r = { 0xC1A4C1A4, 0xABCDABCDABCDABCDULL, 0x12345678 };
    return r;
}

struct Struct8 c_return_8(void) {
    struct Struct8 r = { 0xC1A4C1A4C1A4C1A4ULL, 0xABCDABCDABCDABCDULL, 0x1234567812345678ULL };
    return r;
}

struct Struct9 c_return_9(void) {
    HEX_DOUBLE_VARIABLE(f, C1A4C1A4C1A4C, 123);
    struct Struct9 r = { 0xC1A4C1A4, f };
    return r;
}

struct Struct10 c_return_10(void) {
    HEX_FLOAT_VARIABLE(f, C1A4C0, 123);
    struct Struct10 r = { 0xC1A4C1A4, f };
    return r;
}

struct Struct11 c_return_11(void) {
    HEX_DOUBLE_VARIABLE(f, C1A4C1A4C1A4C, 123);
    struct Struct11 r = { f, 0xC1A4C1A4 };
    return r;
}

struct Struct12 c_return_12(void) {
    HEX_FLOAT_VARIABLE(f, C1A4C0, 123);
    struct Struct12 r = { f, 0xC1A4C1A4 };
    return r;
}

struct Struct13 c_return_13(void) {
    HEX_FLOAT_VARIABLE(f, C1A4C0, 123);
    struct Struct13 r = { f };
    return r;
}

struct Struct14 c_return_14(void) {
    HEX_FLOAT_VARIABLE(f, C1A4C0, 123);
    HEX_FLOAT_VARIABLE(g, ABCDEE, 99);
    struct Struct14 r = { f, g };
    return r;
}

struct Struct15 c_return_15(void) {
    HEX_FLOAT_VARIABLE(f, C1A4C0, 123);
    HEX_FLOAT_VARIABLE(g, ABCDEE, 99);
    HEX_FLOAT_VARIABLE(h, 010102, 10);
    struct Struct15 r = { f, g, h };
    return r;
}

struct Struct16 c_return_16(void) {
    HEX_FLOAT_VARIABLE(f, C1A4C0, 123);
    HEX_FLOAT_VARIABLE(g, ABCDEE, 99);
    HEX_FLOAT_VARIABLE(h, 010102, 10);
    HEX_FLOAT_VARIABLE(i, 020202, 20);
    HEX_FLOAT_VARIABLE(j, 030302, 30);
    struct Struct16 r = { f, g, h, i, j };
    return r;
}

struct Struct17 c_return_17(void) {
    HEX_DOUBLE_VARIABLE(f, C1A4C1A4C1A4C, 123);
    struct Struct17 r = { f };
    return r;
}

struct Struct18 c_return_18(void) {
    HEX_DOUBLE_VARIABLE(f, C1A4C1A4C1A4C, 123);
    HEX_DOUBLE_VARIABLE(g, ABCDEFABCDEFA, 99);
    struct Struct18 r = { f, g };
    return r;
}

struct Struct19 c_return_19(void) {
    HEX_DOUBLE_VARIABLE(f, C1A4C1A4C1A4C, 123);
    HEX_DOUBLE_VARIABLE(g, ABCDEFABCDEFA, 99);
    HEX_DOUBLE_VARIABLE(h, 0101010101010, 10);
    struct Struct19 r = { f, g, h };
    return r;
}

struct Struct20 c_return_20(void) {
    HEX_DOUBLE_VARIABLE(f, C1A4C1A4C1A4C, 123);
    HEX_DOUBLE_VARIABLE(g, ABCDEFABCDEFA, 99);
    HEX_DOUBLE_VARIABLE(h, 0101010101010, 10);
    HEX_DOUBLE_VARIABLE(i, 0202020202020, 20);
    struct Struct20 r = { f, g, h, i };
    return r;
}

struct Struct21 c_return_21(void) {
    HEX_DOUBLE_VARIABLE(f, C1A4C1A4C1A4C, 123);
    HEX_DOUBLE_VARIABLE(g, ABCDEFABCDEFA, 99);
    HEX_DOUBLE_VARIABLE(h, 0101010101010, 10);
    HEX_DOUBLE_VARIABLE(i, 0202020202020, 20);
    HEX_DOUBLE_VARIABLE(j, 0303030303030, 30);
    struct Struct21 r = { f, g, h, i, j };
    return r;
}

union Union22 c_return_22(int tag) {
    union Union22 r;
    HEX_FLOAT_VARIABLE(f, C1A4C0, 123);
    switch (tag) {
    case 0:
        r.a = 0xC1A4C1A4;
        break;
    case 1:
        r.b = f;
        break;
    default:
        abort();
    }
    return r;
}

union Union23 c_return_23(int tag) {
    union Union23 r;
    HEX_DOUBLE_VARIABLE(f, C1A4C1A4C1A4C, 123);
    switch (tag) {
    case 0:
        r.a = 0xC1A4C1A4;
        break;
    case 1:
        r.b = f;
        break;
    default:
        abort();
    }
    return r;
}

struct Struct26 c_return_26(void) {
    struct Struct26 r = { 0xC1 };
    return r;
}

struct Struct27 c_return_27(void) {
    struct Struct27 r = { 0xC1A4 };
    return r;
}

struct Struct28 c_return_28(void) {
    struct Struct28 r = { 0xC1, 0xA4 };
    return r;
}

struct Struct29 c_return_29(void) {
    struct Struct29 r = { 0xC1A4, 0xABCD };
    return r;
}

struct Struct30 c_return_30(void) {
    struct Struct30 r = { 0xC1, 0xA4, 0xAB };
    return r;
}

//
// clay entry points
//

void clay_scalar(uint32_t x, bool y, float z, double w);

void clay_1(struct Struct1 x);

void clay_2(struct Struct2 x);

void clay_3(struct Struct3 x);

void clay_4(struct Struct4 x);

void clay_5(struct Struct5 x);

void clay_6(struct Struct6 x);

void clay_7(struct Struct7 x);

void clay_8(struct Struct8 x);

void clay_9(struct Struct9 x);

void clay_10(struct Struct10 x);

void clay_11(struct Struct11 x);

void clay_12(struct Struct12 x);

void clay_13(struct Struct13 x);

void clay_14(struct Struct14 x);

void clay_15(struct Struct15 x);

void clay_16(struct Struct16 x);

void clay_17(struct Struct17 x);

void clay_18(struct Struct18 x);

void clay_19(struct Struct19 x);

void clay_20(struct Struct20 x);

void clay_21(struct Struct21 x);

void clay_22(union Union22 x, int tag);

void clay_23(union Union23 x, int tag);

void clay_26(struct Struct26 x);

void clay_27(struct Struct27 x);

void clay_28(struct Struct28 x);

void clay_29(struct Struct29 x);

void clay_30(struct Struct30 x);

void clay_flush(void);

//
// return
//

uint32_t clay_return_int(void);

bool clay_return_bool(void);

float clay_return_float(void);

double clay_return_double(void);

struct Struct1 clay_return_1(void);

struct Struct2 clay_return_2(void);

struct Struct3 clay_return_3(void);

struct Struct4 clay_return_4(void);

struct Struct5 clay_return_5(void);

struct Struct6 clay_return_6(void);

struct Struct7 clay_return_7(void);

struct Struct8 clay_return_8(void);

struct Struct9 clay_return_9(void);

struct Struct10 clay_return_10(void);

struct Struct11 clay_return_11(void);

struct Struct12 clay_return_12(void);

struct Struct13 clay_return_13(void);

struct Struct14 clay_return_14(void);

struct Struct15 clay_return_15(void);

struct Struct16 clay_return_16(void);

struct Struct17 clay_return_17(void);

struct Struct18 clay_return_18(void);

struct Struct19 clay_return_19(void);

struct Struct20 clay_return_20(void);

struct Struct21 clay_return_21(void);

union Union22 clay_return_22(int tag);

union Union23 clay_return_23(int tag);

struct Struct26 clay_return_26(void);

struct Struct27 clay_return_27(void);

struct Struct28 clay_return_28(void);

struct Struct29 clay_return_29(void);

struct Struct30 clay_return_30(void);

void c_to_clay(void) {
    printf("\nPassing C arguments to Clay:\n");
    fflush(stdout);

    clay_scalar(c_return_int(), c_return_bool(), c_return_float(), c_return_double());
    clay_1(c_return_1());
    clay_2(c_return_2());
    clay_3(c_return_3());
    clay_4(c_return_4());
    clay_5(c_return_5());
    clay_6(c_return_6());
    clay_7(c_return_7());
    clay_8(c_return_8());
    clay_9(c_return_9());
    clay_10(c_return_10());
    clay_11(c_return_11());
    clay_12(c_return_12());
    clay_13(c_return_13());
    clay_14(c_return_14());
    clay_15(c_return_15());
    clay_16(c_return_16());
    clay_17(c_return_17());
    clay_18(c_return_18());
    clay_19(c_return_19());
    clay_20(c_return_20());
    clay_21(c_return_21());

    clay_22(c_return_22(0), 0);
    clay_22(c_return_22(1), 1);

    clay_23(c_return_23(0), 0);
    clay_23(c_return_23(1), 1);

    clay_26(c_return_26());
    clay_27(c_return_27());
    clay_28(c_return_28());
    clay_29(c_return_29());
    clay_30(c_return_30());

    printf("\nPassing Clay return values to C:\n");
    fflush(stdout);

    c_scalar(clay_return_int(), clay_return_bool(), clay_return_float(), clay_return_double());

    c_1(clay_return_1());

    c_2(clay_return_2());

    c_3(clay_return_3());

    c_4(clay_return_4());

    c_5(clay_return_5());

    c_6(clay_return_6());

    c_7(clay_return_7());

    c_8(clay_return_8());

    c_9(clay_return_9());

    c_10(clay_return_10());

    c_11(clay_return_11());

    c_12(clay_return_12());

    c_13(clay_return_13());

    c_14(clay_return_14());

    c_15(clay_return_15());

    c_16(clay_return_16());

    c_17(clay_return_17());

    c_18(clay_return_18());

    c_19(clay_return_19());

    c_20(clay_return_20());

    c_21(clay_return_21());

    c_22(clay_return_22(0), 0);
    c_22(clay_return_22(1), 1);

    c_23(clay_return_23(0), 0);
    c_23(clay_return_23(1), 1);

    c_26(clay_return_26());
    c_27(clay_return_27());
    c_28(clay_return_28());
    c_29(clay_return_29());
    c_30(clay_return_30());
}

}
