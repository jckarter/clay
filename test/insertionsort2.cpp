#include <iostream>
#include <vector>

using namespace std;

void insertionSort(vector<int> &a) {
    int i = 1;
    while (i < a.size()) {
        int x = a[i];
        vector<int>::iterator j = a.begin() + i;
        while (*(j-1) > x) {
            *j = *(j-1);
            --j;
            if (j == a.begin()) break;
        }
        *j = x;
        i = i + 1;
    }
}

void swap(int &a, int &b) {
    int temp = a;
    a = b;
    b = temp;
}

void reverse(vector<int> &a) {
    int n = a.size();
    for (int i = 0; i < n/2; ++i) {
        swap(a[i], a[n-i-1]);
    }
}

int test() {
    vector<int> a;
    for (int i = 0; i < 1000; ++i) {
        a.push_back(i*2);
    }
    for (int i = 0; i < 100; ++i) {
        insertionSort(a);
        reverse(a);
    }
    return a[0];
}

int main() {
    cout << test() << endl;
    return 0;
}
