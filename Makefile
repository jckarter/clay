# Clay Makefile

ROOTDIR	=	./
BINDIR	=	$(ROOTDIR)bin/
DIRS	=	compiler dbg bindgen
include $(ROOTDIR)Makefile.inc

.PHONY: default all clay clay-bindgen clay-dbg clean

default : clay

all: clay clay-bindgen clay-dbg

clay: 
	echo "Looking into subdir compiler"
	cd compiler; make

clay-bindgen: 
	echo "Looking into subdir bindgen"
	cd bindgen; make

clay-dbg: 
	echo "Looking into subdir dbg"
	cd dbg; make

.PHONY: clean

clean:
	-for d in $(DIRS); do (cd $$d; make clean ); done

