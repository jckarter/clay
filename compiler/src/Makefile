# Clay Makefile

ROOTDIR	=	../../
BINDIR	=	$(ROOTDIR)bin/
DIRS	=	src
include $(ROOTDIR)Makefile.inc

LLVM_CXXFLAGS 	= `llvm-config --cxxflags`
LLVM_LDFLAGS 	= `llvm-config --ldflags --libs all`
CXXFLAGS		+= -Wall

.PHONY: default
default : $(BINDIR)clay

.PHONY: default
32bit :
	$(MAKE) EXTRA_FLAGS=-m32

.cpp.o :
	$(CC) $(LLVM_CXXFLAGS) $(CXXFLAGS) $(EXTRA_FLAGS) -c -MMD -MF$@.dep -o $@ $<

OBJS = \
	error.o \
	printer.o \
	lexer.o \
	parser.o \
	clone.o \
	env.o \
	loader.o \
	llvm.o \
	types.o \
	desugar.o \
	patterns.o \
	lambdas.o \
	invoketables.o \
	matchinvoke.o \
	constructors.o \
	analyzer.o \
	evaluator.o \
	codegen.o \
	main.o

$(BINDIR)clay : $(OBJS)
	if [ ! -e $(BINDIR) ]; then mkdir $(BINDIR); fi;
	$(CC) $(EXTRA_FLAGS) -o $@ $^ $(LLVM_LDFLAGS) $(LDFLAGS)

.PHONY: clean

clean :
	rm -f $(BINDIR)clay
	rm -f *.o
	rm -f *.dep

-include *.dep

