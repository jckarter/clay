LLVM_CXXFLAGS = `llvm-config --cxxflags`
LLVM_LDFLAGS = `llvm-config --ldflags --libs backend engine`

.cpp.o :
	g++ $(LLVM_CXXFLAGS) -MMD -Wall -g -c -o $@ $<

OBJS = \
	evaluator-cpp/error.o \
	evaluator-cpp/printer.o \
	evaluator-cpp/lexer.o \
	evaluator-cpp/parser.o \
	evaluator-cpp/env.o \
	evaluator-cpp/loader.o \
	evaluator-cpp/types.o \
	evaluator-cpp/evaluator.o \
	evaluator-cpp/main.o

clayc : $(OBJS)
	g++ -g -o clayc $(OBJS) $(LLVM_LDFLAGS)

clean :
	rm -f clayc
	rm -f evaluator-cpp/*.o
	rm -f evaluator-cpp/*.d

-include evaluator-cpp/*.d
