# Clay Makefile

ROOTDIR	=	../../../
BINDIR	=	$(ROOTDIR)bin/


default :
	@echo "use 'make macosx[32|64]' or 'make linux[32|64]' or 'make windows'."

macosx : macosx64 macosx32

macosx64 :
	gcc -E generated_from_unix.h > temp64.h
	$(BINDIR)clay-bindgen temp64.h > generated.macosx.64.clay
	rm -f temp64.h

macosx32 :
	gcc -E -m32 generated_from_unix.h > temp32.h
	$(BINDIR)clay-bindgen temp32.h > generated.macosx.32.clay
	rm -f temp32.h

linux32 :
	gcc -E -D_FILE_OFFSET_BITS=64 generated_from_unix.h > temp32.h
	$(BINDIR)clay-bindgen temp32.h > generated.linux.32.clay
	rm -f temp32.h

linux64 :
	gcc -E generated_from_unix.h > temp64.h
	$(BINDIR)clay-bindgen temp64.h > generated.linux.64.clay
	rm -f temp64.h


windows : windows32

windows32 :
	gcc -E generated_from_win32.h > temp32.h
	$(BINDIR)clay-bindgen temp32.h > generated.windows.32.clay
	rm -f temp32.h
