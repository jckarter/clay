
all : clay_mandelbrot_non_simd.exe clay_mandelbrot.exe c_mandelbrot.exe cpp_mandelbrot.exe

clay_mandelbrot_non_simd.exe : mandelbrot_non_simd.clay
	clay -no-exceptions -o clay_mandelbrot_non_simd.exe mandelbrot_non_simd.clay -lm

clay_mandelbrot.exe : mandelbrot.clay
	clay -no-exceptions -o clay_mandelbrot.exe mandelbrot.clay -lm

c_mandelbrot.exe : mandelbrot.c
	clang -O3 -o c_mandelbrot.exe mandelbrot.c

cpp_mandelbrot.exe : mandelbrot.cpp
	clang++ -O3 -o cpp_mandelbrot.exe mandelbrot.cpp


clean :
	rm -f clay_mandelbrot_non_simd.exe
	rm -f clay_mandelbrot.exe
	rm -f c_mandelbrot.exe
	rm -f cpp_mandelbrot.exe
