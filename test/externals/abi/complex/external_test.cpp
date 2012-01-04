#include "../external_test.hpp"
#include <complex.h>

union Union24 {
    uint32_t a;
    complex float b;
};

union Union25 {
    uint32_t a;
    complex double b;
};

extern "C" {

//
// arguments
//

void c_complex_float(complex float x) {
    printf("%a,%a\n", crealf(x), cimagf(x));
}

void c_complex_double(complex double x) {
    printf("%a,%a\n", creal(x), cimag(x));
}

void c_24(union Union24 x, int tag) {
    switch (tag) {
    case 0:
        printf("0: %x\n", x.a);
        break;
    case 1:
        printf("1: %a,%a\n", crealf(x.b), cimagf(x.b));
        break;
    }
}

void c_25(union Union25 x, int tag) {
    switch (tag) {
    case 0:
        printf("0: %x\n", x.a);
        break;
    case 1:
        printf("1: %a,%a\n", crealf(x.b), cimagf(x.b));
        break;
    }
}

//
// return
//

complex float c_return_complex_float(void) {
    return 0x1.C1A4C0p123f+0x1.ABCDEEp99f*I;
}

complex double c_return_complex_double(void) {
    return 0x1.C1A4C1A4C1A4Cp123+0x1.ABCDEFABCDEFAp99*I;
}

union Union24 c_return_24(int tag) {
    switch (tag) {
    case 0:
        return (union Union24){ .a = 0xC1A4C1A4 };
    case 1:
        return (union Union24){ .b = 0x1.C1A4C0p123f+0x1.ABCDEEp99f*I };
    default:
        abort();
    }
}

union Union25 c_return_25(int tag) {
    switch (tag) {
    case 0:
        return (union Union25){ .a = 0xC1A4C1A4 };
    case 1:
        return (union Union25){ .b = 0x1.C1A4C1A4C1A4Cp123+0x1.ABCDEFABCDEFAp99*I };
    default:
        abort();
    }
}

//
// clay entry points
//

void clay_complex_float(complex float x);

void clay_complex_double(complex double x);

void clay_24(union Union24 x, int tag);

void clay_25(union Union25 x, int tag);

void clay_flush(void);

//
// return
//

complex float clay_return_complex_float(void);

complex double clay_return_complex_double(void);

union Union24 clay_return_24(int tag);

union Union25 clay_return_25(int tag);

void c_to_clay(void) {
    printf("\nPassing C arguments to Clay:\n");
    fflush(stdout);

    clay_complex_float(c_return_complex_float());
    clay_complex_double(c_return_complex_double());

    clay_24(c_return_24(0), 0);
    clay_24(c_return_24(1), 1);

    clay_25(c_return_25(0), 0);
    clay_25(c_return_25(1), 1);

    clay_flush();

    printf("\nPassing Clay return values to C:\n");

    c_complex_float(clay_return_complex_float());

    c_complex_double(clay_return_complex_double());

    c_24(clay_return_24(0), 0);
    c_24(clay_return_24(1), 1);

    c_25(clay_return_25(0), 0);
    c_25(clay_return_25(1), 1);

    fflush(stdout);
}

}
