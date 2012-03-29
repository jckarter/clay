package main

func test() float {
	var result float;
	for i := 0; i < 100000; i++ {
		result = 0
		for j := 0; j < 10000; j++ {
			result += float(j);
		}
	}
	result = result / 10000;
    return result;
}

func main() {
     test();
}
