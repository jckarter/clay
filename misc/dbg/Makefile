# Clay Makefile

ROOTDIR	=	../../
BINDIR	=	$(ROOTDIR)bin/
DIRS	=	

CC		 = g++
DEBUG    = -O0 -ggdb -pg
CFLAGS	 = -Wall 
CXXFLAGS = $(CFLAGS)
LDFLAGS	 = -g

LLVM_CXXFLAGS=`llvm-config --cxxflags`
LLVM_LDFLAGS=`llvm-config --ldflags` -lCIndex 
CXXFLAGS+=-fno-rtti -m32 -fexceptions -Iinclude/ -Wall -g -I$(ROOTDIR)/compiler/src
LDFLAGS+=-m32 -lreadline

TARGETS=$(BINDIR)clay-dbg
OBJS=obj/clay-dbg.o obj/Shell.o obj/ShellConsumer.o
LLVM_OBJS=

all:$(TARGETS)

$(BINDIR)clay-dbg: $(OBJS)
	if [ ! -e $(BINDIR) ]; then mkdir $(BINDIR); fi;
	$(CC) $(LLVM_LDFLAGS) $(LDFLAGS) $^ -o $@ 

${OBJS}: obj/%.o : src/%.cpp 
	if [ ! -e obj ]; then mkdir obj; fi;
	$(CC) $(CXXFLAGS) -c $^ -o $@

${LLVM_OBJS}: obj/%.o : src/%.cpp 
	if [ ! -e obj ]; then mkdir obj; fi;
	$(CC) $(CXXFLAGS) $(LLVM_CXXFLAGS) -c $^ -o $@

.PHONY: clean 

clean :
	rm -rf $(BINDIR)clay-dbg
	rm -rf obj/*

