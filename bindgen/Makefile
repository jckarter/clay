# Clay Makefile

ROOTDIR	=	../
BINDIR	=	$(ROOTDIR)bin/
DIRS	=	
include $(ROOTDIR)Makefile.inc

LLVM_CXXFLAGS=`llvm-config --cxxflags`
LLVM_LDFLAGS=`llvm-config --ldflags --libs all`
CXXFLAGS+=-fno-rtti -Iinclude/ -g

CLANG_LDFLAGS = \
	-lclangFrontend -lclangSema -lclangChecker -lclangAnalysis \
	-lclangAST -lclangParse -lclangLex -lclangBasic

TARGETS=$(BINDIR)clay-bindgen
OBJS=obj/clay-bindgen.o obj/BindingsConverter.o obj/InitHeaderSearch.o

all:$(TARGETS)

$(BINDIR)clay-bindgen: $(OBJS)
	if [ ! -e $(BINDIR) ]; then mkdir $(BINDIR); fi;
	$(CC) -o $@ $^ $(CLANG_LDFLAGS) $(LLVM_LDFLAGS)

${OBJS}: obj/%.o : src/%.cpp 
	if [ ! -e obj ]; then mkdir obj; fi;
	$(CC) $(CXXFLAGS) $(LLVM_CXXFLAGS) -c $^ -o $@

.PHONY: clean 

clean :
	rm -rf $(BINDIR)clay-bindgen
	rm -rf obj/*

