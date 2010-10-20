all: generated.macosx.x86.32.clay generated.macosx.x86.64.clay

CLAY_BINDGEN_FLAGS = \
    -import libc \
    -import unix \
    -import cocoa.corefoundation \
    -match /CoreServices

generated.macosx.x86.32.clay: generated_from.h
	clay-bindgen -arch i386 $(CLAY_BINDGEN_FLAGS) -o $@ $<
generated.macosx.x86.64.clay: generated_from.h
	clay-bindgen -arch x86_64 $(CLAY_BINDGEN_FLAGS) -o $@ $<

