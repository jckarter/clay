# Clay Makefile

default :
	@echo "use 'make macosx[32|64]' or 'make IOS_SDK_ROOT=<ROOT> ios' or 'make linux[32|64]' or 'make freebsd[32|64]'."

macosx : macosx64 macosx32

macosx64 :
	clay-bindgen -m64 generated_from_macosx.h -o generated.macosx.x86.64.clay

macosx32 :
	clay-bindgen -m32 generated_from_macosx.h -o generated.macosx.x86.32.clay

ios:
	clay-bindgen -isysroot $(IOS_SDK_ROOT) -arch armv6 generated_from_macosx.h -o generated.macosx.arm.clay

linux32 :
	clay-bindgen -D_FILE_OFFSET_BITS=64 generated_from_linux.h -o generated.linux.32.clay

linux64 :
	clay-bindgen generated_from_linux.h -o generated.linux.64.clay

freebsd32 :
	clay-bindgen generated_from_freebsd.h -o generated.freebsd.32.clay

freebsd64 :
	clay-bindgen generated_from_freebsd.h -o generated.freebsd.64.clay
