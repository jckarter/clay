import sys
sys.setrecursionlimit(2000)

def quickSort(a) :
    sortRange(a, 0, len(a))

def sortRange(a, start, end) :
    if start < end-1 :
        m = start
        for i in range(start+1, end) :
            if a[i] < a[start] :
                m += 1
                a[i], a[m] = a[m], a[i]
        a[start], a[m] = a[m], a[start]
        sortRange(a, start, m)
        sortRange(a, m+1, end)

def reverse(a) :
    n = len(a)
    for i in range(n/2) :
        a[i], a[n-i-1] = a[n-i-1], a[i]

def main() :
    a = [0] * 1000
    for i in range(len(a)) :
        a[i] = i*2
    for i in range(100) :
        quickSort(a)
        reverse(a)
    print "a[0] = %d" % a[0]

main()
