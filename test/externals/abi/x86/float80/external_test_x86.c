#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <complex.h>
#include <emmintrin.h>
#include <inttypes.h>

struct Struct_x86_1 {
    long double a;
    long double b;
};

struct Struct_x86_8 {
    long double a;
};

struct Struct_x86_9 {
    complex long double a;
};

union Union_x86_12 {
    int32_t a;
    long double b;
};

union Union_x86_13 {
    int32_t a;
    complex long double b;
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

void c_x86_long_double(long double x) {
    printf("%La\n", x);
}

void c_x86_complex_long_double(complex long double x) {
    printf("%La,%La\n", creall(x), cimagl(x));
}

void c_x86_1(struct Struct_x86_1 x) {
    printf("%La %La\n", x.a, x.b);
}

void c_x86_8(struct Struct_x86_8 x) {
    printf("%La\n", x.a);
}

void c_x86_9(struct Struct_x86_9 x) {
    printf("%La,%La\n", creall(x.a), cimagl(x.a));
}

void c_x86_12(union Union_x86_12 x, int tag) {
    switch (tag) {
    case 0:
        printf("%x\n", x.a);
        break;
    case 1:
        printf("%La\n", x.b);
        break;
    }
}

void c_x86_13(union Union_x86_13 x, int tag) {
    switch (tag) {
    case 0:
        printf("%x\n", x.a);
        break;
    case 1:
        printf("%La,%La\n", creall(x.b), cimagl(x.b));
        break;
    }
}

//
// returns
//

long double c_x86_return_long_double(void) {
    return 0x8.001122334455667p999L;
}

complex long double c_x86_return_complex_long_double(void) {
    return 0x8.001122334455667p999L+0x8.112233445566778p888L*I;
}

struct Struct_x86_1 c_x86_return_1(void) {
    return (struct Struct_x86_1){ 0x8.001122334455667p999L, 0x8.112233445566778p888L };
}

struct Struct_x86_8 c_x86_return_8(void) {
    return (struct Struct_x86_8){ 0x8.001122334455667p999L };
}

struct Struct_x86_9 c_x86_return_9(void) {
    return (struct Struct_x86_9){ 0x8.001122334455667p999L+0x8.112233445566778p888L*I };
}

union Union_x86_12 c_x86_return_12(int tag) {
    switch (tag) {
    case 0:
        return (union Union_x86_12){ .a = 0x11223344 };
    case 1:
        return (union Union_x86_12){ .b = 0x8.001122334455667p999L };
    default:
        abort();
    }
}

union Union_x86_13 c_x86_return_13(int tag) {
    switch (tag) {
    case 0:
        return (union Union_x86_13){ .a = 0x11223344 };
    case 1:
        return (union Union_x86_13){
            .b = 0x8.001122334455667p999L+0x8.112233445566778p888L*I
        };
    default:
        abort();
    };
}

//
// clay entry points
//

void clay_x86_long_double(long double x);
void clay_x86_complex_long_double(complex long double x);
void clay_x86_1(struct Struct_x86_1 x);
void clay_x86_8(struct Struct_x86_8 x);
void clay_x86_9(struct Struct_x86_9 x);
void clay_x86_12(union Union_x86_12 x, int tag);
void clay_x86_13(union Union_x86_13 x, int tag);

void clay_flush(void);

//
// return
//

long double clay_x86_return_long_double(void);
complex long double clay_x86_return_complex_long_double(void);
struct Struct_x86_1 clay_x86_return_1(void);
struct Struct_x86_8 clay_x86_return_8(void);
struct Struct_x86_9 clay_x86_return_9(void);
union Union_x86_12 clay_x86_return_12(int tag);
union Union_x86_13 clay_x86_return_13(int tag);

void c_to_clay_x86(void) {
    printf("\nPassing C arguments to Clay:\n");
    fflush(stdout);

    clay_x86_long_double(c_x86_return_long_double());
    clay_x86_complex_long_double(c_x86_return_complex_long_double());

    clay_x86_1(c_x86_return_1());
    clay_x86_8(c_x86_return_8());
    clay_x86_9(c_x86_return_9());

    clay_x86_12(c_x86_return_12(0), 0);
    clay_x86_12(c_x86_return_12(1), 1);

    clay_x86_13(c_x86_return_13(0), 0);
    clay_x86_13(c_x86_return_13(1), 1);

    clay_flush();

    printf("\nPassing Clay return values to C:\n");

    c_x86_long_double(clay_x86_return_long_double());
    c_x86_complex_long_double(clay_x86_return_complex_long_double());

    c_x86_1(clay_x86_return_1());
    c_x86_8(clay_x86_return_8());
    c_x86_9(clay_x86_return_9());

    c_x86_12(clay_x86_return_12(0), 0);
    c_x86_12(clay_x86_return_12(1), 1);

    c_x86_13(clay_x86_return_13(0), 0);
    c_x86_13(clay_x86_return_13(1), 1);

    fflush(stdout);
}
