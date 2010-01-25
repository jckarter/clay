LLVM_CXXFLAGS = `llvm-config --cxxflags`
LLVM_LDFLAGS = `llvm-config --ldflags --libs backend engine`

.cpp.o :
	g++ $(LLVM_CXXFLAGS) -m32 -Wall -g -c -MMD -MF$@.dep -o $@ $<

OBJS = \
	src/error.o \
	src/printer.o \
	src/lexer.o \
	src/parser.o \
	src/env.o \
	src/loader.o \
	src/types.o \
	src/invokeutil2.o \
	src/evaluator.o \
	src/partialeval.o \
	src/codegen.o \
	src/main.o

clay2llvm : $(OBJS)
	g++ -m32 -g -o clay2llvm $(OBJS) $(LLVM_LDFLAGS)

clean :
	rm -f clay2llvm
	rm -f src/*.o
	rm -f src/*.dep

-include src/*.dep
