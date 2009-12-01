.cpp.o :
	g++ -MMD -Wall -g -c -o $@ $<

OBJS = \
	evaluator-cpp/main.o

clayc : $(OBJS)
	g++ -g -o clayc $(OBJS)

clean :
	rm -f clayc
	rm -f evaluator-cpp/*.o
	rm -f evaluator-cpp/*.d

-include evaluator-cpp/*.d
