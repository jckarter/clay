
default :
	@echo "use 'make windows'."

windows : windows32

windows32 :
	gcc -E generated_from.h > temp32.h
	clay-bindgen temp32.h > generated.windows.32.clay
	rm -f temp32.h
