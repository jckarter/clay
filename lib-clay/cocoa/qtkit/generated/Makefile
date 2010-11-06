all: generated.macosx.x86.32.clay generated.macosx.x86.64.clay

CLAY_BINDGEN_FLAGS = -x objective-c \
    -import libc \
    -import cocoa.foundation \
    -import cocoa.appkit \
    -import cocoa.quartzcore \
    -match /QTKit

generated.macosx.x86.32.clay: generated_from.h
	clay-bindgen -arch i386 $(CLAY_BINDGEN_FLAGS) -o $@ $<
generated.macosx.x86.64.clay: generated_from.h
	clay-bindgen -arch x86_64 $(CLAY_BINDGEN_FLAGS) -o $@ $<

