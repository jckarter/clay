#include <iostream>
#include <vector>

using namespace std;

void insertionSortWithPointers(int *begin, int *end) {
    int *i = begin + 1;
    while (i < end) {
        int x = *i;
        int *j = i;
        while (*(j-1) > x) {
            *j = *(j-1);
            --j;
            if (j == begin) break;
        }
        *j = x;
        ++i;
    }
}

void insertionSort(vector<int> &a) {
    int *p = &a[0];
    insertionSortWithPointers(p, p+a.size());
}

// void insertionSort(vector<int> &a) {
//     vector<int>::iterator i = a.begin() + 1;
//     while (i < a.end()) {
//         int x = *i;
//         vector<int>::iterator j = i;
//         while (*(j-1) > x) {
//             *j = *(j-1);
//             --j;
//             if (j == a.begin()) break;
//         }
//         *j = x;
//         ++i;
//     }
// }

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

int main() {
    vector<int> a;
    for (int i = 0; i < 1000; ++i) {
        a.push_back(i*2);
    }
    for (int i = 0; i < 1000; ++i) {
        insertionSort(a);
        reverse(a);
    }
    cout << "a[0] = " << a[0] << '\n';
}
