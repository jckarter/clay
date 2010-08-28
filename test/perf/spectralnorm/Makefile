
all : clay_spectralnorm_non_simd.exe clay_spectralnorm.exe c_spectralnorm.exe cpp_spectralnorm.exe

clay_spectralnorm_non_simd.exe : spectralnorm_non_simd.clay
	clay -no-exceptions -o clay_spectralnorm_non_simd.exe spectralnorm_non_simd.clay -lm

clay_spectralnorm.exe : spectralnorm.clay
	clay -no-exceptions -o clay_spectralnorm.exe spectralnorm.clay -lm

c_spectralnorm.exe : spectralnorm.c
	clang -O3 -o c_spectralnorm.exe spectralnorm.c

cpp_spectralnorm.exe : spectralnorm.cpp
	clang++ -O3 -o cpp_spectralnorm.exe spectralnorm.cpp


clean :
	rm -f clay_spectralnorm_non_simd.exe
	rm -f clay_spectralnorm.exe
	rm -f c_spectralnorm.exe
	rm -f cpp_spectralnorm.exe
