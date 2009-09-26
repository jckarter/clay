#include <stdio.h>
#include <stdlib.h>


#define MACHINEOP extern "C" void



/*
 * pointers
 */

MACHINEOP mop_pointerOffset(char **ptr, int *offset, char **result) {
    *result = *ptr + *offset;
}

MACHINEOP mop_pointerSubtract(char **ptr1, char **ptr2, int *result) {
    *result = (*ptr1 - *ptr2);
}

MACHINEOP mop_pointerCopy(char **dest, char **src) {
    *dest = *src;
}

MACHINEOP mop_pointerEquals(char **ptr1, char **ptr2, bool *result) {
    *result = (*ptr1 == *ptr2);
}

MACHINEOP mop_pointerLesser(char **ptr1, char **ptr2, bool *result) {
    *result = (*ptr1 < *ptr2);
}

MACHINEOP mop_allocateMemory(int *size, void **result) {
    *result = malloc(*size);
}

MACHINEOP mop_freeMemory(void **ptr) {
    free(*ptr);
}



/*
 * bool
 */

MACHINEOP mop_boolCopy(bool *dest, bool *src) {
    *dest = *src;
}



/*
 * int
 */

MACHINEOP mop_intCopy(int *dest, int *src) {
    *dest = *src;
}

MACHINEOP mop_intEquals(int *a, int *b, bool *result) {
    *result = (*a == *b);
}

MACHINEOP mop_intLesser(int *a, int *b, bool *result) {
    *result = (*a < *b);
}

MACHINEOP mop_intAdd(int *a, int *b, int *result) {
    *result = *a + *b;
}

MACHINEOP mop_intSubtract(int *a, int *b, int *result) {
    *result = *a - *b;
}

MACHINEOP mop_intMultiply(int *a, int *b, int *result) {
    *result = (*a) * (*b);
}

MACHINEOP mop_intDivide(int *a, int *b, int *result) {
    *result = (*a) / (*b);
}

MACHINEOP mop_intModulus(int *a, int *b, int *result) {
    *result = (*a) % (*b);
}

MACHINEOP mop_intNegate(int *a, int *result) {
    *result = -(*a);
}



/*
 * float
 */

MACHINEOP mop_floatCopy(float *dest, float *src) {
    *dest = *src;
}

MACHINEOP mop_floatEquals(float *a, float *b, bool *result) {
    *result = (*a == *b);
}

MACHINEOP mop_floatLesser(float *a, float *b, bool *result) {
    *result = (*a < *b);
}

MACHINEOP mop_floatAdd(float *a, float *b, float *result) {
    *result = *a + *b;
}

MACHINEOP mop_floatSubtract(float *a, float *b, float *result) {
    *result = *a - *b;
}

MACHINEOP mop_floatMultiply(float *a, float *b, float *result) {
    *result = (*a) * (*b);
}

MACHINEOP mop_floatDivide(float *a, float *b, float *result) {
    *result = (*a) / (*b);
}

MACHINEOP mop_floatNegate(float *a, float *result) {
    *result = -(*a);
}



/*
 * double
 */

MACHINEOP mop_doubleCopy(double *dest, double *src) {
    *dest = *src;
}

MACHINEOP mop_doubleEquals(double *a, double *b, bool *result) {
    *result = (*a == *b);
}

MACHINEOP mop_doubleLesser(double *a, double *b, bool *result) {
    *result = (*a < *b);
}

MACHINEOP mop_doubleAdd(double *a, double *b, double *result) {
    *result = *a + *b;
}

MACHINEOP mop_doubleSubtract(double *a, double *b, double *result) {
    *result = *a - *b;
}

MACHINEOP mop_doubleMultiply(double *a, double *b, double *result) {
    *result = (*a) * (*b);
}

MACHINEOP mop_doubleDivide(double *a, double *b, double *result) {
    *result = (*a) / (*b);
}

MACHINEOP mop_doubleNegate(double *a, double *result) {
    *result = -(*a);
}



/*
 * conversions
 */

MACHINEOP mop_floatToInt(float *a, int *result) {
    *result = (int)(*a);
}

MACHINEOP mop_intToFloat(int *a, float *result) {
    *result = (float)(*a);
}

MACHINEOP mop_floatToDouble(float *a, double *result) {
    *result = (double)(*a);
}

MACHINEOP mop_doubleToFloat(double *a, float *result) {
    *result = (float)(*a);
}

MACHINEOP mop_doubleToInt(double *a, int *result) {
    *result = (int)(*a);
}

MACHINEOP mop_intToDouble(int *a, double *result) {
    *result = (double)(*a);
}



/*
 * testing
 */

extern "C" void mop_hello() {
    printf("Hello World!\n");
}
