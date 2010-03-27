# Clay Makefile

ROOTDIR	=	../
BINDIR	=	$(ROOTDIR)bin/
DIRS	=	
include $(ROOTDIR)Makefile.inc

LLVM_CXXFLAGS=`llvm-config --cxxflags`
LLVM_LDFLAGS=`llvm-config --ldflags --libs all`
CXXFLAGS+=-fno-rtti -Iinclude/ -g

CLANG_LDFLAGS = \
	-lclangAST -lclangBasic -lclangFrontend -lclangLex \
	-lclangIndex -lclangParse -lclangSema -lclangAnalysis -lclangIndex

TARGETS=$(BINDIR)clay-bindgen
OBJS=obj/clay-bindgen.o obj/BindingsConverter.o obj/InitHeaderSearch.o

all:$(TARGETS)

$(BINDIR)clay-bindgen: $(OBJS)
	if [ ! -e $(BINDIR) ]; then mkdir $(BINDIR); fi;
	$(CC) $(LLVM_LDFLAGS) $(CLANG_LDFLAGS) $^ -o $@

${OBJS}: obj/%.o : src/%.cpp 
	if [ ! -e obj ]; then mkdir obj; fi;
	$(CC) $(CXXFLAGS) $(LLVM_CXXFLAGS) -c $^ -o $@

.PHONY: clean 

clean :
	rm -rf $(BINDIR)clay-bindgen
	rm -rf obj/*

