#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <emmintrin.h>
#include <inttypes.h>

struct Struct_x86_2 {
    __m128i a;
};

struct Struct_x86_3 {
    __m128i a;
    __m128i b;
};

struct Struct_x86_4 {
    __m128 a;
};

struct Struct_x86_5 {
    __m128 a;
    __m128 b;
};

struct Struct_x86_6 {
    __m128d a;
};

struct Struct_x86_7 {
    __m128d a;
    __m128d b;
};

union Union_x86_10 {
    int32_t a;
    __m128d b;
};

union Union_x86_11 {
    int64_t a;
    __m128d b;
};

static void print_int_vector(__m128i v) {
    union { __m128i v; uint64_t fields[2]; } u = { v };
    printf("<%" PRIx64 " %" PRIx64 ">\n", u.fields[0], u.fields[1]);
}

static void print_float_vector(__m128 v) {
    union { __m128 v; float fields[4]; } u = { v };
    printf("<%a %a %a %a>\n", u.fields[0], u.fields[1], u.fields[2], u.fields[3]);
}

static void print_double_vector(__m128d v) {
    union { __m128d v; double fields[2]; } u = { v };
    printf("<%a %a>\n", u.fields[0], u.fields[1]);
}

//
// arguments
//

void c_x86_vector(__m128i x, __m128 y, __m128d z) {
    print_int_vector(x);
    print_float_vector(y);
    print_double_vector(z);
}

void c_x86_2(struct Struct_x86_2 x) {
    print_int_vector(x.a);
}

void c_x86_3(struct Struct_x86_3 x) {
    print_int_vector(x.a);
    print_int_vector(x.b);
}

void c_x86_4(struct Struct_x86_4 x) {
    print_float_vector(x.a);
}

void c_x86_5(struct Struct_x86_5 x) {
    print_float_vector(x.a);
    print_float_vector(x.b);
}

void c_x86_6(struct Struct_x86_6 x) {
    print_double_vector(x.a);
}

void c_x86_7(struct Struct_x86_7 x) {
    print_double_vector(x.a);
    print_double_vector(x.b);
}

void c_x86_10(union Union_x86_10 x, int tag) {
    switch (tag) {
    case 0:
        printf("%x\n", x.a);
        break;
    case 1:
        print_double_vector(x.b);
        break;
    }
}

void c_x86_11(union Union_x86_11 x, int tag) {
    switch (tag) {
    case 0:
        printf("%" PRIx64 "\n", (uint64_t)x.a);
        break;
    case 1:
        print_double_vector(x.b);
        break;
    }
}

//
// returns
//

__m128i c_x86_return_int_vector(void) {
    return (__m128i){ 0x1122334455667788LL, 0x33445566778899AALL };
}

__m128 c_x86_return_float_vector(void) {
    return (__m128){ 0x1.112232p99f, 0x1.223344p88f, 0x1.334454p77f, 0x1.445566p66f };
}

__m128d c_x86_return_double_vector(void) {
    return (__m128d){ 0x1.1122334455667p99, 0x1.2233445566778p88 };
}

struct Struct_x86_2 c_x86_return_2(void) {
    return (struct Struct_x86_2){ { 0x1122334455667788LL, 0x33445566778899AALL } };
}

struct Struct_x86_3 c_x86_return_3(void) {
    return (struct Struct_x86_3){
        { 0x1122334455667788LL, 0x33445566778899AALL },
        { 0x2233445566778899LL, 0x445566778899AABBLL }
    };
}

struct Struct_x86_4 c_x86_return_4(void) {
    return (struct Struct_x86_4){
        { 0x1.112232p99f, 0x1.223344p88f, 0x1.334454p77f, 0x1.445566p66f }
    };
}

struct Struct_x86_5 c_x86_return_5(void) {
    return (struct Struct_x86_5){
        { 0x1.112232p99f, 0x1.223344p88f, 0x1.334454p77f, 0x1.445566p66f },
        { 0x1.556676p55f, 0x1.667788p44f, 0x1.778898p33f, 0x1.8899AAp22f }
    };
}

struct Struct_x86_6 c_x86_return_6(void) {
    return (struct Struct_x86_6){
        { 0x1.1122334455667p99, 0x1.2233445566778p88 }
    };
}

struct Struct_x86_7 c_x86_return_7(void) {
    return (struct Struct_x86_7){
        { 0x1.1122334455667p99, 0x1.2233445566778p88 },
        { 0x1.3344556677889p77, 0x1.445566778899Ap66 }
    };
}

union Union_x86_10 c_x86_return_10(int tag) {
    switch (tag) {
    case 0:
        return (union Union_x86_10){ .a = 0x11223344 };
    case 1:
        return (union Union_x86_10){ .b = { 0x1.1122334455667p99, 0x1.2233445566778p88 } };
    default:
        abort();
    }
}

union Union_x86_11 c_x86_return_11(int tag) {
    switch (tag) {
    case 0:
        return (union Union_x86_11){ .a = 0x1122334455667788LL };
    case 1:
        return (union Union_x86_11){ .b = { 0x1.1122334455667p99, 0x1.2233445566778p88 } };
    default:
        abort();
    }
}

//
// clay entry points
//

void clay_x86_vector(__m128i x, __m128 y, __m128d z);
void clay_x86_2(struct Struct_x86_2 x);
void clay_x86_3(struct Struct_x86_3 x);
void clay_x86_4(struct Struct_x86_4 x);
void clay_x86_5(struct Struct_x86_5 x);
void clay_x86_6(struct Struct_x86_6 x);
void clay_x86_7(struct Struct_x86_7 x);
void clay_x86_10(union Union_x86_10 x, int tag);
void clay_x86_11(union Union_x86_11 x, int tag);

void clay_flush(void);

//
// return
//

__m128i clay_x86_return_int_vector(void);
__m128 clay_x86_return_float_vector(void);
__m128d clay_x86_return_double_vector(void);
struct Struct_x86_2 clay_x86_return_2(void);
struct Struct_x86_3 clay_x86_return_3(void);
struct Struct_x86_4 clay_x86_return_4(void);
struct Struct_x86_5 clay_x86_return_5(void);
struct Struct_x86_6 clay_x86_return_6(void);
struct Struct_x86_7 clay_x86_return_7(void);
union Union_x86_10 clay_x86_return_10(int tag);
union Union_x86_11 clay_x86_return_11(int tag);

void c_to_clay_x86(void) {
    printf("\nPassing C arguments to Clay:\n");
    fflush(stdout);

    clay_x86_vector(c_x86_return_int_vector(),
        c_x86_return_float_vector(), c_x86_return_double_vector());

    clay_x86_2(c_x86_return_2());
    clay_x86_3(c_x86_return_3());
    clay_x86_4(c_x86_return_4());
    clay_x86_5(c_x86_return_5());
    clay_x86_6(c_x86_return_6());
    clay_x86_7(c_x86_return_7());

    clay_x86_10(c_x86_return_10(0), 0);
    clay_x86_10(c_x86_return_10(1), 1);

    clay_x86_11(c_x86_return_11(0), 0);
    clay_x86_11(c_x86_return_11(1), 1);

    clay_flush();

    printf("\nPassing Clay return values to C:\n");

    c_x86_vector(clay_x86_return_int_vector(),
        clay_x86_return_float_vector(), clay_x86_return_double_vector());

    c_x86_2(clay_x86_return_2());
    c_x86_3(clay_x86_return_3());
    c_x86_4(clay_x86_return_4());
    c_x86_5(clay_x86_return_5());
    c_x86_6(clay_x86_return_6());
    c_x86_7(clay_x86_return_7());

    c_x86_10(clay_x86_return_10(0), 0);
    c_x86_10(clay_x86_return_10(1), 1);

    c_x86_11(clay_x86_return_11(0), 0);
    c_x86_11(clay_x86_return_11(1), 1);
    fflush(stdout);
}
