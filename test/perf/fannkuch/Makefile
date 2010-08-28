
all : clay_fannkuch.exe c_fannkuch.exe cpp_fannkuch.exe

clay_fannkuch.exe : fannkuch.clay
	clay -no-exceptions -o clay_fannkuch.exe fannkuch.clay

c_fannkuch.exe : fannkuch.c
	clang -O3 -o c_fannkuch.exe fannkuch.c

cpp_fannkuch.exe : fannkuch.cpp
	clang++ -O3 -o cpp_fannkuch.exe fannkuch.cpp


clean :
	rm -f clay_fannkuch.exe
	rm -f c_fannkuch.exe
	rm -f cpp_fannkuch.exe
