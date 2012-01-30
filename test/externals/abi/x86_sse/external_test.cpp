#include "../external_test.hpp"
#include "../external_test_x86.hpp"

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

extern "C" {

//
// arguments
//

void c_x86_vector(__m128i x, __m128 y, __m128d z) {
    print_int_vector(x);
    print_float_vector(y);
    print_double_vector(z);
    fflush(stdout);
}

void c_x86_2(struct Struct_x86_2 x) {
    print_int_vector(x.a);
    fflush(stdout);
}

void c_x86_3(struct Struct_x86_3 x) {
    print_int_vector(x.a);
    print_int_vector(x.b);
    fflush(stdout);
}

void c_x86_4(struct Struct_x86_4 x) {
    print_float_vector(x.a);
    fflush(stdout);
}

void c_x86_5(struct Struct_x86_5 x) {
    print_float_vector(x.a);
    print_float_vector(x.b);
    fflush(stdout);
}

void c_x86_6(struct Struct_x86_6 x) {
    print_double_vector(x.a);
    fflush(stdout);
}

void c_x86_7(struct Struct_x86_7 x) {
    print_double_vector(x.a);
    print_double_vector(x.b);
    fflush(stdout);
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
    fflush(stdout);
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
    fflush(stdout);
}

//
// returns
//

__m128i c_x86_return_int_vector(void) {
    return make_int_vector(0x1122334455667788LL, 0x33445566778899AALL);
}

__m128 c_x86_return_float_vector(void) {
    HEX_FLOAT_VARIABLE(f, 112232, 99);
    HEX_FLOAT_VARIABLE(g, 223344, 88);
    HEX_FLOAT_VARIABLE(h, 334454, 77);
    HEX_FLOAT_VARIABLE(i, 445566, 66);
    return make_float_vector(f,g,h,i);
}

__m128d c_x86_return_double_vector(void) {
    HEX_DOUBLE_VARIABLE(f, 1122334455667, 99);
    HEX_DOUBLE_VARIABLE(g, 2233445566778, 88);
    return make_double_vector(f,g);
}

struct Struct_x86_2 c_x86_return_2(void) {
    struct Struct_x86_2 r = { make_int_vector(0x1122334455667788LL, 0x33445566778899AALL) };
    return r;
}

struct Struct_x86_3 c_x86_return_3(void) {
    struct Struct_x86_3 r = {
        make_int_vector(0x1122334455667788LL, 0x33445566778899AALL),
        make_int_vector(0x2233445566778899LL, 0x445566778899AABBLL)
    };
    return r;
}

struct Struct_x86_4 c_x86_return_4(void) {
    HEX_FLOAT_VARIABLE(f, 112232, 99);
    HEX_FLOAT_VARIABLE(g, 223344, 88);
    HEX_FLOAT_VARIABLE(h, 334454, 77);
    HEX_FLOAT_VARIABLE(i, 445566, 66);
    struct Struct_x86_4 r = {
        make_float_vector(f,g,h,i)
    };
    return r;
}

struct Struct_x86_5 c_x86_return_5(void) {
    HEX_FLOAT_VARIABLE(f, 112232, 99);
    HEX_FLOAT_VARIABLE(g, 223344, 88);
    HEX_FLOAT_VARIABLE(h, 334454, 77);
    HEX_FLOAT_VARIABLE(i, 445566, 66);
    HEX_FLOAT_VARIABLE(j, 556676, 55);
    HEX_FLOAT_VARIABLE(k, 667788, 44);
    HEX_FLOAT_VARIABLE(l, 778898, 33);
    HEX_FLOAT_VARIABLE(m, 8899AA, 22);
    struct Struct_x86_5 r = {
        make_float_vector(f, g, h, i),
        make_float_vector(j, k, l, m)
    };
    return r;
}

struct Struct_x86_6 c_x86_return_6(void) {
    HEX_DOUBLE_VARIABLE(f, 1122334455667, 99);
    HEX_DOUBLE_VARIABLE(g, 2233445566778, 88);
    struct Struct_x86_6 r = {
        make_double_vector(f, g)
    };
    return r;
}

struct Struct_x86_7 c_x86_return_7(void) {
    HEX_DOUBLE_VARIABLE(f, 1122334455667, 99);
    HEX_DOUBLE_VARIABLE(g, 2233445566778, 88);
    HEX_DOUBLE_VARIABLE(h, 3344556677889, 77);
    HEX_DOUBLE_VARIABLE(i, 445566778899A, 66);
    struct Struct_x86_7 r = {
        make_double_vector(f, g),
        make_double_vector(h, i)
    };
    return r;
}

union Union_x86_10 c_x86_return_10(int tag) {
    union Union_x86_10 r;
    HEX_DOUBLE_VARIABLE(f, 1122334455667, 99);
    HEX_DOUBLE_VARIABLE(g, 2233445566778, 88);
    switch (tag) {
    case 0:
        r.a = 0x11223344;
        break;
    case 1:
        r.b = make_double_vector(f, g);
        break;
    default:
        abort();
    }
    return r;
}

union Union_x86_11 c_x86_return_11(int tag) {
    union Union_x86_11 r;
    HEX_DOUBLE_VARIABLE(f, 1122334455667, 99);
    HEX_DOUBLE_VARIABLE(g, 2233445566778, 88);
    switch (tag) {
    case 0:
        r.a = 0x1122334455667788LL;
        break;
    case 1:
        r.b = make_double_vector(f, g);
        break;
    default:
        abort();
    }
    return r;
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

    printf("\nPassing Clay return values to C:\n");
    fflush(stdout);

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
}

}
