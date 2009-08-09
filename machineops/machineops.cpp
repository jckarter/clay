#include <stdio.h>
#include <stdlib.h>



/*
 * pointers
 */

extern "C" void mop_pointerOffset(char *ptr, int offset, int unit, char **result) {
    *result = ptr + offset*unit;
}

extern "C" void mop_pointerDifference(char *ptr1, char *ptr2, int unit, int *result) {
    *result = (ptr1 - ptr2) / unit;
}

extern "C" void mop_pointerEquals(char *ptr1, char *ptr2, bool *result) {
    *result = (ptr1 == ptr2);
}

extern "C" void mop_pointerLesser(char *ptr1, char *ptr2, bool *result) {
    *result = (ptr1 < ptr2);
}

extern "C" void mop_pointerLesserEquals(char *ptr1, char *ptr2, bool *result) {
    *result = (ptr1 <= ptr2);
}

extern "C" void mop_pointerGreater(char *ptr1, char *ptr2, bool *result) {
    *result = (ptr1 > ptr2);
}

extern "C" void mop_pointerGreaterEquals(char *ptr1, char *ptr2, bool *result) {
    *result = (ptr1 >= ptr2);
}

extern "C" void mop_allocate(int unit, void **result) {
    *result = malloc(unit);
}

extern "C" void mop_free(void *buf) {
    free(buf);
}

extern "C" void mop_allocateBlock(int unit, int n, void **result) {
    char *ptr = (char *)malloc(unit*n + 16);
    ptr += 16;
    ((int *)ptr)[-1] = 0;
    *result = ptr;
}

extern "C" void mop_freeBlock(void *buf) {
    char *ptr = (char *)buf;
    ptr -= 16;
    free(ptr);
}

extern "C" void mop_blockSize(void *buf, int *result) {
    int *ptr = (int *)buf;
    *result = ptr[-1];
}



/*
 * bool
 */



/*
 * char
 */



/*
 * int
 */



/*
 * float
 */



/*
 * double
 */



/*
 * conversions
 */



/*
 * testing
 */

extern "C" void mop_hello() {
    printf("Hello World!\n");
}
