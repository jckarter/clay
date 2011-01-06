
default :
	@echo "use 'make windows' or 'make windows64'."

windows : windows32

windows32 :
	gcc -E generated_from.h > temp32.h
	clay-bindgen temp32.h > generated.windows.32.clay
	rm -f temp32.h

windows64 :
	gcc -E generated_from.h > temp64.h
	clay-bindgen temp64.h > generated.windows.64.clay
	rm -f temp64.h
