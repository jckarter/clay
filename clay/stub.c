#include <stdio.h>

extern void clay_main(int *result);

void _clay_print_int(int *x) {
    printf("%d\n", *x);
}

void _clay_print_bool(char *x) {
    if (*x)
        printf("true\n");
    else
        printf("false\n");
}

int main() {
    int x = -1;
    clay_main(&x);
    printf("%d\n", x);
    return 0;
}
