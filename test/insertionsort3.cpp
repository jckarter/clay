#include <iostream>
#include <vector>

using namespace std;

void insertionSort(int *a, int n) {
    int i = 1;
    while (i < n) {
        int x = a[i];
        int j = i;
        while (a[j-1] > x) {
            a[j] = a[j-1];
            j = j - 1;
            if (j == 0) break;
        }
        if (i != j) {
            a[j] = x;
        }
        i = i + 1;
    }
}

void swap(int &a, int &b) {
    int temp = a;
    a = b;
    b = temp;
}

void reverse(int *a, int n) {
    for (int i = 0; i < n/2; ++i) {
        swap(a[i], a[n-i-1]);
    }
}

int test() {
    int a[1000];
    int n = 1000;
    for (int i = 0; i < n; ++i) {
        a[i] = (i*2);
    }
    for (int i = 0; i < 100; ++i) {
        insertionSort(a, n);
        reverse(a, n);
    }
    return a[0];
}

int main() {
    cout << test() << endl;
    return 0;
}
