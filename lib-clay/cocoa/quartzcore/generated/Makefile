ifdef IOS_SDK_ROOT
all: generated.macosx.x86.32.clay generated.macosx.x86.64.clay generated.macosx.arm.clay
else
all: generated.macosx.x86.32.clay generated.macosx.x86.64.clay
endif

CLAY_BINDGEN_FLAGS = -x objective-c \
    -import libc \
    -import cocoa.corefoundation \
    -import cocoa.applicationservices \
    -import cocoa.foundation \
    -match /QuartzCore

generated.macosx.x86.32.clay: generated_from.h
	clay-bindgen -arch i386 $(CLAY_BINDGEN_FLAGS) -o $@ $<
generated.macosx.x86.64.clay: generated_from.h
	clay-bindgen -arch x86_64 $(CLAY_BINDGEN_FLAGS) -o $@ $<

generated.macosx.arm.clay: generated_from.h
	clay-bindgen -arch armv6 -isysroot $(IOS_SDK_ROOT) $(CLAY_BINDGEN_FLAGS) -o $@ $<
