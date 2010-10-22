# Clay Makefile

default :
	@echo "use 'make macosx[32|64]' or 'make IOS_SDK_ROOT=<ROOT> ios' or 'make linux[32|64]'."

macosx : macosx64 macosx32

macosx64 :
	gcc -E generated_from_macosx.h > temp64.h
	clay-bindgen temp64.h > generated.macosx.x86.64.clay
	rm -f temp64.h

macosx32 :
	gcc -E -m32 generated_from_macosx.h > temp32.h
	clay-bindgen temp32.h > generated.macosx.x86.32.clay
	rm -f temp32.h

IOS_GCC=/Developer/Platforms/iPhoneOS.platform/Developer/usr/bin/gcc

ios:
	$(IOS_GCC) -isysroot $(IOS_SDK_ROOT) -arch armv6 -E generated_from_macosx.h >temp32.h
	clay-bindgen temp32.h >generated.macosx.arm.clay
	rm -f temp32.h

linux32 :
	gcc -E -D_FILE_OFFSET_BITS=64 generated_from_linux.h > temp32.h
	clay-bindgen temp32.h > generated.linux.32.clay
	rm -f temp32.h

linux64 :
	gcc -E generated_from_linux.h > temp64.h
	clay-bindgen temp64.h > generated.linux.64.clay
	rm -f temp64.h
