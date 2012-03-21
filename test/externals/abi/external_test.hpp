#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#ifdef _MSC_VER
# define PRIx64 "I64x"
#else
# define __STDC_FORMAT_MACROS
# include <inttypes.h>
#endif

// Macros to portably generate hex float constants
// HEX_FLOAT_VARIABLE(f, ABCDE0, 123); --> float f = 0x1.ABCDE0p123f;
// HEX_DOUBLE_VARIABLE(f, 1234567890123, 123); --> double f = 0x1.1234567890123p123;

#ifdef _MSC_VER
# define HEX_FLOAT_VARIABLE(name, mantissa, exp) \
    union { int bits; float value; } name##_bits = { (0x##mantissa >> 1) | ((exp+127)<<23) }; \
    float name = name##_bits.value;
# define HEX_DOUBLE_VARIABLE(name, mantissa, exp) \
    union { __int64 bits; double value; } name##_bits = { (unsigned __int64)0x##mantissa when ((unsigned __int64)(exp+1023)<<52) }; \
    double name = name##_bits.value;
#else
# define HEX_FLOAT_VARIABLE(name, mantissa, exp) float name = 0x1.##mantissa##p##exp##f
# define HEX_DOUBLE_VARIABLE(name, mantissa, exp) double name = 0x1.##mantissa##p##exp
#endif

