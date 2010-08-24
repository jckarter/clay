
all : clay_binarytrees.exe c_binarytrees.exe cpp_binarytrees.exe

clay_binarytrees.exe : binarytrees.clay
	clay -no-exceptions -o clay_binarytrees.exe binarytrees.clay

c_binarytrees.exe : binarytrees.c
	clang -O3 -I/opt/local/include/apr-1 -o c_binarytrees.exe binarytrees.c -L/opt/local/include -lapr-1

cpp_binarytrees.exe : binarytrees.cpp
	clang++ -O3 -I/opt/local/include -o cpp_binarytrees.exe binarytrees.cpp


clean :
	rm -f clay_binarytrees.exe
	rm -f c_binarytrees.exe
	rm -f cpp_binarytrees.exe
