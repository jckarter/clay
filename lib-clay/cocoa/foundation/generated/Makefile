all: generated.macosx.x86.32.clay generated.macosx.x86.64.clay

generated.macosx.x86.32.clay: generated_from.h
	clay-bindgen -arch i386 \
	-x objective-c \
	    -o generated.macosx.x86.32.clay \
	    -import libc -import unix -import cocoa.corefoundation \
	    -match /Foundation \
	    generated_from.h
generated.macosx.x86.64.clay: generated_from.h
	clay-bindgen -arch x86_64 \
	    -x objective-c \
	    -o generated.macosx.x86.64.clay \
	    -import libc -import unix -import cocoa.corefoundation \
	    -match /Foundation \
	    generated_from.h

