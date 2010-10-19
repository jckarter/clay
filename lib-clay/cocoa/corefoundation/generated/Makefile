all: generated.macosx.x86.32.clay generated.macosx.x86.64.clay

generated.macosx.x86.32.clay: generated_from.h
	clay-bindgen -arch i386 \
	    -o generated.macosx.x86.32.clay \
	    -import libc -import unix \
	    -match CoreFoundation \
	    generated_from.h
generated.macosx.x86.64.clay: generated_from.h
	clay-bindgen -arch x86_64 \
	    -o generated.macosx.x86.64.clay \
	    -import libc -import unix \
	    -match CoreFoundation \
	    generated_from.h

