#include <stdio.h>

extern void clay_main(int *result);

int main() {
    int x = -1;
    clay_main(&x);
    return x;
}
