

default : clay

all: clay clay-bindgen clay-dbg

clay: 
	cd compiler; $(MAKE)

clay-bindgen: 
	cd misc/bindgen; $(MAKE)

clay-dbg: 
	cd misc/dbg; $(MAKE)

.PHONY: clean

clean:
	cd compiler; $(MAKE) clean
	cd misc/bindgen; $(MAKE) clean
	cd misc/dbg; $(MAKE) clean
