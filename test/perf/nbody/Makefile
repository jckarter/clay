
all : clay_nbody.exe c_nbody.exe cpp_nbody.exe

clay_nbody.exe : nbody.clay
	clay -no-exceptions -o clay_nbody.exe nbody.clay -lm

c_nbody.exe : nbody.c
	clang -O3 -o c_nbody.exe nbody.c

cpp_nbody.exe : nbody.cpp
	clang++ -O3 -o cpp_nbody.exe nbody.cpp


clean :
	rm -f clay_nbody.exe
	rm -f clay_nbody_simd.exe
	rm -f c_nbody.exe
	rm -f cpp_nbody.exe
