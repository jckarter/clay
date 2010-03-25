LLVM_CXXFLAGS = `llvm-config --cxxflags`
LLVM_LDFLAGS = `llvm-config --ldflags --libs all`

.PHONY: default
default : clay

.PHONY: default
32bit :
	$(MAKE) EXTRA_FLAGS=-m32

.cpp.o :
	g++ $(LLVM_CXXFLAGS) $(EXTRA_FLAGS) -Wall -c -MMD -MF$@.dep -o $@ $<

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
	g++ $(EXTRA_FLAGS) -o clay $(OBJS) $(LLVM_LDFLAGS)

.PHONY: clean

clean :
	rm -f clay
	rm -f compiler/src/*.o
	rm -f compiler/src/*.dep

-include compiler/src/*.dep
