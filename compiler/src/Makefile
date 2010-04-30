# Clay Makefile

ROOTDIR	=	../../
BINDIR	=	$(ROOTDIR)bin/

LLVM_CXXFLAGS 	= `llvm-config --cxxflags | sed -e 's/-fno-exc/-fexc/'`
LLVM_LDFLAGS 	= `llvm-config --ldflags --libs all`

.PHONY: default
default : $(BINDIR)clay

.PHONY: default
32bit :
	$(MAKE) EXTRA_FLAGS=-m32

.cpp.o :
	g++ $(LLVM_CXXFLAGS) $(EXTRA_FLAGS) -Wall -c -MMD -MF$@.dep -o $@ $<

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
	evaluator2.o \
	codegen.o \
	main.o

$(BINDIR)clay : $(OBJS)
	if [ ! -e $(BINDIR) ]; then mkdir $(BINDIR); fi;
	g++ $(EXTRA_FLAGS) -o $@ $^ $(LLVM_LDFLAGS)

.PHONY: clean

clean :
	rm -f $(BINDIR)clay
	rm -f *.o
	rm -f *.dep

-include *.dep

