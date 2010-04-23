

default : clay

all: clay clay-bindgen clay-dbg

clay: 
	cd compiler; make

clay-bindgen: 
	cd misc/bindgen; make

clay-dbg: 
	cd misc/dbg; make

.PHONY: clean

clean:
	cd compiler; make clean
	cd misc/bindgen; make clean
	cd misc/dbg; make clean
