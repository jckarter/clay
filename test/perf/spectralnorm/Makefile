
all : clay_spectralnorm.exe clay_spectralnorm_simd.exe c_spectralnorm.exe cpp_spectralnorm.exe

clay_spectralnorm.exe : spectralnorm.clay
	clay -no-exceptions -o clay_spectralnorm.exe spectralnorm.clay -lm

clay_spectralnorm_simd.exe : spectralnorm_simd.clay
	clay -no-exceptions -o clay_spectralnorm_simd.exe spectralnorm_simd.clay -lm

c_spectralnorm.exe : spectralnorm.c
	clang -O3 -o c_spectralnorm.exe spectralnorm.c

cpp_spectralnorm.exe : spectralnorm.cpp
	clang++ -O3 -o cpp_spectralnorm.exe spectralnorm.cpp


clean :
	rm -f clay_spectralnorm.exe
	rm -f clay_spectralnorm_simd.exe
	rm -f c_spectralnorm.exe
	rm -f cpp_spectralnorm.exe
