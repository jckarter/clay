package main

// import "fmt"

func sortRange(array []int, start, end int) {
	if start < end - 1 {
		m := start;
		for i := start + 1; i < end; i++ {
			if array[i] < array[start] {
				m++
				array[i], array[m] = array[m], array[i];
			}
		}
		array[start], array[m] = array[m], array[start];
		sortRange(array, start, m)
		sortRange(array, m+1, end)
	}
}

func quickSort(array []int) {
	sortRange(array, 0, len(array));
}

func reverse(array []int) {
	for start, end := 0, len(array)-1; start < end; start, end = start+1, end-1 {
		array[start], array[end] = array[end], array[start]
	}
}

func main() {
	var array [1000]int;
	for i := 0; i < 1000; i++ {
		array[i] = i * 2;
	}
	for i := 0; i < 10000; i++ {
		quickSort(array[0:1000]);
		reverse(array[0:1000]);
	}
}
