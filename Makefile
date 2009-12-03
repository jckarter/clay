.cpp.o :
	g++ -MMD -Wall -g -c -o $@ $<

OBJS = \
	evaluator-cpp/error.o \
	evaluator-cpp/printer.o \
	evaluator-cpp/lexer.o \
	evaluator-cpp/parser.o \
	evaluator-cpp/env.o \
	evaluator-cpp/loader.o \
	evaluator-cpp/main.o

clayc : $(OBJS)
	g++ -g -o clayc $(OBJS)

clean :
	rm -f clayc
	rm -f evaluator-cpp/*.o
	rm -f evaluator-cpp/*.d

-include evaluator-cpp/*.d
