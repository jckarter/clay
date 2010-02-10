#include <stdio.h>
#include <stdlib.h>

void quickSort(int *a, int size);
void sortRange(int *a, int start, int end);
void reverse(int *a, int size);
void swap(int *x, int *y);

void quickSort(int *a, int size) {
    sortRange(a, 0, size);
}

void sortRange(int *a, int start, int end) {
    if (start < end-1) {
        int m = start;
        for (int i = start+1; i < end; ++i) {
            if (a[i] < a[start]) {
                m += 1;
                swap(&a[i], &a[m]);
            }
        }
        swap(&a[start], &a[m]);
        sortRange(a, start, m);
        sortRange(a, m+1, end);
    }
}

void reverse(int *a, int size) {
    for (int i = 0; i < size/2; ++i)
        swap(&a[i], &a[size-i-1]);
}

void swap(int *x, int *y) {
    int temp = *x;
    *x = *y;
    *y = temp;
}

int main() {
    int n = 1000;
    int *a = (int *)malloc(sizeof(int) * n);
    for (int i = 0; i < n; ++i)
        a[i] = i*2;
    for (int i = 0; i < 1000; ++i) {
        quickSort(a, n);
        reverse(a, n);
    }
    printf("a[0] = %d\n", a[0]);
    return 0;
}
