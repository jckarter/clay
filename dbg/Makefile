# Makefile

CC = g++
LLVM_CXXFLAGS=`llvm-config --cxxflags`
LLVM_LDFLAGS=`llvm-config --ldflags` -lCIndex 
CXXFLAGS=-fno-rtti -m32 -fexceptions -Iinclude/ -Wall -g
LDFLAGS=-m32 -lreadline

TARGETS=bin/clay-dbg
OBJS=obj/clay-dbg.o obj/Shell.o obj/ShellConsumer.o
LLVM_OBJS=

all:$(TARGETS)

bin/clay-dbg: $(OBJS)
	if [ ! -e bin ]; then mkdir bin; fi;
	$(CC) $(LLVM_LDFLAGS) $(LDFLAGS) $^ -o $@ 

${OBJS}: obj/%.o : src/%.cpp 
	if [ ! -e obj ]; then mkdir obj; fi;
	$(CC) $(CXXFLAGS) -c $^ -o $@

${LLVM_OBJS}: obj/%.o : src/%.cpp 
	if [ ! -e obj ]; then mkdir obj; fi;
	$(CC) $(CXXFLAGS) $(LLVM_CXXFLAGS) -c $^ -o $@

.PHONY: clean 

clean :
	rm -rf bin/*
	rm -rf obj/*

