// must include external_test.hpp first
#include <emmintrin.h>

static inline __m128i make_int_vector(uint64_t a, uint64_t b) {
    union { uint64_t fields[2]; __m128i v; } u = { { a,b } };
    return u.v;
}

static inline __m128 make_float_vector(float a, float b, float c, float d) {
    union { float fields[4]; __m128 v; } u = { { a,b,c,d } };
    return u.v;
}

static inline __m128d make_double_vector(double a, double b) {
    union { double fields[2]; __m128d v; } u = { { a,b } };
    return u.v;
}

static inline void print_int_vector(__m128i v) {
    union { __m128i v; uint64_t fields[2]; } u = { v };
    printf("<%" PRIx64 " %" PRIx64 ">\n", u.fields[0], u.fields[1]);
}

static inline void print_float_vector(__m128 v) {
    union { __m128 v; float fields[4]; } u = { v };
    printf("<%.6a %.6a %.6a %.6a>\n", u.fields[0], u.fields[1], u.fields[2], u.fields[3]);
}

static inline void print_double_vector(__m128d v) {
    union { __m128d v; double fields[2]; } u = { v };
    printf("<%.13a %.13a>\n", u.fields[0], u.fields[1]);
}

