
all : clay_fannkuch.exe clay_fannkuch2.exe c_fannkuch.exe cpp_fannkuch.exe

clay_fannkuch.exe : fannkuch.clay
	clay -no-exceptions -o clay_fannkuch.exe fannkuch.clay

clay_fannkuch2.exe : fannkuch2.clay
	clay -no-exceptions -o clay_fannkuch2.exe fannkuch2.clay

c_fannkuch.exe : fannkuch.c
	gcc -O3 -o c_fannkuch.exe fannkuch.c

cpp_fannkuch.exe : fannkuch.cpp
	g++ -O3 -o cpp_fannkuch.exe fannkuch.cpp


clean :
	rm -f clay_fannkuch.exe
	rm -f clay_fannkuch2.exe
	rm -f c_fannkuch.exe
	rm -f cpp_fannkuch.exe
