LLVM_CXXFLAGS = `llvm-config --cxxflags`
LLVM_LDFLAGS = `llvm-config --ldflags --libs backend engine`

.cpp.o :
	g++ $(LLVM_CXXFLAGS) -MMD -Wall -g -c -o $@ $<

OBJS = \
	src/error.o \
	src/printer.o \
	src/lexer.o \
	src/parser.o \
	src/env.o \
	src/loader.o \
	src/types.o \
	src/evaluator.o \
	src/analyzer.o \
	src/main.o

clayc : $(OBJS)
	g++ -g -o clayc $(OBJS) $(LLVM_LDFLAGS)

clean :
	rm -f clayc
	rm -f src/*.o
	rm -f src/*.d

-include src/*.d
