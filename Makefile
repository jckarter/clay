LLVM_CXXFLAGS = `llvm-config --cxxflags`
LLVM_LDFLAGS = `llvm-config --ldflags --libs backend engine`

.PHONY: default
default : clay

.cpp.o :
	g++ $(LLVM_CXXFLAGS) -m32 -Wall -c -MMD -MF$@.dep -o $@ $<

OBJS = \
	compiler/src/error.o \
	compiler/src/printer.o \
	compiler/src/lexer.o \
	compiler/src/parser.o \
	compiler/src/clone.o \
	compiler/src/env.o \
	compiler/src/loader.o \
	compiler/src/llvm.o \
	compiler/src/types.o \
	compiler/src/desugar.o \
	compiler/src/patterns.o \
	compiler/src/lambdas.o \
	compiler/src/invoketables.o \
	compiler/src/matchinvoke.o \
	compiler/src/constructors.o \
	compiler/src/analyzer.o \
	compiler/src/evaluator.o \
	compiler/src/codegen.o \
	compiler/src/main.o

clay : $(OBJS)
	g++ -m32 -o clay $(OBJS) $(LLVM_LDFLAGS)

.PHONY: clean test

test: clay
	echo Looking into subdir $@ 
	cd $@; make

clean :
	rm -f clay
	rm -f compiler/src/*.o
	rm -f compiler/src/*.dep

-include compiler/src/*.dep

a.opt.ll : a.ll
	cat a.ll | llvm-as | opt -O3 | llvm-dis > a.opt.ll

a.s : a.opt.ll
	cat a.opt.ll | llvm-as | llc > a.s

a.out : a.s
	gcc -m32 a.s
