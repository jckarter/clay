# Clay Makefile

default :
	@echo "use 'make macosx[32|64]' or 'make IOS_SDK_ROOT=<ROOT> ios' or 'make linux[32|64]' or 'make freebsd[32|64]' or 'make windows' or 'make windows64'."

macosx : macosx64 macosx32

macosx64 :
	clay-bindgen -builtins -m64 generated_from_unix.h -o generated.macosx.x86.64.clay

macosx32 :
	clay-bindgen -builtins -m32 generated_from_unix.h -o generated.macosx.x86.32.clay

IOS_GCC=/Developer/Platforms/iPhoneOS.platform/Developer/usr/bin/gcc

ios:
	clay-bindgen -builtins -isysroot $(IOS_SDK_ROOT) -arch armv6 \
	    generated_from_unix.h -o generated.macosx.arm.clay

linux32 :
	clay-bindgen -m32 -match mathcalls.h -match math.h -match complex.h -import libc -D_FILE_OFFSET_BITS=64 generated_from_unix.h -o generated.linux.32.clay

linux64 :
	clay-bindgen -m64 -match mathcalls.h -match math.h -match complex.h -import libc generated_from_unix.h -o generated.linux.64.clay

freebsd32 :
	clay-bindgen -builtins -D_FILE_OFFSET_BITS=64 generated_from_unix.h -o generated.freebsd.32.clay

freebsd64 :
	clay-bindgen -builtins generated_from_unix.h -o generated.freebsd.64.clay

windows : windows32

windows32 :
	clay-bindgen -builtins generated_from_win32.h -o generated.windows.32.clay

windows64 :
	clay-bindgen -builtins generated_from_win32.h -o generated.windows.64.clay
