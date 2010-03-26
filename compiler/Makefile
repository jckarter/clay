# Clay Makefile

ROOTDIR	=	../
BINDIR	=	$(ROOTDIR)bin/
DIRS	=	src
include $(ROOTDIR)Makefile.inc

.PHONY: clay clean

all: clay 

clay: 
	echo "Looking into subdir src"
	cd src; make

.PHONY: clean

clean:
	-for d in $(DIRS); do (cd $$d; make clean ); done

