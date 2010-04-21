

default :
	@echo "use 'make macosx' or 'make linux' or 'make windows'."

macosx : macosx64 macosx32

macosx64 :
	gcc -E generated_from.h > temp64.h
	clay-bindgen temp64.h > generated.macosx.64.clay
	rm -f temp64.h

macosx32 :
	gcc -E -m32 generated_from.h > temp32.h
	clay-bindgen temp32.h > generated.macosx.32.clay
	rm -f temp32.h

linux : linux32

linux32 :
	gcc -E -D_FILE_OFFSET_BITS=64 generated_from.h > temp32.h
	clay-bindgen temp32.h > generated.linux.32.clay
	rm -f temp32.h

windows : windows32

windows32 :
	gcc -E generated_from.h > temp32.h
	clay-bindgen temp32.h > generated.windows.32.clay
	rm -f temp32.h
