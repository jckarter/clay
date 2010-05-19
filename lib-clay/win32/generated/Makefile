WIN32_ROOTDIR	=	..\..\..\
WIN32_BINDIR	=	$(WIN32_ROOTDIR)bin\

default :
	@echo "use 'make windows'."

windows : windows32

windows32 :
	gcc -E generated_from.h > temp32.h
	$(WIN32_BINDIR)clay-bindgen temp32.h > generated.windows.32.clay
	rm -f temp32.h
