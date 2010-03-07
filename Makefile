LLVM_CXXFLAGS = `llvm-config --cxxflags`
LLVM_LDFLAGS = `llvm-config --ldflags --libs backend engine`

.PHONY: default
default : clay2llvm

.cpp.o :
	g++ $(LLVM_CXXFLAGS) -m32 -Wall -c -MMD -MF$@.dep -o $@ $<

OBJS = \
	src/error.o \
	src/printer.o \
	src/lexer.o \
	src/parser.o \
	src/clone.o \
	src/env.o \
	src/loader.o \
	src/llvm.o \
	src/types.o \
	src/desugar.o \
	src/patterns.o \
	src/lambdas.o \
	src/invoketables.o \
	src/matchinvoke.o \
	src/constructors.o \
	src/analyzer.o \
	src/evaluator.o \
	src/codegen.o \
	src/main.o

clay2llvm : $(OBJS)
	g++ -m32 -o clay2llvm $(OBJS) $(LLVM_LDFLAGS)

.PHONY: clean test

test: clay2llvm
	echo Looking into subdir $@ 
	cd $@; make

clean :
	rm -f clay2llvm
	rm -f src/*.o
	rm -f src/*.dep

-include src/*.dep
