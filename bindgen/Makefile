
CC = g++
LLVM_CXXFLAGS=`llvm-config --cxxflags`
LLVM_LDFLAGS=`llvm-config --ldflags`
CXXFLAGS=-fno-rtti -m32 -Iinclude/ -Wall -g
LDFLAGS=-m32 -lCIndex

TARGETS=bin/clay-bindgen
OBJS=obj/clay-bindgen.o obj/BindingsConverter.o obj/InitHeaderSearch.o

all:$(TARGETS)

bin/clay-bindgen: $(OBJS)
	if [ ! -e bin ]; then mkdir bin; fi;
	$(CC) $(LLVM_LDFLAGS) $(LDFLAGS) $^ -o $@ 

${OBJS}: obj/%.o : src/%.cpp 
	if [ ! -e obj ]; then mkdir obj; fi;
	$(CC) $(CXXFLAGS) $(LLVM_CXXFLAGS) -c $^ -o $@

.PHONY: clean 

clean :
	rm -rf bin/*
	rm -rf obj/*

