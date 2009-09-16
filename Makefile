.PHONY: test
test : test.exe

test.ll : prelude.clay
	python run.py prelude.clay test.ll

test.bc : test.ll
	llvm-as -f -o test.bc test.ll

test-opt.bc : test.bc
	opt -O3 -f -o test-opt.bc test.bc

test.c : test-opt.bc
	llc -march=c -f -o test.c test-opt.bc

test.exe : test.c
	gcc -O3 -o test.exe stub.c test.c

.PHONY: clean
clean :
	rm -f test.exe test.c test-opt.bc test.bc test.ll

.PHONY: all
all : machineops.dylib test

machineops.dylib : machineops/machineops.cpp
	g++ -dynamiclib -Wall -o $@ $^

.PHONY: clean-all
clean-all : clean
	rm -f machineops.dylib
