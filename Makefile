.PHONY: test
test : test.exe

test.s : prelude.clay
	python run.py prelude.clay | llvm-as | llc > test.s

test.exe : test.s
	gcc -o test.exe stub.c test.s

.PHONY: clean
clean :
	rm -f test.exe test.s

.PHONY: all
all : machineops.dylib test

machineops.dylib : machineops/machineops.cpp
	g++ -dynamiclib -Wall -o $@ $^

.PHONY: clean-all
clean-all : clean
	rm -f machineops.dylib
