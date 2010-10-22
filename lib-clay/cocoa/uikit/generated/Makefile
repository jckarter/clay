all: generated.macosx.arm.clay

CLAY_BINDGEN_FLAGS = -x objective-c \
    -import libc \
    -import cocoa.corefoundation \
    -import cocoa.applicationservices \
    -import cocoa.foundation \
    -import cocoa.quartzcore \
    -match /UIKit

generated.macosx.arm.clay: generated_from.h
	clay-bindgen -arch armv6 -isysroot $(IOS_SDK_ROOT) $(CLAY_BINDGEN_FLAGS) -o $@ $<

