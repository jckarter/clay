def insertionSort(a) :
    i = 1
    while i < len(a) :
        x = a[i]
        j = i
        while a[j-1] > x :
            a[j] = a[j-1]
            j = j - 1
            if j == 0 :
                break
        a[j] = x;
        i = i + 1;

def reverse(a) :
    n = len(a)
    for i in range(n/2) :
        temp = a[i]
        a[i] = a[n-i-1]
        a[n-i-1] = temp

def main() :
    a = [0] * 1000
    for i in range(len(a)) :
        a[i] = i * 2
    for i in range(100) :
        insertionSort(a)
        reverse(a)
    return a[0]

if __name__ == "__main__" :
    print main()
